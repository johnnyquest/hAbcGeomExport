/**
		@file		hAbcGeomExport.cpp
		@author		xy
		@since		2011-11-29



		Run the command
			
			hcustom hAbcGeomExport.cpp

		to build.


*/

#include "hAbcGeomExport.h"


#include <cassert>

#include <iostream>
#include <vector>
#include <map>

#include <UT/UT_DSOVersion.h>
#include <CH/CH_LocalVariable.h>
#include <PRM/PRM_Include.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_Director.h>
#include <SOP/SOP_Node.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>

#include <GEO/GEO_Point.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Vertex.h>
#include <GEO/GEO_PrimPoly.h>

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>


namespace Abc = Alembic::Abc;
namespace AbcGeom = Alembic::AbcGeom;


// always-debug for now, TODO: remove this
#undef NDEBUG


#ifndef NDEBUG
#define DBG if (true) std::cerr << "[hAbcGeomExport.cpp]: "
#define dbg if (true) std::cerr
#else
#define DBG if () std::cerr << "[hAbcGeomExport.cpp]: "
#define dbg if () std::cerr
#endif


using namespace std;
using namespace HDK_Sample;



// static (shared) per-class data
//
Alembic::AbcGeom::OArchive * GeoObject::_oarchive(0);
Alembic::AbcGeom::TimeSampling * GeoObject::_ts(0);


int *			hAbcGeomExport::ifdIndirect = 0;

static PRM_Name		prm_objpath("objpath", "Path to Root Object");
static PRM_Default	prm_objpath_d(0, "/obj");

static PRM_Name		prm_abcoutput("abcoutput", "Save to file");
static PRM_Default	prm_abcoutput_d(0, "./out.abc");





static PRM_Template * getTemplates()
{
	static PRM_Template *t=0;
	if (t) return t;

	t = new PRM_Template[15]; // should equal to the c++ lines below
	int c=0;
	t[c++] = PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &prm_objpath, &prm_objpath_d);
	t[c++] = PRM_Template(PRM_FILE, 1, &prm_abcoutput, &prm_abcoutput_d);
	t[c++] = theRopTemplates[ROP_TPRERENDER_TPLATE];
	t[c++] = theRopTemplates[ROP_PRERENDER_TPLATE];
	t[c++] = theRopTemplates[ROP_LPRERENDER_TPLATE];
	t[c++] = theRopTemplates[ROP_TPREFRAME_TPLATE];
	t[c++] = theRopTemplates[ROP_PREFRAME_TPLATE];
	t[c++] = theRopTemplates[ROP_LPREFRAME_TPLATE];
	t[c++] = theRopTemplates[ROP_TPOSTFRAME_TPLATE];
	t[c++] = theRopTemplates[ROP_POSTFRAME_TPLATE];
	t[c++] = theRopTemplates[ROP_LPOSTFRAME_TPLATE];
	t[c++] = theRopTemplates[ROP_TPOSTRENDER_TPLATE];
	t[c++] = theRopTemplates[ROP_POSTRENDER_TPLATE];
	t[c++] = theRopTemplates[ROP_LPOSTRENDER_TPLATE];
	t[c++] = PRM_Template();
	return t;
}





OP_TemplatePair * hAbcGeomExport::getTemplatePair()
{
	static OP_TemplatePair *ropPair=0;

	if (!ropPair) {
		OP_TemplatePair *base;
		base = new OP_TemplatePair(getTemplates());
		ropPair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), base);
	}
	return ropPair;
}





OP_VariablePair * hAbcGeomExport::getVariablePair()
{
	static OP_VariablePair *pair=0;
	if (!pair) pair = new OP_VariablePair(ROP_Node::myVariableList);
	return pair;
}





OP_Node * hAbcGeomExport::myConstructor(
	OP_Network * net,
	const char *name,
	OP_Operator * op
)
{
	return new hAbcGeomExport(net, name, op);
}





hAbcGeomExport::hAbcGeomExport(
	OP_Network * net, 
	const char *name,
	OP_Operator * entry
)
: ROP_Node(net, name, entry)
, _oarchive(0)
{
	if (!ifdIndirect)
		ifdIndirect = allocIndirect(16);
}





hAbcGeomExport::~hAbcGeomExport()
{
}





/**		GeoObject, constructor.
*/
GeoObject::GeoObject( OP_node *obj_node, GeoObject *parent )
: _parent(parent)
, _op_obj(obj_node)
, _op_sop( (SOP_Node *) obj_node->getRenderNodePtr() )
, _name( obj_node->getName() )
, _path( obj_node->getPath() )
, _sopname( _op_sop->getName() )
, _xform(0)
, _outmesh(0)
{
	DBG << " --- GeoObject() " << obj_node->getPath() << "\n";
	assert(_op_sop && "no SOP node");
	
	DBG << "   -- " << _path << " (" << _name << "): " << _sopname << "\n";

	assert(_oarchive && "no oarchive given");
	assert(_ts && "no timesampling given");

	Alembic::AbcGeom::OXform *p =
		_parent  ?  _parent->_xform  :  &_oarchive->getTop();

	assert(p && "no valid parent found");
	
	_xform = new Alembic::AbcGeom::OXform(*p, _name, _ts);
	_outmesh = new Alembic::AbcGeom::OPolyMesh(*_xform, _sopname, _ts);
}



/**		GeoObject, destructor.
*/
GeoObject::~GeoObject()
{
	DBG << " --- ~GeoObject() " << _path << "\n";
	if (_outmesh) delete _outmesh; _outmesh=0;
	if (_xform) delete _xform; _xform=0;
}



/**		GeoObject: write a sample (xform+geom) for the specified time.

@TODO
		- export normals/uvs (support both per-point and per-vertex)
		- export point velocities
		- export other attributes
		- support for particles
*/
bool GeoObject::writeSample( float time )
{
	assert(_op_sop && "no SOP node");
	assert(_xform && "no abc output xform");
	assert(_outmesh && "no abc outmesh");

	// * xform sample *
	//
	Alembic::AbcGeom::XformSample xform_samp;
	// TODO: fill the xform sample with the proper data (local transformations)
	_xform->getSchema().set(xform_samp);


	// * geom sample *
	//
	GU_DetailHandle gdh = _op_sop->getCookedGeoHandle( OP_Context(time) );
	GU_DetailHandleAutoReadLock gdl(gdh);
	const GU_Detail *gdp = gdl.getGdp();

	if (!gdp) {
		addError(ROP_COOK_ERROR, pathname());
		//addError(ROP_COOK_ERROR, sop_name());
		return false;
	}


	// collect polymesh data
	//
	std::map<GEO_Point const *, int> ptmap; // this should be replaced if possible

	std::vector<Abc::float32_t>	g_pts;			// point coordinates
	std::vector<Abc::int32_t>	g_pts_ids;		// point indices for each per-face-vertex
	std::vector<Abc::int32_t>	g_facevtxcounts;	// vertex count for each face

	GEO_Point const		*pt;
	GEO_Primitive const	*prim;

	// collect point coords
	//
	int c=0;
	FOR_ALL_GPOINTS(gdp, pt)
	{
		UT_Vector4 const & P = pt->getPos();
		g_pts.push_back(P.x());
		g_pts.push_back(P.y());
		g_pts.push_back(P.z());
		
		ptmap[pt]=c; // store point in point->ptindex map
		++c;
	}

	// collect primitives
	//
	FOR_ALL_PRIMITIVES(gdp, prim)
	{
		int prim_id = prim->getPrimitiveId();

		if ( prim_id == GEOPRIMPOLY )
		{
			int num_verts = prim->getVertexCount();
			g_facevtxcounts.push_back(num_verts);

			for(int v=0; v<num_verts; ++v) {
				pt = prim->getVertex(v).getPt();
				g_pts_ids.push_back( ptmap[pt] );
			}
		}
	}

	// construct mesh sample
	//
	AbcGeom::OPolyMeshSchema::Sample mesh_samp(
		AbcGeom::V3fArraySample( (const AbcGeom::V3f *)&g_pts[0], g_pts.size()/3 ),
		AbcGeom::Int32ArraySample( &g_pts_ids[0], g_pts_ids.size() ),
		AbcGeom::Int32ArraySample( &g_facevtxcounts[0], g_facevtxcounts.size() )
	);

	_outmesh->getSchema().set(mesh_samp); // export mesh sample

	return true;
}







/**		Collect all objects to be exported (including all children).
*/
void collect_geo_objs( GeoObjects & objects, OP_Node *node )
{
	if (objects.size()==0) DBG << "collect_geo_objs()\n";
	DBG << " -- " << node->getPath() << "\n";

	boost::shared_ptr<GeoObject> obj( new GeoObject(node) );
	objects.push_back(obj);
	
	for( int i=0, m=obj_node->getNchildren();  i<m;  ++i )
		collect_geo_objs(objects, node->getChild(i));
}





/**		Called by Houdini before the rendering of frame(s).
*/
int hAbcGeomExport::startRender( int nframes, float tstart, float tend )
{
	DBG << "startRender(): " << nframes << " (" << tstart << " -> " << tend << ")\n";

	_start_time = tstart;
	_end_time = tend;
	_num_frames = nframes;

	if (false) {
		// TODO: init simulation OPs
		// (got this from ROP_Field3D.C)
		initSimulationOPs();
		OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(1);
	}

	UT_String	objpath_name,
			abcfile_name;

	get_str_parm("objpath", tstart, objpath_name);
	get_str_parm("abcoutput", tstart, abcfile_name);
	
	_objpath = objpath_name.toStdString();
	_abcfile = abcfile_name.toStdString();

	DBG	<< "START EXPORT"
		<< "\n -- obj path: " << _objpath
		<< "\n -- abc file: " << _abcfile
		<< "\n";

	OP_Node *root_obj = getObjNode(_objpath.c_str());

	if ( !root_obj ) {
		addError(ROP_MESSAGE, "ERROR: couldn't find object");
		addError(ROP_MESSAGE, _objpath.c_str());
		return false;
	}

	if (error() < UT_ERROR_ABORT) {
		if (!executePreRenderScript(tstart))
			return false;
	}

	// NOTE: this needs to be dynamically allocated, so we can
	// explicitly destroy it (to trigger the final flush-to-disk)
	//
	_oarchive = new Alembic::AbcGeom::OArchive(Alembic::AbcCoreHDF5::WriteArchive(), _abcfile);
	// TODO: add metadata (see CreateArchiveWithInfo func)

	// time-sampler with the appropriate timestep
	//
	float t_step = tend-tstart;
	if (nframes>1) t_step /= float(nframes-1);
	DBG << " -- time step: " << t_step << "(@24fps it's " << (1.0/24.0) << ")\n";

	_ts = AbcGeom::TimeSamplingPtr( new AbcGeom::TimeSampling(t_step, tstart) );

	// build list of objects
	//
	GeoObject::init(_oarchive, _ts);
	_objs.clear();
	collect_geo_objs(_objs, root_obj);

	return true;
}





/**		Render (export) one frame (called by Houdini for each frame).

		(Can return ROP_CONTINUE_RENDER, ROP_ABORT_RENDER, ROP_RETRY_RENDER)
*/
ROP_RENDER_CODE hAbcGeomExport::renderFrame( float time, UT_Interrupt * )
{
	DBG << "renderFrame() time=" << time << "\n";

	executePreFrameScript(time); // run pre-frame cmd

	for( GeoObjects::iterator i=_objs.begin(), m=_objs.end();  i!=m;  ++i )
	{
		char const *obj_name = i->pathname();

		DBG << " - " << obj_name << "\n";
		bool r = i->writeSample(time);

		if (!r) {
			addError(ROP_MESSAGE, "failed to export object");
			addError(ROP_MESSAGE, obj_name);
			return ROP_ABORT_RENDER;
		}
	}

	if (error() < UT_ERROR_ABORT)
		executePostFrameScript(time); // run post-frame cmd

	return ROP_CONTINUE_RENDER;
}




/**		Called by Houdini on render (export) finish.

		This function is always called (even on user abort),
		so it provides a reliable cleanup/exit point.
@TODO
		Check if this function is called if renderFrame() returns ROP_ABORT_RENDER
*/
ROP_RENDER_CODE hAbcGeomExport::endRender()
{
	DBG << "endRender()\n";

	// delete the output archive 'stream'
	// (so it gets flushed to disk)
	//
	if (_oarchive) delete _oarchive;
	_oarchive=0;

	if (error() < UT_ERROR_ABORT)
		executePostRenderScript(_end_time); // run post-render cmd

	return ROP_CONTINUE_RENDER;
}




/**		Function that installs our ROP node.
*/
void newDriverOperator(OP_OperatorTable * table)
{
	OP_Operator *abc_rop = new OP_Operator(
		"hAbcGeomExport",
		"Alembic Geometry Export",
		hAbcGeomExport::myConstructor,
		hAbcGeomExport::getTemplatePair(),
		0, 0,
		hAbcGeomExport::getVariablePair(),
		OP_FLAG_GENERATOR
	);

	// set icon
	abc_rop->setIconName("SOP_alembic");

	// install operator
	table->addOperator(abc_rop);

	DBG
		<< __FILE__
		<< ": "
		<< __DATE__
		<< ", "
		<< __TIME__
		<< "\n";
}





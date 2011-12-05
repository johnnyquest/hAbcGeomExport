/**
		@file		hAbcGeomExport.cpp
		@author		Imre Tuske
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
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>

#include <GU/GU_Detail.h>
#include <GEO/GEO_Point.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Vertex.h>
#include <GEO/GEO_PrimPoly.h>
#include <GEO/GEO_AttributeHandle.h>

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
#define DBG if (false) std::cerr << "[hAbcGeomExport.cpp]: "
#define dbg if (false) std::cerr
#endif


using namespace std;
using namespace HDK_Sample;



// static (shared) per-class data
//
Alembic::AbcGeom::OArchive * GeoObject::_oarchive(0);
Alembic::AbcGeom::TimeSamplingPtr GeoObject::_ts;


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
GeoObject::GeoObject( OP_Node *obj_node, GeoObject *parent )
: _parent(parent)
//, _op_obj( (OBJ_Node *) obj_node) // TODO: make sure this is an OBJ_Node!
, _op_sop( (SOP_Node *) ((OBJ_Node *)obj_node)->getRenderSopPtr() )
, _name( obj_node->getName() )
//, _path( obj_node->getPath() )
, _sopname( _op_sop->getName() )
, _xform(0)
, _outmesh(0)
{
	UT_String s; obj_node->getFullPath(s);
	_path = s.toStdString();

	DBG << " --- GeoObject() " << _path << "\n";
	assert(_op_sop && "no SOP node");

	_op_obj = (OBJ_Node *) obj_node; // TODO: make sure this is an OBJ_Node!
	
	DBG << "   -- " << _path << " (" << _name << "): " << _sopname << "\n";

	assert(_oarchive && "no oarchive given");
	assert(_ts && "no timesampling given");
/*
	Alembic::AbcGeom::OObject *p =
		_parent  ?  _parent->_xform
		:  &_oarchive->getTop();

	DBG << " --- parent " << p << " (" << parent << ")\n";
	assert(p && "no valid parent found");
	
	_xform = new Alembic::AbcGeom::OXform(*p, _name, _ts);
*/
	_xform = new Alembic::AbcGeom::OXform(
		_parent ? *(_parent->_xform) : _oarchive->getTop(),
		_name, _ts);
	
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
	DBG << "writeSample() " << _path << " @ " << time << "\n";
	assert(_op_sop && "no SOP node");
	assert(_xform && "no abc output xform");
	assert(_outmesh && "no abc outmesh");

	OP_Context ctx(time);


	// * xform sample *
	//
	Alembic::AbcGeom::XformSample xform_samp;
	// TODO: fill the xform sample with the proper data (local transformations)
	// with hints and all (how to include preTransform elegantly?)

	UT_DMatrix4 const & hou_prexform = _op_obj->getPreTransform();
	UT_DMatrix4 hou_dmtx;
	
	_op_obj->getParmTransform(ctx, hou_dmtx);
	hou_dmtx = hou_prexform * hou_dmtx; // apply pretransform

	AbcGeom::M44d mtx( (const double (*)[4]) hou_dmtx.data() );
	xform_samp.setMatrix(mtx);

	_xform->getSchema().set(xform_samp); // export xform sample


	// * geom sample *
	//
	GU_DetailHandle gdh = _op_sop->getCookedGeoHandle(ctx);
	GU_DetailHandleAutoReadLock gdl(gdh);
	const GU_Detail *gdp = gdl.getGdp();

	if (!gdp)
		return false;

	GEO_AttributeHandle	h_pN = gdp->getPointAttribute("N"),
				h_vN = gdp->getVertexAttribute("N"),
				h_pUV = gdp->getPointAttribute("uv"),
				h_vUV = gdp->getVertexAttribute("uv");
	
	bool	N_pt   = h_pN.isAttributeValid(),
		N_vtx  = h_vN.isAttributeValid(),
		uv_pt  = h_pUV.isAttributeValid(),
		uv_vtx = h_vUV.isAttributeValid(),
		has_N  = N_pt  || N_vtx,
		has_uv = uv_pt || uv_vtx;

	DBG	<< " - ATTRS:"
		<< " has_N:" << has_N
		<< " N_pt:" << N_pt
		<< " N_vtx:" << N_vtx
		<< " has_uv:" << has_uv
		<< " uv_pt:" << uv_pt
		<< " uv_vtx:" << uv_vtx
		<< "\n";


	// collect polymesh data
	//
	std::map<GEO_Point const *, int> ptmap; 	// (this should be replaced if possible)

	std::vector<Abc::float32_t>	g_pts;			// point coordinates (3 values)
	std::vector<Abc::int32_t>	g_pts_ids;		// point indices for each per-face-vertex
	std::vector<Abc::int32_t>	g_facevtxcounts;	// vertex count for each face
	
	std::vector<Abc::float32_t>	g_N;			// normals (3 values; per-point or per-vertex)
	std::vector<Abc::float32_t>	g_uv;			// uv coords (2 values; per-point or per-vertex)

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

		// collect per-point normals/uvs
		//
		UT_Vector3 V;

		if ( N_pt ) {
			h_pN.setElement(pt);
			V = h_pN.getV3();
			g_N.push_back(V.x());
			g_N.push_back(V.y());
			g_N.push_back(V.z());
			//DBG << " -- pN: " << V.x() << " " << V.y() << " " << V.z() << "\n";
		}

		if ( uv_pt ) {
			h_pUV.setElement(pt);
			V = h_pUV.getV3();
			g_uv.push_back(V.x());
			g_uv.push_back(V.y());
			//DBG << " -- pUV: " << V.x() << " " << V.y() << " " << V.z() << "\n";
		}

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
			UT_Vector3 V;

			for(int v=0; v<num_verts; ++v)
			{
				GEO_Vertex const & vtx = prim->getVertex(v);
				pt = vtx.getPt();
				g_pts_ids.push_back( ptmap[pt] );

				// collect per-vertex normals/uvs
				//
				if ( N_vtx ) {
					h_vN.setElement(&vtx);
					V = h_vN.getV3();
					g_N.push_back(V.x());
					g_N.push_back(V.y());
					g_N.push_back(V.z());
					//DBG << " -- vN: " << V.x() << " " << V.y() << " " << V.z() << "\n";
				}

				if ( uv_vtx ) {
					h_vUV.setElement(&vtx);
					V = h_vUV.getV3();
					g_uv.push_back(V.x());
					g_uv.push_back(V.y());
					//DBG << " -- vUV: " << V.x() << " " << V.y() << " " << V.z() << "\n";
				}
			}
		}
	}

	AbcGeom::ON3fGeomParam::Sample N_samp;
	AbcGeom::OV2fGeomParam::Sample uv_samp;

	if ( has_N ) {
		N_samp.setScope( N_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVaryingScope );
		N_samp.setVals( AbcGeom::N3fArraySample( (const AbcGeom::N3f *)&g_N[0], g_N.size()/3) );
	}

	if ( has_uv ) {
		uv_samp.setScope( uv_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVaryingScope );
		uv_samp.setVals( AbcGeom::V2fArraySample( (const AbcGeom::V2f *)&g_uv[0], g_uv.size()/2) );
	}

	// construct mesh sample
	//
	AbcGeom::OPolyMeshSchema::Sample mesh_samp(
		AbcGeom::V3fArraySample( (const AbcGeom::V3f *)&g_pts[0], g_pts.size()/3 ),
		AbcGeom::Int32ArraySample( &g_pts_ids[0], g_pts_ids.size() ),
		AbcGeom::Int32ArraySample( &g_facevtxcounts[0], g_facevtxcounts.size() ),
		uv_samp, N_samp
	);

	_outmesh->getSchema().set(mesh_samp); // export mesh sample

	return true;
}







/**		Collect all objects to be exported (including all children).
*/
void collect_geo_objs( GeoObjects & objects, OP_Node *node, GeoObject *parent=0 )
{
	if (objects.size()==0) DBG << "collect_geo_objs()\n";
	DBG << " -- " << node->getName() << "\n";

	boost::shared_ptr<GeoObject> obj( new GeoObject(node, parent) );
	objects.push_back(obj);
	
	for( int i=0, m=node->nOutputs();  i<m;  ++i ) {
		DBG << i << " (parent will be " << obj.get() << ")\n";
		collect_geo_objs(objects, node->getOutput(i), obj.get());
	}
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

	OP_Node *root_obj = findNode(_objpath.c_str());

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
	_oarchive = new Alembic::AbcGeom::OArchive(
		Alembic::AbcCoreHDF5::WriteArchive(),
		_abcfile);
	// TODO: add metadata (see CreateArchiveWithInfo func)

	// time-sampler with the appropriate timestep
	//
	float t_step = tend-tstart;
	if (t_step<=0) t_step = 1.0/24.0;
	if (nframes>1) t_step /= float(nframes-1);
	_t_step = t_step;
	DBG << " -- time step: " << t_step << " (@24fps it's " << (1.0/24.0) << ")\n";

	_ts = AbcGeom::TimeSamplingPtr( new AbcGeom::TimeSampling(t_step, tstart+t_step) );

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
		char const *obj_name = (*i)->pathname();

		DBG << " - " << obj_name << "\n";
		bool r = (*i)->writeSample(time);

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
	_objs.clear();

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

	// print the regular startup message to stderr
	//
	std::cerr
		<< "** hAbcGeomExport ROP 0.01 ** (compiled "
		<< __DATE__ << ", " << __TIME__ << ") "
#ifdef NDEBUG
		<< "release build"
#endif
#ifdef DEBUG
		<< "DEBUG build"
#endif
		<< "\n";
}



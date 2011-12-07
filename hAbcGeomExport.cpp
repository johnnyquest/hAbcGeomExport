/**
		@file		hAbcGeomExport.cpp
		@author		Imre Tuske
		@since		2011-11-29

		@brief		Alembic Geometry Export ROP (implementation).

		Based on a HDK ROP example and the Maya Alembic export code
		by Lucas Miller.

@todo
		Figure out how to do proper debug/release builds with hcustom!

@todo
		If/when implementing primgroups-to-facesets functionality,
		make sure it behaves symmetrically with the importer (ie.
		primitive groups will be re-created on import).

*/

#define PLUGIN_VERSION_STR "0.02"
#define _DEBUG


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

#include <GB/GB_AttributeDictOffsetIterator.h>

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>


namespace Abc = Alembic::Abc;
namespace AbcGeom = Alembic::AbcGeom;



#ifdef _DEBUG
#define DBG if (true) std::cerr << "[hAbcGeomExport.cpp " << __LINE__ << "]: "
#define dbg if (true) std::cerr
#else
#define DBG if (false) std::cerr
#define dbg if (false) std::cerr
#endif


using namespace std;
using namespace HDK_AbcExportSimple;



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





/**		Collect all attributes from an attribute dict.
		(Used to get all point/prim/vertex/... attribute names).
*/
void get_attrs(
	GB_AttributeDict & dict,
	AttrArray & names,
	char const *type="?" // TODO: remove this
)
{
	DBG << "get_attr_names() (" << type << ")\n";
	GB_AttributeDictOffsetIterator it(dict);

	for(; !it.atEnd(); ++it) {
		GB_Attribute *attr = it.attrib();
		names[ attr->getName() ] = attr;
		DBG
			<< " - ATTR:" << attr->getName()
			<< " type=" << attr->getType()
			<< " size=" << attr->getSize()
			<< "\n";
	}
}



/**		Get the size of a given attribute type (in bytes).
*/
size_t get_attribtype_size( GB_AttribType t )
{
	typedef  std::map<GB_AttribType, size_t> SizeMap;
	static SizeMap _gb_ts;

	if ( _gb_ts.size()==0 ) {
		// one-time init of static map
		_gb_ts[GB_ATTRIB_INT] = sizeof(int);
		_gb_ts[GB_ATTRIB_FLOAT] = sizeof(float);
		_gb_ts[GB_ATTRIB_VECTOR] = sizeof(float)*3; // TODO: this is to be corrected!
		dbg << _gb_ts.size();
	}

	SizeMap::const_iterator i = _gb_ts.find(t);
	size_t r =  i!=_gb_ts.end()  ?  i->second  :  0;

	assert(r>0 && "unknown/unsupported type...");
	return r;
}



/**		Get number of components for an attribute.
*/
int get_num_comps( GB_Attribute *attr ) {
	assert(attr);
	return attr->getSize() / get_attribtype_size(attr->getType());
}



/**	Store 2 components of a vector in a container.
*/
template<class T, class V> inline void push_v2( T & container, V const & v ) {
	container.push_back(v.x());
	container.push_back(v.y());
}

/**	Store 3 components of a vector in a container.
*/
template<class T, class V> inline void push_v3( T & container, V const & v ) {
	container.push_back(v.x());
	container.push_back(v.y());
	container.push_back(v.z());
}






/**		GeoObject, constructor.
*/
GeoObject::GeoObject( OP_Node *obj_node, GeoObject *parent )
: _parent(parent)
, _op_sop( (SOP_Node *) ((OBJ_Node *)obj_node)->getRenderSopPtr() )
, _name( obj_node->getName() )
, _sopname( _op_sop ? _op_sop->getName() : "<no SOP>" )
, _xform(0)
, _outmesh(0)
{
	UT_String s; obj_node->getFullPath(s);
	_path = s.toStdString();

	// TODO: make sure this is an OBJ_Node!
	_op_obj = (OBJ_Node *) obj_node;

	dbg << "(" << _path << "): " << _sopname;

	assert(_oarchive && "no oarchive given");
	assert(_ts && "no timesampling given");

	_xform = new Alembic::AbcGeom::OXform(
		_parent ? *(_parent->_xform) : _oarchive->getTop(),
		_name, _ts);

	if ( _op_obj->getObjectType()==OBJ_GEOMETRY  && _op_sop )
	{
		dbg << " [GEO]";
		_outmesh = new Alembic::AbcGeom::OPolyMesh(*_xform, _sopname, _ts);
	}
	else {
		dbg << " [NULL]";
		_outmesh = 0;
	}
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
		- (DONE) export normals/uvs (support both per-point and per-vertex)
		- export point velocities
		- export other attributes
		- support for particles
*/
bool GeoObject::writeSample( float time )
{
	dbg << "sample for " << _path << " @ " << time << ": ";
	assert(_xform && "no abc output xform");

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

	if ( _outmesh==0 ) {
		dbg << "null/xform\n";
		return true;
	}


	// * geom sample *
	//
	dbg << "GEO\n";

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

	typedef std::vector<Abc::float32_t> FloatVec;

	FloatVec			g_pts;			// point coordinates (3 values)
	std::vector<Abc::int32_t>	g_pts_ids;		// point indices for each per-face-vertex
	std::vector<Abc::int32_t>	g_facevtxcounts;	// vertex count for each face
	
	FloatVec			g_N;			// normals (3 values; per-point or per-vertex)
	FloatVec			g_uv;			// uv coords (2 values; per-point or per-vertex)

	GEO_Point const		*pt;
	GEO_Primitive const	*prim;

	// collect point coords
	//
	int c=0;
	FOR_ALL_GPOINTS(gdp, pt)
	{
		UT_Vector4 const & P = pt->getPos();
		push_v3<FloatVec, UT_Vector4>(g_pts, P);

		// collect per-point normals/uvs
		//
		UT_Vector3 V;

		if ( N_pt ) {
			h_pN.setElement(pt);
			V = h_pN.getV3();
			push_v3<FloatVec, UT_Vector3>(g_N, V);
		}

		if ( uv_pt ) {
			h_pUV.setElement(pt);
			V = h_pUV.getV3();
			push_v2<FloatVec, UT_Vector3>(g_uv, V);
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
					push_v3<FloatVec, UT_Vector3>(g_N, V);
				}

				if ( uv_vtx ) {
					h_vUV.setElement(&vtx);
					V = h_vUV.getV3();
					push_v2<FloatVec, UT_Vector3>(g_uv, V);
				}
			}
		}
	}

	AbcGeom::ON3fGeomParam::Sample N_samp;
	AbcGeom::OV2fGeomParam::Sample uv_samp;

	if ( has_N ) {
		N_samp.setScope( N_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVertexScope );
		N_samp.setVals( AbcGeom::N3fArraySample( (const AbcGeom::N3f *)&g_N[0], g_N.size()/3) );
	}

	if ( has_uv ) {
		uv_samp.setScope( uv_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVertexScope );
		uv_samp.setVals( AbcGeom::V2fArraySample( (const AbcGeom::V2f *)&g_uv[0], g_uv.size()/2) );
	}


	// collect other geometry attribute data
	// (NOTE: make sure these dict don't get out of scope until the attrs are exported!)
	//
	GEO_PointAttribDict	d_pt = gdp->pointAttribs();
	GEO_PrimAttribDict	d_pr = gdp->primitiveAttribs();
	GEO_VertexAttribDict	d_vtx = gdp->vertexAttribs();
	GB_AttributeTable	d_dtl = gdp->attribs();

	AttrArray	attrs_pt,
			attrs_prim,
			attrs_vtx,
			attrs_detail;
	if (true)
	{
		get_attrs(d_pt, attrs_pt, "point");		// AbcGeom::kVertexScope
		get_attrs(d_pr, attrs_prim, "prim");		// AbcGeom::kUniformScope
		get_attrs(d_vtx, attrs_vtx, "vtx");		// AbcGeom::kFacevaryingScope
		get_attrs(d_dtl, attrs_detail, "detail");	// AbcGeom::kConstantScope
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
void collect_geo_objs(
	GeoObjects &	objects,
	OP_Node *	node,
	GeoObject *	parent=0,
	int		depth=1
)
{
	if (objects.size()==0)
		DBG << "Collecting object(s) to export\n";

	DBG << " | " << string(depth, '-') << " " << node->getName() << " ";

	boost::shared_ptr<GeoObject> obj( new GeoObject(node, parent) );
	objects.push_back(obj);

	int m = node->nOutputs();
	if (m>0) dbg << " c:" << m;
	dbg << "\n";
	
	for( int i=0; i<m; ++i ) {
		collect_geo_objs(objects, node->getOutput(i), obj.get(), depth+1);
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

		DBG << "- " << obj_name << ": ";
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
		<< "** hAbcGeomExport ROP " PLUGIN_VERSION_STR " ** (compiled "
		<< __DATE__ << ", " << __TIME__ << ") "
#ifndef _DEBUG
		<< "release build"
#else
		<< "DEBUG build"
#endif
		<< "\n";
}

#undef PLUGIN_VERSION_STR


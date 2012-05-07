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

#define PLUGIN_VERSION_STR "0.03"


#include "hAbcGeomExport.h"

#include <cassert>

#include <iostream>
#include <vector>
#include <map>

#include <UT/UT_Version.h>
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

#if UT_MAJOR_VERSION_INT >= 12
// ...
#include <GA/GA_AttributeDict.h>
#else
#include <GB/GB_AttributeDictOffsetIterator.h>
#endif

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>

#include <time.h>



namespace Abc = Alembic::Abc;
namespace AbcGeom = Alembic::AbcGeom;



#ifdef _DEBUG
#define DBG if (true) std::cerr << "[hAbcGeomExport.cpp:" << __LINE__ << "]: "
#define dbg if (true) std::cerr
#else
#define DBG if (false) std::cerr
#define dbg if (false) std::cerr
#endif


using namespace std;
using namespace HDK_AbcExportSimple;



// static (shared) per-class data
//

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
/*
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
*/


/**		Get the size of a given attribute type (in bytes).
*/
/*
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
*/


/**		Get number of components for an attribute.
*/
/*
int get_num_comps( GB_Attribute *attr ) {
	assert(attr);
	return attr->getSize() / get_attribtype_size(attr->getType());
}
*/












/**		Collect all objects to be exported (including all children).
		Specify zero depth to collect a single object only (ie. disable hierarchy).
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

	if (depth<=0)
		return;

	int m = node->nOutputs();
	if (m>0) dbg << " c:" << m;
	dbg << "\n";
	
	for( int i=0; i<m; ++i ) {
		collect_geo_objs(objects, node->getOutput(i), obj.get(), depth+1);
	}
}





/**		Called by Houdini before the rendering of frame(s).
*/
int hAbcGeomExport::startRender( int nframes, fpreal tstart, fpreal tend )
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
ROP_RENDER_CODE hAbcGeomExport::renderFrame( fpreal now, UT_Interrupt * )
{
	DBG << "renderFrame() now=" << now << "\n";
	clock_t t = clock();

	executePreFrameScript(now); // run pre-frame cmd

	for( GeoObjects::iterator i=_objs.begin(), m=_objs.end();  i!=m;  ++i )
	{
		char const *obj_name = (*i)->pathname();

		DBG
			<< "- " << obj_name << ": "
			<< "\n";
		bool r = (*i)->writeSample(now);

		if (!r) {
			addError(ROP_MESSAGE, "failed to export object");
			addError(ROP_MESSAGE, obj_name);
			return ROP_ABORT_RENDER;
		}
	}

	if (error() < UT_ERROR_ABORT)
		executePostFrameScript(now); // run post-frame cmd

	t = clock()-t;
	
	//DBG << " -- elapsed " << t << "\n";

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
	GeoObject::cleanup();

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


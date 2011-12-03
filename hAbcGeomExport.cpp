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
#include <fstream>
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

static PRM_Name		prm_soppath("soppath", "SOP Path");
static PRM_Default	prm_soppath_d(0, "dunno");

static PRM_Name		prm_abcoutput("abcoutput", "Save to file");
static PRM_Default	prm_abcoutput_d(0, "./out.abc");





static PRM_Template * getTemplates()
{
	static PRM_Template * t = 0;

	if (t)
		return t;

	t = new PRM_Template[15]; // should equal to the c++ lines below

	int c=0;
	t[c++] = PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &prm_soppath, &prm_soppath_d);
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
	static OP_TemplatePair *ropPair = 0;

	if (!ropPair)
	{
		OP_TemplatePair *base;

		base = new OP_TemplatePair(getTemplates());
		ropPair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), base);
	}
	return ropPair;
}





OP_VariablePair * hAbcGeomExport::getVariablePair()
{
	static OP_VariablePair *pair = 0;
	if (!pair)
		pair = new OP_VariablePair(ROP_Node::myVariableList);
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
, _sopnode(0)
, _oarchive(0)
, _xform(0)
, _outmesh(0)
{
	if (!ifdIndirect)
		ifdIndirect = allocIndirect(16);
}





hAbcGeomExport::~hAbcGeomExport()
{
}



/*
		Geometry export function:

		polymesh
		- point coordinates
		- normals (per-point, per-vertex)
		- uvs (per-point, per-vertex)
		- per-face: vertex counts
		- per-vertex: point indices for each per-face vertex
*/



int abc_fileSave(
	AbcGeom::OPolyMesh *	outmesh,
	float			time,
	GEO_Detail const *	gdp,
	char const *		filename
)
{
	GEO_Point const		*pt;
	GEO_Primitive const	*prim;

	int		num_points = gdp->points().entries(),
			num_prims  = gdp->primitives().entries();
	
	DBG
		<< "NUM POINTS: " << num_points
		<< "\nNUM PRIMS: " << num_prims
		<< "\n";

	std::map<GEO_Point const *, int> ptmap; // this should be replaced if possible

	std::vector<Abc::float32_t>	g_pts;
	std::vector<Abc::int32_t>	g_pts_ids;
	std::vector<Abc::int32_t>	g_facevtxcounts;
	size_t				g_num_pts=0,
					g_num_facevtxcounts=0,
					g_num_pts_ids=0;


	// collect point coords
	//
	//DBG << "POINTS:\n";
	int c=0;
	FOR_ALL_GPOINTS(gdp, pt)
	{
		UT_Vector4 const & P = pt->getPos();
		ptmap[pt]=c;
		g_pts.push_back(P.x());
		g_pts.push_back(P.y());
		g_pts.push_back(P.z());
		++c;
	}

	g_num_pts = c;
	DBG << " --- g_num_pts: " << g_num_pts << " (/3!)\n";


	// collect primitives
	//
	//DBG << "PRIMITIVES:\n";
	FOR_ALL_PRIMITIVES(gdp, prim)
	{
		int prim_id = prim->getPrimitiveId();

		//DBG << " ---- " << prim_id;

		if ( prim_id == GEOPRIMPOLY )
		{
			int	num_verts = prim->getVertexCount(),
				v, pti;

			g_facevtxcounts.push_back(num_verts);
			//dbg << " (" << num_verts << ") ";

			for(v=0; v<num_verts; ++v)
			{
				pt = prim->getVertex(v).getPt();
				pti = ptmap[pt];
				//dbg << " " << pti;

				g_pts_ids.push_back(pti);
			}
		}

		//dbg << "\n";
	}

	g_num_pts_ids = g_pts_ids.size();
	g_num_facevtxcounts = g_facevtxcounts.size();

	// write to output archive
	//
	AbcGeom::OPolyMeshSchema::Sample mesh_samp(
		AbcGeom::V3fArraySample( (const AbcGeom::V3f *)&g_pts[0], g_num_pts ),
		AbcGeom::Int32ArraySample( &g_pts_ids[0], g_num_pts_ids ),
		AbcGeom::Int32ArraySample( &g_facevtxcounts[0], g_num_facevtxcounts )
	);
/*
	AbcGeom::TimeSamplingPtr ts( new AbcGeom::TimeSampling(1.0/24.0, time) );
	AbcGeom::OXform xform(oarchive->getTop(), "xform", ts);
	AbcGeom::XformSample xform_samp;

	xform.getSchema().set(xform_samp);
*/
	//AbcGeom::OPolyMesh outmesh(xform, "meshukku", ts);
	outmesh->getSchema().set(mesh_samp);

	return 1;
}






bool hAbcGeomExport::export_geom( char const *sopname, SOP_Node *sop, float time )
{
	DBG
		<< "export_geom()"
		<< " sop:" << sop
		<< "\n";

	OP_Context ctx(time);
	GU_DetailHandle gdh = sop->getCookedGeoHandle(ctx);

	GU_DetailHandleAutoReadLock gdl(gdh);

	const GU_Detail *gdp = gdl.getGdp();

	if (!gdp) {
		addError(ROP_COOK_ERROR, sopname);
		return false;
	}

	abc_fileSave(_outmesh, time, gdp, "dunnno-whatt");

	return true;
}






//------------------------------------------------------------------------------
// The startRender(), renderFrame(), and endRender() render methods are
// invoked by Houdini when the ROP runs.





int hAbcGeomExport::startRender( int nframes, float tstart, float tend )
{
	DBG
		<< "startRender()"
		<< nframes << " "
		<< tstart << " "
		<< tend
		<< "\n";

	_start_time = tstart;
	_end_time = tend;
	_num_frames = nframes;

	if (false)
	{
		// TODO: init simulation OPs
		// (got this from ROP_Field3D.C)
		initSimulationOPs();
		OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(1);
	}

	UT_String	soppath_name,
			abcfile_name;

	get_str_parm("soppath", tstart, soppath_name);
	get_str_parm("abcoutput", tstart, abcfile_name);
	
	_soppath = soppath_name.toStdString();
	_abcfile = abcfile_name.toStdString();

	//_sopnode = OPgetDirector()->findSOPNode(_soppath.c_str());
	_sopnode = getSOPNode(_soppath.c_str());

	DBG
		<< " -- soppath:" << _soppath.c_str()
		<< " sopnode:" << _sopnode
		<< "\n";

	if (!_sopnode)
	{
		addError(ROP_MESSAGE, "ERROR: couldn't find SOP node");
		addError(ROP_MESSAGE, _soppath.c_str());
		return false;
	}

	if (error() < UT_ERROR_ABORT)
	{
		if (!executePreRenderScript(tstart))
			return false;
	}
/*
	_archie = CreateArchiveWithInfo(
			Alembic::AbcCoreHDF5::WriteArchive(),
			_abcfile,
			"houdini x.y, exporter y.z (appWriter)",
			"exported from: (...).hip (userInfo)",
			Alembic::Abc::ErrorHandler::kThrowPolicy
		);
*/
	// this dyn-allocated to allow destroy-by-hand
	// (the only way to write to file)
	_oarchive = new Alembic::AbcGeom::OArchive(Alembic::AbcCoreHDF5::WriteArchive(), _abcfile);
	// TODO: add metadata

	_ts = AbcGeom::TimeSamplingPtr( new AbcGeom::TimeSampling(1.0/24.0, tstart) );
	_xform = new AbcGeom::OXform(_oarchive->getTop(), "xformukku"); // ..., ts);
	_outmesh = new AbcGeom::OPolyMesh(*_xform, "meshukku", _ts);

	return true;
}





static void printNode( ostream & os, OP_Node * node, int indent )
{
	UT_WorkBuffer wbuf;
	wbuf.sprintf("%*s", indent, "");
	os << wbuf.buffer() << node->getName() << endl;

	for(int i=0;  i<node->getNchildren();  ++i)
		printNode(os, node->getChild(i), indent+2);
}






ROP_RENDER_CODE hAbcGeomExport::renderFrame( float time, UT_Interrupt * )
{
	DBG << "renderFrame()\n";

	// Execute the pre-render script.
	executePreFrameScript(time);

	// Evaluate the parameter for the file name and write something to the
	// file.
	UT_String	soppath_name,
			abc_file_name;

	//get_str_parm("soppath", time, soppath_name);
	get_str_parm("abcoutput", time, abc_file_name);

	_sopnode = getSOPNode(_soppath.c_str());

	DBG	
		<< " -- time:" << time
		<< " soppath:" << _soppath
		<< " sopnode:" << _sopnode
		<< " file:" << abc_file_name << "\n";

	if ( !export_geom(_soppath.c_str(), _sopnode, time) )
	{
		// ERROR: couldn't export SOP geometry
		return ROP_ABORT_RENDER;
	}


	if (false)
	{
		ofstream os(abc_file_name);
		printNode(os, OPgetDirector(), 0);
		os.close();
	}

	// Execute the post-render script.
	if (error() < UT_ERROR_ABORT)
		executePostFrameScript(time);

	return ROP_CONTINUE_RENDER;
	// ROP_CONTINUE_RENDER, ROP_ABORT_RENDER, ROP_RETRY_RENDER
}





ROP_RENDER_CODE hAbcGeomExport::endRender()
{
	DBG << "endRender()\n";

	if (_oarchive) delete _oarchive;
	_oarchive=0;

	if (_xform) delete _xform;
	_xform=0;

	if (_outmesh) delete _outmesh;
	_outmesh=0;

	if (error() < UT_ERROR_ABORT)
		executePostRenderScript(_end_time);

	return ROP_CONTINUE_RENDER;
}




/**		Function that installs our ROP node.
*/
void newDriverOperator(OP_OperatorTable * table)
{
	OP_Operator *abc_rop = new OP_Operator(
			"hAbcGeomExport",
			"Alembic Geo Export",
			hAbcGeomExport::myConstructor,
			hAbcGeomExport::getTemplatePair(),
			0,
			0,
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





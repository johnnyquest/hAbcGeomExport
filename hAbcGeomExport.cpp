/**
		@file		hAbcGeomExport.cpp
		@author		xy
		@since		2011-11-29



		Run the command
			
			hcustom hAbcGeomExport.cpp

		to build.


*/

#include <fstream.h>

#include <UT/UT_DSOVersion.h>
#include <CH/CH_LocalVariable.h>
#include <PRM/PRM_Include.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_Director.h>
#include <SOP/SOP_Node.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <GEO/GEO_Point.h>

#include "hAbcGeomExport.h"


#include <iostream>


#define DBG if (true) std::cerr << "[hAbcGeomExport.cpp]: "


using namespace HDK_Sample;




int *			hAbcGeomExport::ifdIndirect = 0;

static PRM_Name		prm_soppath("soppath", "SOP Path");
static PRM_Default	prm_soppath_d(0, "dunno");

static PRM_Name		prm_abcoutput("abcoutput", "Save to file");
static PRM_Default	prm_abcoutput_d(0, "junk.out");





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
	GEO_Detail const *	gdp,
	char const *		filename
)
{
	GEO_Point const *pt;

	std::vector<UT_Vector4> pts;

	FOR_ALL_GPOINTS(gdp, pt)
	{
		pts.push_back( UT_Vector4( pt->getPos() ) );
	}

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

	UT_String	s;
	get_str_parm("soppath", _start_time, s);
	_soppath = s.toStdString();
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

	return true;
}





static void printNode( ostream & os, OP_Node * node, int indent )
{
	UT_WorkBuffer wbuf;
	wbuf.sprintf ("%*s", indent, "");
	os << wbuf.buffer () << node->getName () << endl;

	for(int i=0;  i<node->getNchildren ();  ++i)
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

	if (error() < UT_ERROR_ABORT)
		executePostRenderScript(_end_time);

	return ROP_CONTINUE_RENDER;
}






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

	abc_rop->setIconName("SOP_alembic");

	table->addOperator(abc_rop);
}





/**
		@file		hAbcGeomExport.cpp
		@author		xy
		@since		2011-11-29

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
#include "hAbcGeomExport.h"


#include <iostream>


#define DBG if (true) std::cerr << "[hAbcGeomExport.cpp]: "


using namespace HDK_Sample;




int *			hAbcGeomExport::ifdIndirect = 0;

static PRM_Name		theFileName ("file", "Save to file");
static PRM_Default	theFileDefault (0, "junk.out");




static PRM_Template * getTemplates()
{
	static PRM_Template * theTemplate = 0;

	if (theTemplate)
		return theTemplate;

	theTemplate = new PRM_Template[14];
	theTemplate[0] = PRM_Template (PRM_FILE, 1, &theFileName, &theFileDefault);
	theTemplate[1] = theRopTemplates[ROP_TPRERENDER_TPLATE];
	theTemplate[2] = theRopTemplates[ROP_PRERENDER_TPLATE];
	theTemplate[3] = theRopTemplates[ROP_LPRERENDER_TPLATE];
	theTemplate[4] = theRopTemplates[ROP_TPREFRAME_TPLATE];
	theTemplate[5] = theRopTemplates[ROP_PREFRAME_TPLATE];
	theTemplate[6] = theRopTemplates[ROP_LPREFRAME_TPLATE];
	theTemplate[7] = theRopTemplates[ROP_TPOSTFRAME_TPLATE];
	theTemplate[8] = theRopTemplates[ROP_POSTFRAME_TPLATE];
	theTemplate[9] = theRopTemplates[ROP_LPOSTFRAME_TPLATE];
	theTemplate[10] = theRopTemplates[ROP_TPOSTRENDER_TPLATE];
	theTemplate[11] = theRopTemplates[ROP_POSTRENDER_TPLATE];
	theTemplate[12] = theRopTemplates[ROP_LPOSTRENDER_TPLATE];
	theTemplate[13] = PRM_Template ();

	return theTemplate;
}





OP_TemplatePair * hAbcGeomExport::getTemplatePair()
{
	static OP_TemplatePair *ropPair = 0;
	if (!ropPair)
	  {
		  OP_TemplatePair *base;

		  base = new OP_TemplatePair (getTemplates ());
		  ropPair =
			  new OP_TemplatePair (ROP_Node::
					       getROPbaseTemplate (), base);
	  }
	return ropPair;
}




OP_VariablePair * hAbcGeomExport::getVariablePair()
{
	static OP_VariablePair *pair = 0;
	if (!pair)
		pair = new OP_VariablePair (ROP_Node::myVariableList);
	return pair;
}




OP_Node * hAbcGeomExport::myConstructor( OP_Network * net, const char *name, OP_Operator * op )
{
	return new hAbcGeomExport (net, name, op);
}





hAbcGeomExport::hAbcGeomExport(
	OP_Network * net, 
	const char *name,
	OP_Operator * entry
)
: ROP_Node (net, name, entry)
{

	if (!ifdIndirect)
		ifdIndirect = allocIndirect (16);
}


hAbcGeomExport::~hAbcGeomExport()
{
}




//------------------------------------------------------------------------------
// The startRender(), renderFrame(), and endRender() render methods are
// invoked by Houdini when the ROP runs.





int hAbcGeomExport::startRender( int nframes, float tstart, float tend )
{
	DBG << "startRender()" << nframes << " " << tstart << " " << tend <<
		"\n";

	myEndTime = tend;
	if (error () < UT_ERROR_ABORT)
		executePreRenderScript (tstart);

	return 1;
}





static void printNode( ostream & os, OP_Node * node, int indent )
{
	UT_WorkBuffer wbuf;
	wbuf.sprintf ("%*s", indent, "");
	os << wbuf.buffer () << node->getName () << endl;

	for (int i = 0; i < node->getNchildren (); ++i)
		printNode (os, node->getChild (i), indent + 2);
}






ROP_RENDER_CODE hAbcGeomExport::renderFrame( float time, UT_Interrupt * )
{
	DBG << "renderFrame()\n";

	// Execute the pre-render script.
	executePreFrameScript (time);

	// Evaluate the parameter for the file name and write something to the
	// file.
	UT_String file_name;
	OUTPUT (file_name, time);

	ofstream os (file_name);
	printNode (os, OPgetDirector (), 0);
	os.close ();

	// Execute the post-render script.
	if (error () < UT_ERROR_ABORT)
		executePostFrameScript (time);

	return ROP_CONTINUE_RENDER;
}





ROP_RENDER_CODE hAbcGeomExport::endRender()
{
	DBG << "endRender()\n";

	if (error () < UT_ERROR_ABORT)
		executePostRenderScript (myEndTime);
	return ROP_CONTINUE_RENDER;
}






void
newDriverOperator (OP_OperatorTable * table)
{
	table->addOperator (new OP_Operator ("hAbcGeomExport",
					     "Alembic Geo Export",
					     hAbcGeomExport::myConstructor,
					     hAbcGeomExport::
					     getTemplatePair (), 0, 0,
					     hAbcGeomExport::
					     getVariablePair (),
					     OP_FLAG_GENERATOR));
}


/**
		@file		abcexportctrl.cpp
		@author		Imre Tuske
		@since		2011-12-13

		@brief		Quick-and-dirty wrapper cmd for alembic calls not available in its python API.

		Doing this as a hscript command is kinda awkward
		but I'm following the path of least resistance. :|
		Peace.

*/

/*
		commands:

		- new oarchive + time sampling
		- new object (xform + optional mesh shape)
		- object: output new xform matrix sample
		- object: output new (mesh) geometry sample
		- cleanup: delete all objects (xform+mesh), time sampling, oarchive

		oarchive <filename>
		timesampling <v1> <v2>
		newobject <ObjNode_name> <parent obj name> <SOP_name>
		xformsample <time> <objname> <matrix>
		geosample <time> <objname> [<sopname>]
		cleanup
*/

#define PLUGIN_VERSION_STR "0.01"

// TODO: remove this as soon as debugging is over
#define _DEBUG


#include <CMD/CMD_Args.h>
#include <CMD/CMD_Manager.h>

#include <string.h>
#include <malloc.h>
#include <math.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "GeoObject.h"


namespace Abc = Alembic::Abc;
namespace AbcGeom = Alembic::AbcGeom;



#ifdef _DEBUG
#define DBG if (true) std::cerr << "[abcexportctrl.cpp " << __LINE__ << "]: "
#define dbg if (true) std::cerr
#else
#define DBG if (false) std::cerr
#define dbg if (false) std::cerr
#endif



using namespace std;
using namespace HDK_AbcExportSimple;



static Alembic::AbcGeom::OArchive *		_oarchive=0;
static Alembic::AbcGeom::TimeSamplingPtr	_ts;
static HDK_AbcExportSimple::GeoObjects		_objs;



typedef std::map<std::string, GeoObject *>	ObjMap;
static ObjMap					_objmap;



GeoObject * find_obj( std::string & objname, bool do_throw=true )
{
	DBG << "( find_obj() " << objname << " )\n";
	ObjMap::iterator p = _objmap.find(objname);
	if (p==_objmap.end()) {
		if (do_throw) throw("couldn't look up "+objname);
		else return 0;
	}
	DBG << "( find_obj() " << objname << " -> " << (p->second) << " )\n";
	return p->second;
}




/**		Callback for abcexportctrl.
*/
static void cmd_abcexportctrl( CMD_Args & args )
{
	try
	{
		int argc = args.argc();

		if (argc<2)
			throw(std::string("not enough arguments"));

		string func(args(1));
		DBG << "func: " << func << "\n";


#define CHK(n, msg) if (argc<n+2) throw(std::string("not enough arguments--usage: " msg))

		if (func=="oarchive")
		{
			CHK(3, "oarchive <output file> <timestep> <timestart>");
			std::string	abc_file(args(2));
			fpreal		step = atof(args(3)),
					start = atof(args(4));

			DBG << "NEW OARCHIVE"
				<< " file=" << abc_file
				<< " step=" << step
				<< " start=" << start
				<< "\n";

			_oarchive = new Alembic::AbcGeom::OArchive(
				Alembic::AbcCoreHDF5::WriteArchive(),
				abc_file);
			
			_ts = AbcGeom::TimeSamplingPtr(
				new AbcGeom::TimeSampling(step, start)
			);

			GeoObject::init(_oarchive, _ts);
			_objs.clear();
			_objmap.clear();
		}
		else if (func=="timesampling")
		{
			CHK(2, "timesampling <timestep> <timestart>");
			;
		}
		else if (func=="newobject")
		{
			CHK(3, "newobject <obj full pathname> <parent full pathname> <obj outname> [<sop full pathname>]");
			std::string	objpath(args(2)),
					parentp(args(3)),
					outname(args(4)),
					soppath(args(5));

			DBG << "NEW OBJECT"
				<< " obj=" << objpath
				<< " parent=" << parentp
				<< " obj=" << outname
				<< " sop=" << soppath
				<< "\n";

			;
		}
		else if (func=="xformsample")
		{
			CHK(2+16, "xformsample <time> <obj_name> <matrix array ... (16 floats)>");
			fpreal		now=atof(args(2));
			std::string	objpath(args(3));
			// TODO: read matrix from args (16 floats)

			DBG << "XFORM SAMPLE"
				<< " time=" << time
				<< " obj=" << objpath
				<< "\n";

			;
		}
		else if (func=="geosample")
		{
			CHK(2, "geosample <time> <obj_name>");
			fpreal		now=atof(args(2));
			std::string	objpath(args(3));
			
			DBG << "GEO SAMPLE"
				<< " time=" << time
				<< " obj=" << objpath
				<< "\n";

			;
		}
		else if (func=="cleanup")
		{
			DBG << "CLEANUP--...\n";

			// TODO: finish this

			_objmap.clear();
			_objs.clear();
			
			if (_oarchive) delete _oarchive;
			_oarchive=0;
		}
		else
		{
			throw("unsupported function "+func);
		}
#undef CHK

		// TODO: test code, remove
		//
		if ( args.found('e') )
		{
			args.out() << "found -e option with: " << args.argp('e') << "...\n";

			args.err() << "this is an error (?)\n";
			args.showUsage();
		}

		// test: dump all arguments
		//
		if (true) {
			args.out() << "arguments:\n";
			for(int i=0, m=args.argc();  i<m;  ++i)
				args.out() << i << ".: " << args(i) << "\n";
		}
	}

	catch( std::string & e )
	{
		args.err() << "ERROR: " << e << "\n";
	}
}







/**		Install cmd.
*/
void CMDextendLibrary( CMD_Manager *cmgr )
{
	// install command
	cmgr->installCommand("abcexportctrl", "", cmd_abcexportctrl);

	// print the regular startup message to stderr
	//
	std::cerr
		<< "** abcexportctrl (hscript) " PLUGIN_VERSION_STR " ** (compiled "
		<< __DATE__ << ", " << __TIME__ << ") "
#ifndef _DEBUG
		<< "release build"
#else
		<< "DEBUG build"
#endif
		<< "\n";
}

#undef PLUGIN_VERSION_STR


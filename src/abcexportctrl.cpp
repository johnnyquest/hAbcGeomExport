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


#include <OP/OP_Director.h>
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


static HDK_AbcExportSimple::GeoObjects		_objs;


typedef std::map<std::string, GeoObject *>	ObjMap;
static ObjMap					_objmap;



GeoObject * find_obj( std::string & objname, bool do_throw=true )
{
	//DBG << "( find_obj() " << objname << " )\n";
	ObjMap::iterator p = _objmap.find(objname);

	if (p==_objmap.end()) {
		//DBG << " -- (not found)\n";
		if (do_throw) throw("couldn't look up "+objname);
		else return 0;
	}
	//DBG << "( find_obj() " << objname << "==" << (p->second) << " )\n";
	return p->second;
}




/**		Callback for abcexportctrl.
*/
static void cmd_abcexportctrl( CMD_Args & args )
{
	try
	{
		int argc = args.argc();
		if (argc<2) throw(std::string("not enough arguments"));
		string func(args(1));

#define CHK(n, msg) if (argc<n+2) throw(std::string("not enough arguments--usage: " msg))

		if (func=="oarchive")
		{
			CHK(3, "oarchive <output file> <timestep> <timestart>");
			std::string	abc_file(args(2));
			fpreal		step = atof(args(3)),
					start = atof(args(4));

			DBG << "NEW OARCHIVE"
				<< "\n\tfile=  " << abc_file
				<< "\n\tstep=  " << step
				<< "\n\tstart= " << start
				<< "\n";
			
			AbcGeom::TimeSamplingPtr
				ts( new AbcGeom::TimeSampling(step, start) );

			GeoObject::init(
				new Alembic::AbcGeom::OArchive(
					Alembic::AbcCoreHDF5::WriteArchive(),
					abc_file), ts);

			_objs.clear();
			_objmap.clear();
		}
		else if (func=="newobject")
		{
			CHK(4, "newobject <obj full pathname> <obj-src pathname> <parent full pathname> <obj outname> [<sop full pathname>]");
			std::string	objpath(args(2)),
					obj_src(args(3)),
					parentp(args(4)),
					outname(args(5)),
					soppath(args(6));

			DBG << "NEW OBJECT"
				<< "\n\tobj=     " << objpath
				<< "\n\tobj_src= " << obj_src
				<< "\n\tparent=  " << parentp
				<< "\n\toutname= " << outname
				<< "\n\tsop=     " << soppath
				<< "\n\n";

			if ( find_obj(objpath, false)==0 )
			{
				// add object
				//
				GeoObject	*parent = find_obj(parentp, false);
				OP_Node		*objnode = OPgetDirector()->findNode(obj_src.c_str());
				SOP_Node	*sopnode = (SOP_Node *)OPgetDirector()->findNode(soppath.c_str());

				if (!objnode)
					throw("couldn't find obj "+obj_src);

				DBG << "--objnode:" << objnode << " parent=" << parent << " ";

				boost::shared_ptr<GeoObject> obj(
					new GeoObject(objnode, parent,
					sopnode, &outname) );

				_objs.push_back(obj);
				_objmap[objpath] = obj.get();
			}
			else throw("object "+objpath+" already added");

		}
		else if (func=="writesample")
		{
			// write an xform (+geom--optional) sample

			CHK(2, "writesample <time> <obj_name> [<matrix(4x4>]");
			fpreal		now=atof(args(2));
			std::string	objpath(args(3));

			DBG << "WRITE SAMPLE"
				<< "\n\ttime= " << now
				<< "\n\tobj=  " << objpath
				<< "\n";

			GeoObject *obj = find_obj(objpath);

			if ( argc > (2+2) )
			{
				DBG << " --- using EXPLICIT matrix\n";
				CHK(2+16, "writesample <time> <obj_name> <matrix4x4>");
				
				UT_DMatrix4 mtx;
				for( int i=0, p=4;  i<16;  ++i, ++p )
					mtx.data()[i] = atof(args(p));

				obj->setMatrix(mtx);
			}
			else
			{
				DBG << " --- using its own matrix\n";
				obj->useExplicitMatrix(false);
			}

			// write sample
			//	
			obj->writeSample(now);
		}
		else if (func=="cleanup")
		{
			DBG << "CLEANUP--...\n";

			// TODO: finish this

			_objmap.clear();
			_objs.clear();
			GeoObject::cleanup();
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
		std::cerr << "ERROR: " << e << "\n";
	}

	catch(...)
	{
		std::cerr << "SOME OTHER SHIT HAPPENED :( \n";
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


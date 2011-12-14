/**
		@file		hAbcGeomExport.h
		@author		Imre Tuske
		@since		2011-11-29

		@brief		Alembic Geometry Export ROP (declarations).

*/

#ifndef __hAbcGeomExport_h__
#define __hAbcGeomExport_h__

#include <OBJ/OBJ_Node.h>
#include <ROP/ROP_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Node.h>

#include <GB/GB_Attribute.h>

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>

#include <boost/shared_ptr.hpp>
#include <vector>
#include <string>
#include <map>


#include "GeoObject.h"


#define STR_PARM(name, idx, vi, t) \
		{ evalString(str, name, &ifdIndirect[idx], vi, (float)t); }

#define STR_SET(name, idx, vi, t) \
		{ setString(str, name, ifdIndirect[idx], vi, (float)t); }

#define STR_GET(name, idx, vi, t) \
		{ evalStringRaw(str, name, &ifdIndirect[idx], vi, (float)t); }




class OP_TemplatePair;
class OP_VariablePair;

namespace HDK_AbcExportSimple
{
	typedef std::map< std::string, GB_Attribute * >
		AttrArray;



	/**	Alembic Geometry Export ROP node declaration.
	*/
	class hAbcGeomExport : public ROP_Node
	{
	public:
		static OP_TemplatePair *getTemplatePair();	///< Provides access to our parm templates.
		static OP_VariablePair *getVariablePair();	///< Provides access to our variables.

		/// Creates an instance of this node.
		static OP_Node *myConstructor(OP_Network *net, const char *name, OP_Operator *op);

	protected:
		hAbcGeomExport(OP_Network * net, const char *name, OP_Operator * entry);
		virtual ~hAbcGeomExport();

		virtual int			startRender( int nframes, fpreal s, fpreal e );
		virtual ROP_RENDER_CODE		renderFrame( fpreal time, UT_Interrupt * boss );
		virtual ROP_RENDER_CODE		endRender();

	public:
		/// Cnvenience method to evaluate a string parameter.

		inline void get_str_parm( char const *parm, float t, UT_String & out )
		{
			//evalString(out, parm, &ifdIndirect[0], 0, (float)t);
			evalString(out, parm, 0, (float)t);
		}

		inline void OUTPUT(UT_String & str, float t)
		{
			STR_PARM("file", 0, 0, t)
		}

	private:
		static int *		ifdIndirect;
	private:
		bool			export_geom(
						char const *sopname,
						SOP_Node *sop,
						float time
					);
	private:
		float			_start_time;		///< start time (in secs)
		float			_end_time;		///< end time (in secs)
		int			_num_frames;		///< no. of frames to export
		float			_t_step;		///< time step (usually 1/FPS or 1/24)

		std::string		_objpath;		///< name of root object to export
		std::string		_abcfile;		///< output file name

		GeoObjects				_objs;
		Alembic::AbcGeom::OArchive *		_oarchive;
		Alembic::AbcGeom::TimeSamplingPtr	_ts;
	};

}				// End HDK_Sample namespace


#undef STR_PARM
#undef STR_SET
#undef STR_GET

#endif



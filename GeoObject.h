/**
		@file		GeoObject.h
		@author		Imre Tuske
		@since		2011-12-13

		@brief		Class for storing all about an object to be exported.

*/

#ifndef __GeoObject_h__
#define __GeoObject_H__

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>

#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Node.h>

#include <boost/shared_ptr.hpp>
#include <vector>
#include <cassert>
#include <string>


/**		Macro for placing class-static data.
*/
#define GEOOBJECT_STATICS_HERE \
Alembic::AbcGeom::OArchive * HDK_AbcExportSimple::GeoObject::_oarchive(0); \
Alembic::AbcGeom::TimeSamplingPtr HDK_AbcExportSimple::GeoObject::_ts; \



namespace HDK_AbcExportSimple
{
	/**		Class for storing all related stuff about an object to be exported.

			The object is an Obj/SOP (xform+geometry) combination.
			Currently only poly meshes are supported.
	*/
	class GeoObject
	{
	public:
		/**	Initialize data shared between all objects.
		*/
		static void init(
			Alembic::AbcGeom::OArchive *archive,
			Alembic::AbcGeom::TimeSamplingPtr & timesampling
		)
		{
			assert(archive && timesampling);
			_oarchive = archive;
			_ts = timesampling;
		}

	public:
		GeoObject( OP_Node *obj_node, GeoObject *parent=0 );
		~GeoObject();

	public:
		bool		writeSample( float time );
		char const *	pathname() const { return _path.c_str(); }
		char const *	sop_name() const { return _sopname.c_str(); }


	private:
		/// output archive 'stream' to work in
		static Alembic::AbcGeom::OArchive *
						_oarchive;

		/// time-sampling spec (boost shared_ptr)
		static Alembic::AbcGeom::TimeSamplingPtr
						_ts;
	private:
		GeoObject *			_parent;	///< hierarchy parent
		OBJ_Node *			_op_obj;	///< geometry xform node
		SOP_Node *			_op_sop;	///< SOP node to export
		std::string			_name;		///< obj (xform) name
		std::string			_path;		///< obj full path
		std::string			_sopname;	///< SOP name
		Alembic::AbcGeom::OXform *	_xform;		///< output xform obj
		Alembic::AbcGeom::OPolyMesh *	_outmesh;	///< output polymesh obj
	};



	/// Type: array of GeoObjects.
	typedef std::vector< boost::shared_ptr<GeoObject> > GeoObjects;



}

#endif




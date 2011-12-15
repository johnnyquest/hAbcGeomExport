/**
		@file		GeoObject.h
		@author		Imre Tuske
		@since		2011-12-13

		@brief		Class for storing all about an object to be exported.

*/

#ifndef __GeoObject_h__
#define __GeoObject_h__

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>

#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Node.h>
#include <UT/UT_DMatrix4.h>

#include <boost/shared_ptr.hpp>
#include <vector>
#include <cassert>
#include <string>


/**		Macro for placing class-static data.
*/



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

		static void cleanup()
		{
			if (_oarchive) delete _oarchive;
			_oarchive=0;
			// delete _ts?
		}

	public:
		GeoObject(
			OP_Node *	obj_node,
			GeoObject *	parent=0,
			SOP_Node *	sop_node=0,
			std::string *	outname=0
		);
		~GeoObject();

	public:
		bool		writeSample( float time );
		char const *	pathname() const { return _path.c_str(); }
		char const *	sop_name() const { return _sopname.c_str(); }

		void		useExplicitMatrix( bool use=true ) { _mtx_soho = use; }
		void		setMatrix( UT_DMatrix4 const & mtx ) { _matrix = mtx; useExplicitMatrix(); }
	
	private:
		bool		get_mtx_from_api( OP_Context & ctx );
		bool		get_mtx_from_soho( OP_Context & ctx );


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
		OBJ_OBJECT_TYPE			_obj_type;	///< obj node type (see OBJ/OBJ_Node.h)

		std::string			_name;		///< obj (xform) name
		std::string			_path;		///< obj full path
		std::string			_sopname;	///< SOP name

		bool				_mtx_soho;	///< flag: get xform from soho? (TODO: should be named '_mtx_explicit')
		UT_DMatrix4			_matrix;	///< xform matrix to be output

		Alembic::AbcGeom::OXform *	_xform;		///< output xform obj
		Alembic::AbcGeom::OPolyMesh *	_outmesh;	///< output polymesh obj
	};



	/// Type: array of GeoObjects.
	typedef std::vector< boost::shared_ptr<GeoObject> > GeoObjects;


}

#endif




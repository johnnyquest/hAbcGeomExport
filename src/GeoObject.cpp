/**
		@file		GeoObject.cpp
		@author		Imre Tuske
		@since		2011-12-14

		@brief		Class for storing all about an object to be exported (implementation).

*/

#include "GeoObject.h"

#define _DEBUG

#ifdef _DEBUG
#define DBG if (true) std::cerr << "[GeoObject.cpp " << __LINE__ << "]: "
#define dbg if (true) std::cerr
#else
#define DBG if (false) std::cerr
#define dbg if (false) std::cerr
#endif



namespace Abc = Alembic::Abc;
namespace AbcGeom = Alembic::AbcGeom;

using namespace std;
using namespace HDK_AbcExportSimple;



// static (shared) per-class data
//
Alembic::AbcGeom::OArchive * HDK_AbcExportSimple::GeoObject::_oarchive(0);
Alembic::AbcGeom::TimeSamplingPtr HDK_AbcExportSimple::GeoObject::_ts;




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

@note
		Instances returned by SOHO might not have an obj node that is
		exactly the same as the instance name: in this case pass the
		ptr to the 'original' obj_node, and pass the instance name
		in the outname parameter.
*/
GeoObject::GeoObject(
	OP_Node *	obj_node,
	GeoObject *	parent,
	SOP_Node *	sop_node,
	std::string *	outname
)
: _parent(parent)
, _op_sop(sop_node)
, _name( outname ? *outname : obj_node->getName().toStdString() )
, _sopname("<no SOP>")
, _mtx_soho(false)
, _static_geo(false)
, _geo_ok(false)
, _matrix()
, _xform(0)
, _outmesh(0)
, _Cd_param(0)
, _v_param(0)
{
	UT_String s; obj_node->getFullPath(s);
	_path = s.toStdString();

	_op_obj = obj_node->castToOBJNode(); // either an OBJ_Node or zero
	assert(_op_obj && "this should always be an obj node");

	if (_op_obj) {
		if (!_op_sop) _op_sop = _op_obj->getRenderSopPtr();
		_obj_type = _op_obj->getObjectType();
	}

	if (_op_sop)
		_sopname = _op_sop->getName();

	dbg << "(" << _path << "): " << _name << " | " << _sopname << " | ";

	assert(_oarchive && "no oarchive given");
	assert(_ts && "no timesampling given");

	_xform = new Alembic::AbcGeom::OXform(
		_parent ? *(_parent->_xform) : _oarchive->getTop(),
		_name, _ts);

	if ( _op_obj && _obj_type==OBJ_GEOMETRY && _op_sop )
	{
		dbg << "[GEO]";
		_outmesh = new Alembic::AbcGeom::OPolyMesh(*_xform, _sopname, _ts);
	}
	else {
		dbg << "[NULL]";
		_outmesh = 0;
	}
}



/**		GeoObject, destructor.
*/
GeoObject::~GeoObject()
{
	//DBG << " --- ~GeoObject() " << _path << "\n";
	if (_v_param) delete _v_param; _v_param=0;
	if (_Cd_param) delete _Cd_param; _Cd_param=0;
	if (_outmesh) delete _outmesh; _outmesh=0;
	if (_xform) delete _xform; _xform=0;
}




/**		Get the object's xforms using plain HDK API (as opposed to SOHO).
*/
bool GeoObject::get_mtx_from_api( OP_Context & ctx )
{
	if ( _op_obj )
	{
		UT_DMatrix4 const & hou_prexform = _op_obj->getPreTransform();
		UT_DMatrix4 hou_dmtx;
		
		_op_obj->getParmTransform(ctx, hou_dmtx);
		_matrix = hou_prexform * hou_dmtx; // apply pretransform
		return true;
	}
	
	return false;
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
	//dbg << "sample for " << _path << " @ " << time << ": ";
	assert(_op_obj && "an obj should be given");
	assert(_xform && "no abc output xform");

	OP_Context ctx(time);

	// * xform sample *
	//
	Alembic::AbcGeom::XformSample xform_samp;
	// TODO: fill the xform sample with the proper data (local transformations)
	// with hints and all (how to include preTransform elegantly?)


	if (!_mtx_soho) get_mtx_from_api(ctx);

	AbcGeom::M44d mtx( (const double (*)[4]) _matrix.data() );
	xform_samp.setMatrix(mtx);

	_xform->getSchema().set(xform_samp); // export xform sample

	if ( _outmesh==0 ) {
		//dbg << "null/xform\n";
		return true;
	}

	if (_static_geo && _geo_ok) {
		// skip if static and already written
		//DBG << "skipping static geo " << _name << " (" << _path << ")\n";
		return true;
	}

	// * geom sample *
	//
	//dbg << "GEO\n";

	GU_DetailHandle gdh = _op_sop->getCookedGeoHandle(ctx);
	GU_DetailHandleAutoReadLock gdl(gdh);
	const GU_Detail *gdp = gdl.getGdp();

	if (!gdp)
		return false;

	GEO_AttributeHandle	h_pN = gdp->getPointAttribute("N"),
				h_vN = gdp->getVertexAttribute("N"),
				h_pUV = gdp->getPointAttribute("uv"),
				h_vUV = gdp->getVertexAttribute("uv"),
				h_pCd = gdp->getPointAttribute("Cd"),
				h_vCd = gdp->getVertexAttribute("Cd"),
				h_pv = gdp->getPointAttribute("v"),
				h_vv = gdp->getVertexAttribute("v");
	
	bool	N_pt   = h_pN.isAttributeValid(),
		N_vtx  = h_vN.isAttributeValid(),
		uv_pt  = h_pUV.isAttributeValid(),
		uv_vtx = h_vUV.isAttributeValid(),
		Cd_pt  = h_pCd.isAttributeValid(),
		Cd_vtx = h_vCd.isAttributeValid(),
		v_pt   = h_pv.isAttributeValid(),
		v_vtx  = h_vv.isAttributeValid(),
		has_N  = N_pt  || N_vtx,
		has_uv = uv_pt || uv_vtx,
		has_Cd = Cd_pt || Cd_vtx,
		has_v  = v_pt  || v_vtx;

	DBG
		<< "TIME: " << time
		<< "\n";

/*
	DBG	<< " - ATTRS:"
		<< "\n has_N:" << has_N
		<< " N_pt:" << N_pt
		<< " N_vtx:" << N_vtx
		<< "\n has_uv:" << has_uv
		<< " uv_pt:" << uv_pt
		<< " uv_vtx:" << uv_vtx
		<< "\n has_Cd:" << has_Cd
		<< " Cd_pt:" << Cd_pt
		<< " Cd_vtx:" << Cd_vtx
		<< "\n has_v:" << has_v
		<< " v_pt:" << v_pt
		<< " v_vtx:" << v_vtx
		<< "\n";
*/

	// collect polymesh data
	//
	std::map<GEO_Point const *, int> ptmap; 	// (this should be replaced if possible)

	typedef std::vector<Abc::float32_t> FloatVec;

	FloatVec			g_pts;			// point coordinates (3 values)
	std::vector<Abc::int32_t>	g_pts_ids;		// point indices for each per-face-vertex
	std::vector<Abc::int32_t>	g_facevtxcounts;	// vertex count for each face
	int				num_pfv=0;		// no. of per-face vertices
	
	FloatVec			g_N;			// normals (3 values; per-point or per-vertex)
	FloatVec			g_uv;			// uv coords (2 values; per-point or per-vertex)
	FloatVec			g_Cd;			// color (RGB: 3 values; per-point or per-vertex)
	FloatVec			g_v;			// velocity (3 values; per-point or per-vertex)

	GEO_Point const		*pt;
	GEO_Primitive const	*prim;

	// collect point coords
	//
	int c=0;
#if UT_MAJOR_VERSION_INT >= 12
	GA_FOR_ALL_GPOINTS(gdp, pt)
#else
	FOR_ALL_GPOINTS(gdp, pt)
#endif
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

		if ( Cd_pt ) {
			h_pCd.setElement(pt);
			V = h_pCd.getV3();
			push_v3<FloatVec, UT_Vector3>(g_Cd, V);
		}

		if ( v_pt ) {
			h_pv.setElement(pt);
			V = h_pv.getV3();
			push_v3<FloatVec, UT_Vector3>(g_v, V);
		}

		ptmap[pt]=c; // store point in point->ptindex map
		++c;
	}

	// collect primitives
	//
#if UT_MAJOR_VERSION_INT >= 12
	GA_FOR_ALL_PRIMITIVES(gdp, prim)
#else
	FOR_ALL_PRIMITIVES(gdp, prim)
#endif
	{
#if UT_MAJOR_VERSION_INT >= 12
		int prim_id = prim->getTypeId().get();
		if ( prim_id == GEO_PRIMPOLY )
#else
		int prim_id = prim->getPrimitiveId();
		if ( prim_id == GEOPRIMPOLY )
#endif
		{
			int num_verts = prim->getVertexCount();
			g_facevtxcounts.push_back(num_verts);
			num_pfv += num_verts;
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

				if ( Cd_vtx ) {
					h_vCd.setElement(&vtx);
					V = h_vCd.getV3();
					push_v3<FloatVec, UT_Vector3>(g_Cd, V);
				}

				if ( v_vtx ) {
					h_vv.setElement(&vtx);
					V = h_vv.getV3();
					push_v3<FloatVec, UT_Vector3>(g_v, V);
				}
			}
		}
	}

	AbcGeom::ON3fGeomParam::Sample N_samp;
	AbcGeom::OV2fGeomParam::Sample uv_samp;
	AbcGeom::OC3fGeomParam::Sample Cd_samp;
	AbcGeom::OC3fGeomParam::Sample v_samp;

	if ( has_N ) {
		N_samp.setScope( N_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVertexScope );
		N_samp.setVals( AbcGeom::N3fArraySample( (const AbcGeom::N3f *)&g_N[0], g_N.size()/3) );
	}

	if ( has_uv ) {
		uv_samp.setScope( uv_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVertexScope );
		uv_samp.setVals( AbcGeom::V2fArraySample( (const AbcGeom::V2f *)&g_uv[0], g_uv.size()/2) );
	}


	AbcGeom::OPolyMeshSchema & mesh_schema = _outmesh->getSchema();
	Alembic::Abc::OCompoundProperty arb_params = mesh_schema.getArbGeomParams();

	DBG
		<< " - num_pfv=" << num_pfv
		<< "\n";

	if (false)
	{
		std::vector< Alembic::Util::uint32_t > pfv_indices;
	 
		if ( Cd_vtx || v_vtx ) {
			pfv_indices.resize(num_pfv);
			for( int i=0; i<num_pfv; ++i ) pfv_indices[i]=i;
		}
	}

	if ( has_Cd )
	{
		Cd_samp.setScope( Cd_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVertexScope );
		Cd_samp.setVals( AbcGeom::C3fArraySample( (const AbcGeom::C3f *)&g_Cd[0], g_Cd.size()/3) );
		//Cd_samp.setIndices( Alembic::Abc::UInt32ArraySample( &pfv_indices[0], pfv_indices.size() ) );

		// NOTE369: this 'fake' indexing must be exported for now
		// as Maya AbcImport can't handle non-indexed colorsets
		//Cd_samp.setIndices( Alembic::Abc::UInt32ArraySample( &pfv_indices[0], pfv_indices.size() ) );

		Alembic::AbcCoreAbstract::MetaData md;
		md.set("mayaColorSet", "1");

		if (_Cd_param==0) {
			// NOTE386: this must be indexed for now, see NOTE369 above
			_Cd_param = new AbcGeom::OC3fGeomParam(arb_params,
				"Cd", false, // isIndexed
				Cd_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVertexScope,
				1, _ts, md);
		}

		_Cd_param->set(Cd_samp);
	}

	if ( has_v )
	{
		v_samp.setScope( v_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVertexScope );
		v_samp.setVals( AbcGeom::C3fArraySample( (const AbcGeom::C3f *)&g_v[0], g_v.size()/3) );
		//v_samp.setIndices( Alembic::Abc::UInt32ArraySample( &pfv_indices[0], pfv_indices.size() ) );

		Alembic::AbcCoreAbstract::MetaData md;
		md.set("mayaColorSet", "0");

		if (_v_param==0) {
			_v_param = new AbcGeom::OC3fGeomParam(arb_params,
				"velocity", false, // isIndexed
				v_vtx ? AbcGeom::kFacevaryingScope : AbcGeom::kVertexScope,
				1, _ts, md);
		}

		_v_param->set(v_samp);

	}

	// construct mesh sample
	//
	AbcGeom::OPolyMeshSchema::Sample mesh_samp(
		AbcGeom::C3fArraySample( (const AbcGeom::C3f *)&g_pts[0], g_pts.size()/3 ),
		AbcGeom::Int32ArraySample( &g_pts_ids[0], g_pts_ids.size() ),
		AbcGeom::Int32ArraySample( &g_facevtxcounts[0], g_facevtxcounts.size() ),
		uv_samp, N_samp
	);

	mesh_schema.set(mesh_samp); // export mesh sample
	_geo_ok=true;

	return true;
}










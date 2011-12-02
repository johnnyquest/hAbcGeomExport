#!/bin/sh

rm -f *.o
rm -f ~/houdini11.1/dso/hAbcGeomExport.so

hcustom \
	-L /home/tusi/work/alembic/libs/linux/lib \
	-l Half \
	-l hdf5 \
	-l hdf5_hl \
	-L /home/tusi/work/alembic/libs/linux/alembic-1.0.3/lib/static \
	-l AlembicAbcCoreHDF5 \
	-l AlembicAbcCoreAbstract \
	-l AlembicAbc \
	-l AlembicAbcGeom \
	-l AbcWFObjConvert \
	-l AlembicUtil \
	-I /usr/include/OpenEXR \
	hAbcGeomExport.cpp



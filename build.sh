#!/bin/sh
#
#	A simple (re)build shell script for linux.
#	Might work for mac (windows users: roll your own :)).
#
#	NOTES:
#
#	* This script should be run from a shell with a Houdini
#	environment initialized.
#
#	* If any of the linked libraries use additional .so libs
#	(not entirely statically linked), those .so file should
#	be in the library path, or the LD_LIBRARY_PATH variable
#	should be set up explicitly to their location.
#
#	TODO:
#
#	A waf version.
#
#


# set these variables accordingly
#
HOUDINI_PREFS=$HIH
#OPENEXR_INCLUDES=~/work/dev/alembic/libs/linux/include/OpenEXR
#ALEMBIC_SUPPORT_LIBS=~/work/dev/alembic/libs/linux/lib
#ALEMBIC_SUPPORT_INCLUDES=~/work/dev/alembic/libs/linux/include
#ALEMBIC_LIBS=~/work/dev/alembic/libs/linux/alembic-0.9.3/lib/static
#ALEMBIC_INCLUDES=~/work/dev/alembic/libs/linux/alembic-0.9.3/include

OPENEXR_INCLUDES=/usr/include/OpenEXR

ALEMBIC_SUPPORT_LIBS=~/work/alembic/libs/linux/lib
#ALEMBIC_SUPPORT_LIBS=$HFS/dsolib

ALEMBIC_SUPPORT_INCLUDES=~/work/alembic/libs/linux/include
ALEMBIC_LIBS=~/work/alembic/libs/linux/alembic-1.0.3/lib/static
ALEMBIC_INCLUDES=~/work/alembic/libs/linux/alembic-1.0.3/include


# remove previous build
#
rm -f $HOUDINI_PREFS/dso/hAbcGeomExport.so


# perform build (link library order is important!)
#
export SESI_TAGINFO="Johnny Quest / Questetics Inc."
hcustom -g \
	-L $ALEMBIC_SUPPORT_LIBS \
	-l Half -l Iex -l hdf5 -l hdf5_hl \
	-L $ALEMBIC_LIBS \
	-l AlembicAbcCoreHDF5 -l AlembicAbcCoreAbstract \
	-l AlembicAbc -l AlembicAbcGeom -l AbcWFObjConvert -l AlembicUtil \
	-I $OPENEXR_INCLUDES \
	-I $ALEMBIC_INCLUDES \
	-I $ALEMBIC_SUPPORT_INCLUDES \
	src/hAbcGeomExport.cpp

# delete leftover stuff
#
rm -f src/*.o


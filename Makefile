# Makefile.
#

DSONAME = $(HIH)/dso/hAbcExport.so


SOURCES = src/hAbcGeomExport.cpp src/abcexportctrl.cpp

#INCDIRS = \
#	-I/usr/include/OpenEXR \
#	-I/home/tusi/work/dev/alembic/libs/linux/include \
#	-I/home/tusi/work/dev/alembic/libs/linux/alembic-0.9.3/include
#
#LIBDIRS = \
#	-L/home/tusi/work/dev/alembic/libs/linux/lib \
#	-L/home/tusi/work/dev/alembic/libs/linux/alembic-0.9.3/lib/static

INCDIRS = \
	-I/usr/include/OpenEXR \
	-I/home/tusi/work/alembic/libs/linux/include \
	-I/home/tusi/work/alembic/libs/linux/alembic-1.0.3/include

LIBDIRS = \
	-L/home/tusi/work/alembic/libs/linux/lib \
	-L/home/tusi/work/alembic/libs/linux/alembic-1.0.3/lib/static

LIBS = \
	-lHalf -lIex -lhdf5 -lhdf5_hl \
	-lAlembicAbcCoreHDF5 -lAlembicAbcCoreAbstract \
	-lAlembicAbc -lAlembicAbcGeom -lAbcWFObjConvert -lAlembicUtil



###############################################################################
# For GNU make, use this line:
#      include $(HFS)/toolkit/makefiles/Makefile.gnu
# For Microsoft Visual Studio's nmake use this line instead
#      !INCLUDE $(HFS)/toolkit/makefiles/Makefile.nmake
#
include $(HFS)/toolkit/makefiles/Makefile.gnu



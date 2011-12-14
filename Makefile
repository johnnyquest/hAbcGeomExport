# Makefile
#

DSONAME = $(HIH)/dso/hAbcExport.so
SOURCES = src/abcexportctrl.cpp #src/hAbcGeomExport.cpp


###############################################################################
# For GNU make, use this line:
#      include $(HFS)/toolkit/makefiles/Makefile.gnu
# For Microsoft Visual Studio's nmake use this line instead
#      !INCLUDE $(HFS)/toolkit/makefiles/Makefile.nmake
#
include $(HFS)/toolkit/makefiles/Makefile.gnu


#!/bin/sh

rm -f *.o
rm -f ~/houdini11.1/dso/hAbcGeomExport.so

hcustom hAbcGeomExport.cpp



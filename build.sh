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
#	TODO:
#
#	A waf version.
#
#


# remove previous build
#
rm -f $HOUDINI_PREFS/dso/hAbcGeomExport.so


make clean
make


# delete leftover stuff
#
rm -f src/*.o


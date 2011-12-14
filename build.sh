#!/bin/sh
#
#	A simple (re)build shell script for linux.
#
#	TODO:
#
#	A waf version.
#
#


# set these variables accordingly
#
HOUDINI_PREFS=$HIH


# remove previous build
#
rm -f $HOUDINI_PREFS/dso/abcexportctrl.so

make clean
make


# delete leftover stuff
#
rm -f src/*.o


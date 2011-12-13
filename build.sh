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


# perform build (link library order is important!)
#
export SESI_TAGINFO="Johnny Quest / Questetics Inc."
hcustom -g \
	src/abcexportctrl.cpp

# delete leftover stuff
#
rm -f src/*.o


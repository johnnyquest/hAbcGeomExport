#!/bin/bash


echo "*** EXTRACTING OTLS ***"

find . -name "*.otl" | awk '{ d=$0; sub(/\.otl$/, "_OTL", d); print "hotl -X " d " " $0 }' | bash



#!/bin/sh

# delete previous extracted hip folders
# then extract hip files to ascii

rm -rf *.dir *.contents
ls -1 *.hip | awk '{ print "hexpand -p " $0  }' | bash



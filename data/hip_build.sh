#!/bin/sh

# build hip files
# from extracted ascii-hip contents

ls -1 cpio.*.contents | awk 'match($0, /[^.]+\.hip/) { print "hcollapse -p -r " substr($0,RSTART,RLENGTH) }' | bash


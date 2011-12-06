#!/bin/sh

# run a houdini/ddd debugger session with the test.hip file
#

#export LD_LIBRARY_PATH='/home/tusi/work/alembic/libs/linux/lib'
export LD_LIBRARY_PATH='/home/tusi/work/dev/alembic/libs/linux/lib'
ddd hmaster test.hip



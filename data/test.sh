#!/bin/sh

# run a houdini quick test session with the test.hip file
#

export LD_LIBRARY_PATH='/home/tusi/work/alembic/libs/linux/lib'
#export LD_LIBRARY_PATH='/home/tusi/work/dev/alembic/libs/linux/lib'
hmaster test.hip


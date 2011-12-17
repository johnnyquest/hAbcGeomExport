#!/bin/sh


S=`pwd`/soho
T=$HIH/soho

rm -rf $T/hAbcExport.py
rm -rf $T/hAbcExportFrame.py
rm -rf $T/*.pyc

ln -sf $S/hAbcExport.py $T
ln -sf $S/hAbcExportFrame.py $T



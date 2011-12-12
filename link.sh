
S=`pwd`/soho
T=~/houdini11.0/soho

rm -rf $T/hAbcExport.py
rm -rf $T/*.pyc
rm -rf $T/parameters/*

ln -sf $S/hAbcExport.py $T

#mkdir -p $T/parameters

#ln -fs $S/parameters/arnold3110.ds $T/parameters
#ln -fs $S/parameters/SOHOparameters $T/parameters

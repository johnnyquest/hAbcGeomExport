
S=`pwd`/soho
T=$HIH/soho

rm -rf $T/hAbcExport.py
rm -rf $T/*.pyc

ln -sf $S/hAbcExport.py $T

#mkdir -p $T/parameters

#ln -fs $S/parameters/arnold3110.ds $T/parameters
#ln -fs $S/parameters/SOHOparameters $T/parameters

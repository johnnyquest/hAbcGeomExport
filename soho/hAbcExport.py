##		@file		hAbcExport.py
#		@author		xy
#		@since		2011-12-12
#
#


import sys


def msg(m):
	sys.__stderr__.write("[hAbcExport.py]: %s\n" % str(m))


def dbg(m):
	msg("(debug) %s" % str(m))



import hAbcExportFrame
reload(hAbcExportFrame)



hAbcExportFrame.export()




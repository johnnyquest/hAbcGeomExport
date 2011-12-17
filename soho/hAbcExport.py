##		@file		hAbcExport.py
#		@author		xy
#		@since		2011-12-12
#
#

import sys

def msg(m):	sys.__stderr__.write("[hAbcExport.py]: %s\n" % str(m))
def dbg(m):	msg("(debug) %s" % str(m))


archy = []		# objects to be exported (array of tuples)
archy_objs = []		# object names to be exported (for incremental-checking purposes)


import hAbcExportFrame
reload(hAbcExportFrame)

hAbcExportFrame.export()




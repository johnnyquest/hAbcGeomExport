##		@file		hAbcExportFrame.py
#		@author		xy
#		@since		2011-12-12
#
#

import time
import soho
import sys
import string

from soho import Precision
from soho import SohoParm

import hou


def msg(m):
	sys.__stderr__.write("[hAbcExportFrame.py]: %s\n" % str(m))


def dbg(m):
	msg("(debug) %s" % str(m))




dbg("hAbcExportFrame.py -- RUNNING")



def collect_archy( objname, archy=None, level=1 ):

	n = hou.node(objname)

	if n:
		if archy is None: archy = []
		dbg( " %s %s"  %  ('-' * level, n.path() ) )
		archy.append(n.path())

		cs = [ c.path() for c in n.outputs() ]
		for c in cs:
			archy = collect_archy(c, archy, level+1)
		return archy
	
	return []





def export():
	dbg("1")

	ps = soho.evaluate({
		'now':		SohoParm('state:time',			'real', [0],  False, key='now'),
		'fps':		SohoParm('state:fps',			'real', [24],  False, key='fps'),
		'hver':		SohoParm('state:houdiniversion',	'string', [''],  False, key='hver'),
		'objpath':	SohoParm('objpath',		'string',	[''], False),
		'abcoutput':	SohoParm('abcoutput',		'string',	[''], False),
		'trange':	SohoParm('trange',		'int',		[0], False),
		'f':		SohoParm('f',			'int',		None, False)
	})
	
	dbg("2")

	now = ps['now'].Value[0]
	fps = ps['fps'].Value[0]
	hver = ps['hver'].Value[0]

	if not soho.initialize(now):
		soho.error("couldn't initialize")
		return

	frame = int(now*fps)+1

	objpath = ps['objpath'].Value[0]
	abcoutput = ps['abcoutput'].Value[0]
	trange = ps['trange'].Value[0]
	f = ps['f'].Value

	dbg("3")

	dbg("now=%.3f fps=%.3f -> %.3f" % (now, fps, frame))

	dbg("objpath=%s abcoutput=%s trange=%d f=%s" % \
		(objpath, abcoutput, trange, str(f)))

	soho.addObjects(now, '*', '*', '', True)
	soho.lockObjects(now)

	dbg("4")

	objlist = soho.objectList('objlist:instance')

	dbg("5")

	sop_dict = {} # {objname: sopname}

	for obj in objlist:
		dbg(" -- %s" % obj.getName())
		path = obj.getDefaultedString('object:soppath', now, [''])[0]
		#dbg(" ---- %s" % path)
		sop_dict[obj.getName()] = path

	archy = collect_archy(objpath)

	for obj in archy:
		pass






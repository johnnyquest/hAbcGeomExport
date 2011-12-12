##		@file		hAbcExport.py
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
	sys.__stderr__.write("%s\n" % str(m))





msg("hAbcExport.py -- RUNNING")


def export():
	msg("1")

	ps = soho.evaluate({
		'now':		SohoParm('state:time',			'real', [0],  False, key='now'),
		'fps':		SohoParm('state:fps',			'real', [24],  False, key='fps'),
		'hver':		SohoParm('state:houdiniversion',	'string', [''],  False, key='hver'),
		'objpath':	SohoParm('objpath',		'string',	[''], False),
		'abcoutput':	SohoParm('abcoutput',		'string',	[''], False),
		'trange':	SohoParm('trange',		'int',		[0], False),
		'f':		SohoParm('f',			'int',		None, False)
	})
	
	msg("2")

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

	msg("3")

	msg("now=%.3f fps=%.3f -> %.3f" % (now, fps, frame))

	msg("objpath=%s abcoutput=%s trange=%d f=%s" % \
		(objpath, abcoutput, trange, str(f)))

	soho.addObjects(now, objpath, '', '', True)
	
	msg("4")

	objlist = soho.objectList('objlist:instance')
	soho.lockObjects(now)

	msg("5")

	for obj in objlist:
		msg(" -- %s" % obj.getName())




export()


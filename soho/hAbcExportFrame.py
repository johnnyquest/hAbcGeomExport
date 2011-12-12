##		@file		hAbcExportFrame.py
#		@author		xy
#		@since		2011-12-12
#
#

import time
import soho
import sys
import string
import re

from soho import Precision
from soho import SohoParm

import hou


import hAbcExport as G



def msg(m):	sys.__stderr__.write("[hAbcExportFrame.py]: %s\n" % str(m))
def dbg(m):	msg("(dbg) %s" % str(m))
def warn(m):	msg("WARNING: %s" % str(m))




#dbg("(hAbcExportFrame.py)")






def collect_archy( objname, parentname=None, archy=None, level=1 ):
	"""
	Collect obj hierarchy. Returns array of (<parentpath>, <objpath>) tuples.
	"""

	n = hou.node(objname)

	if n:
		if archy is None: archy = []

		p = n.path()
		#dbg( " %s %s"  %  ('-' * level, p ) )
		archy.append( (parentname, p) )

		cs = [ c.path() for c in n.outputs() ]
		for c in cs:
			archy = collect_archy(c, p, archy, level+1)
		return archy
	
	return []





def export():
	"""Main export function."""

	ps = soho.evaluate({
		'now':		SohoParm('state:time',			'real', [0],  False, key='now'),
		'fps':		SohoParm('state:fps',			'real', [24],  False, key='fps'),
		'hver':		SohoParm('state:houdiniversion',	'string', [''],  False, key='hver'),
		'objpath':	SohoParm('objpath',		'string',	[''], False),
		'abcoutput':	SohoParm('abcoutput',		'string',	[''], False),
		'trange':	SohoParm('trange',		'int',		[0], False),
		'f':		SohoParm('f',			'int',		None, False)
	})
	
	now = ps['now'].Value[0]
	fps = ps['fps'].Value[0]
	hver = ps['hver'].Value[0]

	if not soho.initialize(now):
		soho.error("couldn't initialize soho")
		return

	# NOTE: this is prone to float inaccuracies
	frame = now*fps + 1.0

	objpath   = ps['objpath'].Value[0]
	abcoutput = ps['abcoutput'].Value[0]
	trange    = ps['trange'].Value[0]
	f         = ps['f'].Value

	is_first  = frame < f[0]+1 # working around float funniness
	is_last   = frame > f[1]-1

	if trange==0:
		is_first= is_last= True

	dbg("is_first=%d is_last=%d" % (is_first, is_last))
	dbg("now=%.3f fps=%.3f -> %f" % (now, fps, frame))

	dbg("objpath=%s abcoutput=%s trange=%d f=%s" % \
		(objpath, abcoutput, trange, str(f)))


	# collect hierarchy to be exported
	# (read from scene directly, ie. not containing instances, etc.)
	# results in array [ (parentname, objname) [, ...]  ] -- (full pathnames)
	#
	archy = collect_archy(objpath)
	#dbg("archy: %s" % str(archy))


	# collect geometry to be exported and their render SOPS
	# (including point- and other instances, etc)
	#
	soho.addObjects(now, '*', '*', '', True)
	soho.lockObjects(now)

	objs = soho.objectList('objlist:instance')
	obj_list = []
	sop_dict = {} # {objname: sopname}

	for obj in objs:
		n = obj.getName() # full pathname
		#dbg(" -- %s" % n )
		obj_list.append(n)
		path = obj.getDefaultedString('object:soppath', now, [None])[0]
		#dbg(" ---- %s" % path)
		if path and path!="":
			sop_dict[n] = path


	# extend hierarchy with per-point instances
	#
	p1 = re.compile(":[^:]+:")
	for obj in obj_list:
		if re.search(p1, obj):
			m = obj.split(":")
			p = m[-2] # parent: 2nd from right
			archy.append( ( p, obj, "%s->%s" % (m[-2], m[-1]) )  )
			#dbg(" -+- %s %s" % (p, obj))


	# fill rest of the archy array
	# elem: (parentpath, objpath, exportname, soppath)
	#
	archy2 = []
	for a in archy:
		N = a[:]
		if len(N)<3: N = (N[0], N[1], N[1])
		if N[1] in sop_dict:
			N = (N[0], N[1], N[2], sop_dict[N[1]])
			archy2.append(N)
		else:
			N = (N[0], N[1], N[1], None)
			archy2.append(N)
	archy = archy2

	if False:
		dbg( '-' * 40 )
		dbg("archy:")
		for a in archy:
			#dbg("- %s: %s" % (a[1], a[3]))
			dbg("- %s" % str(a))


	# we now have a list of all objects to be exported
	# (parentname, objname, exportname, soppath)
	#
	archy_objs = [ n[1] for n in archy ]

	# first frame: init all internal stuff
	#
	if is_first:
		dbg("IS_FIRST--INIT")
		G.archy = archy[:]
		G.archy_objs = archy_objs[:]

		# TODO: export hierarchy, allocate outmesh objs, etc.



	# frame export: collect xforms, geoms, and export them
	#
	if archy_objs == G.archy_objs:
		dbg(" -- exporting frame %.1f" % frame)
		pass
	
	else:
		#soho.error("couldn't export frame %.1f--no. of objects changed" % frame)
		warn("couldn't export frame %.1f--no. of objects changed" % frame)



	# last frame: cleanup all internal stuff,
	# finish export
	#
	if is_last:
		dbg("IS_LAST--FINISHING...")

		# TODO: close export process
		pass





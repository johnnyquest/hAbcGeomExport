##		@file		hAbcExportFrame.py
#		@author		xy
#		@since		2011-12-12
#
#		@brief		Alembic-export of object(s).
#
#		@todo		Clean up this mess. :|
#
#

import time
import soho
import sys
import string
import re

import hou
from soho import Precision
from soho import SohoParm

import hAbcExport as G


def msg(m):	sys.__stderr__.write("[hAbcExportFrame.py]: %s\n" % str(m))
def dbg(m):	msg("(dbg) %s" % str(m))
def warn(m):	msg("WARNING: %s" % str(m))


#dbg("(hAbcExportFrame.py)")



class timer(object):
	"""Timer class, for performance measurements."""

	def ct(self):
		#return time.clock()
		return time.time()

	def __init__(self, name='timer'):
		self.name = name
		t = self.ct()
		self.t0 = t
		self.last = t
		self.times = []

	def elapsed(self):
		return self.ct()-self.t0

	def lap(self, name):
		t = self.ct()
		d = t-self.last
		self.last = t
		self.times.append( (str(name), d) )

	def __str__(self):
		return '\n'.join([ '%20s: %.2f' % (v[0], v[1]) for v in self.times ])

	def stats(self, name=None):
		if True:
			if not name: name=self.name
			msg("TIMER STATS for %s:\n%s" % (name, self.__str__()))
		else:
			# keep quiet
			pass







# hscript command for abc export control
CCMD = 'abcexportctrl'


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




def abc_init(abcfile, tstart=0.0, tstep=1.0/24.0):
	"""Create a new alembic archive."""
	dbg("abc_init() abcfile=%s" % str(abcfile))
	hou.hscript('%s oarchive "%s" %f %f' % (CCMD, abcfile, tstep, tstart))
	return True



def abc_cleanup():
	"""Clean up and close alembic archive."""
	dbg("abc_cleanup()")
	hou.hscript('%s cleanup' % CCMD)
	pass







def export():
	"""Main export function."""

	ps = soho.evaluate({
		'now':		SohoParm('state:time',			'real', [0],  False, key='now'),
		'fps':		SohoParm('state:fps',			'real', [24],  False, key='fps'),
		'hver':		SohoParm('state:houdiniversion',	'string', [''],  False, key='hver'),
		'objpath':	SohoParm('objpath',		'string',	[''], False),
		'abcoutput':	SohoParm('abcoutput',		'string',	[''], False),
		'camera':	SohoParm('camera',		'string',	[None], False),
		'trange':	SohoParm('trange',		'int',		[0], False),
		'f':		SohoParm('f',			'int',		None, False)
	})
	
	now = ps['now'].Value[0]
	fps = ps['fps'].Value[0]
	hver = ps['hver'].Value[0]
	camera = ps['camera'].Value[0]

	dbg("now=%.3f fps=%.3f" % (now, fps))

	if not soho.initialize(now, camera):
		soho.error("couldn't initialize soho (make sure camera is set)")
		abc_cleanup()
		return

	# NOTE: this is prone to float inaccuracies
	frame = now*fps + 1.0

	objpath   = ps['objpath'].Value[0]
	abc_file  = ps['abcoutput'].Value[0]
	trange    = ps['trange'].Value[0]
	f         = ps['f'].Value

	is_first  = frame < f[0]+0.5 # working around float funniness
	is_last   = frame > f[1]-0.5

	if trange==0:
		is_first= is_last= True

	dbg("is_first=%d is_last=%d" % (is_first, is_last))
	dbg("now=%.3f fps=%.3f -> %f" % (now, fps, frame))

	dbg("objpath=%s camera=%s abcoutput=%s trange=%d f=%s" % \
		(objpath, camera, abc_file, trange, str(f)))


	# collect hierarchy to be exported
	# (read from scene directly, ie. not containing instances, etc.)
	# results in array [ (parentname, objname) [, ...]  ] -- (full pathnames)
	#
	#dbg("COLLECTING ARCHY:")
	archy = collect_archy(objpath)
	archy_objs = [ n[1] for n in archy ]
	#dbg("DONE.")


	# collect geometry to be exported and their render SOPS
	# (including point- and other instances, etc)
	# (NOTE: the entire scene is to be searched, unfortunately)
	#
	soho.addObjects(now, '*', '*', '', do_culling=False)
	soho.lockObjects(now)

	soho_objs = {} # {objname: soho_obj}
	soho_only = {} # soho-only objects (can't be accessed directly with hdk)

	obj_list = []
	sop_dict = {} # {objname: sopname}
	objs = soho.objectList('objlist:instance')

	#dbg("COLLECT soho instance/sop pairs ------------------")
	for obj in objs:
		n = obj.getName() # full pathname
		obj_list.append(n)
		soho_objs[n] = obj
		path = obj.getDefaultedString('object:soppath', now, [None])[0]
		#dbg(" -- %s (sop:%s)" % (n, str(path)) )
		if path and path!="":
			sop_dict[n] = path

	if False:
		dbg( '-' * 40 )
		dbg("sop_dict: %s" % str(sop_dict))

	# extend hierarchy with per-point instances
	#
	p1 = re.compile(":[^:]+:")
	for obj in obj_list:
		if re.search(p1, obj):
			m = obj.split(":")
			p = m[-2] # parent: 2nd from right
			if p in archy_objs:
				archy.append( ( p, obj, "%s__%s" % (m[-2], m[-1]) )  )
				soho_only[obj]=p
				#dbg(" -+- %s %s" % (p, obj))


	# fill rest of the archy array
	# elem: (parentpath, objpath, exportname, soppath)
	#
	archy2 = []
	for a in archy:
		N = list(a)
		if len(N)<3: N.append(N[1]) # N = [ N[0], N[1], N[1] ]
		if N[1] in sop_dict:
			N = [ N[0], N[1], N[2], sop_dict[N[1]] ]
		else:
			# empty xform (no sop)
			N = [ N[0], N[1], N[1], None ]
		
		N[2] = re.search("[^/]+$", N[2]).group(0)
		archy2.append(N)
	archy = archy2

	if False:
		dbg( '-' * 40 )
		dbg("COLLECTED ARCHY:")
		for a in archy:
			dbg("- %s: " % (a[1], ))

	dbg("COLLECTED ARCHY: %d elems" % len(archy))


	# we now have a list of all objects to be exported
	# (parentname, objname, exportname, soppath)
	#
	archy_objs = [ n[1] for n in archy ]
	skip_frame = False

	now_out = now+(1.0/fps)

	# check for user abort
	#
	if False: # TODO: check for user abort!
		skip_frame = True
		is_last = True
		warn("user abort")


	# first frame: init all internal stuff
	#
	if is_first:
		dbg("\n\n\n")
		dbg("IS_FIRST--INIT")
		G.archy = archy[:]
		G.archy_objs = archy_objs[:]

		s = abc_init(abc_file, tstep=1.0/fps, tstart=now_out)
		if s:
			# build objects for oarchive
			#
			for E in archy:
				objname = E[1]
				parent  = E[0]
				outname = E[2]
				soppath = E[3]
				if parent is None: parent="-"
				if soppath is None: soppath="-"

				# TODO: if instance, objname should be the base obj name
				obj_src = objname
				if objname in soho_only:
					obj_src = soho_only[objname]

				#dbg("-- new xform\n\toutname= %s\n\tobj    = %s\n\tparent = %s\n\tsop    = %s" % (outname, objname, parent, soppath))

				hou.hscript('%s newobject "%s" "%s" "%s" "%s" "%s"' % \
					(CCMD, objname, obj_src, parent, outname, soppath))

		else:
			warn("couldn't output to file %s--aborting" % abc_file)
			skip_frame = True
			is_last = True



	# frame export: collect xforms, geoms, and export them
	#
	if archy_objs==G.archy_objs  and  not skip_frame:

		dbg("\n")
		dbg(" -- EXPORTING frame %.1f" % frame)

		for E in archy:
			#dbg("\n-")
			#dbg("- OBJ: %s" % str(E))
			objname = E[1]
			soppath = E[3]
			#dbg("- OBJ: %s" % E[1])

			# get xform matrix (TODO: get pretransform too!)
			#
			xform = ''

			# TODO: use this only for instances!
			#if objname in soho_objs:
			if objname in soho_only:
				# get matrix from soho
				#dbg(" --- (mtx from soho)")
				xform = []
				soho_objs[objname].evalFloat('space:local', now, xform)
				xform = hou.Matrix4(xform)
				xform = ' '.join([ str(n) for n in xform.asTuple() ])

			# perform sample write
			# (c++ code decides if geom is to be written)
			#
			hou.hscript('%s writesample %f "%s" %s' % \
				(CCMD, now_out, objname, xform))

	else:
		#soho.error("couldn't export frame %.1f--no. of objects changed" % frame)
		warn("couldn't export frame %.1f--no. of objects changed" % frame)



	# last frame: cleanup all internal stuff,
	# finish export
	#
	if is_last:
		dbg("\n\n\n")
		dbg("IS_LAST--FINISHING...")
		abc_cleanup()





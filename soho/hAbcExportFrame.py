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

import hou
from soho import Precision
from soho import SohoParm

import hAbcExport as G


def msg(m):	sys.__stderr__.write("[hAbcExportFrame.py]: %s\n" % str(m))
def dbg(m):	msg("(dbg) %s" % str(m))
def warn(m):	msg("WARNING: %s" % str(m))


#dbg("(hAbcExportFrame.py)")


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
	hou.hscript('%s oarchive "%s" %f %f' % (CCMD, abcfile, tstep, tstart+tstep))
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
		'trange':	SohoParm('trange',		'int',		[0], False),
		'f':		SohoParm('f',			'int',		None, False)
	})
	
	now = ps['now'].Value[0]
	fps = ps['fps'].Value[0]
	hver = ps['hver'].Value[0]

	if not soho.initialize(now):
		soho.error("couldn't initialize soho")
		abc_cleanup()
		return

	# NOTE: this is prone to float inaccuracies
	frame = now*fps + 1.0

	objpath   = ps['objpath'].Value[0]
	abc_file  = ps['abcoutput'].Value[0]
	trange    = ps['trange'].Value[0]
	f         = ps['f'].Value

	is_first  = frame < f[0]+1 # working around float funniness
	is_last   = frame > f[1]-1

	if trange==0:
		is_first= is_last= True

	dbg("is_first=%d is_last=%d" % (is_first, is_last))
	dbg("now=%.3f fps=%.3f -> %f" % (now, fps, frame))

	dbg("objpath=%s abcoutput=%s trange=%d f=%s" % \
		(objpath, abc_file, trange, str(f)))


	# collect hierarchy to be exported
	# (read from scene directly, ie. not containing instances, etc.)
	# results in array [ (parentname, objname) [, ...]  ] -- (full pathnames)
	#
	archy = collect_archy(objpath)
	#dbg("archy: %s" % str(archy))


	# collect geometry to be exported and their render SOPS
	# (including point- and other instances, etc)
	#
	soho.addObjects(now, '*', '*', '', do_culling=False)
	soho.lockObjects(now)

	soho_objs = {} # {objname: soho_obj}

	obj_list = []
	sop_dict = {} # {objname: sopname}
	objs = soho.objectList('objlist:instance')

	for obj in objs:
		n = obj.getName() # full pathname
		#dbg(" -- %s" % n )
		obj_list.append(n)
		soho_objs[n] = obj
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
	skip_frame = False

	# first frame: init all internal stuff
	#
	if is_first:
		dbg("IS_FIRST--INIT")
		G.archy = archy[:]
		G.archy_objs = archy_objs[:]

		# TODO: export hierarchy, allocate outmesh objs, etc.

		"""
		alembic todo:
		- create an oarchive
		- create new time sampling obj (AbcGeom::TimeSampling)
		- for each object, create new abc objects
			- obj/transform: AbcGeom::OXform
			- if geometry (mesh): AbcGeom::OPolyMesh
		"""

		# TODO: get timesampling values
		s = abc_init(abc_file, tstep=1.0/24.0, tstart=0.0)
		if s:
			# build objects for oarchive
			#
			for E in archy:
				objname = E[1]
				parent  = E[0]
				outname = E[2]
				soppath = E[3]

				dbg(" - new xform %s (obj=%s parent=%s)" % (outname, objname, parent))

				hou.hscript('%s newobject %s %s %s %s' % \
					(CCMD, objname, parent, outname, soppath))

		else:
			warn("couldn't output to file %s--aborting" % abc_file)
			skip_frame = True
			is_last = True



	# frame export: collect xforms, geoms, and export them
	#
	if archy_objs==G.archy_objs  and  not skip_frame:

		dbg(" -- exporting frame %.1f" % frame)
		pass

		"""
		alembic todo:
		- for each object
			- read/export xform matrix
			- if geom: read/export geometry (polymesh)
		"""

		for E in archy:
			objname = E[1]
			soppath = E[3]
			dbg(" - OBJ: %s" % E[1])

			# get xform matrix (TODO: get pretransform too!)
			#
			xform = None

			if objname in soho_objs:
				# get matrix from soho
				dbg(" --- (mtx from soho)")
				xform = []
				soho_objs[objname].evalFloat('space:local', now, xform)
				xform = hou.Matrix4(xform)
			else:
				# get matrix from hou
				dbg(" --- (mtx from hou)")
				n = hou.node(objname) # should be an hou.ObjNode
				pre = n.preTransform()
				xform = n.parmTransform()
				xform = pre * xform
			
			#dbg(" --- mtx: %s" % str(xform.asTupleOfTuples()))

			if True:
				hou.hscript('%s xformsample %f %s %s' % \
					(CCMD, now, objname, " ".join([ str(n) for n in xform.asTuple() ]) ))


			# get geom shape (if geometry)
			#
			if soppath:
				dbg(" --- SOP: %s" % soppath)

				hou.hscript('%s geosample %f %s %s' % \
					(CCMD, now, objname, soppath))

			else:
				dbg(" --- (no SOP)")

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

		"""
		alembic todo:
		- delete all alembic objs
			- transforms: OXform
			- geometry (mesh): OPolyMesh
		- delete oarchive
		"""

		abc_cleanup()





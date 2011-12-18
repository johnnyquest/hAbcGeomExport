hAbcGeomExport 0.03 (alpha)
===================
(Imre Tuske @ DiGiC Pictures, 2011-11)

(This is our small contribution for the Alembic project -- hopefully you'll
find it useful. Many thanks and big respects go to all the people working
on Alembic!)

For more information, see the github repository (especially the issues
section). Also "feedback is silver, contribution is gold" -- if you
find a bug, good for you, if you can fix it, good for all of us ;)

Thanks to Lucas Miller and Mark Elendt for their help.



WHAT IS
-------

This is a package of two simple geometry exporters to the Alembic file
format. They're available as Output Drivers (ROPs):




1. Custom/Alembic Geometry Export
---------------------------------
(ROP)

This is an old-school (pre-soho) output driver: it exports a single object
along with its hierarchical children. No fancy stuff (such as instancing)
is supported.

Simple as it is. this ROP works and it is stable and (should be) reliable.
It will be replaced by the SOHO-based ROP, as soon as it's ready.




2. Geometry/Alembic Export
--------------------------
(SOHO ROP -- alpha version)

This is a SOHO-based exporter, which means that all Houdini facilities are
fully available (most notably all kinds of instancing). Alembic is capable
of auto-detecting geometry duplicates (it automatically creates instances),
so instance away.

This ROP also supports an 'abc_staticgeo' parameter (inheritable), which can
be used to tag objects as non-deforming. Static objects are much faster to
export.

As this parameter is inheritable, you can use it directly on the ROP node,
or on individual object nodes.

Make sure you read the Notes below.




Export Notes/Known Issues
-------------------------

- The export root node is always a single Obj node. For all Obj nodes,
	the contents of the 'render' SOP will be exported.

- For now only polymeshes are supported. Unsupported object types (ie.
	non-polymesh) are exported as empty transforms (nulls).

- As for attributes, only normals (N) and UVs (uv) are supported, in
	per-point and per-vertex types.

- Hierarchies supported as connected Obj nodes; Subnets are NOT SUPPORTED
	yet (although they might work). This is quite the opposite behaviour
	to the H11.1+ alembic import, but hey, no one said life's easy.
	Both ways of hierarchy representations are planned to be supported.

- Transforms are exported as matrices for now--meaning that the final
	placements will match, but the xyz rotation values might NOT.
	(Expect Euler-style rotation-popping for animated 360+ rotations.)
	Alembic allows more sophisticated storing of rotations though,
	but it's not implemented here yet.

There are a few issues that can cause Houdini or Maya crash or hang (see
below), but the exported files are always correct (so far--knock on wood).



SOHO-Exporter Notes
-------------------

- It's an alpha version, meaning it kinda works (but parameters, UI, etc.
	will most certainly change).

- SOHO requires a render camera to be specified. It won't affect the export
	in any way, so any camera will do. (This will be done automatically
	in later versions.)

- Interrupting an export procedure doesn't work properly (the output file
	will be closed completely only on deleting the ROP node or on
	starting a new export -- the easiest workaround is to restart
	Houdini).




Houdini-related notes
---------------------

If you compiled your own version of Alembic in order to compile this ROP
(which is probably the case), and you use a different version of a library
(say hdf5) than what comes with Houdini, the libraries might collide.

For instance: if you export geometry with this ROP, then try to import it
right back in, Houdini might crash (with hdf5 complaining in the console).
In such cases simply do a restart of Houdini before re-importing. (Your
.abc files will be intact, no worries -- but you can use the various hdf5
utils to check.)

You can probably avoid all unpleasantries by building/linking your Alembic
version with the exact same lib-versions that the Houdini version uses.


Maya-related notes
------------------

Maya by default uses per-face-vertex storage for both normals and UVs
(called per-vertex in Houdini), but in Houdini it's possible to have them
as per vertex (called per-point in Houdini). Although Alembic might be able
to convert between these datatypes to a certain degree (?), you might want
to use the data type that's more appropriate.

In short: better to stick to per-face-vertex normals and UVs.

Maya 2011 seems to have problems when it comes to importing any normals
from alembic, which can result in crashes or hangs. Either upgrade or
forget about importing normals (or have a few sad days, like I did :|).



History
-------

version 0.03 (2011-12-15)
	Initial version of a SOHO interface (able to export
	geometry instances, etc).



version 0.02 (2011-12-06)
	First version to export of hierarchies (transforms+shapes),
	polymesh normals and UVs.



version 0.01 (2011-12-02)
	Can export a lonely polygonal shape.



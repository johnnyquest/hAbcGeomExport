hAbcGeomExport 0.03
===================
(Imre Tuske @ DiGiC Pictures, 2011-11)


(This is our small contribution for the Alembic project -- hopefully you'll
find it useful. Many thanks and big respects go to all the people working
on Alembic!)


A simple geometry exporter to the Alembic file format, implemented as
a ROP node (or output driver).

It exports a hierarchy of object nodes from the specified root object,
including geometry (currently poly meshes only). The SOPs flagged as
'renderable' are used.

The exported objects will be either polymeshes (for OBJ_Nodes containing
polygon data) or empty transforms (for everything else).

For more information, see the github repository (especially the issues
section). Also "feedback is silver, contribution is gold" -- if you
find a bug, good for you, if you can fix it, good for all of us ;)

And while I'm at it, thanks to Lucas Miller and Mark Elendt for their help.




IMPORTANT WINDOWS-RELATED NOTE
------------------------------

I cannot and WILL NOT build this thing under windows as for me all windows-
related build procedures turn out nothing but pure frustration. However since
we'd like to use this on windows, too, I'd really appreciate if someone could
put together an MSVC++ project file, or something to that effect. Thanks!




Known limitations
-----------------

1.

It only exports hierarchies as connected Obj nodes -- subnets are not supported.
(It's actually quite the opposite of how the Alembic importer builds a scene,
for example.)

2.

Only the most regular type of polymesh supported (no open polylines, nurbs
curves/surfaces or other fanciness.)

Hierarchy is maintained, but all transforms are written out as local-space
ones (as usual) -- this might give 'unexpected' results if a partial
hierarchy is exported where the non-exported parents also have various
transformations.

Transforms are exported as matrices for now--meaning that the final
placements will match, but the xyz rotation values might NOT.
(Expect Euler-style rotation-popping for animated 360+ rotations.)

There are a few issues that can cause Houdini or Maya crash or hang (see
below), but the exported files are always correct (so far--knock on wood).



Build notes
-----------

You probably have to build Alembic first (which can be a lot of fun :)).

For building this ROP, see

	./build.sh

for testing, see

	./data/test.sh

(test.sh uses LD_LIBRARY_PATH to set some library paths explicitly, which
is a quick hack, try to avoid that--compile with the exact same lib
versions, or go for full static linking, if possible (?) )

To build testing/example hip files, go to the ./data folder in a shell and
type
	./hip_build.sh

This will build .hip files from their extracted versions.




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
	Initial version of a SOHO interface (hence the ability to
	export geometry instances, etc).



version 0.02 (2011-12-06)
	First version to export of hierarchies (transforms+shapes),
	polymesh normals and UVs.



version 0.01 (2011-12-02)
	Can export a lonely polygonal shape.



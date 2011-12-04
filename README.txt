hAbcGeomExport 0.01
===================
(Imre Tuske, 2011)

A simple geometry exporter to the Alembic file format, implemented as
a ROP node (or output driver).

It exports a hierarchy of object nodes from the specified root object,
including geometry (currently poly meshes only). The SOPs flagged as
'renderable' are used.



Known limitations
-----------------

Hierarchy is maintained, but all transforms are written out as local-space
ones (as usual) -- this might give 'unexpected' results if a partial
hierarchy is exported where the 'missing' parents also have various
transformations.





TODO
----
	- normal/uv export
		- should support both per-point and per-vertex types
	
	- export transformations not as a 'raw' matrix but translate+rotate+etc.
		with hints and all

	- per-point velocity attribute (velocity blur) export

	- export of all custom attributes
		- per-point, per-vertex, per-prim, detail

	- (?) support primitive groups as alembic face sets

	- (?) export specified primitive groups as individual objects
		- support a special case for fractured rigid bodies
		  (export them as multiple transformed objects instead
		  of a single 'deforming' one)

	- a proper build script (waf?)



version 0.01 (2011-12-06)


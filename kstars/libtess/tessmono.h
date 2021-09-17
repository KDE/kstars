/*
** Author: Eric Veach, July 1994.
**
*/

#ifndef __tessmono_h_
#define __tessmono_h_

/* __gl_meshTessellateMonoRegion( face ) tessellates a monotone region
 * (what else would it do??)  The region must consist of a single
 * loop of half-edges (see mesh.h) oriented CCW.  "Monotone" in this
 * case means that any vertical line intersects the interior of the
 * region in a single interval.
 *
 * Tessellation consists of adding interior edges (actually pairs of
 * half-edges), to split the region into non-overlapping triangles.
 *
 * __gl_meshTessellateInterior( mesh ) tessellates each region of
 * the mesh which is marked "inside" the polygon.  Each such region
 * must be monotone.
 *
 * __gl_meshDiscardExterior( mesh ) zaps (ie. sets to NULL) all faces
 * which are not marked "inside" the polygon.  Since further mesh operations
 * on NULL faces are not allowed, the main purpose is to clean up the
 * mesh so that exterior loops are not represented in the data structure.
 *
 * __gl_meshSetWindingNumber( mesh, value, keepOnlyBoundary ) resets the
 * winding numbers on all edges so that regions marked "inside" the
 * polygon have a winding number of "value", and regions outside
 * have a winding number of 0.
 *
 * If keepOnlyBoundary is TRUE, it also deletes all edges which do not
 * separate an interior region from an exterior one.
 */

int __gl_meshTessellateMonoRegion(GLUface *face);
int __gl_meshTessellateInterior(GLUmesh *mesh);
void __gl_meshDiscardExterior(GLUmesh *mesh);
int __gl_meshSetWindingNumber(GLUmesh *mesh, int value, GLboolean keepOnlyBoundary);

#endif

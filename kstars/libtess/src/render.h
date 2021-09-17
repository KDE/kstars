/*
** Author: Eric Veach, July 1994.
**
*/

#ifndef __render_h_
#define __render_h_

#include "mesh.h"

/* __gl_renderMesh( tess, mesh ) takes a mesh and breaks it into triangle
 * fans, strips, and separate triangles.  A substantial effort is made
 * to use as few rendering primitives as possible (ie. to make the fans
 * and strips as large as possible).
 *
 * The rendering output is provided as callbacks (see the api).
 */
void __gl_renderMesh(GLUtesselator *tess, GLUmesh *mesh);
void __gl_renderBoundary(GLUtesselator *tess, GLUmesh *mesh);

GLboolean __gl_renderCache(GLUtesselator *tess);

#endif

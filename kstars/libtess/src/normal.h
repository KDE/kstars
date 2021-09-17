/*
** Author: Eric Veach, July 1994.
**
*/

#ifndef __normal_h_
#define __normal_h_

#include "tess.h"

/* __gl_projectPolygon( tess ) determines the polygon normal
 * and project vertices onto the plane of the polygon.
 */
void __gl_projectPolygon(GLUtesselator *tess);

#endif

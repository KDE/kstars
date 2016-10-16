/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  
 *  Copyright (C) 2009 Petr Kubanek
 */

#ifndef _LN_HELIOCENTRIC_TIME_H
#define _LN_HELIOCENTRIC_TIME_H

#include <libnova/ln_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
* \defgroup heliocentric Heliocentric time
*/

/*! \fn double ln_get_heliocentric_time_diff (double JD, struct ln_equ_posn *object)
* \ingroup heliocentric
* \brief Calculate approximate heliocentric (barycentric) time correction for given date and object
*
*/
double LIBNOVA_EXPORT ln_get_heliocentric_time_diff (double JD, struct ln_equ_posn *object);

#ifdef __cplusplus
};
#endif

#endif

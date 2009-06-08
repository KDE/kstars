/*************************************************************************
             byteswap.h  -  platform independent byteswap macros
                             -------------------
    begin                : Fri Jul 31 2004
    copyright            : (C) 2004 by Thomas Eschenbacher
    email                : Thomas.Eschenbacher@gmx.de
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Modified appropriately and incorporated for use in KStars on 9 Jun
// 2008 by Akarsh Simha <akarsh.simha@kdemail.net>

#ifndef BYTESWAP_H
#define BYTESWAP_H

#define bswap_16(x)   ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))

#define bswap_32(x)   ((((x) & 0xFF000000) >> 24) | \
                       (((x) & 0x00FF0000) >> 8)  | \
                       (((x) & 0x0000FF00) << 8)  | \
                       (((x) & 0x000000FF) << 24) )

#endif

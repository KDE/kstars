/***************************************************************************
                     stardata.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon 9 Jun 2008
    copyright            : (C) 2008 by Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef STARDATA_H
#define STARDATA_H

#include <QtGlobal>

/**
 *@short  A 32-byte Structure that holds star data
 *@author Akarsh Simha
 *@version 1.0
 */
struct starData
{
    qint32 RA;          /**< Raw signed 32-bit RA value. Needs to be multiplied by the scale (1e6) */
    qint32 Dec;         /**< Raw signed 32-bit DE value. Needs to be multiplied by the scale (1e6) */
    qint32 dRA;
    qint32 dDec;
    qint32 parallax;
    qint32 HD;          /**< signed? 32-bit Henry Draper Index */
    qint16 mag;         /**< signed 16-bit raw magnitude. Needs to be divided by the scale (1e2) */
    qint16 bv_index;
    char spec_type[2];
    char flags;
    char unused;
};

#endif

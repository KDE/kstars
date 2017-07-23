/***************************************************************************
                   deepstardata.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 5 Aug 2008
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

#pragma once

#include <QtGlobal>

/**
 * @short  A 16-byte structure that holds star data for really faint stars.
 *
 * @author Akarsh Simha
 * @version 1.0
 */
struct DeepStarData
{
    qint32 RA { 0 };  /**< Raw signed 32-bit RA value. Needs to be multiplied by the scale (1e6) */
    qint32 Dec { 0 }; /**< Raw signed 32-bit DE value. Needs to be multiplied by the scale (1e6) */
    qint16 dRA { 0 };
    qint16 dDec { 0 };
    qint16 B { 0 };
    qint16 V { 0 };
};

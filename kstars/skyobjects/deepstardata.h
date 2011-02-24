/***************************************************************************
                     stardata.h  -  K Desktop Planetarium
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

#ifndef DEEPSTARDATA_H
#define DEEPSTARDATA_H

#include <QtGlobal>

/**
 *@short  Structure that holds star data for really faint stars
 *@author Akarsh Simha
 *@version 1.0
 */
struct deepStarData {
    qint32 RA;
    qint32 Dec;
    qint16 dRA;
    qint16 dDec;
    qint16 B;
    qint16 V;
};

#endif

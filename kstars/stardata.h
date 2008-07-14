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
#include <sys/types.h>

/**
 *@short  Structure that holds star data
 *@author Akarsh Simha
 *@version 1.0
 */

typedef struct starData {
    int32_t RA;
    int32_t Dec;
    int32_t dRA;
    int32_t dDec;
    int32_t parallax;
    int32_t HD;
    int16_t mag;
    int16_t bv_index;
    char spec_type[2];
    char flags;
    char unused;
} starData;

#endif

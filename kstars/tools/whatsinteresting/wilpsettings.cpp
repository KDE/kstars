/***************************************************************************
                          wilpsettings.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2013/07/01
    copyright            : (C) 2013 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "wilpsettings.h"
#include "kstars.h"

WILPSettings::WILPSettings(KStars *ks): QFrame(ks), m_Ks(ks)
{
    setupUi(this);
    connect(kcfg_BortleClass, SIGNAL(valueChanged(int)), this, SLOT(bortleValueChanged(int)));
}

void WILPSettings::bortleValueChanged(int value)
{
    kcfg_BortleClass->setValue(value);
}

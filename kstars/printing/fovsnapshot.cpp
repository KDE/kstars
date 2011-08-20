/***************************************************************************
                          fovsnapshot.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 14 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "fovsnapshot.h"

FovSnapshot::FovSnapshot(const QPixmap &pixmap, QString description, FOV *fov, const SkyPoint &center) :
        m_Pixmap(pixmap), m_Description(description), m_Fov(fov), m_CentralPoint(center)
{}

FovSnapshot::~FovSnapshot()
{}





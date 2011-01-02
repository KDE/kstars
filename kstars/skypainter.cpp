/*
    SkyPainter: class for painting onto the sky for KStars
    Copyright (C) 2010 Henry de Valence <hdevalence@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include "skypainter.h"

#include "skymap.h"
#include "Options.h"
#include "kstarsdata.h"
#include "skycomponents/skiplist.h"
#include "skycomponents/linelistlabel.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/trailobject.h"

SkyPainter::SkyPainter()
    : m_sizeMagLim(10.)
{
    m_sm = SkyMap::Instance();
}

SkyPainter::~SkyPainter()
{

}

SkyMap* SkyPainter::skyMap() const
{
    return m_sm;
}

void SkyPainter::setSizeMagLimit(float sizeMagLim)
{
    m_sizeMagLim = sizeMagLim;
}

float SkyPainter::starWidth(float mag) const
{
    //adjust maglimit for ZoomLevel
    const double maxSize = 10.0;

    double lgmin = log10(MINZOOM);
//    double lgmax = log10(MAXZOOM);
    double lgz = log10(Options::zoomFactor());

    float sizeFactor = maxSize + (lgz - lgmin);

    float size = ( sizeFactor*( m_sizeMagLim - mag ) / m_sizeMagLim ) + 1.;
    if( size <= 1.0 ) size = 1.0;
    if( size > maxSize ) size = maxSize;

    return size;
}


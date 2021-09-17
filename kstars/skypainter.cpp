/*
    SkyPainter: class for painting onto the sky for KStars
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skypainter.h"

#include "skymap.h"
#include "Options.h"
#include "kstarsdata.h"
#include "skycomponents/skiphashlist.h"
#include "skycomponents/linelistlabel.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/trailobject.h"
#include "skyobjects/constellationsart.h"

SkyPainter::SkyPainter()
{
    m_sm = SkyMap::Instance();
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

    float size = (sizeFactor * (m_sizeMagLim - mag) / m_sizeMagLim) + 1.;
    if (size <= 1.0)
        size = 1.0;
    if (size > maxSize)
        size = maxSize;

    return size;
}

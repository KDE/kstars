/*  Ekos Dark View
 *  Child of FTISView with few additions necessary for Alignment functions

    Copyright (C) 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "darkview.h"

#include "kstarsdata.h"
#include "Options.h"
#include "fitsviewer/fitsdata.h"
#include "defectmap.h"

#include <QPainter>
#include <QtConcurrent>
#include <QRectF>

DarkView::DarkView(QWidget *parent, FITSMode mode, FITSScale filter) : FITSView(parent, mode, filter)
{
}

void DarkView::drawOverlay(QPainter *painter, double scale)
{
    Q_UNUSED(scale);
    painter->setOpacity(0.5);
    FITSView::drawOverlay(painter, getScale());
    painter->setOpacity(1);

    if (m_CurrentDefectMap)
        drawBadPixels(painter, scale);
    // Add here our own overlays
    //drawRaAxis(painter);
}



void DarkView::reset()
{
    //    correctionFrom = QPointF();
    //    correctionTo = QPointF();
    //    correctionAltTo = QPointF();
    //    markerCrosshair = QPointF();
    //    celestialPolePoint = QPointF();
    //    raAxis = QPointF();
    //    starCircle = QPointF();
    //    releaseImage();
}

void DarkView::setDefectMap(const QSharedPointer<DefectMap> &defect)
{
    m_CurrentDefectMap = defect;

    connect(m_CurrentDefectMap.data(), &DefectMap::pixelsUpdated, this, &DarkView::updateFrame);
}

void DarkView::drawBadPixels(QPainter * painter, double scale)
{
    //QFont painterFont;
    //double fontSize = painterFont.pointSizeF() * 2;
    //painter->setRenderHint(QPainter::Antialiasing);

    painter->setPen(QPen(QColor(qRgba(255, 0, 0, 128)), scale));
    for (BadPixelSet::const_iterator onePixel = m_CurrentDefectMap->hotThreshold();
            onePixel != m_CurrentDefectMap->hotPixels().cend(); ++onePixel)
        //painter->drawPoint(QPointF(((*onePixel).x + 0.5) * scale, ((*onePixel).y + 0.5) * scale));
        painter->drawEllipse(QRectF(((*onePixel).x - 1) * scale, ((*onePixel).y - 1) * scale, 2 * scale, 2 * scale));

    painter->setPen(QPen(QColor(qRgba(0, 0, 255, 128)), scale));
    for (BadPixelSet::const_iterator onePixel = m_CurrentDefectMap->coldPixels().cbegin();
            onePixel != m_CurrentDefectMap->coldThreshold(); ++onePixel)
        //painter->drawPoint(QPointF(((*onePixel).x + 0.5) * scale, ((*onePixel).y + 0.5) * scale));
        painter->drawEllipse(QRectF(((*onePixel).x - 1) * scale, ((*onePixel).y - 1) * scale, 2 * scale, 2 * scale));
}


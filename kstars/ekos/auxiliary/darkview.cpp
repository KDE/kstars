/*  Ekos Dark View
    Child of FTISView with few additions necessary for Alignment functions

    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

    if (m_CurrentDefectMap && m_DefectMapEnabled)
        drawBadPixels(painter, scale);
}

void DarkView::reset()
{
    m_CurrentDefectMap.clear();
}

void DarkView::setDefectMapEnabled(bool enabled)
{
    if (m_DefectMapEnabled == enabled)
        return;

    m_DefectMapEnabled = enabled;
    updateFrame();
}

void DarkView::setDefectMap(const QSharedPointer<DefectMap> &defect)
{
    m_CurrentDefectMap = defect;
    connect(m_CurrentDefectMap.data(), &DefectMap::pixelsUpdated, this, &DarkView::updateFrame, Qt::UniqueConnection);
}

void DarkView::drawBadPixels(QPainter * painter, double scale)
{
    if (!m_CurrentDefectMap)
        return;

    if (m_CurrentDefectMap->hotCount() > 0)
    {
        painter->setPen(QPen(QColor(qRgba(255, 0, 0, 128)), scale));
        for (BadPixelSet::const_iterator onePixel = m_CurrentDefectMap->hotThreshold();
                onePixel != m_CurrentDefectMap->hotPixels().cend(); ++onePixel)
        {
            painter->drawEllipse(QRectF(((*onePixel).x - 1) * scale, ((*onePixel).y - 1) * scale, 2 * scale, 2 * scale));
        }
    }

    if (m_CurrentDefectMap->coldCount() > 0)
    {
        painter->setPen(QPen(QColor(qRgba(0, 0, 255, 128)), scale));
        for (BadPixelSet::const_iterator onePixel = m_CurrentDefectMap->coldPixels().cbegin();
                onePixel != m_CurrentDefectMap->coldThreshold(); ++onePixel)
        {
            painter->drawEllipse(QRectF(((*onePixel).x - 1) * scale, ((*onePixel).y - 1) * scale, 2 * scale, 2 * scale));
        }
    }
}


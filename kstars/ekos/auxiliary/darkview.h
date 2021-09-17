/*  Ekos Dark View
    Child of FTISView with few additions necessary for Alignment functions

    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitsviewer/fitsview.h"

#include <QVector3D>

class QPainter;
class DefectMap;

class DarkView : public FITSView
{
        Q_OBJECT

    public:
        explicit DarkView(QWidget *parent = nullptr, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE);

        void drawOverlay(QPainter *, double scale) override;

        // Resets the marker and lines, celestial pole point and raAxis.
        void reset();
        void setDefectMap(const QSharedPointer<DefectMap> &defect);
        void setDefectMapEnabled(bool enabled);

    protected:
        void drawBadPixels(QPainter * painter, double scale);

    private:
        QSharedPointer<DefectMap> m_CurrentDefectMap;
        bool m_DefectMapEnabled {false};

};

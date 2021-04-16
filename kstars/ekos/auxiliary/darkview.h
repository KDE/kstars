/*  Ekos Dark View
 *  Child of FTISView with few additions necessary for Alignment functions

    Copyright (C) 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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

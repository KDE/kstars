/*  Ekos Alignment View
    Child of FITSView with few additions necessary for Alignment functions

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitsviewer/fitsview.h"

#include <QVector3D>

class QPainter;

class AlignView : public FITSView
{
        Q_OBJECT
    public:
        explicit AlignView(QWidget *parent = nullptr, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE);

        /* Calculate WCS header info and update WCS info */
        bool injectWCS(double orientation, double ra, double dec, double pixscale, bool eastToTheRight, bool extras);

        void drawOverlay(QPainter *, double scale) override;

        // Resets the marker and lines, celestial pole point and raAxis.
        void reset();

        // Correction line
        void setCorrectionParams(const QPointF &from, const QPointF &to, const QPointF &altTo);

        void setRaAxis(const QPointF &value);
        void setCelestialPole(const QPointF &value);
        void setRefreshEnabled(bool enable);

        // When non-null, alignview draws a small circle in the pixel position specified.
        void setStarCircle(const QPointF &pixel = QPointF());

        void holdOnToImage();
        void releaseImage();
        const QSharedPointer<FITSData> keptImage() const
        {
            return keptImagePointer;
        }

    protected:
        void drawTriangle(QPainter *painter);
        void drawRaAxis(QPainter *painter);
        void drawStarCircle(QPainter *painter);

        virtual void processMarkerSelection(int x, int y) override;

    private:
        // Correction points. from=user-selected point. to=destination for the point.
        // altTo = destination to correct altitude only.
        QPointF correctionFrom, correctionTo, correctionAltTo;
        // The celestial pole's position on the image.
        QPointF celestialPolePoint;
        // The mount's RA axis' position on the image.
        QPointF raAxis;
        QSharedPointer<FITSData> keptImagePointer;
        // The position of a star being tracked in the polar-alignment routine.
        QPointF starCircle;

    signals:
        void newCorrectionVector(QLineF correctionVector);
};

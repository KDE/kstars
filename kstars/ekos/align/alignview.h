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

        // Calculate WCS header info and update WCS info.
        // If the expensive computations are not included, e.g. extras == false, then there's
        // no reason to block (i.e. use the wcsWatcher). The computations are quick.
        bool injectWCS(double orientation, double ra, double dec, double pixscale, bool eastToTheRight, bool block = true);

        void drawOverlay(QPainter *, double scale) override;

        // Resets the marker and lines, celestial pole point and raAxis.
        void reset();

        // Setup correction triangle
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
        // Draw the polar-align triangle which guides the user how to correct polar alignment.
        void drawTriangle(QPainter *painter, const QPointF &from, const QPointF &to, const QPointF &altTo);
        // Draws the mounts current RA axis (set in setRaAxis() above).
        void drawRaAxis(QPainter *painter);
        // Draw the circle around the star used to help the user correct polar alignment.
        void drawStarCircle(QPainter *painter, const QPointF &center, double radius, const QColor &color);

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

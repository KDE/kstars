/*
    SPDX-FileCopyrightText: 2003-2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2016-2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dms.h"
#include "fitscommon.h"

#include <qpoint.h>
#include <QLabel>
#include <QRubberBand>

class FITSView;

class QMouseEvent;
class QString;

class FITSLabel : public QLabel
{
        Q_OBJECT
    public:
        explicit FITSLabel(FITSView *img, QWidget *parent = nullptr);
        virtual ~FITSLabel() override = default;

        void setSize(double w, double h);
        void centerTelescope(double raJ2000, double decJ2000);
        bool getMouseButtonDown();
        void updatePersistentRoiLabel(const QRect &roiGeometry);

    public slots:
        void setRubberBand(QRect rect);
        void showRubberBand(bool on);
        void zoomRubberBand(double scale);

    protected:
        virtual void mouseMoveEvent(QMouseEvent *e) override;
        virtual void mousePressEvent(QMouseEvent *e) override;
        virtual void mouseReleaseEvent(QMouseEvent *e) override;
        virtual void mouseDoubleClickEvent(QMouseEvent *e) override;
        virtual void leaveEvent(QEvent *e) override;

    private slots:
        void handleImageDataUpdated();

    private:
        bool mouseButtonDown { false };
        bool isRoiSelected { false };
        QPoint lastMousePoint;
        FITSView *view { nullptr };
        QLabel *m_roiStatsLabel { nullptr };
        dms m_RA;
        dms m_DE;
        float prevscale{ 1.0 };
        double m_Width { 0 };
        double m_Height { 0 };
        double m_Size { 0 };

        QPoint m_p1;
        QPoint m_p2;
        QRect diffRect;
        QRubberBand *roiRB;
        QPoint prevPoint;


    signals:
        void newStatus(const QString &msg, FITSBar id);
        void pointSelected(int x, int y);
        void highlightSelected(int x, int y);
        void markerSelected(int x, int y);
        void rectangleSelected(QPoint p1, QPoint p2, bool refreshCenter);
        void circleSelected(QPoint p1, QPoint p2);
        void mouseOverPixel(int x, int y);
};

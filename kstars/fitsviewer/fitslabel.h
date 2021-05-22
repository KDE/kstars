/*  FITS Label
    Copyright (C) 2003-2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2016-2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "dms.h"
#include "fitscommon.h"

#include <qpoint.h>
#include <QLabel>

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

    protected:
        virtual void mouseMoveEvent(QMouseEvent *e) override;
        virtual void mousePressEvent(QMouseEvent *e) override;
        virtual void mouseReleaseEvent(QMouseEvent *e) override;
        virtual void mouseDoubleClickEvent(QMouseEvent *e) override;
        virtual void leaveEvent(QEvent *e) override;

    private:
        bool mouseButtonDown { false };
        QPoint lastMousePoint;
        FITSView *view { nullptr };
        dms m_RA;
        dms m_DE;
        double m_Width { 0 };
        double m_Height { 0 };
        double m_Size { 0 };

    signals:
        void newStatus(const QString &msg, FITSBar id);
        void pointSelected(int x, int y);
        void markerSelected(int x, int y);
};

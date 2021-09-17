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

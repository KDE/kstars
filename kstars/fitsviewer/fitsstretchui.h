/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_fitsstretchui.h"
#include "stretch.h"

#include <QWidget>

class FITSView;

class FITSStretchUI : public QWidget, public Ui::FITSStretchUI
{
        Q_OBJECT

    public:
        FITSStretchUI(const QSharedPointer<FITSView> &view, QWidget * parent = nullptr);

        void generateHistogram();
        void setStretchValues(double shadows, double midtones, double highlights);

    private:
        void setupButtons();
        void setupHistoPlot();
        void setupHistoSlider();
        void setStretchUIValues(const StretchParams1Channel &params);
        void setupConnections();
        void setupPresetText(bool enabled);
        void onHistoDoubleClick(QMouseEvent *event);
        void onHistoMouseMove(QMouseEvent *event);

        QCPItemLine * setCursor(int position, const QPen &pen);
        void setCursors(const StretchParams &params);
        void removeCursors();

        QSharedPointer<FITSView> m_View;
        QCPItemLine *minCursor = nullptr;
        QCPItemLine *maxCursor = nullptr;
        QVector<QCPItemLine*> pixelCursors;
        int m_StretchPresetNumber = 1;
};


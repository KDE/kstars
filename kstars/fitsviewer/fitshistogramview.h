/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitscommon.h"
#include "fitsdata.h"
#include "fitshistogramcommand.h"

#include "qcustomplot.h"

class QMouseEvent;

class FITSHistogramView : public QCustomPlot
{
        Q_OBJECT

        Q_PROPERTY(bool axesLabelEnabled MEMBER m_AxesLabelEnabled)
        Q_PROPERTY(bool linear MEMBER m_Linear)
        friend class histDrawArea;

    public:
        explicit FITSHistogramView(QWidget *parent = nullptr);

        void reset();
        void setImageData(const QSharedPointer<FITSData> &data);
        void syncGUI();

    protected:
        void showEvent(QShowEvent * event) override;
        void driftMouseOverLine(QMouseEvent * event);

    signals:
        void constructed();

    public slots:
        //void applyScale();
        void resizePlot();

    private:
        void createNonLinearHistogram();
        QVector<QCPGraph *> graphs;
        QVector<int> numDecimals;
        bool isGUISynced { false};
        bool m_AxesLabelEnabled {true};
        bool m_Linear { true };
        QSharedPointer<FITSData> m_ImageData;
        QVector<QVector<double>> m_HistogramIntensity;
        QVector<QVector<double>> m_HistogramFrequency;
};

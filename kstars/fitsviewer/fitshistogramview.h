/*  FITS Histogram
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

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

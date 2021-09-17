/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitscommon.h"
#include "fitsdata.h"
#include "ui_fitshistogramui.h"

#include <QDialog>
#include <QUndoCommand>

class QMouseEvent;

class FITSTab;

class histogramUI : public QDialog, public Ui::FITSHistogramUI
{
        Q_OBJECT

    public:
        explicit histogramUI(QDialog * parent = nullptr);
};

class FITSHistogram : public QDialog
{
        Q_OBJECT

        friend class histDrawArea;

    public:
        explicit FITSHistogram(QWidget * parent);

        enum { RED_CHANNEL, GREEN_CHANNEL, BLUE_CHANNEL };

        void constructHistogram();
        void syncGUI();
        void reset()
        {
            m_Constructed = false;
        }

        void applyFilter(FITSScale ftype);

        double getBinWidth(int channel = 0)
        {
            return binWidth[channel];
        }

        QVector<uint32_t> getCumulativeFrequency(int channel = 0) const;

        double getJMIndex() const;

        bool isConstructed()
        {
            return m_Constructed;
        }


    protected:
        void showEvent(QShowEvent * event) override;
        void driftMouseOverLine(QMouseEvent * event);

    public slots:
        void applyScale();
        void resizePlot();

    private:
        template <typename T>
        void constructHistogram();
        double cutMin;
        double cutMax;

        histogramUI * ui { nullptr };
        FITSTab * tab { nullptr };

        QVector<QVector<uint32_t>> cumulativeFrequency;
        QVector<QVector<double>> intensity;
        QVector<QVector<double>> frequency;
        QVector<QVector<QWidget *>> rgbWidgets;
        QVector<ctkRangeSlider *> sliders;
        QVector<QDoubleSpinBox *> minBoxes, maxBoxes;

        QVector<double> FITSMin;
        QVector<double> FITSMax;
        QVector<double> sliderScale, sliderTick;
        QVector<int> numDecimals;

        QVector<QCPGraph *> graphs;
        QVector<double> binWidth;
        uint16_t binCount { 0 };
        double JMIndex { 0 };

        int maxFrequency {0};
        FITSScale type { FITS_AUTO };
        bool isGUISynced { false};
        bool m_Constructed { false };
        QCustomPlot * customPlot { nullptr };
};

class FITSHistogramCommand : public QUndoCommand
{
    public:
        FITSHistogramCommand(QWidget * parent, FITSHistogram * inHisto, FITSScale newType, const QVector<double> &lmin, const QVector<double> &lmax);
        virtual ~FITSHistogramCommand();

        virtual void redo() override;
        virtual void undo() override;
        virtual QString text() const;

    private:
        bool calculateDelta(const uint8_t * buffer);
        bool reverseDelta();

        FITSImage::Statistic stats;
        FITSHistogram * histogram { nullptr };
        FITSScale type;
        QVector<double> min, max;

        unsigned char * delta { nullptr };
        unsigned long compressedBytes { 0 };
        FITSTab * tab { nullptr };
};

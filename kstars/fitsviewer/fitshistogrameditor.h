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
#include "fitshistogramview.h"
#include "ui_fitshistogramui.h"

#include <QDialog>

class QMouseEvent;

class FITSTab;

class histogramUI : public QDialog, public Ui::FITSHistogramUI
{
        Q_OBJECT

    public:
        explicit histogramUI(QDialog * parent = nullptr);
};

class FITSHistogramEditor : public QDialog
{
        Q_OBJECT

        friend class histDrawArea;

    public:
        explicit FITSHistogramEditor(QWidget * parent);

        //void createNonLinearHistogram();
        void setImageData(const QSharedPointer<FITSData> &data);
        void syncGUI();
        void applyFilter(FITSScale ftype);

    protected:
        void showEvent(QShowEvent * event) override;

    signals:
        void newHistogramCommand(FITSHistogramCommand *command);

    public slots:
        void applyScale();
        void resizePlot();

    private:
        histogramUI * ui { nullptr };

        QVector<QVector<QWidget *>> rgbWidgets;
        QVector<ctkRangeSlider *> sliders;
        QVector<QDoubleSpinBox *> minBoxes, maxBoxes;

        QVector<int> numDecimals;
        QVector<double> sliderScale, sliderTick;

        FITSScale type { FITS_AUTO };
        bool isGUISynced { false};
        bool m_Constructed { false };
        QSharedPointer<FITSData> m_ImageData;
};

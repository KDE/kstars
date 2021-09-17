/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

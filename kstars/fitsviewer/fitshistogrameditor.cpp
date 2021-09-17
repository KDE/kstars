/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitshistogrameditor.h"

#include "fits_debug.h"

#include "Options.h"
#include "fitsdata.h"
#include "fitstab.h"
#include "fitsview.h"
#include "fitsviewer.h"

#include <KMessageBox>

#include <QtConcurrent>
#include <type_traits>

histogramUI::histogramUI(QDialog * parent) : QDialog(parent)
{
    setupUi(parent);
    setModal(false);
}

FITSHistogramEditor::FITSHistogramEditor(QWidget * parent) : QDialog(parent)
{
    ui = new histogramUI(this);

    minBoxes << ui->minREdit << ui->minGEdit << ui->minBEdit;
    maxBoxes << ui->maxREdit << ui->maxGEdit << ui->maxBEdit;
    sliders << ui->redSlider << ui->greenSlider << ui->blueSlider;

    ui->histogramPlot->setProperty("linear", !Options::nonLinearHistogram());

    connect(ui->applyB, &QPushButton::clicked, this, &FITSHistogramEditor::applyScale);
    //    connect(ui->hideSaturated, &QCheckBox::stateChanged, [this]()
    //    {
    //        m_ImageData->resetHistogram();
    //        m_ImageData->constructHistogram();
    //        ui->histogramPlot->syncGUI();
    //    });

    rgbWidgets.resize(3);
    rgbWidgets[RED_CHANNEL] << ui->RLabel << ui->minREdit << ui->redSlider
                            << ui->maxREdit;
    rgbWidgets[GREEN_CHANNEL] << ui->GLabel << ui->minGEdit << ui->greenSlider
                              << ui->maxGEdit;
    rgbWidgets[BLUE_CHANNEL] << ui->BLabel << ui->minBEdit << ui->blueSlider
                             << ui->maxBEdit;

    for (int i = 0; i < 3; i++)
    {
        // Box --> Slider
        QVector<QWidget *> w = rgbWidgets[i];
        connect(qobject_cast<QDoubleSpinBox *>(w[1]), &QDoubleSpinBox::editingFinished, [this, i, w]()
        {
            double value = qobject_cast<QDoubleSpinBox *>(w[1])->value();
            w[2]->blockSignals(true);
            qobject_cast<ctkRangeSlider *>(w[2])->setMinimumPosition((value - m_ImageData->getMin(i))*sliderScale[i]);
            w[2]->blockSignals(false);
        });
        connect(qobject_cast<QDoubleSpinBox *>(w[3]), &QDoubleSpinBox::editingFinished, [this, i, w]()
        {
            double value = qobject_cast<QDoubleSpinBox *>(w[3])->value();
            w[2]->blockSignals(true);
            qobject_cast<ctkRangeSlider *>(w[2])->setMaximumPosition((value - m_ImageData->getMin(i) - sliderTick[i])*sliderScale[i]);
            w[2]->blockSignals(false);
        });

        // Slider --> Box
        connect(qobject_cast<ctkRangeSlider *>(w[2]), &ctkRangeSlider::minimumValueChanged, [this, i, w](int position)
        {
            qobject_cast<QDoubleSpinBox *>(w[1])->setValue(m_ImageData->getMin(i) + (position / sliderScale[i]));
        });
        connect(qobject_cast<ctkRangeSlider *>(w[2]), &ctkRangeSlider::maximumValueChanged, [this, i, w](int position)
        {
            qobject_cast<QDoubleSpinBox *>(w[3])->setValue(m_ImageData->getMin(i) + sliderTick[i] + (position / sliderScale[i]));
        });
    }
}

void FITSHistogramEditor::showEvent(QShowEvent * event)
{
    Q_UNUSED(event)
    //    if (!Options::nonLinearHistogram() && !m_ImageData->isHistogramConstructed())
    //        m_ImageData->constructHistogram();

    syncGUI();

}

//void FITSHistogramEditor::createNonLinearHistogram()
//{
//    ui->histogramPlot->createNonLinearHistogram();
//}

void FITSHistogramEditor::resizePlot()
{
    ui->histogramPlot->resizePlot();
}

void FITSHistogramEditor::syncGUI()
{
    if (isGUISynced)
        return;

    sliderTick.clear();
    sliderScale.clear();
    for (int n = 0; n < m_ImageData->channels(); n++)
    {
        sliderTick  << fabs(m_ImageData->getMax(n) - m_ImageData->getMin(n)) / 99.0;
        sliderScale << 99.0 / (m_ImageData->getMax(n) - m_ImageData->getMin(n) - sliderTick[n]);
    }

    ui->histogramPlot->syncGUI();

    bool isColor = m_ImageData->channels() > 1;
    // R/K is always enabled
    for (auto w : rgbWidgets[RED_CHANNEL])
        w->setEnabled(true);
    // G Channel
    for (auto w : rgbWidgets[GREEN_CHANNEL])
        w->setEnabled(isColor);
    // B Channel
    for (auto w : rgbWidgets[BLUE_CHANNEL])
        w->setEnabled(isColor);

    ui->meanEdit->setText(QString::number(m_ImageData->getMean()));
    ui->medianEdit->setText(QString::number(m_ImageData->getMedian()));

    for (int n = 0; n < m_ImageData->channels(); n++)
    {
        double median = m_ImageData->getMedian(n);

        if (median > 100)
            numDecimals << 0;
        else if (median > 1)
            numDecimals << 2;
        else if (median > .01)
            numDecimals << 4;
        else if (median > .0001)
            numDecimals << 6;
        else
            numDecimals << 10;

        minBoxes[n]->setDecimals(numDecimals[n]);
        minBoxes[n]->setSingleStep(fabs(m_ImageData->getMax(n) - m_ImageData->getMin(n)) / 20.0);
        minBoxes[n]->setMinimum(m_ImageData->getMin(n));
        minBoxes[n]->setMaximum(m_ImageData->getMax(n) - sliderTick[n]);
        minBoxes[n]->setValue(m_ImageData->getMin(n) + (sliders[n]->minimumValue() / sliderScale[n]));

        maxBoxes[n]->setDecimals(numDecimals[n]);
        maxBoxes[n]->setSingleStep(fabs(m_ImageData->getMax(n) - m_ImageData->getMin(n)) / 20.0);
        maxBoxes[n]->setMinimum(m_ImageData->getMin(n) + sliderTick[n]);
        maxBoxes[n]->setMaximum(m_ImageData->getMax(n));
        maxBoxes[n]->setValue(m_ImageData->getMin(n) + sliderTick[n] + (sliders[n]->maximumValue() / sliderScale[n]));
    }

    ui->histogramPlot->syncGUI();

    isGUISynced = true;
}



void FITSHistogramEditor::applyScale()
{
    QVector<double> min, max;

    min << minBoxes[0]->value() << minBoxes[1]->value() <<  minBoxes[2]->value();
    max << maxBoxes[0]->value() << maxBoxes[1]->value() << maxBoxes[2]->value();

    //    if (ui->logR->isChecked())
    //        type = FITS_LOG;
    //    else
    type = FITS_LINEAR;
    emit newHistogramCommand(new FITSHistogramCommand(m_ImageData, this, type, min, max));
}

void FITSHistogramEditor::applyFilter(FITSScale ftype)
{
    QVector<double> min, max;
    min.append(ui->minREdit->value());
    type = ftype;

    emit newHistogramCommand(new FITSHistogramCommand(m_ImageData, this, type, min, max));
}

void FITSHistogramEditor::setImageData(const QSharedPointer<FITSData> &data)
{
    m_ImageData = data;
    ui->histogramPlot->setImageData(data);

    connect(m_ImageData.data(), &FITSData::dataChanged, [this]
    {
        isGUISynced = false;
        syncGUI();
    });
}

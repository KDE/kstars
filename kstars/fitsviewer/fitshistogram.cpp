/*  FITS Histogram
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "fitshistogram.h"

#include "fits_debug.h"

#include "fitsdata.h"
#include "fitstab.h"
#include "fitsview.h"
#include "fitsviewer.h"
#include "Options.h"

#include <KMessageBox>

#include <zlib.h>

histogramUI::histogramUI(QDialog *parent) : QDialog(parent)
{
    setupUi(parent);
    setModal(false);
}

FITSHistogram::FITSHistogram(QWidget *parent) : QDialog(parent)
{
    ui   = new histogramUI(this);
    tab  = dynamic_cast<FITSTab *>(parent);

    customPlot = ui->histogramPlot;

    customPlot->setBackground(QBrush(Qt::black));

    customPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    customPlot->yAxis->setBasePen(QPen(Qt::white, 1));

    customPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    customPlot->yAxis->setTickPen(QPen(Qt::white, 1));

    customPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    customPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));

    customPlot->xAxis->setTickLabelColor(Qt::white);
    customPlot->yAxis->setTickLabelColor(Qt::white);

    customPlot->xAxis->setLabelColor(Qt::white);
    customPlot->yAxis->setLabelColor(Qt::white);

    customPlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    customPlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    customPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    customPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    customPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    customPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);

    connect(ui->applyB, &QPushButton::clicked, this, &FITSHistogram::applyScale);

    connect(ui->minEdit, SIGNAL(valueChanged(double)), this, SLOT(updateLimits(double)));
    connect(ui->maxEdit, SIGNAL(valueChanged(double)), this, SLOT(updateLimits(double)));
    connect(ui->minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateSliders(int)));
    connect(ui->maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateSliders(int)));
    connect(ui->hideSaturated, &QCheckBox::stateChanged, this, &FITSHistogram::toggleHideSaturated);
    connect(customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(checkRangeLimit(QCPRange)));
    connect(customPlot, &QCustomPlot::mouseMove, this, &FITSHistogram::driftMouseOverLine);
    sliderScale = 10;
    numDecimals = 0;
}

void FITSHistogram::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    syncGUI();
}

void FITSHistogram::toggleHideSaturated(int x)
{
    constructHistogram();
    Q_UNUSED(x)
}

void FITSHistogram::constructHistogram()
{
    FITSData *image_data = tab->getView()->getImageData();

    isGUISynced = false;

    switch (image_data->property("dataType").toInt())
    {
        case TBYTE:
            constructHistogram<uint8_t>();
            break;

        case TSHORT:
            constructHistogram<int16_t>();
            break;

        case TUSHORT:
            constructHistogram<uint16_t>();
            break;

        case TLONG:
            constructHistogram<int32_t>();
            break;

        case TULONG:
            constructHistogram<uint32_t>();
            break;

        case TFLOAT:
            constructHistogram<float>();
            break;

        case TLONGLONG:
            constructHistogram<int64_t>();
            break;

        case TDOUBLE:
            constructHistogram<double>();
            break;

        default:
            break;
    }

    if (isVisible())
        syncGUI();
}

template <typename T>
void FITSHistogram::constructHistogram()
{    
    FITSData *image_data = tab->getView()->getImageData();
    uint16_t fits_w = image_data->width(), fits_h = image_data->height();

    auto *buffer = reinterpret_cast<T *>(image_data->getImageBuffer());

    image_data->getMinMax(&fits_min, &fits_max);

    uint32_t samples = fits_w * fits_h;

    binCount = static_cast<uint16_t>(sqrt(samples));

    intensity.fill(0, binCount);
    r_frequency.fill(0, binCount);
    cumulativeFrequency.fill(0, binCount);

    double pixel_range = fits_max - fits_min;
    binWidth           = pixel_range / (binCount - 1);

    qCDebug(KSTARS_FITS) << "Histogram min:" << fits_min << ", max:" << fits_max << ", range:" << pixel_range << ", binW:" << binWidth << ", bin#:" << binCount;

    for (int i = 0; i < binCount; i++)
        intensity[i] = fits_min + (binWidth * i);

    uint16_t r_id = 0;

    if (image_data->channels() == 1)
    {
        for (uint32_t i = 0; i < samples; i += 4)
        {
            r_id = static_cast<uint16_t>(round((buffer[i] - fits_min) / binWidth));
            r_frequency[r_id >= binCount ? binCount - 1 : r_id] += 4;
        }
    }
    else
    {
        g_frequency.fill(0, binCount);
        b_frequency.fill(0, binCount);

        int g_offset = static_cast<int>(samples);
        int b_offset = static_cast<int>(samples * 2);
        for (uint32_t i = 0; i < samples; i += 4)
        {
            uint16_t g_id = 0, b_id = 0;

            r_id = static_cast<uint16_t>(round((buffer[i] - fits_min) / binWidth));
            r_frequency[r_id >= binCount ? binCount - 1 : r_id] += 4;

            g_id = static_cast<uint16_t>(round((buffer[i + g_offset] - fits_min) / binWidth));
            g_frequency[g_id >= binCount ? binCount - 1 : g_id] += 4;

            b_id = static_cast<uint16_t>(round((buffer[i + b_offset] - fits_min) / binWidth));
            b_frequency[b_id >= binCount ? binCount - 1 : b_id] += 4;
        }
    }

    // Cumulative Frequency
    int j = 0;
    double val = 0;
    for (int i = 0; i < binCount; i++) {
        val += r_frequency[j++];
        cumulativeFrequency.replace(i, val);
    }


    if (image_data->channels() == 1)
    {
        for (int i = 0; i < binCount; i++)
        {
            if (r_frequency[i] > maxFrequency)
                maxFrequency = static_cast<int>(r_frequency[i]);
        }
    }
    else
    {
        for (int i = 0; i < binCount; i++)
        {
            if (r_frequency[i] > maxFrequency)
                maxFrequency = static_cast<int>(r_frequency[i]);
            if (g_frequency[i] > maxFrequency)
                maxFrequency = static_cast<int>(g_frequency[i]);
            if (b_frequency[i] > maxFrequency)
                maxFrequency = static_cast<int>(b_frequency[i]);
        }
    }

    double median      = 0;
    int halfCumulative = static_cast<int>(cumulativeFrequency[binCount - 1] / 2);
    for (int i = 0; i < binCount; i++)
    {
        if (cumulativeFrequency[i] >= halfCumulative)
        {
            median = i * binWidth + fits_min;
            break;
        }
    }

    // Custom index to indicate the overall contrast of the image
    JMIndex = cumulativeFrequency[binCount / 8] / cumulativeFrequency[binCount / 4];
    qCDebug(KSTARS_FITS) << "FITHistogram: JMIndex " << JMIndex;

    if(ui->hideSaturated->isChecked())
    {
        intensity.removeFirst();
        intensity.removeLast();
        r_frequency.removeFirst();
        r_frequency.removeLast();
        if (image_data->channels() > 1)
        {
            g_frequency.removeFirst();
            g_frequency.removeLast();
            b_frequency.removeFirst();
            b_frequency.removeLast();
        }
    }

    image_data->setMedian(median);
    if(median<1)
        sliderScale=1/median*100;
    else
        sliderScale=10;
}

void FITSHistogram::syncGUI()
{
    if (isGUISynced)
        return;

    FITSData *image_data = tab->getView()->getImageData();

    disconnect(ui->minEdit, SIGNAL(valueChanged(double)), this, SLOT(updateLimits(double)));
    disconnect(ui->maxEdit, SIGNAL(valueChanged(double)), this, SLOT(updateLimits(double)));
    disconnect(ui->minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateSliders(int)));
    disconnect(ui->maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateSliders(int)));

    ui->meanEdit->setText(QString::number(image_data->getMean()));
    ui->medianEdit->setText(QString::number(image_data->getMedian()));

    double median = image_data->getMedian();

    if(median > 100)
        numDecimals=0;
    else if(median > 1)
        numDecimals=2;
    else if(median > .01)
        numDecimals=4;
    else if(median > .0001)
        numDecimals=6;
    else
        numDecimals=10;

    if(!ui->minSlider->isSliderDown())
    {
        ui->minEdit->setDecimals(numDecimals);
        ui->minEdit->setSingleStep(fabs(fits_max - fits_min) / 20.0);
        ui->minEdit->setMinimum(fits_min);
        ui->minEdit->setMaximum(fits_max - ui->minEdit->singleStep()); //minus one step
        ui->minEdit->setValue(fits_min);


        ui->minSlider->setSingleStep(static_cast<int>((fabs(fits_max - fits_min) / 20.0)*sliderScale));
        ui->minSlider->setMinimum(static_cast<int>(fits_min*sliderScale));
        ui->minSlider->setMaximum(static_cast<int>((fits_max)*sliderScale - ui->minSlider->singleStep()));
        ui->minSlider->setValue(static_cast<int>(fits_min*sliderScale));
    }

    if(!ui->maxSlider->isSliderDown())
    {
        ui->maxEdit->setDecimals(numDecimals);
        ui->maxEdit->setSingleStep(fabs(fits_max - fits_min) / 20.0);
        ui->maxEdit->setMinimum(fits_min + ui->maxEdit->singleStep());
        ui->maxEdit->setMaximum(fits_max);
        ui->maxEdit->setValue(fits_max);

        ui->maxSlider->setSingleStep(static_cast<int>((fabs(fits_max - fits_min) / 20.0)*sliderScale));
        ui->maxSlider->setMinimum(static_cast<int>((fits_min)*sliderScale + ui->maxSlider->singleStep()));
        ui->maxSlider->setMaximum(static_cast<int>(fits_max*sliderScale));
        ui->maxSlider->setValue(static_cast<int>(fits_max*sliderScale));

    }

    connect(ui->minEdit, SIGNAL(valueChanged(double)), this, SLOT(updateLimits(double)));
    connect(ui->maxEdit, SIGNAL(valueChanged(double)), this, SLOT(updateLimits(double)));
    connect(ui->minSlider, SIGNAL(valueChanged(int)), this, SLOT(updateSliders(int)));
    connect(ui->maxSlider, SIGNAL(valueChanged(int)), this, SLOT(updateSliders(int)));
    customPlot->clearGraphs();

    r_graph = customPlot->addGraph();
    r_graph->setBrush(QBrush(QColor(170, 40, 80)));
    r_graph->setPen(QPen(Qt::red));
    r_graph->setData(intensity, r_frequency);

    if (image_data->channels() > 1)
    {
        g_graph = customPlot->addGraph();
        b_graph = customPlot->addGraph();

        g_graph->setBrush(QBrush(QColor(40, 170, 80)));
        b_graph->setBrush(QBrush(QColor(80, 40, 170)));

        g_graph->setPen(QPen(Qt::green));
        b_graph->setPen(QPen(Qt::blue));

        g_graph->setData(intensity, g_frequency);
        b_graph->setData(intensity, b_frequency);
    }

    customPlot->axisRect(0)->setRangeDrag(Qt::Horizontal);
    customPlot->axisRect(0)->setRangeZoom(Qt::Horizontal);

    customPlot->xAxis->setLabel(i18n("Intensity"));
    customPlot->yAxis->setLabel(i18n("Frequency"));

    customPlot->xAxis->setRange(fits_min - ui->minEdit->singleStep(), fits_max + ui->maxEdit->singleStep());
    if (maxFrequency > 0)
        customPlot->yAxis->rescale();

    customPlot->setInteraction(QCP::iRangeDrag, true);
    customPlot->setInteraction(QCP::iRangeZoom, true);
    customPlot->setInteraction(QCP::iSelectPlottables, true);

    customPlot->replot();
    resizePlot();

    isGUISynced = true;
}

void FITSHistogram::resizePlot()
{
    if(customPlot->width()<300)
        customPlot->yAxis->setTickLabels(false);
    else
        customPlot->yAxis->setTickLabels(true);
    customPlot->xAxis->ticker()->setTickCount(customPlot->width()/100);

}

#if 0
template <typename T>
void FITSHistogram::constructHistogram()
{
    uint16_t fits_w = 0, fits_h = 0;
    FITSData *image_data = tab->getView()->getImageData();

    T *buffer = reinterpret_cast<T *>(image_data->getImageBuffer());

    image_data->getDimensions(&fits_w, &fits_h);
    image_data->getMinMax(&fits_min, &fits_max);

    uint32_t samples = fits_w * fits_h;

    binCount = sqrt(samples);

    intensity.fill(0, binCount);
    r_frequency.fill(0, binCount);
    cumulativeFrequency.fill(0, binCount);

    double pixel_range = fits_max - fits_min;
    binWidth           = pixel_range / (binCount - 1);

    qCDebug(KSTARS_FITS) << "Histogram min:" << fits_min << ", max:" << fits_max << ", range:" << pixel_range << ", binW:" << binWidth << ", bin#:" << binCount;

    for (int i = 0; i < binCount; i++)
        intensity[i] = fits_min + (binWidth * i);

    uint16_t r_id = 0;

    if (image_data->getNumOfChannels() == 1)
    {
        for (uint32_t i = 0; i < samples; i += 4)
        {
            r_id = round((buffer[i] - fits_min) / binWidth);
            r_frequency[r_id >= binCount ? binCount - 1 : r_id] += 4;
        }
    }
    else
    {
        g_frequency.fill(0, binCount);
        b_frequency.fill(0, binCount);

        int g_offset = samples;
        int b_offset = samples * 2;
        for (uint32_t i = 0; i < samples; i += 4)
        {
            uint16_t g_id = 0, b_id = 0;

            r_id = round((buffer[i] - fits_min) / binWidth);
            r_frequency[r_id >= binCount ? binCount - 1 : r_id] += 4;

            g_id = round((buffer[i + g_offset] - fits_min) / binWidth);
            g_frequency[g_id >= binCount ? binCount - 1 : g_id] += 4;

            b_id = round((buffer[i + b_offset] - fits_min) / binWidth);
            b_frequency[b_id >= binCount ? binCount - 1 : b_id] += 4;
        }
    }

    // Cumulative Frequency
    for (int i = 0; i < binCount; i++)
        for (int j = 0; j <= i; j++)
            cumulativeFrequency[i] += r_frequency[j];

    int maxFrequency = 0;
    if (image_data->getNumOfChannels() == 1)
    {
        for (int i = 0; i < binCount; i++)
        {
            if (r_frequency[i] > maxFrequency)
                maxFrequency = r_frequency[i];
        }
    }
    else
    {
        for (int i = 0; i < binCount; i++)
        {
            if (r_frequency[i] > maxFrequency)
                maxFrequency = r_frequency[i];
            if (g_frequency[i] > maxFrequency)
                maxFrequency = g_frequency[i];
            if (b_frequency[i] > maxFrequency)
                maxFrequency = b_frequency[i];
        }
    }

    double median      = 0;
    int halfCumulative = cumulativeFrequency[binCount - 1] / 2;
    for (int i = 0; i < binCount; i++)
    {
        if (cumulativeFrequency[i] >= halfCumulative)
        {
            median = i * binWidth + fits_min;
            break;
        }
    }

    // Custom index to indicate the overall constrast of the image
    JMIndex = cumulativeFrequency[binCount / 8] / cumulativeFrequency[binCount / 4];
    qCDebug(KSTARS_FITS) << "FITHistogram: JMIndex " << JMIndex;

    image_data->setMedian(median);

    ui->meanEdit->setText(QString::number(image_data->getMean()));
    ui->medianEdit->setText(QString::number(median));

    ui->minEdit->setMinimum(fits_min);
    ui->minEdit->setMaximum(fits_max - 1);
    ui->minEdit->setSingleStep(fabs(fits_max - fits_min) / 20.0);
    ui->minEdit->setValue(fits_min);

    ui->maxEdit->setMinimum(fits_min + 1);
    ui->maxEdit->setMaximum(fits_max);
    ui->maxEdit->setSingleStep(fabs(fits_max - fits_min) / 20.0);
    ui->maxEdit->setValue(fits_max);

    r_graph->setData(intensity, r_frequency);
    if (image_data->getNumOfChannels() > 1)
    {
        g_graph = customPlot->addGraph();
        b_graph = customPlot->addGraph();

        g_graph->setBrush(QBrush(QColor(40, 170, 80)));
        b_graph->setBrush(QBrush(QColor(80, 40, 170)));

        g_graph->setPen(QPen(Qt::green));
        b_graph->setPen(QPen(Qt::blue));

        g_graph->setData(intensity, g_frequency);
        b_graph->setData(intensity, b_frequency);
    }

    customPlot->axisRect(0)->setRangeDrag(Qt::Horizontal);
    customPlot->axisRect(0)->setRangeZoom(Qt::Horizontal);

    customPlot->xAxis->setLabel(i18n("Intensity"));
    customPlot->yAxis->setLabel(i18n("Frequency"));

    customPlot->xAxis->setRange(fits_min, fits_max);
    if (maxFrequency > 0)
        customPlot->yAxis->setRange(0, maxFrequency);

    customPlot->setInteraction(QCP::iRangeDrag, true);
    customPlot->setInteraction(QCP::iRangeZoom, true);
    customPlot->setInteraction(QCP::iSelectPlottables, true);

    customPlot->replot();
}
#endif
void FITSHistogram::updateLimits(double value)
{
    if (sender() == ui->minEdit)
    {
        if (value > ui->maxEdit->value())
            ui->maxEdit->setValue(value + 1);
    }
    else if (sender() == ui->maxEdit)
    {
        if (value < ui->minEdit->value())
        {
            ui->minEdit->setValue(value);
            ui->maxEdit->setValue(value + 1);
        }
    }
}
void FITSHistogram::updateSliders(int value)
{
    if (sender() == ui->minSlider)
    {
        if(value/sliderScale > ui->minEdit->value())
        {
            ui->minEdit->setValue(value/sliderScale);
            if (value/sliderScale > ui->maxEdit->value())
                ui->maxEdit->setValue(value/sliderScale + ui->maxEdit->singleStep());
        }
    }
    else if (sender() == ui->maxSlider)
    {
        if(value/sliderScale < ui->maxEdit->value())
        {
            ui->maxEdit->setValue(value/sliderScale);
            if (value/sliderScale < ui->minEdit->value())
            {
                ui->minEdit->setValue(value/sliderScale);
                ui->maxEdit->setValue(value/sliderScale + ui->maxEdit->singleStep());
            }
        }
    }
    applyScale();
}

void FITSHistogram::checkRangeLimit(const QCPRange &range)
{
    if (range.lower < fits_min - ui->minEdit->singleStep())
        customPlot->xAxis->setRangeLower(fits_min - ui->minEdit->singleStep());
    else if (range.upper > fits_max + ui->maxEdit->singleStep())
        customPlot->xAxis->setRangeUpper(fits_max + ui->maxEdit->singleStep());
}

double FITSHistogram::getJMIndex() const
{
    return JMIndex;
}

void FITSHistogram::applyScale()
{
    double min = ui->minEdit->value();
    double max = ui->maxEdit->value();

    FITSHistogramCommand *histC;

    if (ui->logR->isChecked())
        type = FITS_LOG;
    else
        type = FITS_LINEAR;

    histC = new FITSHistogramCommand(tab, this, type, min, max);

    tab->getUndoStack()->push(histC);
}

void FITSHistogram::applyFilter(FITSScale ftype)
{
    double min = ui->minEdit->value();
    double max = ui->maxEdit->value();

    FITSHistogramCommand *histC;

    type = ftype;

    histC = new FITSHistogramCommand(tab, this, type, min, max);

    tab->getUndoStack()->push(histC);
}

QVector<double> FITSHistogram::getCumulativeFrequency() const
{
    return cumulativeFrequency;
}

FITSHistogramCommand::FITSHistogramCommand(QWidget *parent, FITSHistogram *inHisto, FITSScale newType, double lmin,
                                           double lmax)
{
    tab       = dynamic_cast<FITSTab*>(parent);
    type      = newType;
    histogram = inHisto;
    min       = lmin;
    max       = lmax;
}

FITSHistogramCommand::~FITSHistogramCommand()
{
    delete[] delta;
}

bool FITSHistogramCommand::calculateDelta(const uint8_t *buffer)
{
    FITSData *image_data = tab->getView()->getImageData();

    uint8_t *image_buffer    = image_data->getImageBuffer();
    int totalPixels          = image_data->width() * image_data->height() * image_data->channels();
    unsigned long totalBytes = totalPixels * image_data->getBytesPerPixel();

    auto *raw_delta = new uint8_t[totalBytes];

    if (raw_delta == nullptr)
    {
        qWarning() << "Error! not enough memory to create image delta" << endl;
        return false;
    }

    for (unsigned int i = 0; i < totalBytes; i++)
        raw_delta[i] = buffer[i] ^ image_buffer[i];

    compressedBytes = sizeof(uint8_t) * totalBytes + totalBytes / 64 + 16 + 3;
    delete[] delta;
    delta = new uint8_t[compressedBytes];

    if (delta == nullptr)
    {
        delete[] raw_delta;
        qCCritical(KSTARS_FITS) << "FITSHistogram Error: Ran out of memory compressing delta";
        return false;
    }

    int r = compress2(delta, &compressedBytes, raw_delta, totalBytes, 5);

    if (r != Z_OK)
    {
        delete[] raw_delta;
        /* this should NEVER happen */
        qCCritical(KSTARS_FITS) << "FITSHistogram Error: Failed to compress raw_delta";
        return false;
    }

    //qDebug() << "compressed bytes size " << compressedBytes << " bytes" << endl;

    delete[] raw_delta;

    return true;
}

bool FITSHistogramCommand::reverseDelta()
{
    FITSView *image       = tab->getView();
    FITSData *image_data  = image->getImageData();
    uint8_t *image_buffer = (image_data->getImageBuffer());

    int totalPixels          = image_data->width() * image_data->height() * image_data->channels();
    unsigned long totalBytes = totalPixels * image_data->getBytesPerPixel();

    auto *output_image = new uint8_t[totalBytes];

    if (output_image == nullptr)
    {
        qWarning() << "Error! not enough memory to create output image" << endl;
        return false;
    }

    auto *raw_delta = new uint8_t[totalBytes];

    if (raw_delta == nullptr)
    {
        delete[] output_image;
        qWarning() << "Error! not enough memory to create image delta" << endl;
        return false;
    }

    int r = uncompress(raw_delta, &totalBytes, delta, compressedBytes);
    if (r != Z_OK)
    {
        qCCritical(KSTARS_FITS) << "FITSHistogram compression error in reverseDelta()";
        delete[] output_image;
        delete[] raw_delta;
        return false;
    }

    for (unsigned int i = 0; i < totalBytes; i++)
        output_image[i] = raw_delta[i] ^ image_buffer[i];

    image_data->setImageBuffer(output_image);

    delete[] raw_delta;

    return true;
}

void FITSHistogramCommand::redo()
{
    FITSView *image      = tab->getView();
    FITSData *image_data = image->getImageData();

    uint8_t *image_buffer = image_data->getImageBuffer();
    uint8_t *buffer = nullptr;
    unsigned int size     = image_data->width() * image_data->height() * image_data->channels();
    int BBP               = image_data->getBytesPerPixel();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (delta != nullptr)
    {        
        FITSData::Statistic prevStats;
        image_data->saveStatistics(prevStats);

        reverseDelta();

        image_data->restoreStatistics(stats);

        stats = prevStats;
    }
    else
    {
        image_data->saveStatistics(stats);

        // If it's rotation of flip, no need to calculate delta
        if (type >= FITS_ROTATE_CW && type <= FITS_FLIP_V)
        {
            image_data->applyFilter(type, image_buffer);
        }
        else
        {
            buffer = new uint8_t[size * BBP];

            if (buffer == nullptr)
            {
                qWarning() << "Error! not enough memory to create image buffer in redo()" << endl;
                QApplication::restoreOverrideCursor();
                return;
            }

            memcpy(buffer, image_buffer, size * BBP);

            double dataMin = min, dataMax = max;
            switch (type)
            {
                case FITS_AUTO:
                case FITS_LINEAR:
                    image_data->applyFilter(FITS_LINEAR, nullptr, &dataMin, &dataMax);
                    break;

                case FITS_LOG:
                    image_data->applyFilter(FITS_LOG, nullptr, &dataMin, &dataMax);
                    break;

                case FITS_SQRT:
                    image_data->applyFilter(FITS_SQRT, nullptr, &dataMin, &dataMax);
                    break;

                default:
                    image_data->applyFilter(type);
                    break;
            }

            calculateDelta(buffer);
            delete[] buffer;
        }
    }

    if (histogram != nullptr)
    {
        histogram->constructHistogram();

        if (tab->getViewer()->isStarsMarked())
            image_data->findStars();
    }

    image->pushFilter(type);
    image->rescale(ZOOM_KEEP_LEVEL);
    image->updateFrame();

    QApplication::restoreOverrideCursor();
}

void FITSHistogramCommand::undo()
{
    FITSView *image      = tab->getView();
    FITSData *image_data = image->getImageData();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (delta != nullptr)
    {
        FITSData::Statistic prevStats;
        image_data->saveStatistics(prevStats);

        reverseDelta();

        image_data->restoreStatistics(stats);

        stats = prevStats;
    }
    else
    {
        switch (type)
        {
            case FITS_ROTATE_CW:
                image_data->applyFilter(FITS_ROTATE_CCW);
                break;
            case FITS_ROTATE_CCW:
                image_data->applyFilter(FITS_ROTATE_CW);
                break;
            case FITS_FLIP_H:
            case FITS_FLIP_V:
                image_data->applyFilter(type);
                break;
            default:
                break;
        }
    }

    if (histogram != nullptr)
    {
        histogram->constructHistogram();

        if (tab->getViewer()->isStarsMarked())
            image_data->findStars();
    }

    image->popFilter();
    image->rescale(ZOOM_KEEP_LEVEL);
    image->updateFrame();

    QApplication::restoreOverrideCursor();
}

QString FITSHistogramCommand::text() const
{
    switch (type)
    {
        case FITS_AUTO:
            return i18n("Auto Scale");
        case FITS_LINEAR:
            return i18n("Linear Scale");
        case FITS_LOG:
            return i18n("Logarithmic Scale");
        case FITS_SQRT:
            return i18n("Square Root Scale");

        default:
            if (type - 1 <= FITSViewer::filterTypes.count())
                return FITSViewer::filterTypes.at(type - 1);
            break;
    }

    return i18n("Unknown");
}

void FITSHistogram::driftMouseOverLine(QMouseEvent *event)
{
    double intensity = customPlot->xAxis->pixelToCoord(event->localPos().x());

    if (customPlot->xAxis->range().contains(intensity))
    {
            int r_index= r_graph->findBegin(intensity, true);
            double r_Frequency = r_graph->dataMainValue(r_index);


            if(b_graph && g_graph)
            {
                int g_index= g_graph->findBegin(intensity, true);
                double g_Frequency = g_graph->dataMainValue(g_index);

                int b_index= b_graph->findBegin(intensity, true);
                double b_Frequency = g_graph->dataMainValue(b_index);

                if( r_Frequency>0.0 || g_Frequency>0.0 || b_Frequency>0.0 )
                {
                    QToolTip::showText(
                        event->globalPos(),
                                i18nc("Histogram tooltip; %1 is intensity; %2 is frequency;",
                                      "<table>"
                                      "<tr><td>Intensity:   </td><td>%1</td></tr>"
                                      "<tr><td>R Frequency:   </td><td>%2</td></tr>"
                                      "<tr><td>G Frequency:   </td><td>%3</td></tr>"
                                      "<tr><td>B Frequency:   </td><td>%4</td></tr>"
                                      "</table>",
                                      QString::number(intensity, 'f', numDecimals),
                                      QString::number(r_Frequency, 'f', 0),
                                      QString::number(g_Frequency, 'f', 0),
                                      QString::number(b_Frequency, 'f', 0)));
                }
                else
                    QToolTip::hideText();

            }
            else
            {
                if(r_Frequency>0.0)
                {
                    QToolTip::showText(
                        event->globalPos(),
                                i18nc("Histogram tooltip; %1 is intensity; %2 is frequency;",
                                      "<table>"
                                      "<tr><td>Intensity:   </td><td>%1</td></tr>"
                                      "<tr><td>R Frequency:   </td><td>%2</td></tr>"
                                      "</table>",
                                      QString::number(intensity, 'f', numDecimals),
                                      QString::number(r_Frequency, 'f', 0)));
                }
                else
                    QToolTip::hideText();
            }



        customPlot->replot();
    }
}

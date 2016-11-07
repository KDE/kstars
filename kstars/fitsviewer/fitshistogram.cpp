/*  FITS Histogram
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "fitsviewer.h"
#include "fitshistogram.h"
#include "fitstab.h"
#include "fitsview.h"
#include "fitsdata.h"

#include <cmath>
#include <cstdlib>
#include <zlib.h>

#include <QPainter>
#include <QSlider>
#include <QCursor>
#include <QPen>
#include <QPixmap>
#include <QRadioButton>
#include <QPushButton>
#include <QMouseEvent>
#include <QPaintEvent>

#include <QUndoStack>
#include <QDebug>
//#include <klineedit.h>
#include <KLocalizedString>
#include <KMessageBox>

#include "Options.h"

#define LOW_PASS_MARGIN 0.01
#define LOW_PASS_LIMIT  .05

histogramUI::histogramUI(QDialog *parent) : QDialog(parent)
{
    setupUi(parent);
    setModal(false);
}

FITSHistogram::FITSHistogram(QWidget *parent) : QDialog(parent)
{
    ui = new histogramUI(this);
    tab = static_cast<FITSTab *> (parent);
    type   = FITS_AUTO;

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

    r_graph = customPlot->addGraph();
    r_graph->setBrush(QBrush(QColor(170, 40, 80)));
    r_graph->setPen(QPen(Qt::red));
    //r_graph->setLineStyle(QCPGraph::lsImpulse);

    connect(ui->applyB, SIGNAL(clicked()), this, SLOT(applyScale()));

    connect(customPlot, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(updateValues(QMouseEvent*)));

    connect(ui->minEdit, SIGNAL(valueChanged(double)), this, SLOT(updateLimits(double)));
    connect(ui->maxEdit, SIGNAL(valueChanged(double)), this, SLOT(updateLimits(double)));
    connect(customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(checkRangeLimit(QCPRange)));

    constructHistogram();
}

FITSHistogram::~FITSHistogram()
{

}

void FITSHistogram::constructHistogram()
{
    FITSData *image_data = tab->getView()->getImageData();

    switch (image_data->getDataType())
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
}

template<typename T> void FITSHistogram::constructHistogram()
{
    uint16_t fits_w=0, fits_h=0;
    FITSData *image_data = tab->getView()->getImageData();

    T *buffer = reinterpret_cast<T*>(image_data->getImageBuffer());

    image_data->getDimensions(&fits_w, &fits_h);
    image_data->getMinMax(&fits_min, &fits_max);

    uint32_t samples = fits_w*fits_h;

    binCount = sqrt(samples);

    intensity.fill(0, binCount);
    r_frequency.fill(0, binCount);
    cumulativeFrequency.fill(0, binCount);

    double pixel_range = fits_max - fits_min;
    binWidth = pixel_range / (binCount - 1);

    if (Options::fITSLogging())
        qDebug() << "fits MIN: " << fits_min << " - fits MAX: " << fits_max << " - pixel range: " << pixel_range << " - bin width " << binWidth << " bin count " << binCount;

    for (int i=0; i < binCount; i++)
        intensity[i] = fits_min + (binWidth * i);

    uint16_t r_id=0, g_id=0, b_id=0;

    if (image_data->getNumOfChannels() == 1)
    {
        for (uint32_t i=0; i < samples ; i++)
        {
            r_id = round((buffer[i] - fits_min) / binWidth);
            r_frequency[r_id >= binCount ? binCount - 1 : r_id]++;
        }
    }
    else
    {
        g_frequency.fill(0, binCount);
        b_frequency.fill(0, binCount);

        int g_offset = samples;
        int b_offset = samples*2;
        for (uint32_t i=0; i < samples ; i++)
        {
            r_id = round((buffer[i] - fits_min) / binWidth);
            r_frequency[r_id >= binCount ? binCount - 1 : r_id]++;

            g_id = round((buffer[i+g_offset] - fits_min) / binWidth);
            g_frequency[g_id >= binCount ? binCount - 1 : g_id]++;

            b_id = round((buffer[i+b_offset] - fits_min) / binWidth);
            b_frequency[b_id >= binCount ? binCount - 1 : b_id]++;
        }
    }

    // Cumuliative Frequency
    for (int i=0; i < binCount; i++)
        for (int j=0; j <= i; j++)
            cumulativeFrequency[i] += r_frequency[j];

    int maxFrequency=0;
    if (image_data->getNumOfChannels() == 1)
    {
        for (int i=0; i < binCount; i++)
        {
            if (r_frequency[i] > maxFrequency)
                maxFrequency = r_frequency[i];
        }
    }
    else
    {
        for (int i=0; i < binCount; i++)
        {
            if (r_frequency[i] > maxFrequency)
                maxFrequency = r_frequency[i];
            if (g_frequency[i] > maxFrequency)
                maxFrequency = g_frequency[i];
            if (b_frequency[i] > maxFrequency)
                maxFrequency = b_frequency[i];
        }
    }

    double median=0;
    int halfCumulative = cumulativeFrequency[binCount - 1]/2;
    for (int i=0; i < binCount; i++)
    {
        if (cumulativeFrequency[i] >= halfCumulative)
        {
            median = i * binWidth + fits_min;
            break;
        }
    }

    // Custom index to indicate the overall constrast of the image
    JMIndex = cumulativeFrequency[binCount/8]/cumulativeFrequency[binCount/4];
    if (Options::fITSLogging())
        qDebug() << "FITHistogram: JMIndex " << JMIndex;

    image_data->setMedian(median);

    ui->meanEdit->setText(QString::number(image_data->getMean()));
    ui->medianEdit->setText(QString::number(median));

    ui->minEdit->setMinimum(fits_min);
    ui->minEdit->setMaximum(fits_max-1);
    ui->minEdit->setSingleStep( fabs(fits_max-fits_min) / 20.0);
    ui->minEdit->setValue(fits_min);

    ui->maxEdit->setMinimum(fits_min+1);
    ui->maxEdit->setMaximum(fits_max);
    ui->maxEdit->setSingleStep( fabs(fits_max-fits_min) / 20.0);
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
    customPlot->yAxis->setRange(0, maxFrequency);

    customPlot->setInteraction(QCP::iRangeDrag, true);
    customPlot->setInteraction(QCP::iRangeZoom, true);
    customPlot->setInteraction(QCP::iSelectPlottables, true);

    customPlot->replot();

}

void FITSHistogram::updateLimits(double value)
{
    if (sender() == ui->minEdit)
    {
        if (value > ui->maxEdit->value())
            ui->maxEdit->setValue(value+1);
    }
    else if (sender() == ui->maxEdit)
    {
        if (value < ui->minEdit->value())
        {
            ui->minEdit->setValue(value);
            ui->maxEdit->setValue(value+1);
        }
    }
}

void FITSHistogram::checkRangeLimit(const QCPRange &range)
{
    if (range.lower < fits_min)
        customPlot->xAxis->setRangeLower(fits_min);
    else if (range.upper > fits_max)
        customPlot->xAxis->setRangeUpper(fits_max);
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

void FITSHistogram::updateValues(QMouseEvent *event)
{
   int x = event->x();

   double intensity_key = customPlot->xAxis->pixelToCoord(x);

   if (intensity_key < 0)
       return;

   double frequency_val = 0;

   for (int i=0; i < binCount; i++)
   {
       if (intensity[i] > intensity_key)
       {
           frequency_val = r_frequency[i];
           break;
       }
   }

   ui->intensityEdit->setText(QString::number(intensity_key));
   ui->frequencyEdit->setText(QString::number(frequency_val));

}

FITSHistogramCommand::FITSHistogramCommand(QWidget * parent, FITSHistogram *inHisto, FITSScale newType, double lmin, double lmax)
{
    tab         = (FITSTab *) parent;
    type        = newType;
    histogram   = inHisto;
    delta  = NULL;
    original_buffer = NULL;

    min = lmin;
    max = lmax;
}

FITSHistogramCommand::~FITSHistogramCommand()
{
    delete(delta);
}

bool FITSHistogramCommand::calculateDelta(uint8_t *buffer)
{
    FITSData *image_data = tab->getView()->getImageData();

    uint8_t *image_buffer = image_data->getImageBuffer();
    int totalPixels = image_data->getSize() * image_data->getNumOfChannels();
    unsigned long totalBytes = totalPixels * image_data->getBytesPerPixel();

    uint8_t *raw_delta = new uint8_t[totalBytes];
    if (raw_delta == NULL)
    {
        qWarning() << "Error! not enough memory to create image delta" << endl;
        return false;
    }

    for (unsigned int i=0; i < totalBytes; i++)
        raw_delta[i] = buffer[i] ^ image_buffer[i];

    compressedBytes = sizeof(uint8_t) * totalBytes + totalBytes / 64 + 16 + 3;
    delete [] delta;
    delta = new uint8_t[compressedBytes];

    if (delta == NULL)
    {
        delete [] raw_delta;
        qDebug() << "FITSHistogram Error: Ran out of memory compressing delta" << endl;
        return false;
    }

    int r = compress2(delta, &compressedBytes, raw_delta, totalBytes, 5);
    if (r != Z_OK)
    {
        /* this should NEVER happen */
        qDebug() << "FITSHistogram Error: Failed to compress raw_delta" << endl;
        return false;
    }

    //qDebug() << "compressed bytes size " << compressedBytes << " bytes" << endl;

    delete [] raw_delta;

    return true;
}

bool FITSHistogramCommand::reverseDelta()
{
    FITSView *image = tab->getView();
    FITSData *image_data = image->getImageData();
    uint8_t *image_buffer = (image_data->getImageBuffer());

    unsigned int size = image_data->getSize();
    int channels = image_data->getNumOfChannels();

    int totalPixels = size * channels;
    unsigned long totalBytes = totalPixels * image_data->getBytesPerPixel();

    uint8_t *output_image = new uint8_t[totalBytes];
    if (output_image == NULL)
    {
        qWarning() << "Error! not enough memory to create output image" << endl;
        return false;
    }

    uint8_t *raw_delta = new uint8_t[totalBytes];
    if (raw_delta == NULL)
    {
        delete(output_image);
        qWarning() << "Error! not enough memory to create image delta" << endl;
        return false;
    }

    int r = uncompress(raw_delta, &totalBytes, delta, compressedBytes);
    if (r != Z_OK)
    {
        qDebug() << "FITSHistogram compression error in reverseDelta()" << endl;
        delete [] raw_delta;
        return false;
    }

    for (unsigned int i=0; i < totalBytes; i++)
        output_image[i] = raw_delta[i] ^ image_buffer[i];

    image_data->setImageBuffer(output_image);

    delete [] raw_delta;

    return true;
}

void FITSHistogramCommand::redo()
{
    FITSView *image = tab->getView();
    FITSData *image_data = image->getImageData();

    uint8_t *image_buffer = image_data->getImageBuffer();
    unsigned int size = image_data->getSize();
    int channels = image_data->getNumOfChannels();
    int BBP = image_data->getBytesPerPixel();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (delta != NULL)
    {
        double min,max,stddev,average,median,snr;
        min      = image_data->getMin();
        max      = image_data->getMax();
        stddev   = image_data->getStdDev();
        average  = image_data->getMean();
        median   = image_data->getMedian();
        snr      = image_data->getSNR();

        reverseDelta();

        restoreStats();

        saveStats(min, max, stddev, average, median, snr);
    }
    else
    {
        saveStats(image_data->getMin(), image_data->getMax(), image_data->getStdDev(), image_data->getMean(), image_data->getMedian(), image_data->getSNR());

        // If it's rotation of flip, no need to calculate delta
        if (type >= FITS_ROTATE_CW && type <= FITS_FLIP_V)
        {
            image_data->applyFilter(type, image_buffer);
        }
        else
        {
            uint8_t *buffer = new uint8_t[size * channels * BBP];
            if (buffer == NULL)
            {
                qWarning() << "Error! not enough memory to create image buffer in redo()" << endl;
                QApplication::restoreOverrideCursor();
                return;
            }

            memcpy(buffer, image_buffer, size * channels * BBP);
            float dataMin = min, dataMax = max;

            switch (type)
            {
            case FITS_AUTO:
            case FITS_LINEAR:
                image_data->applyFilter(FITS_LINEAR, NULL, &dataMin, &dataMax);
                break;

            case FITS_LOG:
                image_data->applyFilter(FITS_LOG, NULL, &dataMin, &dataMax);
                break;

            case FITS_SQRT:
                image_data->applyFilter(FITS_SQRT, NULL, &dataMin, &dataMax);
                break;

            default:
               image_data->applyFilter(type);
               break;
            }

            calculateDelta(buffer);
            delete [] buffer;
        }
    }

    if (histogram != NULL)
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
    FITSView *image = tab->getView();
    FITSData *image_data = image->getImageData();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (delta != NULL)
    {
       double min,max,stddev,average,median,snr;
       min      = image_data->getMin();
       max      = image_data->getMax();
       stddev   = image_data->getStdDev();
       average  = image_data->getMean();
       median   = image_data->getMedian();
       snr      = image_data->getSNR();

       reverseDelta();

       restoreStats();

       saveStats(min, max, stddev, average, median, snr);
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

    if (histogram != NULL)
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
        break;
    case FITS_LINEAR:
        return i18n("Linear Scale");
        break;
    case FITS_LOG:
        return i18n("Logarithmic Scale");
        break;
    case FITS_SQRT:
        return i18n("Square Root Scale");
        break;

    default:
        if (type-1 <= FITSViewer::filterTypes.count())
            return FITSViewer::filterTypes.at(type-1);
        break;
    }

    return i18n("Unknown");

}

void FITSHistogramCommand::saveStats(double min, double max, double stddev, double mean, double median, double SNR)
{
    stats.min       = min;
    stats.max       = max;
    stats.stddev    = stddev;
    stats.mean      = mean;
    stats.median    = median;
    stats.SNR       = SNR;

}

void FITSHistogramCommand::restoreStats()
{
    FITSData *image_data = tab->getView()->getImageData();

    image_data->setMinMax(stats.min, stats.max);
    image_data->setStdDev(stats.stddev);
    image_data->setMean(stats.mean);
    image_data->setMedian(stats.median);
}



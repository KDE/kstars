/***************************************************************************
                          fitshistogram.cpp  -  FITS Historgram
                          -----------------
    begin                : Thu Mar 4th 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
#include <klineedit.h>
#include <KLocalizedString>
#include <KMessageBox>

//#define HIST_LOG

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

    tab = (FITSTab *) parent;

    type   = FITS_AUTO;
    napply = 0;
    histFactor=1;

    connect(ui->applyB, SIGNAL(clicked()), this, SLOT(applyScale()));
    connect(ui->minOUT, SIGNAL(editingFinished()), this, SLOT(updateLowerLimit()));
    connect(ui->maxOUT, SIGNAL(editingFinished()), this, SLOT(updateUpperLimit()));

    connect(ui->minSlider, SIGNAL(valueChanged(int)), this, SLOT(minSliderUpdated(int)));
    connect(ui->maxSlider, SIGNAL(valueChanged(int)), this, SLOT(maxSliderUpdated(int)));

    ui->histFrame->init();

    constructHistogram(ui->histFrame->getHistWidth(), ui->histFrame->getHistHeight());
}

FITSHistogram::~FITSHistogram()
{
    //free (histArray);
}

void FITSHistogram::constructHistogram(int hist_width, int hist_height)
{
    int id;
    double fits_w=0, fits_h=0;
    FITSData *image_data = tab->getView()->getImageData();
    float *buffer = image_data->getImageBuffer();

    image_data->getDimensions(&fits_w, &fits_h);
    image_data->getMinMax(&fits_min, &fits_max);

    double pixel_range = fits_max - fits_min;

    #ifdef HIST_LOG
    qDebug() << "fits MIN: " << fits_min << " - fits MAX: " << fits_max << " - pixel range: " << pixel_range;
    #endif

    if (hist_width > histArray.size())
        histArray.resize(hist_width);

    cumulativeFreq.resize(histArray.size());

    for (int i=0; i < hist_width; i++)
    {
        histArray[i] = 0;
        cumulativeFreq[i] = 0;
    }

    binWidth = hist_width / pixel_range;

    #ifdef HIST_LOG
    qDebug() << "Hist Array is now " << hist_width << " wide..., pixel range is " << pixel_range << " Bin width is " << binWidth << endl;
    #endif

    if (binWidth == 0 || buffer == NULL)
        return;

    for (int i=0; i < fits_w * fits_h ; i++)
    {
        id = (int) round((buffer[i] - fits_min) * binWidth);

        if (id >= hist_width)
            id = hist_width - 1;
        else if (id < 0)
            id=0;

        histArray[id]++;
    }

    // Cumuliative Frequency
    for (int i=0; i < histArray.size(); i++)
        for (int j=0; j <= i; j++)
            cumulativeFreq[i] += histArray[j];

    int maxIntensity=0;
    int maxFrequency=histArray[0];
    for (int i=0; i < hist_width; i++)
    {
        if (histArray[i] > maxFrequency)
        {
            maxIntensity = i;
            maxFrequency = histArray[i];
        }
    }


    double median=0;
    int halfCumulative = cumulativeFreq[histArray.size() - 1]/2;
    for (int i=0; i < histArray.size(); i++)
    {
        if (cumulativeFreq[i] > halfCumulative)
        {
            median = i / binWidth + fits_min;
            break;

        }
    }

    image_data->setMedian(median);
    JMIndex = (double) maxIntensity / (double) hist_width;

    #ifdef HIST_LOG
    qDebug() << "maxIntensity " << maxIntensity <<  " JMIndex " << JMIndex << endl;
    #endif

    // Normalize histogram height. i.e. the maximum value will take the whole height of the widget
    histHeightRatio = ((double) hist_height) / (findMax(hist_width));

    histogram_height = hist_height;
    histogram_width = hist_width;

    updateBoxes(fits_min, fits_max);

    ui->histFrame->update();
}

void FITSHistogram::updateBoxes(double lower_limit, double upper_limit)
{

    histFactor=1;

    if ((upper_limit - lower_limit) <= 1)
        histFactor *= histogram_width;

    ui->minSlider->setMinimum(lower_limit*histFactor);
    ui->maxSlider->setMinimum(lower_limit*histFactor);

    ui->minSlider->setMaximum(upper_limit*histFactor);
    ui->maxSlider->setMaximum(upper_limit*histFactor);

    ui->minSlider->setValue(lower_limit*histFactor);
    ui->maxSlider->setValue(upper_limit*histFactor);

    ui->minOUT->setText(QString::number(lower_limit, 'f', 2));
    ui->maxOUT->setText(QString::number(upper_limit, 'f', 2));

}

void FITSHistogram::applyScale()
{

    double min = ui->minSlider->value() / histFactor;
    double max = ui->maxSlider->value() / histFactor;

    FITSHistogramCommand *histC;

    napply++;

    // Auto
    if (ui->autoR->isChecked())
        type = FITS_AUTO;
    // Linear
    else if (ui->linearR->isChecked())
        type = FITS_LINEAR;
    // Log
    else if (ui->logR->isChecked())
        type = FITS_LOG;
    // Exp
    else if (ui->sqrtR->isChecked())
        type = FITS_SQRT;

    histC = new FITSHistogramCommand(tab, this, type, min, max);

    tab->getUndoStack()->push(histC);
}

void FITSHistogram::applyFilter(FITSScale ftype)
{
    double min = ui->minSlider->value() / histFactor;
    double max = ui->maxSlider->value() / histFactor;

    napply++;

    FITSHistogramCommand *histC;

    type = ftype;

    histC = new FITSHistogramCommand(tab, this, type, min, max);

    tab->getUndoStack()->push(histC);

}

void FITSHistogram::minSliderUpdated(int value)
{
    if (value >= ui->maxSlider->value())
    {
        ui->minSlider->setValue(ui->minOUT->text().toDouble()*histFactor);
        return;
    }

    ui->minOUT->setText(QString::number(value/histFactor, 'f', 3));
}

void FITSHistogram::maxSliderUpdated(int value)
{
    if (value <= ui->minSlider->value())
    {
        ui->maxSlider->setValue(ui->maxOUT->text().toDouble()*histFactor);
        return;
    }

    ui->maxOUT->setText(QString::number(value/histFactor, 'f', 3));
}

void FITSHistogram::updateIntenFreq(int x)
{
    if (x < 0 || x >= histogram_width)
        return;

    //ui->intensityOUT->setText(QString("%1").arg( ceil(x / binWidth) + tab->getView()->getImageData()->getMin()));
    ui->intensityOUT->setText(QString::number((x / binWidth) + fits_min, 'f', 3));

    ui->frequencyOUT->setText(QString::number(histArray[x]));
}

void FITSHistogram::updateLowerLimit()
{
    bool conversion_ok = false;
    int newLowerLimit=0;

    newLowerLimit = ui->minOUT->text().toDouble(&conversion_ok)*histFactor;

    if (conversion_ok == false)
        return;

    ui->minSlider->setValue(newLowerLimit);
}

void FITSHistogram::updateUpperLimit()
{
    bool conversion_ok = false;
    int newUpperLimit=0;

    newUpperLimit = ui->maxOUT->text().toDouble(&conversion_ok)*histFactor;

    if (conversion_ok == false)
        return;

    ui->maxSlider->setValue(newUpperLimit);
}

double FITSHistogram::findMax(int hist_width)
{
    double max = -1e9;

    for (int i=0; i < hist_width; i++)
        if (histArray[i] > max) max = histArray[i];

    return max;
}

void FITSHistogram::updateHistogram()
{
    constructHistogram(ui->histFrame->maximumWidth(), ui->histFrame->maximumHeight());
    //constructHistogram(histogram_width, histogram_height);
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

bool FITSHistogramCommand::calculateDelta(unsigned char *buffer)
{
    FITSData *image_data = tab->getView()->getImageData();

    unsigned char *image_buffer = (unsigned char *) image_data->getImageBuffer();
    int totalPixels = image_data->getSize() * image_data->getNumOfChannels();
    unsigned long totalBytes = totalPixels * sizeof(float);
    //qDebug() << "raw total bytes " << totalBytes << " bytes" << endl;

    unsigned char *raw_delta = new unsigned char[totalBytes];
    if (raw_delta == NULL)
    {
        qWarning() << "Error! not enough memory to create image delta" << endl;
        return false;
    }

    for (unsigned int i=0; i < totalBytes; i++)
        raw_delta[i] = buffer[i] ^ image_buffer[i];

    compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
    delete(delta);
    delta = new unsigned char[compressedBytes];

    if (delta == NULL)
    {
        delete(raw_delta);
        qDebug() << "Error: Ran out of memory compressing delta" << endl;
        return false;
    }

    int r = compress2(delta, &compressedBytes, raw_delta, totalBytes, 5);
    if (r != Z_OK)
    {
        /* this should NEVER happen */
        qDebug() << "Error: Failed to compress raw_delta" << endl;
        return false;
    }

    //qDebug() << "compressed bytes size " << compressedBytes << " bytes" << endl;

    delete(raw_delta);

    return true;
}

bool FITSHistogramCommand::reverseDelta()
{
    FITSView *image = tab->getView();
    FITSData *image_data = image->getImageData();
    unsigned char *image_buffer = (unsigned char *) (image_data->getImageBuffer());

    unsigned int size = image_data->getSize();
    int channels = image_data->getNumOfChannels();

    int totalPixels = size * channels;
    unsigned long totalBytes = totalPixels * sizeof(float);

    unsigned char *output_image = new unsigned char[totalBytes];
    if (output_image == NULL)
    {
        qWarning() << "Error! not enough memory to create output image" << endl;
        return false;
    }

    unsigned char *raw_delta = new unsigned char[totalBytes];
    if (raw_delta == NULL)
    {
        delete(output_image);
        qWarning() << "Error! not enough memory to create image delta" << endl;
        return false;
    }

    int r = uncompress(raw_delta, &totalBytes, delta, compressedBytes);
    if (r != Z_OK)
    {
        qDebug() << "compression error in reverseDelta()" << endl;
        delete(raw_delta);
        return false;
    }

    for (unsigned int i=0; i < totalBytes; i++)
        output_image[i] = raw_delta[i] ^ image_buffer[i];

    image_data->setImageBuffer((float *)output_image);

    delete(raw_delta);

    return true;
}

void FITSHistogramCommand::redo()
{
    FITSView *image = tab->getView();
    FITSData *image_data = image->getImageData();

    float *image_buffer = image_data->getImageBuffer();    
    unsigned int size = image_data->getSize();
    int channels = image_data->getNumOfChannels();   

    QApplication::setOverrideCursor(Qt::WaitCursor);

    gamma = image->getGammaValue();

    if (delta != NULL)
    {
        double min,max,stddev,average,median;
        min      = image_data->getMin();
        max      = image_data->getMax();
        stddev   = image_data->getStdDev();
        average  = image_data->getAverage();
        median   = image_data->getMedian();

        reverseDelta();

        restoreStats();

        saveStats(min, max, stddev, average, median);
    }
    else
    {
        saveStats(image_data->getMin(), image_data->getMax(), image_data->getStdDev(), image_data->getAverage(), image_data->getMedian());

        // If it's rotation of flip, no need to calculate delta
        if (type >= FITS_ROTATE_CW && type <= FITS_FLIP_V)
        {
            image_data->applyFilter(type, image_buffer);
        }
        else
        {
            float *buffer = new float[size * channels];
            if (buffer == NULL)
            {
                qWarning() << "Error! not enough memory to create image buffer in redo()" << endl;
                QApplication::restoreOverrideCursor();
                return;
            }

            memcpy(buffer, image_buffer, size * channels * sizeof(float));

            switch (type)
            {
            case FITS_AUTO:
            case FITS_LINEAR:
                image_data->applyFilter(FITS_LINEAR, image_buffer, min, max);
                break;

            case FITS_LOG:
                image_data->applyFilter(FITS_LOG, image_buffer, min, max);
                break;

            case FITS_SQRT:
                image_data->applyFilter(FITS_SQRT, image_buffer, min, max);
                break;

            default:
               image_data->applyFilter(type, image_buffer);
               break;
            }

            calculateDelta( (unsigned char *) buffer);
            delete (buffer);
        }
    }

    if (histogram != NULL)
    {
        histogram->updateHistogram();

        if (tab->getViewer()->isStarsMarked())
            image_data->findStars();
    }

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
       double min,max,stddev,average,median;
       min      = image_data->getMin();
       max      = image_data->getMax();
       stddev   = image_data->getStdDev();
       average  = image_data->getAverage();
       median   = image_data->getMedian();

       reverseDelta();

       restoreStats();

       saveStats(min, max, stddev, average, median);
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
        histogram->updateHistogram();

        if (tab->getViewer()->isStarsMarked())
            image_data->findStars();
    }

    image->rescale(ZOOM_KEEP_LEVEL);
    image->updateFrame();

    image->setGammaValue(gamma);

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

void FITSHistogramCommand::saveStats(double min, double max, double stddev, double average, double median)
{
    stats.min       = min;
    stats.max       = max;
    stats.stddev    = stddev;
    stats.average   = average;
    stats.median    = median;

}

void FITSHistogramCommand::restoreStats()
{
    FITSData *image_data = tab->getView()->getImageData();

    image_data->setMinMax(stats.min, stats.max);
    image_data->setStdDev(stats.stddev);
    image_data->setAverage(stats.average);
    image_data->setMedian(stats.median);
}



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

#include "fitshistogram.h"
#include "fitsviewer.h"
#include "fitsimage.h"

#include <math.h>
#include <stdlib.h>

#include <QPainter>
#include <QSlider>
#include <QCursor>
#include <QPen>
#include <QPixmap>
#include <QRadioButton>
#include <QPushButton>
#include <QMouseEvent>
#include <QPaintEvent>

#include <KUndoStack>
#include <kdebug.h>
#include <klineedit.h>
#include <klocale.h>
#include <kmessagebox.h>

histogramUI::histogramUI(QDialog *parent) : QDialog(parent)
{
    setupUi(parent);
    setModal(false);

}

FITSHistogram::FITSHistogram(QWidget *parent) : QDialog(parent)
{
    viewer = (FITSViewer *) parent;
    ui = new histogramUI(this);

    //histArray = NULL;

    type   = 0;
    napply = 0;

    connect(ui->applyB, SIGNAL(clicked()), this, SLOT(applyScale()));
    connect(ui->minOUT, SIGNAL(editingFinished()), ui->histFrame, SLOT(updateLowerLimit()));
    connect(ui->maxOUT, SIGNAL(editingFinished()), ui->histFrame, SLOT(updateUpperLimit()));

    for (int i=0; i < histArray.size(); i++)
        histArray[i] = -1;

    ui->histFrame->init();
}

FITSHistogram::~FITSHistogram()
{
    //free (histArray);

}

void FITSHistogram::updateBoxes(int lowerLimit, int upperLimit)
{

    double lower_limit_x, upper_limit_x;

    lower_limit_x = ceil(lowerLimit / binSize) + fits_min;
    upper_limit_x = ceil(upperLimit / binSize) + fits_min;

    ui->minOUT->setText(QString::number((int) lower_limit_x));
    ui->maxOUT->setText(QString::number((int) upper_limit_x));

}

void FITSHistogram::applyScale()
{
    int swap;

    int min = ui->histFrame->getLowerLimit();
    int max = ui->histFrame->getUpperLimit();

    viewer->image->getFITSMinMax(&fits_min, &fits_max);

    FITSHistogramCommand *histC;

    if (min > max)
    {
        swap = min;
        min  = max;
        max  = swap;
    }

    min  = (int) (min / binSize + fits_min);
    max  = (int) (max / binSize + fits_min);

    napply++;

    // Auto
    if (ui->autoR->isChecked())
        type = 0;
    // Linear
    else if (ui->linearR->isChecked())
        type = 1;
    // Log
    else if (ui->logR->isChecked())
        type = 2;
    // Exp
    else if (ui->sqrtR->isChecked())
        type = 3;

    histC = new FITSHistogramCommand(viewer, this, type, min, max);

    viewer->history->push(histC);
}

void FITSHistogram::constructHistogram(int hist_width, int hist_height)
{

    int id;
    double fits_w=0, fits_h=0;
    int binRoundSize=0, binRange=0;
    float *buffer = viewer->image->getImageBuffer();

    viewer->image->getFITSSize(&fits_w, &fits_h);
    viewer->image->getFITSMinMax(&fits_min, &fits_max);

    int pixel_range = (int) (fits_max - fits_min);

    //kDebug() << "hist_width: " << hist_width << " - hist_height: " << hist_height << " - pixel range: " << pixel_range;

    if (hist_width > histArray.size())
        histArray.resize(hist_width);

    for (int i=0; i < histArray.size(); i++)
        histArray[i] = -1;

    binSize = ((double) hist_width / (double) pixel_range);
    binRoundSize = (int) floor(binSize);

    if (binSize == 0 || buffer == NULL)
        return;

    for (int i=0; i < fits_w * fits_h; i++)
    {
        id = (int) round((buffer[i] - fits_min) * binSize);
        if (id >= hist_width)
            id = hist_width - 1;

        if (binRoundSize <= 1)
            histArray[id]++;
        else
        {
            binRange = (binRoundSize + id);
            for (int j=id; j < binRange && j < hist_width; j++)
                histArray[j]++;
        }
    }

    // Normalize histogram height. i.e. the maximum value will take the whole height of the widget
    histFactor = ((double) hist_height) / ((double) findMax(hist_width));
    for (int i=0; i < hist_width; i++)
    {
        if (histArray[i] == -1 && (i+1) != hist_width)
        {
            //kDebug () << "Histarray of " << i << " is not filled, it will take value of " << i+1 << " which is " << histArray[i+1];
            histArray[i] = histArray[i+1];
        }

        //kDebug() << "Normalizing, we have for i " << i << " a value of: " << histArray[i];
        histArray[i] = (int) (((double) histArray[i]) * histFactor);
        //kDebug() << "Normalized to " << histArray[i] << " since the factor is " << histFactor;
    }

    histogram_height = hist_height;
    histogram_width = hist_width;

    // Initially
    updateBoxes(ui->histFrame->getLowerLimit(), ui->histFrame->getUpperLimit());

    ui->histFrame->update();
}


void FITSHistogram::updateIntenFreq(int x)
{

    if (x < 0 || x >= histogram_width)
        return;

    int index = (int) ceil(x / binSize);

    ui->intensityOUT->setText(QString("%1").arg( index + viewer->image->stats.min));

    ui->frequencyOUT->setText(QString("%1").arg(histArray[x]));

}


int FITSHistogram::findMax(int hist_width)
{
    double max = -1e9;

    for (int i=0; i < hist_width; i++)
        if (histArray[i] > max) max = histArray[i];

    return ((int) max);
}

void FITSHistogram::updateHistogram()
{
    constructHistogram(ui->histFrame->getValidWidth(), ui->histFrame->getValidHeight());
}

FITSHistogramCommand::FITSHistogramCommand(QWidget * parent, FITSHistogram *inHisto, int newType, int lmin, int lmax)
{

    viewer    = (FITSViewer *) parent;
    type      = newType;
    histo     = inHisto;
    //oldImage  = new QImage();
    // TODO apply maximum compression against this buffer
    buffer = (float *) malloc (viewer->image->getWidth() * viewer->image->getHeight() * sizeof(float));

    min = lmin;
    max = lmax;

}

FITSHistogramCommand::~FITSHistogramCommand()
{
    free(buffer);
    //delete (oldImage);
}

void FITSHistogramCommand::redo()
{

    float val, bufferVal;
    double coeff;
    FITSImage *image = viewer->image;
    float *image_buffer = image->getImageBuffer();
    int width  = image->getWidth();
    int height = image->getHeight();

    /*
    if (buffer == NULL)
    {
     // TODO how to remove this item from redo/undo history?
     KMessageBox(0, i18n("There is no enough memory to perform this operation."));
     return;
    }
    */

    memcpy(buffer, image_buffer, width * height * sizeof(float));
    //*oldImage = image->displayImage->copy();

    switch (type)
    {
    case FITSImage::FITSAuto:
    case FITSImage::FITSLinear:
        for (int i=0; i < height; i++)
            for (int j=0; j < width; j++)
            {
                bufferVal = image_buffer[i * width + j];
                if (bufferVal < min) bufferVal = min;
                else if (bufferVal > max) bufferVal = max;
                image_buffer[i * width + j] = bufferVal;

            }
        break;

    case FITSImage::FITSLog:
        coeff = max / log(1 + max);

        for (int i=0; i < height; i++)
            for (int j=0; j < width; j++)
            {
                bufferVal = image_buffer[i * width + j];
                if (bufferVal < min) bufferVal = min;
                else if (bufferVal > max) bufferVal = max;
                val = (coeff * log(1 + bufferVal));
                if (val < min) val = min;
                else if (val > max) val = max;
                image_buffer[i * width + j] = val;
            }
        break;

    case FITSImage::FITSSqrt:
        coeff = max / sqrt(max);

        for (int i=0; i < height; i++)
            for (int j=0; j < width; j++)
            {
                bufferVal = (int) image_buffer[i * width + j];
                if (bufferVal < min) bufferVal = min;
                else if (bufferVal > max) bufferVal = max;
                val = (int) (coeff * sqrt(bufferVal));
                image_buffer[i * width + j] = val;
            }
        break;


    default:
        break;
    }

    image->rescale(FITSImage::ZOOM_KEEP_LEVEL);

    if (histo != NULL)
        histo->updateHistogram();

    viewer->fitsChange();
}

void FITSHistogramCommand::undo()
{

    FITSImage *image = viewer->image;
    memcpy( image->getImageBuffer(), buffer, image->getWidth() * image->getHeight() * sizeof(float));
    image->rescale(FITSImage::ZOOM_KEEP_LEVEL);
    image->calculateStats();


    if (histo != NULL)
        histo->updateHistogram();

}

QString FITSHistogramCommand::text() const
{

    switch (type)
    {
    case 0:
        return i18n("Auto Scale");
        break;
    case 1:
        return i18n("Linear Scale");
        break;
    case 2:
        return i18n("Logarithmic Scale");
        break;
    case 3:
        return i18n("Square Root Scale");
        break;
    default:
        break;
    }

    return i18n("Unknown");

}

#include "fitshistogram.moc"

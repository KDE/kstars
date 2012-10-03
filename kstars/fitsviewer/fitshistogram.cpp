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
    float *buffer = tab->getImage()->getImageBuffer();

    tab->getImage()->getSize(&fits_w, &fits_h);
    tab->getImage()->getMinMax(&fits_min, &fits_max);

    int pixel_range = (int) (fits_max - fits_min);

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

    binWidth = ((double) hist_width / (double) pixel_range);
    //binRoundSize = (int) floor(binSize);

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

        //histArray[id]++;
        histArray[id]++;
    }

    // Cumuliative Frequency
    for (int i=0; i < histArray.size(); i++)
        for (int j=0; j <= i; j++)
            cumulativeFreq[i] += histArray[j];


     mean = (tab->getImage()->getAverage()-fits_min)*binWidth;
     mean_p_std = (tab->getImage()->getAverage()-fits_min+tab->getImage()->getStdDev()*3)*binWidth;

    // Indicator of information content of an image in a typical star field.
    JMIndex = mean_p_std - mean;


    #ifdef HIST_LOG
    qDebug() << "Mean " << mean << " , mean plus std " << mean_p_std << " JMIndex " << JMIndex << endl;
    #endif

    if (mean == 0)
        JMIndex = 0;
    // Reject diffuse images by setting JMIndex to zero.
    else if (tab->getImage()->getMode() != FITS_GUIDE && mean_p_std / mean < 2)
        JMIndex =0;

    #ifdef HIST_LOG
    qDebug() << "Final JMIndex " << JMIndex << endl;
    #endif

    // Normalize histogram height. i.e. the maximum value will take the whole height of the widget
    histFactor = ((double) hist_height) / ((double) findMax(hist_width));

    histogram_height = hist_height;
    histogram_width = hist_width;

    updateBoxes(fits_min, fits_max);

    ui->histFrame->update();

}

void FITSHistogram::updateBoxes(int lower_limit, int upper_limit)
{

    ui->minSlider->setMinimum(lower_limit);
    ui->maxSlider->setMinimum(lower_limit);

    ui->minSlider->setMaximum(upper_limit);
    ui->maxSlider->setMaximum(upper_limit);

    ui->minSlider->setValue(lower_limit);
    ui->maxSlider->setValue(upper_limit);

    ui->minOUT->setText(QString::number(lower_limit));
    ui->maxOUT->setText(QString::number(upper_limit));

}

void FITSHistogram::applyScale()
{

    int min = ui->minSlider->value();
    int max = ui->maxSlider->value();

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
    int min = ui->minSlider->value();
    int max = ui->maxSlider->value();

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
        ui->minSlider->setValue(ui->minOUT->text().toInt());
        return;
    }

    ui->minOUT->setText(QString::number(value));
}

void FITSHistogram::maxSliderUpdated(int value)
{
    if (value <= ui->minSlider->value())
    {
        ui->maxSlider->setValue(ui->maxOUT->text().toInt());
        return;
    }

    ui->maxOUT->setText(QString::number(value));
}

void FITSHistogram::updateIntenFreq(int x)
{
    if (x < 0 || x >= histogram_width)
        return;

    ui->intensityOUT->setText(QString("%1").arg( ceil(x / binWidth) + tab->getImage()->stats.min));

    ui->frequencyOUT->setText(QString("%1").arg(histArray[x]));
}

void FITSHistogram::updateLowerLimit()
{
    bool conversion_ok = false;
    int newLowerLimit=0;

    newLowerLimit = ui->minOUT->text().toInt(&conversion_ok);

    if (conversion_ok == false)
        return;

    ui->minSlider->setValue(newLowerLimit);
}

void FITSHistogram::updateUpperLimit()
{
    bool conversion_ok = false;
    int newUpperLimit=0;

    newUpperLimit = ui->maxOUT->text().toInt(&conversion_ok);

    if (conversion_ok == false)
        return;

    ui->maxSlider->setValue(newUpperLimit);
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
    constructHistogram(ui->histFrame->maximumWidth(), ui->histFrame->maximumHeight());
    //constructHistogram(histogram_width, histogram_height);
}

FITSHistogramCommand::FITSHistogramCommand(QWidget * parent, FITSHistogram *inHisto, FITSScale newType, int lmin, int lmax)
{
    tab         = (FITSTab *) parent;
    type        = newType;
    histogram   = inHisto;
    buffer = (float *) malloc (tab->getImage()->getWidth() * tab->getImage()->getHeight() * sizeof(float));

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

    FITSImage *image = tab->getImage();

    float *image_buffer = image->getImageBuffer();
    int width  = image->getWidth();
    int height = image->getHeight();

    memcpy(buffer, image_buffer, width * height * sizeof(float));


    switch (type)
    {
    case FITS_AUTO:
    case FITS_LINEAR:
        image->applyFilter(FITS_LINEAR, image_buffer, min, max);
        image->updateFrame();
        break;

    case FITS_LOG:
        image->applyFilter(FITS_LOG, image_buffer, min, max);
        image->updateFrame();
        break;

    case FITS_SQRT:
        image->applyFilter(FITS_SQRT, image_buffer, min, max);
        image->updateFrame();
        break;

    default:
       image->applyFilter(type, image_buffer);
       image->updateFrame();
       break;


    }

    if (histogram != NULL)
        histogram->updateHistogram();

}

void FITSHistogramCommand::undo()
{
    FITSImage *image = tab->getImage();
    memcpy( image->getImageBuffer(), buffer, image->getWidth() * image->getHeight() * sizeof(float));
    image->calculateStats(true);
    image->rescale(ZOOM_KEEP_LEVEL);
    image->updateFrame();


    if (histogram != NULL)
        histogram->updateHistogram();

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

#include "fitshistogram.moc"

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

histogramUI::histogramUI(QDialog *parent) : QDialog(parent)
{
    setupUi(parent);
    setModal(false);

}

FITSHistogram::FITSHistogram(QWidget *parent) : QDialog(parent)
{
    ui = new histogramUI(this);

    tab = (FITSTab *) parent;

    //histArray = NULL;

    type   = 0;
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

    tab->getImage()->getFITSSize(&fits_w, &fits_h);
    tab->getImage()->getFITSMinMax(&fits_min, &fits_max);

    int pixel_range = (int) (fits_max - fits_min);

    //qDebug() << "fits MIN: " << fits_min << " - fits MAX: " << fits_max << " - pixel range: " << pixel_range;

    if (hist_width > histArray.size())
        histArray.resize(hist_width);

    for (int i=0; i < hist_width; i++)
        histArray[i] = 0;

    binWidth = ((double) hist_width / (double) pixel_range);
    //binRoundSize = (int) floor(binSize);

    //qDebug() << "Hist Array is not " << hist_width << " wide..., pixel range is " << pixel_range << " Bin width is " << binWidth << endl;

    if (binWidth == 0 || buffer == NULL)
        return;

    //for (int i=0; i < fits_w * fits_h; i++)
    for (int i=0; i < fits_w * fits_h ; i++)
    {
        id = (int) round((buffer[i] - fits_min) * binWidth);

       // qDebug() << "Value: " << buffer[i] << " and we got bin ID: " << id << endl;
        if (id >= hist_width)
            id = hist_width - 1;

        histArray[id]++;
    }

    // Normalize histogram height. i.e. the maximum value will take the whole height of the widget
    histFactor = ((double) hist_height) / ((double) findMax(hist_width));
    /*for (int i=0; i < hist_width; i++)
    {
        if (histArray[i] == -1 && (i+1) != hist_width)
        {
            //kDebug () << "Histarray of " << i << " is not filled, it will take value of " << i+1 << " which is " << histArray[i+1];
            histArray[i] = histArray[i+1];
        }

        //kDebug() << "Normalizing, we have for i " << i << " a value of: " << histArray[i];
        histArray[i] = (int) (((double) histArray[i]) * histFactor);
        //kDebug() << "Normalized to " << histArray[i] << " since the factor is " << histFactor;
    }*/

    histogram_height = hist_height;
    histogram_width = hist_width;

    // Initially
    //updateBoxes(ui->histFrame->getLowerLimit(), ui->histFrame->getUpperLimit());

    updateBoxes(fits_min, fits_max);

    ui->histFrame->update();

}

void FITSHistogram::updateBoxes(int lower_limit, int upper_limit)
{

    /*double lower_limit_x, upper_limit_x;

    lower_limit_x = ceil(lowerLimit * binWidth) + fits_min;
    upper_limit_x = ceil(upperLimit * binWidth) + fits_min;*/

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

    //qDebug() << "Update freq for X " << x << endl;

    if (x < 0 || x >= histogram_width)
        return;



    //qDebug() << "Index is " << index << " with value from array of " <<  histArray[x] << endl;

    ui->intensityOUT->setText(QString("%1").arg( ceil(x / binWidth) + tab->getImage()->stats.min));

    ui->frequencyOUT->setText(QString("%1").arg(histArray[floor(x / binWidth)]));


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

FITSHistogramCommand::FITSHistogramCommand(QWidget * parent, FITSHistogram *inHisto, int newType, int lmin, int lmax)
{


    tab    = (FITSTab *) parent;
    type      = newType;
    histo     = inHisto;
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

    float val, bufferVal;
    double coeff;
    FITSImage *image = tab->getImage();
    float *image_buffer = image->getImageBuffer();
    int width  = image->getWidth();
    int height = image->getHeight();

    memcpy(buffer, image_buffer, width * height * sizeof(float));


    switch (type)
    {
    case FITSAuto:
    case FITSLinear:
        for (int i=0; i < height; i++)
            for (int j=0; j < width; j++)
            {
                bufferVal = image_buffer[i * width + j];
                if (bufferVal < min) bufferVal = min;
                else if (bufferVal > max) bufferVal = max;
                image_buffer[i * width + j] = bufferVal;

            }
        break;

    case FITSLog:
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

    case FITSSqrt:
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

    tab->getImage()->calculateStats(true);
    tab->getImage()->rescale(ZOOM_KEEP_LEVEL);

    if (histo != NULL)
        histo->updateHistogram();

    //tab->modifyFITSState(false);

}

void FITSHistogramCommand::undo()
{
    FITSImage *image = tab->getImage();
    memcpy( image->getImageBuffer(), buffer, image->getWidth() * image->getHeight() * sizeof(float));
    image->calculateStats(true);
    image->rescale(ZOOM_KEEP_LEVEL);


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

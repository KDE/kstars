/***************************************************************************
                          fitshistogram.h  -  FITS Historgram
                          ---------------
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

#ifndef FITSHISTOGRAM
#define FITSHISTOGRAM

#include "ui_fitshistogramui.h"

#include <QUndoCommand>
#include <QPixmap>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QDialog>
#include <QVarLengthArray>

#define CIRCLE_DIM	16

const int INITIAL_MAXIMUM_WIDTH = 1024;

class FITSTab;
class QPixmap;


class histogramUI : public QDialog, public Ui::FITSHistogramUI
{
    Q_OBJECT

public:
    histogramUI(QDialog *parent=0);

};

class FITSHistogram : public QDialog
{
    Q_OBJECT

    friend class histDrawArea;

public:
    FITSHistogram(QWidget *parent);
    ~FITSHistogram();

    void constructHistogram(int hist_width, int hist_height);
    void updateHistogram();
    int  findMax(int hist_width);
    int type;
    int napply;
    double histFactor;
    double fits_min, fits_max;
    FITSTab *tab;

private:

    double binSize;
    histogramUI *ui;
    int histogram_height, histogram_width;
    QVarLengthArray<int, INITIAL_MAXIMUM_WIDTH> histArray;

public slots:
    void applyScale();
    void updateBoxes(int lowerLimit, int upperLimit);
    void updateIntenFreq(int x);


};

class FITSHistogramCommand : public QUndoCommand
{
public:
    FITSHistogramCommand(QWidget * parent, FITSHistogram *inHisto, int newType, int lmin, int lmax);
    virtual ~FITSHistogramCommand();

    virtual void redo();
    virtual void undo();
    virtual QString text() const;


private:
    FITSHistogram *histo;
    int type;
    int min, max;
    float *buffer;
    FITSTab *tab;
};


#endif

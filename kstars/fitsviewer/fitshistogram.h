/*  FITS Histogram
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#ifndef FITSHISTOGRAM
#define FITSHISTOGRAM

#include "ui_fitshistogramui.h"
#include "fitscommon.h"

#include <QUndoCommand>
#include <QPixmap>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QDialog>
#include <QVarLengthArray>

#define CIRCLE_DIM	16

const int INITIAL_MAXIMUM_WIDTH = 500;

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

    void constructHistogram();

    void applyFilter(FITSScale ftype);

    double getBinWidth() { return binWidth; }

    //QVarLengthArray<int, INITIAL_MAXIMUM_WIDTH> getCumulativeFreq() { return cumulativeFreq; }
    //QVarLengthArray<int, INITIAL_MAXIMUM_WIDTH> getHistogram() { return histArray; }


    //int napply;
    //double histHeightRatio;

    //double histFactor;



    QVector<double> getCumulativeFrequency() const;

    double getJMIndex() const;

public slots:
    void applyScale();
    void updateValues(QMouseEvent *event);
    void updateLimits(double value);
    void checkRangeLimit(const QCPRange &range);

private:

    template<typename T> void constructHistogram();

    histogramUI *ui;
    FITSTab *tab;

    QVector<double> intensity;
    QVector<double> r_frequency, g_frequency, b_frequency;
    QCPGraph *r_graph, *g_graph, *b_graph;
    QVector<double> cumulativeFrequency;

    double binWidth;
    double JMIndex;
    double fits_min, fits_max;
    uint16_t binCount;
    FITSScale type;    
    QCustomPlot *customPlot;


};

class FITSHistogramCommand : public QUndoCommand
{
public:
    FITSHistogramCommand(QWidget * parent, FITSHistogram *inHisto, FITSScale newType, double lmin, double lmax);
    virtual ~FITSHistogramCommand();

    virtual void redo();
    virtual void undo();
    virtual QString text() const;


private:

    /* stats struct to hold statisical data about the FITS data */
    struct
    {
        double min, max;
        double mean;
        double stddev;
        double median;
        double SNR;
        int bitpix;
        int ndim;
        unsigned int size;
        long dim[2];
    } stats;

    bool calculateDelta(uint8_t *buffer);
    bool reverseDelta();
    void saveStats(double min, double max, double stddev, double mean, double median, double SNR);
    void restoreStats();

    FITSHistogram *histogram;
    FITSScale type;
    double min, max;
    int gamma;

    unsigned char *delta;
    unsigned long compressedBytes;
    uint8_t *original_buffer;
    FITSTab *tab;
};


#endif

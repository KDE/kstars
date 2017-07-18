/*  FITS Histogram
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#pragma once

#include "fitscommon.h"
#include "ui_fitshistogramui.h"

#include <QDialog>
#include <QUndoCommand>

class QMouseEvent;

class FITSTab;

class histogramUI : public QDialog, public Ui::FITSHistogramUI
{
    Q_OBJECT

  public:
    explicit histogramUI(QDialog *parent = 0);
};

class FITSHistogram : public QDialog
{
    Q_OBJECT

    friend class histDrawArea;

  public:
    explicit FITSHistogram(QWidget *parent);
    ~FITSHistogram();

    void constructHistogram();

    void applyFilter(FITSScale ftype);

    double getBinWidth() { return binWidth; }

    QVector<double> getCumulativeFrequency() const;

    double getJMIndex() const;

  public slots:
    void applyScale();
    void updateValues(QMouseEvent *event);
    void updateLimits(double value);
    void checkRangeLimit(const QCPRange &range);

  private:
    template <typename T>
    void constructHistogram();

    histogramUI *ui { nullptr };
    FITSTab *tab { nullptr };

    QVector<double> intensity;
    QVector<double> r_frequency, g_frequency, b_frequency;
    QCPGraph *r_graph { nullptr };
    QCPGraph *g_graph { nullptr };
    QCPGraph *b_graph { nullptr };
    QVector<double> cumulativeFrequency;

    double binWidth { 0 };
    double JMIndex { 0 };
    double fits_min { 0 };
    double fits_max { 0 };
    uint16_t binCount { 0 };
    FITSScale type { FITS_AUTO };
    QCustomPlot *customPlot { nullptr };
};

class FITSHistogramCommand : public QUndoCommand
{
  public:
    FITSHistogramCommand(QWidget *parent, FITSHistogram *inHisto, FITSScale newType, double lmin, double lmax);
    virtual ~FITSHistogramCommand();

    virtual void redo();
    virtual void undo();
    virtual QString text() const;

  private:
    /* stats struct to hold statistical data about the FITS data */
    struct
    {
        double min { 0 };
        double max { 0 };
        double mean { 0 };
        double stddev { 0 };
        double median { 0 };
        double SNR { 0 };
        int bitpix { 0 };
        int ndim { 0 };
        unsigned int size { 0 };
        long dim[2];
    } stats;

    bool calculateDelta(uint8_t *buffer);
    bool reverseDelta();
    void saveStats(double min, double max, double stddev, double mean, double median, double SNR);
    void restoreStats();

    FITSHistogram *histogram { nullptr };
    FITSScale type;
    double min { 0 };
    double max { 0 };

    unsigned char *delta { nullptr };
    unsigned long compressedBytes { 0 };
    FITSTab *tab { nullptr };
};

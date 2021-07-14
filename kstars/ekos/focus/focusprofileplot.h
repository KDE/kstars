/*  Ekos focus profile plot widget
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
                  2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QObject>
#include <QWidget>
#include "qcustomplot.h"

class FocusProfilePlot : public QCustomPlot
{
public:
    FocusProfilePlot(QWidget *parent = nullptr);

    void setFocusAuto(bool isAuto) {focusAuto = isAuto;}
    void clear();

public slots:
    void drawProfilePlot(double currentHFR);

private:
    bool focusAuto = true;
    QCPGraph *currentGaus { nullptr };
    QCPGraph *firstGaus { nullptr };
    QCPGraph *lastGaus { nullptr };

    // Last gaussian fit values
    QVector<double> lastGausIndexes;
    QVector<double> lastGausFrequencies;

};

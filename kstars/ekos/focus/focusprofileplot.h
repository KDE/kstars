/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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

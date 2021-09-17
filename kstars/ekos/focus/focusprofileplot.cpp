/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusprofileplot.h"

FocusProfilePlot::FocusProfilePlot(QWidget *parent) : QCustomPlot (parent)
{
    Q_UNUSED(parent);

    setBackground(QBrush(Qt::black));
    xAxis->setBasePen(QPen(Qt::white, 1));
    yAxis->setBasePen(QPen(Qt::white, 1));
    xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    xAxis->grid()->setZeroLinePen(Qt::NoPen);
    yAxis->grid()->setZeroLinePen(Qt::NoPen);
    xAxis->setBasePen(QPen(Qt::white, 1));
    yAxis->setBasePen(QPen(Qt::white, 1));
    xAxis->setTickPen(QPen(Qt::white, 1));
    xAxis->setTickLabelFont(QFont(font().family(), 9));
    yAxis->setTickPen(QPen(Qt::white, 1));
    yAxis->setTickLabelFont(QFont(font().family(), 9));
    xAxis->setSubTickPen(QPen(Qt::white, 1));
    yAxis->setSubTickPen(QPen(Qt::white, 1));
    xAxis->setTickLabelColor(Qt::white);
    yAxis->setTickLabelColor(Qt::white);
    xAxis->setLabelColor(Qt::white);
    yAxis->setLabelColor(Qt::white);

    currentGaus = addGraph();
    currentGaus->setLineStyle(QCPGraph::lsLine);
    currentGaus->setPen(QPen(Qt::red, 2));

    lastGaus = addGraph();
    lastGaus->setLineStyle(QCPGraph::lsLine);
    QPen pen(Qt::darkGreen);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(2);
    lastGaus->setPen(pen);

}

void FocusProfilePlot::drawProfilePlot(double currentHFR)
{
    QVector<double> currentIndexes;
    QVector<double> currentFrequencies;

    // HFR = 50% * 1.36 = 68% aka one standard deviation
    double stdDev = currentHFR * 1.36;
    float start   = -stdDev * 4;
    float end     = stdDev * 4;
    float step    = stdDev * 4 / 20.0;
    for (double x = start; x < end; x += step)
    {
        currentIndexes.append(x);
        currentFrequencies.append((1 / (stdDev * sqrt(2 * M_PI))) * exp(-1 * (x * x) / (2 * (stdDev * stdDev))));
    }

    currentGaus->setData(currentIndexes, currentFrequencies);

    if (lastGausIndexes.count() > 0)
        lastGaus->setData(lastGausIndexes, lastGausFrequencies);

    if (focusAuto && firstGaus == nullptr)
    {
        firstGaus = addGraph();
        QPen pen;
        pen.setStyle(Qt::DashDotLine);
        pen.setWidth(2);
        pen.setColor(Qt::darkMagenta);
        firstGaus->setPen(pen);

        firstGaus->setData(currentIndexes, currentFrequencies);
    }
    else if (firstGaus)
    {
        removeGraph(firstGaus);
        firstGaus = nullptr;
    }

    rescaleAxes();
    replot();

    lastGausIndexes     = currentIndexes;
    lastGausFrequencies = currentFrequencies;
}

void FocusProfilePlot::clear()
{
    if (firstGaus)
    {
        removeGraph(firstGaus);
        firstGaus = nullptr;
    }

}

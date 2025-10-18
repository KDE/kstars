/*  Target plot for alignment.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>, 2025

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "qcustomplot.h"


class TargetTag : public QObject
{
    Q_OBJECT
public:
    explicit TargetTag(QCustomPlot *parent);

    /**
     * @brief displayAlignment display the current alignment error on the target
     */
    void displayAlignment(double deltaAlt, double deltaAz);

    QPointer<QCPItemTracer> innerCircle, outerCircle;
    QPointer<QCPItemLine> upperSpike , rightSpike , lowerSpike , leftSpike ;
    QPointer<QCPItemText> upperLabel, lowerLabel, leftLabel, rightLabel;
};


class AlignmentPlot : public QCustomPlot
{
    Q_OBJECT

public:
    AlignmentPlot(QWidget *parent);

    /**
     * @brief buildTarget build the basic target graphs
     */
    void buildTarget();

    /**
     * @brief displayAlignment display the current alignment error on the target
     */
    void displayAlignment(double deltaAlt, double deltaAz);

private:
    // ensure that it is always a square
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w; }
    QSize sizeHint() const override { return QSize(200, 200); }

    // the current position error
    QPointer<TargetTag> currentError;
    void initGraph();

};

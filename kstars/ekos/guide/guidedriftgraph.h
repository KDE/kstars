/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once


#include <QObject>
#include <QWidget>

#include "qcustomplot.h"
#include "guidegraph.h"
#include "guideinterface.h"

class GuideDriftGraph : public QCustomPlot
{
    Q_OBJECT

public:
    GuideDriftGraph(QWidget *parent = nullptr);
    void guideHistory(int sliderValue, bool graphOnLatestPt);
    /**
     * @brief setRMSVisibility Decides which RMS plot is visible.
     */
    void setRMSVisibility();
    void exportGuideData();
    void resetTimer();
    void connectGuider(Ekos::GuideInterface *guider);

public slots:
    void handleVerticalPlotSizeChange();
    void handleHorizontalPlotSizeChange();

    void setupNSEWLabels();
    void autoScaleGraphs();
    void zoomX(int zoomLevel);
    void zoomInX();
    void zoomOutX();
    void setCorrectionGraphScale(int value);
    void setLatestGuidePoint(bool isChecked) {graphOnLatestPt = isChecked;}
    void clear();

    void toggleShowPlot(GuideGraph::DRIFT_GRAPH_INDICES plot, bool isChecked);
    void setAxisDelta(double ra, double de);
    void setAxisSigma(double ra, double de);
    void setAxisPulse(double ra, double de);
    void setSNR(double snr);
    void updateCorrectionsScaleVisibility();

    // Display guide information when hovering over the drift graph
    void mouseOverLine(QMouseEvent *event);

    // Reset graph if right clicked
    void mouseClicked(QMouseEvent *event);

    /** @brief Update colors following a color scheme update notification.
     */
    void refreshColorScheme();

private:
    // The scales of these zoom levels are defined in Guide::zoomX().
    static constexpr int defaultXZoomLevel = 3;
    int driftGraphZoomLevel {defaultXZoomLevel};
    bool graphOnLatestPt = true;

    // Axis for the SNR part of the driftGraph. Qt owns this pointer's memory.
    QCPAxis *snrAxis;

    // Guide timer
    QTime guideTimer;

    QUrl guideURLPath;
};

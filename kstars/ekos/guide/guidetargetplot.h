/*  Ekos guide target graph plot widget
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
#include "guideinterface.h"

class GuideTargetPlot: public QCustomPlot
{
    Q_OBJECT

public:
    GuideTargetPlot(QWidget *parent = nullptr);
    void showPoint(double ra, double de);
    void setLatestGuidePoint(bool isChecked) {graphOnLatestPt = isChecked;}
    void connectGuider(Ekos::GuideInterface *guider);
    void resize(int w, int h);

public slots:
    void handleVerticalPlotSizeChange();
    void handleHorizontalPlotSizeChange();

    void buildTarget(double accuracyRadius);
    void setupNSEWLabels();
    void autoScaleGraphs(double accuracyRadius);
    void clear();

protected:
    // virtual void resizeEvent(QResizeEvent *resize) override;

private slots:
    void setAxisDelta(double ra, double de);


private:
    QCPCurve *centralTarget { nullptr };
    QCPCurve *yellowTarget { nullptr };
    QCPCurve *redTarget { nullptr };
    QCPCurve *concentricRings { nullptr };

    bool graphOnLatestPt = true;

};

/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "guidetargetplot.h"
#include "guidegraph.h"
#include "klocalizedstring.h"
#include "kstarsdata.h"
#include "Options.h"


GuideTargetPlot::GuideTargetPlot(QWidget *parent) : QCustomPlot (parent)
{
    Q_UNUSED(parent);
    //drift plot
    double accuracyRadius = Options::guiderAccuracyThreshold();

    setBackground(QBrush(Qt::black));
    setSelectionTolerance(10);

    xAxis->setBasePen(QPen(Qt::white, 1));
    yAxis->setBasePen(QPen(Qt::white, 1));

    xAxis->setTickPen(QPen(Qt::white, 1));
    yAxis->setTickPen(QPen(Qt::white, 1));

    xAxis->setSubTickPen(QPen(Qt::white, 1));
    yAxis->setSubTickPen(QPen(Qt::white, 1));

    xAxis->setTickLabelColor(Qt::white);
    yAxis->setTickLabelColor(Qt::white);

    xAxis->setLabelColor(Qt::white);
    yAxis->setLabelColor(Qt::white);

    xAxis->setLabelFont(QFont(font().family(), 9));
    yAxis->setLabelFont(QFont(font().family(), 9));
    xAxis->setTickLabelFont(QFont(font().family(), 9));
    yAxis->setTickLabelFont(QFont(font().family(), 9));

    xAxis->setLabelPadding(2);
    yAxis->setLabelPadding(2);

    xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    xAxis->grid()->setZeroLinePen(QPen(Qt::gray));
    yAxis->grid()->setZeroLinePen(QPen(Qt::gray));

    xAxis->setLabel(i18n("dRA (arcsec)"));
    yAxis->setLabel(i18n("dDE (arcsec)"));

    xAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    yAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);

    setInteractions(QCP::iRangeZoom);
    setInteraction(QCP::iRangeDrag, true);

    addGraph();
    graph(GuideGraph::G_RA)->setLineStyle(QCPGraph::lsNone);
    graph(GuideGraph::G_RA)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssStar, Qt::gray, 5));

    addGraph();
    graph(GuideGraph::G_DEC)->setLineStyle(QCPGraph::lsNone);
    graph(GuideGraph::G_DEC)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle, QPen(Qt::yellow, 2), QBrush(), 10));

    setupNSEWLabels();

    // resize(190, 190);
    replot();
}

void GuideTargetPlot::showPoint(double ra, double de)
{
    graph(GuideGraph::G_DEC)->data()->clear(); //Clear Guide highlighted point
    graph(GuideGraph::G_DEC)->addData(ra, de); //Set guide highlighted point
    replot();
}

void GuideTargetPlot::connectGuider(Ekos::GuideInterface *guider)
{
    connect(guider, &Ekos::GuideInterface::newAxisDelta, this, &GuideTargetPlot::setAxisDelta);
}

void GuideTargetPlot::handleHorizontalPlotSizeChange()
{
    resize(size().width(), size().height());
    replot();
}

void GuideTargetPlot::handleVerticalPlotSizeChange()
{
    resize(size().width(), size().height());
    replot();
}

void GuideTargetPlot::resize(int w, int h)
{
    QCustomPlot::resize(w, h);

    const int accuracyRadius = Options::guiderAccuracyThreshold();
    if (w > h)
    {
        yAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
        xAxis->setScaleRatio(yAxis, 1.0);
    }
    else
    {
        xAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
        yAxis->setScaleRatio(xAxis, 1.0);
    }
    replot();
}

void GuideTargetPlot::buildTarget(double accuracyRadius)
{
    if (centralTarget)
    {
        concentricRings->data()->clear();
        redTarget->data()->clear();
        yellowTarget->data()->clear();
        centralTarget->data()->clear();
    }
    else
    {
        concentricRings = new QCPCurve(xAxis, yAxis);
        redTarget       = new QCPCurve(xAxis, yAxis);
        yellowTarget    = new QCPCurve(xAxis, yAxis);
        centralTarget   = new QCPCurve(xAxis, yAxis);
    }
    const int pointCount = 200;
    QVector<QCPCurveData> circleRings(
        pointCount * (5)); //Have to multiply by the number of rings, Rings at : 25%, 50%, 75%, 125%, 175%
    QVector<QCPCurveData> circleCentral(pointCount);
    QVector<QCPCurveData> circleYellow(pointCount);
    QVector<QCPCurveData> circleRed(pointCount);

    int circleRingPt = 0;
    for (int i = 0; i < pointCount; i++)
    {
        double theta = i / static_cast<double>(pointCount) * 2 * M_PI;

        for (double ring = 1; ring < 8; ring++)
        {
            if (ring != 4 && ring != 6)
            {
                if (i % (9 - static_cast<int>(ring)) == 0) //This causes fewer points to draw on the inner circles.
                {
                    circleRings[circleRingPt] = QCPCurveData(circleRingPt, accuracyRadius * ring * 0.25 * qCos(theta),
                                                accuracyRadius * ring * 0.25 * qSin(theta));
                    circleRingPt++;
                }
            }
        }

        circleCentral[i] = QCPCurveData(i, accuracyRadius * qCos(theta), accuracyRadius * qSin(theta));
        circleYellow[i]  = QCPCurveData(i, accuracyRadius * 1.5 * qCos(theta), accuracyRadius * 1.5 * qSin(theta));
        circleRed[i]     = QCPCurveData(i, accuracyRadius * 2 * qCos(theta), accuracyRadius * 2 * qSin(theta));
    }

    concentricRings->setLineStyle(QCPCurve::lsNone);
    concentricRings->setScatterSkip(0);
    concentricRings->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(255, 255, 255, 150), 1));

    concentricRings->data()->set(circleRings, true);
    redTarget->data()->set(circleRed, true);
    yellowTarget->data()->set(circleYellow, true);
    centralTarget->data()->set(circleCentral, true);

    concentricRings->setPen(QPen(Qt::white));
    redTarget->setPen(QPen(Qt::red));
    yellowTarget->setPen(QPen(Qt::yellow));
    centralTarget->setPen(QPen(Qt::green));

    concentricRings->setBrush(Qt::NoBrush);
    redTarget->setBrush(QBrush(QColor(255, 0, 0, 50)));
    yellowTarget->setBrush(
        QBrush(QColor(0, 255, 0, 50))); //Note this is actually yellow.  It is green on top of red with equal opacity.
    centralTarget->setBrush(QBrush(QColor(0, 255, 0, 50)));

    if (size().width() > 0)
        resize(size().width(), size().height());
}

void GuideTargetPlot::setupNSEWLabels()
{
    //Labels for N/S/E/W
    QColor raLabelColor(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"));
    QColor deLabelColor(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"));

    QCPItemText *northLabel = new QCPItemText(this);
    northLabel->setColor(deLabelColor);
    northLabel->setFont(QFont(font().family(), 9));
    northLabel->setText(i18nc("North", "N"));
    northLabel->position->setType(QCPItemPosition::ptViewportRatio);
    northLabel->position->setCoords(0.25, 0.2);
    northLabel->setVisible(true);

    QCPItemText *southLabel = new QCPItemText(this);
    southLabel->setColor(deLabelColor);
    southLabel->setFont(QFont(font().family(), 9));
    southLabel->setText(i18nc("South", "S"));
    southLabel->position->setType(QCPItemPosition::ptViewportRatio);
    southLabel->position->setCoords(0.25, 0.7);
    southLabel->setVisible(true);

    QCPItemText *westLabel = new QCPItemText(this);
    westLabel->setColor(raLabelColor);
    westLabel->setFont(QFont(font().family(), 9));
    westLabel->setText(i18nc("West", "W"));
    westLabel->position->setType(QCPItemPosition::ptViewportRatio);
    westLabel->position->setCoords(0.8, 0.75);
    westLabel->setVisible(true);

    QCPItemText *eastLabel = new QCPItemText(this);
    eastLabel->setColor(raLabelColor);
    eastLabel->setFont(QFont(font().family(), 9));
    eastLabel->setText(i18nc("East", "E"));
    eastLabel->position->setType(QCPItemPosition::ptViewportRatio);
    eastLabel->position->setCoords(0.3, 0.75);
    eastLabel->setVisible(true);
}

void GuideTargetPlot::autoScaleGraphs(double accuracyRadius)
{
    xAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    yAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    yAxis->setScaleRatio(xAxis, 1.0);
    xAxis->setScaleRatio(yAxis, 1.0);
    replot();
}

void GuideTargetPlot::clear()
{
    graph(GuideGraph::G_RA)->data()->clear(); //Guide data
    graph(GuideGraph::G_DEC)->data()->clear(); //Guide highlighted point
    setupNSEWLabels();
    replot();
}

void GuideTargetPlot::setAxisDelta(double ra, double de)
{
    //Add to Drift Plot
    graph(GuideGraph::G_RA)->addData(ra, de);
    if(graphOnLatestPt)
    {
        graph(GuideGraph::G_DEC)->data()->clear(); //Clear highlighted point
        graph(GuideGraph::G_DEC)->addData(ra, de); //Set highlighted point to latest point
    }

    if (xAxis->range().contains(ra) == false || yAxis->range().contains(de) == false)
    {
        setBackground(QBrush(Qt::gray));
        QTimer::singleShot(300, this, [ = ]()
        {
            setBackground(QBrush(Qt::black));
            replot();
        });
    }

    replot();
}

/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scheduleraltitudegraph.h"
#include "ui_scheduleraltitudegraph.h"
#include "kplotwidget.h"
#include "kplotobject.h"
#include "kplotaxis.h"
#include "ksalmanac.h"
#include <QPen>

namespace Ekos
{
SchedulerAltitudeGraph::SchedulerAltitudeGraph(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SchedulerAltitudeGraph)
{
    ui->setupUi(this);
}

void SchedulerAltitudeGraph::setTitle(const QString &title)
{
    //setWindowTitle(title);
    ui->label->setText(title);
}

// If scheduledRun is true, you previously needed to call plot with it false.
// It doesn't makes sense to call it with false after a call with it true,
// as that would wipe out the data from the previous call.
void SchedulerAltitudeGraph::plot(const GeoLocation *geo, KSAlmanac *ksal,
                                  const QVector<double> &times, const QVector<double> &alts, bool scheduledRun)
{
    KPlotObject *po = new KPlotObject(Qt::white, KPlotObject::Lines, 2);
    if (scheduledRun)
    {
        QPen pen;
        pen.setWidth(10);
        pen.setColor(Qt::green);
        po->setLinePen(pen);
    }
    else
    {
        ui->avt->setLimits(times[0], times.last(), -90.0, 90.0);
        ui->avt->setSecondaryLimits(times[0], times.last(), -90.0, 90.0);
        ui->avt->axis(KPlotWidget::BottomAxis)->setTickLabelFormat('t');
        ui->avt->axis(KPlotWidget::BottomAxis)->setLabel(i18n("Local Time"));
        ui->avt->axis(KPlotWidget::TopAxis)->setTickLabelFormat('t');
        ui->avt->axis(KPlotWidget::TopAxis)->setTickLabelsShown(true);
        ui->avt->setGeoLocation(geo);

        ui->avt->setSunRiseSetTimes(ksal->getSunRise(), ksal->getSunSet());
        ui->avt->setDawnDuskTimes(ksal->getDawnAstronomicalTwilight(), ksal->getDuskAstronomicalTwilight());
        ui->avt->setMinMaxSunAlt(ksal->getSunMinAlt(), ksal->getSunMaxAlt());
        ui->avt->setMoonRiseSetTimes(ksal->getMoonRise(), ksal->getMoonSet());
        ui->avt->setMoonIllum(ksal->getMoonIllum());

        const double noonOffset = times[0] - -12;
        const double plotDuration = times.last() - times[0];
        ui->avt->setPlotExtent(noonOffset, plotDuration);
        ui->avt->removeAllPlotObjects();
    }

    for (int i = 0; i < times.size(); ++i)
        po->addPoint(times[i], alts[i]);
    ui->avt->addPlotObject(po);

    ui->avt->update();
}

SchedulerAltitudeGraph::~SchedulerAltitudeGraph()
{
    delete ui;
}

}

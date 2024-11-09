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
    ui->avt->plot(geo, ksal, times, alts, scheduledRun);
}

SchedulerAltitudeGraph::~SchedulerAltitudeGraph()
{
    delete ui;
}

}

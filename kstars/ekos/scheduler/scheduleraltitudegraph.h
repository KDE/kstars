/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include "ekos/ekos.h"

class GeoLocation;
class KStarsDateTime;
class KSAlmanac;

namespace Ui
{
class SchedulerAltitudeGraph;
}

namespace Ekos
{
class SchedulerAltitudeGraph : public QDialog
{
        Q_OBJECT

    public:
        explicit SchedulerAltitudeGraph(QWidget *parent = nullptr);
        ~SchedulerAltitudeGraph();

    void plot(const GeoLocation *geo, KSAlmanac *ksal,
              const QVector<double> &times, const QVector<double> &alts);
    void plotOverlay(const QVector<double> &times, const QVector<double> &alts, int penWidth = 5, Qt::GlobalColor = Qt::green);
    void setTitle(const QString &name);

    private:
        Ui::SchedulerAltitudeGraph *ui;
};

}


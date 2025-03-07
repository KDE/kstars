/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_scheduleraltitudegraph.h"

#include "ekos/ekos.h"

#include <QFrame>
#include <QDateTime>

namespace Ekos
{

class SchedulerModuleState;

class SchedulerAltitudeGraph : public QFrame, public Ui::SchedulerAltitudeGraph
{
        Q_OBJECT

    public:
      SchedulerAltitudeGraph(QWidget * parent = nullptr);

      // Must be called at setup to point to the scheduler state.
      void setState(QSharedPointer<SchedulerModuleState> state);

      // Plots the graph.
      void plot();

      // Calls plot, but only if it hasn't been called in the past 120s.
      void tickle();

    private:
      void setup();
      void handleButtons(bool disable = false);

      QSharedPointer<SchedulerModuleState> m_State;

      // Last time we plotted.
      QDateTime m_AltitudeGraphUpdateTime;

      // Tells plot to increment the day to be plotted by this many days.
      int m_AltGraphDay { 0 };

    protected slots:
        void next();
        void prev();
};
}


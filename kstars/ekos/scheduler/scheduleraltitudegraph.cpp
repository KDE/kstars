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
#include "schedulerjob.h"
#include "schedulerutils.h"

// This is just to get the time (including simulated time).
#include "schedulermodulestate.h"


namespace Ekos
{

SchedulerAltitudeGraph::SchedulerAltitudeGraph(QWidget * parent) : QFrame(parent)
{
    setupUi(this);
    setup();
}

void SchedulerAltitudeGraph::setState(QSharedPointer<SchedulerModuleState> state)
{
    m_State = state;
}

// The job table gets updated constantly (e.g. every second), so this
// reduces the rate that the altitude graphs get regenerated.
void SchedulerAltitudeGraph::tickle()
{
    if (m_State->jobs().isEmpty() ||
            !m_AltitudeGraphUpdateTime.isValid() ||
            m_AltitudeGraphUpdateTime.secsTo(SchedulerModuleState::getLocalTime()) > 120)
    {
        m_AltGraphDay = 0;
        plot();
    }
}

void SchedulerAltitudeGraph::next()
{
    m_AltGraphDay++;
    if (m_AltGraphDay > 2)
        m_AltGraphDay = 2;
    plot();
}
void SchedulerAltitudeGraph::prev()
{
    m_AltGraphDay--;
    if (m_AltGraphDay < 0)
        m_AltGraphDay = 0;
    plot();
}

void SchedulerAltitudeGraph::setup()
{
    // Hide the buttons and disable them
    altMoveLeftB->setStyleSheet("background-color: black;");
    altMoveRightB->setStyleSheet("background-color: black;");
    altMoveLeftB->setIcon(QIcon());
    altMoveRightB->setIcon(QIcon());
    altMoveLeftB->setEnabled(false);
    altMoveRightB->setEnabled(false);
    altMoveLeftB->setFixedWidth(16);
    altMoveRightB->setFixedWidth(16);
    altMoveLeftB->setFixedHeight(16);
    altMoveRightB->setFixedHeight(16);

    altGraph->setAltitudeAxis(-20.0, 90.0);
    altGraph->setTopPadding(0);
    altGraph->setBottomPadding(25);
    altGraph->setLeftPadding(25);
    altGraph->setRightPadding(10);
    altGraph->disableAxis(KPlotWidget::TopAxis);

    connect(altMoveRightB, &QPushButton::clicked, this, &SchedulerAltitudeGraph::next);
    connect(altMoveLeftB, &QPushButton::clicked, this, &SchedulerAltitudeGraph::prev);
}

void SchedulerAltitudeGraph::handleButtons(bool disable)
{

    if (disable)
    {
        m_AltGraphDay = 0;
        altMoveLeftB->setEnabled(false);
        altMoveLeftB->setIcon(QIcon());
        altMoveRightB->setEnabled(false);
        altMoveRightB->setIcon(QIcon());
    }
    else if (m_AltGraphDay == 1)
    {
        altMoveLeftB->setEnabled(true);
        altMoveRightB->setEnabled(true);
        altMoveLeftB->setIcon(QIcon::fromTheme("arrow-left"));
        altMoveRightB->setIcon(QIcon::fromTheme("arrow-right"));
    }
    else if (m_AltGraphDay == 2)
    {
        altMoveLeftB->setEnabled(true);
        altMoveLeftB->setIcon(QIcon::fromTheme("arrow-left"));
        altMoveRightB->setEnabled(false);
        altMoveRightB->setIcon(QIcon());
    }
    else
    {
        m_AltGraphDay = 0;
        altMoveLeftB->setEnabled(false);
        altMoveLeftB->setIcon(QIcon());
        altMoveRightB->setEnabled(true);
        altMoveRightB->setIcon(QIcon::fromTheme("arrow-right"));
    }
}

namespace
{
int additionalOffset(const QList<SchedulerJob *> &jobs,
                     const QDateTime &now, const QDateTime &normalStart)
{
    QDateTime newStartTime;
    for (int index = 0; index < jobs.size(); index++)
    {
        const auto job = jobs.at(index);
        for (const auto &jobSchedule : job->getSimulatedSchedule())
        {
            const auto startTime = jobSchedule.startTime;
            const auto stopTime = jobSchedule.stopTime;
            if (!startTime.isValid())
                continue;

            if (startTime >= now && startTime < normalStart)
            {
                if (!newStartTime.isValid() || newStartTime > startTime)
                    newStartTime = startTime.addSecs(-1800);
            }
            else if (startTime < now && (!stopTime.isValid() || (stopTime > now)))
            {
                if (!newStartTime.isValid() || newStartTime > now)
                    newStartTime = now.addSecs(-1800);
            }
        }
    }
    if (newStartTime.isValid())
        return static_cast<int>(0.99 + newStartTime.secsTo(normalStart) / 3600.0);
    else return 0;
}
}  // namespace

void SchedulerAltitudeGraph::plot()
{
    if (m_State->jobs().size() == 0)
    {
        altGraph->removeAllPlotObjects();
        altGraph->update();
        m_AltGraphDay = 0;
        altGraphLabel->setText("");
        handleButtons(true);
        return;
    }
    m_AltitudeGraphUpdateTime = SchedulerModuleState::getLocalTime();
    const QDateTime now = SchedulerModuleState::getLocalTime().addDays(m_AltGraphDay), start, end;
    QDateTime nextDawn, nextDusk;
    SchedulerModuleState::calculateDawnDusk(now, nextDawn, nextDusk);
    QDateTime plotStart = (nextDusk < nextDawn) ? nextDusk : nextDusk.addDays(-1);

    // Normally start the plot 1 hour before dusk and end it an hour after dawn.
    int startOffset = 1;

    KStarsDateTime midnight = KStarsDateTime(now.date().addDays(1), QTime(0, 1), Qt::LocalTime);
    // Midnight not quite right if it's in the wee hours before dawn.
    // Then we use the midnight before now.
    if (now.secsTo(nextDawn) < now.secsTo(nextDusk) && now.date() == nextDawn.date())
        midnight = KStarsDateTime(now.date(), QTime(0, 1), Qt::LocalTime);
    else if (now < nextDusk.addSecs(-startOffset * 3600))
    {
        // It's in the (day)time between dawn and dusk. If there is a job scheduled to run
        // during this (day)time, then extend the startOffset to cover this run time.
        startOffset += additionalOffset(m_State->jobs(), now,
                                        plotStart.addSecs(-startOffset * 3600));
    }

    plotStart = plotStart.addSecs(-startOffset * 3600);
    auto plotEnd = nextDawn.addSecs(startOffset * 3600);

    const QString dayName = m_AltGraphDay == 1 ? i18n("Tomorrow") : (m_AltGraphDay == 2 ? i18n("Day After Tomorrow") :
                            i18n("Tonight"));
    const QString plotTitle = QString("%1 -- %2 -- %3")
                              .arg(midnight.addSecs(-6 * 3600).date().toString("MMM d"))
                              .arg(dayName)
                              .arg(midnight.date().toString("MMM d"));
    altGraphLabel->setText(plotTitle);

    const KStarsDateTime ut  = SchedulerModuleState::getGeo()->LTtoUT(KStarsDateTime(midnight));
    KSAlmanac ksal(ut, SchedulerModuleState::getGeo());

    handleButtons(false);

    const int currentPosition = m_State->currentPosition();

    for (int index = 0; index < m_State->jobs().size(); index++)
    {
        auto t = plotStart;
        QVector<double> times, alts;
        auto job = m_State->jobs().at(index);
        while (t.secsTo(plotEnd) > 0)
        {
            double alt = SchedulerUtils::findAltitude(job->getTargetCoords(), t);
            alts.push_back(alt);
            double hour = midnight.secsTo(t) / 3600.0;
            times.push_back(hour);
            t = t.addSecs(60 * 10);
        }

        const int lineWidth = (index == currentPosition) ? 2 : 1;
        if (index == 0)
            altGraph->plot(SchedulerModuleState::getGeo(), &ksal, times, alts, lineWidth, Qt::white, job->getName());
        else
            altGraph->plotOverlay(times, alts, lineWidth, Qt::white, job->getName());
    }

    altGraph->setCurrentLine(currentPosition);

    // The below isn't part of the above loop because we want the altitude plot objects created above
    // to have the same indexes as the jobs to simplify the connection between clicking a line
    // and knowing what job it is.
    //
    // Create additional plots overlaying the above, that are the intervals that the job is scheduled to run.
    for (int index = 0; index < m_State->jobs().size(); index++)
    {
        auto job = m_State->jobs().at(index);
        for (const auto &jobSchedule : job->getSimulatedSchedule())
        {
            auto startTime = jobSchedule.startTime;
            auto stopTime = jobSchedule.stopTime;
            if (!startTime.isValid())
                continue;
            if (startTime < plotStart)
                startTime = plotStart;
            if (!stopTime.isValid() || stopTime > plotEnd)
                stopTime = plotEnd;
            if (startTime.isValid() && startTime < plotEnd && stopTime.isValid() && stopTime > plotStart)
            {
                QVector<double> runTimes, runAlts;
                auto t = startTime;
                while (t.secsTo(stopTime) >= 0)
                {
                    double alt = SchedulerUtils::findAltitude(job->getTargetCoords(), t);
                    runAlts.push_back(alt);
                    double hour = midnight.secsTo(t) / 3600.0;
                    runTimes.push_back(hour);
                    int secsToStop = t.secsTo(stopTime);
                    if (secsToStop <= 0) break;
                    t = t.addSecs(std::min(60 * 1, secsToStop));
                }

                altGraph->plotOverlay(runTimes, runAlts, 4, Qt::green);
            }
        }
    }
}
}

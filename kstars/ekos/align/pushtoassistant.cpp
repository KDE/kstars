/*  Tool for Push-To support for manual mounts.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>, 2025

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pushtoassistant.h"
#include "kstarsdata.h"
#include "ekos/manager.h"
#include "skymapcomposite.h"

namespace Ekos
{

PushToAssistant::PushToAssistant(QWidget *parent) : QDialog(parent)

{
    setupUi(this);
    setWindowFlags(Qt::Window);
    // handle the solving button
    connect(toggleSolvingB, &QPushButton::clicked, this, &PushToAssistant::toggleSolving);
    // adapt the mount target widgets
    mountTarget->syncButtonObject->setText(i18n("Select Target"));
    mountTarget->syncButtonObject->setToolTip(i18n("Select the target that you are aiming for."));
    mountTarget->gotoButtonObject->setVisible(false);
    // forward target commands
    connect(mountTarget, &MountTargetWidget::sync, this, &PushToAssistant::sync);
    connect(mountTarget, &MountTargetWidget::newTarget, [this]()
    {
        enableSolving(true);
    });
    // repeat solving after given delay, always started after a completed plate solve
    solvingTimer.setSingleShot(true);
    solvingTimer.setInterval(refreshIntervalSB->value() * 1000);
    connect(loopSolvingB, &QPushButton::toggled, this, &PushToAssistant::repeatSolving);
    connect(&solvingTimer, &QTimer::timeout, [this]()
    {
        emit captureAndSolve(false);
    });
    connect(refreshIntervalSB, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [&](double value)
    {
        solvingTimer.setInterval(value * 1000);
    });

    initStatusLine();
    // initialize the plate solving button
    updateToggleSolvingB(true);
}

PushToAssistant *PushToAssistant::Instance()
{
    if (_PushToAssistant == nullptr)
        _PushToAssistant = new PushToAssistant(Manager::Instance());

    return _PushToAssistant;
}

void PushToAssistant::updateTelescopeCoords(const SkyPoint &position)
{
    // take the target from the current position if no target is set
    if (mountTarget->currentTarget() == nullptr && position.isValid())
        setTargetPosition(new SkyPoint(position), false);
}

void PushToAssistant::enableMountPosition(bool enable)
{
    mountTarget->setEnabled(enable);
    mountStatusLed->setColor(enable ? Qt::darkGreen : Qt::gray);
}

void PushToAssistant::enableSolving(bool enable)
{
    // show status
    alignStatusLed->setColor(enable ? Qt::darkGreen : Qt::gray);

    // force disabled if no valid target is set
    if (mountTarget->currentTarget() == nullptr || !mountTarget->currentTarget()->isValid())
        enable = false;

    mountTarget->syncButtonObject->setEnabled(enable);
    toggleSolvingB->setEnabled(enable);
    loopSolvingB->setEnabled(enable);
    refreshIntervalSB->setEnabled(enable);

    statusLED->setEnabled(enable);
}

void PushToAssistant::handleEkosStatus(CommunicationStatus status)
{
    switch (status)
    {
        case Ekos::Success:
            processEkosB->setIcon(QIcon::fromTheme("media-playback-stop"));
            processEkosB->setToolTip(i18n("Stop EKOS"));
            processEkosLabel->setText(processEkosB->toolTip());
            break;
        case Ekos::Idle:
        case Ekos::Error:
            processEkosB->setIcon(QIcon::fromTheme("media-playback-start"));
            processEkosB->setToolTip(i18n("Start EKOS"));
            processEkosLabel->setText(processEkosB->toolTip());
            // mount and alignment are not available
            enableMountPosition(false);
            enableSolving(false);
            break;
        default:
            // do nothing
            break;
    }
    // avoid double actions
    processEkosB->setEnabled(status != Ekos::Pending);
}

void PushToAssistant::setTargetPosition(SkyPoint *position, bool updateTargetName)
{
    mountTarget->setTargetPosition(position);

    if (updateTargetName)
    {
        // determine the object name for the given position
        double maxrad = 1.0;
        SkyObject *so = KStarsData::Instance()->skyComposite()->objectNearest(position, maxrad);

        if (so != nullptr)
            mountTarget->setTargetName(so->name());
    }
}

void PushToAssistant::updateSolution(const QVariantMap &solution)
{
    scopePosition = SkyPoint(solution["ra.Hours"].toDouble(), solution["de.Degrees"].toDouble());
    updateTargetError();
}

void PushToAssistant::setAlignState(AlignState state)
{
    statusLED->setAlignState(state);
    m_alignState = state;

    switch (state)
    {
        case ALIGN_IDLE:
        case ALIGN_FAILED:
        case ALIGN_SUCCESSFUL:
            updateToggleSolvingB(true);
            break;
        case ALIGN_COMPLETE:
            updateToggleSolvingB(true);
            if (loopSolvingB->isChecked())
                solvingTimer.start();
            break;
        case ALIGN_ABORTED:
            // disable looping
            loopSolvingB->setChecked(false);
            updateToggleSolvingB(true);
            break;
        case ALIGN_PROGRESS:
        case ALIGN_SYNCING:
        case ALIGN_SLEWING:
        case ALIGN_ROTATING:
        case ALIGN_SUSPENDED:
            // stop plate solving
            updateToggleSolvingB(false);
            break;
    }
}

PushToAssistant::~PushToAssistant()
{
    _PushToAssistant = nullptr;
}

void PushToAssistant::initStatusLine()
{
    alignStatusLed = new KLed(Qt::gray, KLed::On, KLed::Flat, KLed::Circular, this);
    mountStatusLed = new KLed(Qt::gray, KLed::On, KLed::Flat, KLed::Circular, this);
    statusLayout->insertWidget(3, alignStatusLed, 0, Qt::AlignVCenter);
    statusLayout->insertWidget(5, mountStatusLed, 0, Qt::AlignVCenter);
    // listen to Ekos status changes
    connect(Ekos::Manager::Instance(), &Manager::ekosStatusChanged, this, &PushToAssistant::handleEkosStatus);
    // handle start/stop of EKOS from the status line
    connect(processEkosB, &QPushButton::clicked, Manager::Instance(), &Manager::processINDI);
    // enable starting EKOS
    handleEkosStatus(Ekos::Idle);
}

void PushToAssistant::updateTargetError()
{
    // do nothing unless we know the target and the scope position
    if (mountTarget->currentTarget() == nullptr || !mountTarget->currentTarget()->isValid() || !scopePosition.isValid())
        return;

    // update the alt/az positions
    auto lst = KStarsData::Instance()->lst(); // use the same timestamp
    mountTarget->currentTarget()->EquatorialToHorizontal(lst, KStarsData::Instance()->geo()->lat());
    scopePosition.EquatorialToHorizontal(lst, KStarsData::Instance()->geo()->lat());

    // determine the distance between current and target position
    const dms altError = scopePosition.alt().deltaAngle(mountTarget->currentTarget()->alt());
    const dms azError  = scopePosition.az().deltaAngle(mountTarget->currentTarget()->az());

    // show it in the alignment graph
    targetPlot->displayAlignment(altError.Degrees(), azError.Degrees());
}

void PushToAssistant::toggleSolving()
{
    switch (m_alignState)
    {
        case ALIGN_IDLE:
        case ALIGN_COMPLETE:
        case ALIGN_FAILED:
        case ALIGN_ABORTED:
        case ALIGN_SUCCESSFUL:
            // start plate solving
            emit captureAndSolve(true);
            updateToggleSolvingB(false);
            break;
        case ALIGN_PROGRESS:
        case ALIGN_SYNCING:
        case ALIGN_SLEWING:
        case ALIGN_ROTATING:
        case ALIGN_SUSPENDED:
            // stop plate solving
            emit abort();
            updateToggleSolvingB(true);
            break;
    }
}

void PushToAssistant::repeatSolving(bool repeat)
{
    if (repeat)
    {
        solvingTimer.start(refreshIntervalSB->value() * 1000);
        // start solving if not already running
        if(m_alignState == AlignState::ALIGN_IDLE || m_alignState == AlignState::ALIGN_COMPLETE)
        {
            updateToggleSolvingB(true);
            emit captureAndSolve(false);
        }
    }
    else
        solvingTimer.stop();
}

void PushToAssistant::updateToggleSolvingB(bool platesolve)
{
    if (platesolve)
    {
        toggleSolvingB->setText(i18n("Solve Position"));
        toggleSolvingB->setToolTip(i18n("Start plate solving the position you are currently aiming at."));
    }
    else
    {
        toggleSolvingB->setText(i18n("Abort"));
        toggleSolvingB->setToolTip(i18n("Stop the current plate solving run."));
    }
}
} // namespace

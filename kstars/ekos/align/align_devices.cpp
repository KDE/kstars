/*
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2018-2020 Robert Lancaster <rlancaste@gmail.com>
    SPDX-FileCopyrightText: 2019-2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "align.h"
#include "align_p.h"
#include "polaralignmentassistant.h"
#include "remoteastrometryparser.h"
#include "Options.h"

#include "ekos/auxiliary/filtermanager.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "ekos/manager.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/indifilterwheel.h"
#include "indi/indirotator.h"
#include "ksnotification.h"

#include <ekos_align_debug.h>

namespace Ekos
{

using PAA = PolarAlignmentAssistant;

bool Align::isParserOK()
{
    return true; //For now
}

void Align::checkRemoteAlignmentTimeout()
{
    if (m_SolveFromFile || ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
        abort();
    else
    {
        appendLogText(i18n("Remote solver timed out."));
        parser->stopSolver();

        setAlignTableResult(ALIGN_RESULT_FAILED);
        captureAndSolve(false);
    }
    // TODO must also account for loadAndSlew. Retain file name
}

void Align::setSolverMode(int mode)
{
    if (sender() == nullptr && mode >= 0 && mode <= 1)
    {
        solverModeButtonGroup->button(mode)->setChecked(true);
    }

    Options::setSolverMode(mode);

    if (mode == SOLVER_REMOTE)
    {
        if (remoteParser.get() != nullptr && m_RemoteParserDevice != nullptr)
        {
            parser = remoteParser.get();
            (dynamic_cast<RemoteAstrometryParser *>(parser))->setAstrometryDevice(m_RemoteParserDevice);
            return;
        }

        remoteParser.reset(new Ekos::RemoteAstrometryParser());
        parser = remoteParser.get();
        (dynamic_cast<RemoteAstrometryParser *>(parser))->setAstrometryDevice(m_RemoteParserDevice);
        if (m_Camera)
            (dynamic_cast<RemoteAstrometryParser *>(parser))->setCCD(m_Camera->getDeviceName());

        parser->setAlign(this);
        if (parser->init())
        {
            connect(parser, &AstrometryParser::solverFinished, this, &Ekos::Align::solverFinished, Qt::UniqueConnection);
            connect(parser, &AstrometryParser::solverFailed, this, &Ekos::Align::solverFailed, Qt::UniqueConnection);
        }
        else
            parser->disconnect();
    }
}

QString Align::camera()
{
    if (m_Camera)
        return m_Camera->getDeviceName();

    return QString();
}

void Align::checkCamera()
{
    if (!m_Camera)
        return;

    // Do NOT perform checks if align is in progress as this may result
    // in signals/slots getting disconnected.
    switch (state)
    {
        // Idle, camera change is OK.
        case ALIGN_IDLE:
        case ALIGN_COMPLETE:
        case ALIGN_FAILED:
        case ALIGN_ABORTED:
        case ALIGN_SUCCESSFUL:
            break;

        // Busy, camera change is not OK.
        case ALIGN_PROGRESS:
        case ALIGN_SYNCING:
        case ALIGN_SLEWING:
        case ALIGN_SUSPENDED:
        case ALIGN_ROTATING:
            return;
    }

    auto targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    if (targetChip == nullptr || (targetChip && targetChip->isCapturing()))
        return;

    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
        (dynamic_cast<RemoteAstrometryParser *>(remoteParser.get()))->setCCD(m_Camera->getDeviceName());

    syncCameraInfo();
    syncCameraControls();
    syncTelescopeInfo();
}

bool Align::setCamera(ISD::Camera *device)
{
    if (m_Camera && m_Camera == device)
    {
        checkCamera();
        return false;
    }

    if (m_Camera)
        m_Camera->disconnect(this);

    m_Camera = device;

    if (m_Camera)
    {
        connect(m_Camera, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
            controlBox->setEnabled(isConnected);
            gotoBox->setEnabled(isConnected);
            plateSolverOptionsGroup->setEnabled(isConnected);
            tabWidget->setEnabled(isConnected);
        });
        connect(m_Camera, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
            controlBox->setEnabled(isConnected);
            gotoBox->setEnabled(isConnected);
            plateSolverOptionsGroup->setEnabled(isConnected);
            tabWidget->setEnabled(isConnected);

            opticalTrainCombo->setEnabled(true);
            trainLabel->setEnabled(true);
        });
    }

    auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
    controlBox->setEnabled(isConnected);
    gotoBox->setEnabled(isConnected);
    plateSolverOptionsGroup->setEnabled(isConnected);
    tabWidget->setEnabled(isConnected);

    if (!m_Camera)
        return false;

    checkCamera();

    return true;
}

bool Align::setMount(ISD::Mount *device)
{
    if (m_Mount && m_Mount == device)
    {
        syncTelescopeInfo();
        return false;
    }

    if (m_Mount)
        m_Mount->disconnect(this);

    m_Mount = device;

    if (m_Mount)
    {
        connect(m_Mount, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
            controlBox->setEnabled(isConnected);
            gotoBox->setEnabled(isConnected);
            plateSolverOptionsGroup->setEnabled(isConnected);
            tabWidget->setEnabled(isConnected);
        });
        connect(m_Mount, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
            controlBox->setEnabled(isConnected);
            gotoBox->setEnabled(isConnected);
            plateSolverOptionsGroup->setEnabled(isConnected);
            tabWidget->setEnabled(isConnected);

            opticalTrainCombo->setEnabled(true);
            trainLabel->setEnabled(true);
        });
    }

    auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
    controlBox->setEnabled(isConnected);
    gotoBox->setEnabled(isConnected);
    plateSolverOptionsGroup->setEnabled(isConnected);
    tabWidget->setEnabled(isConnected);

    if (!m_Mount)
        return false;

    RUN_PAH(setCurrentTelescope(m_Mount));

    connect(m_Mount, &ISD::Mount::propertyUpdated, this, &Ekos::Align::updateProperty, Qt::UniqueConnection);
    connect(m_Mount, &ISD::Mount::Disconnected, this, [this]()
    {
        m_isRateSynced = false;
    });

    syncTelescopeInfo();
    return true;
}

bool Align::setDome(ISD::Dome *device)
{
    if (m_Dome && m_Dome == device)
        return false;

    if (m_Dome)
        m_Dome->disconnect(this);

    m_Dome = device;

    if (!m_Dome)
        return false;

    connect(m_Dome, &ISD::Dome::propertyUpdated, this, &Ekos::Align::updateProperty, Qt::UniqueConnection);
    return true;
}

bool Align::setPAC(ISD::PAC *device)
{
    RUN_PAH(setCurrentPAC(device));
    return true;
}

bool Align::setDustCap(ISD::DustCap *device)
{
    if (m_DustCap && m_DustCap == device)
    {
        return false;
    }

    if (m_DustCap)
    {
        disconnect(m_DustCap, &ISD::DustCap::newStatus, this, &Ekos::Align::onDustCapStatusChanged);
    }

    m_DustCap = device;

    if (m_DustCap)
    {
        connect(m_DustCap, &ISD::DustCap::newStatus, this, &Ekos::Align::onDustCapStatusChanged, Qt::UniqueConnection);
    }
    return true;
}

void Align::onDustCapStatusChanged(ISD::DustCap::Status status)
{
    if (m_waitingForDustCapUnpark && status == ISD::DustCap::CAP_IDLE)
    {
        appendLogText(i18n("Dustcap unparked. Resuming capture and solve."));
        m_waitingForDustCapUnpark = false;
        captureAndSolve(false); // Resume capture and solve
    }
    else if (status == ISD::DustCap::CAP_ERROR)
    {
        appendLogText(i18n("Dustcap error detected. Aborting alignment."));
        m_waitingForDustCapUnpark = false;
        stop(ALIGN_ABORTED); // Abort alignment
    }
}

void Align::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    auto name = device->getDeviceName();
    device->disconnect(this);

    // Check mounts
    if (m_Mount && m_Mount->getDeviceName() == name)
    {
        m_Mount->disconnect(this);
        m_Mount = nullptr;
    }

    // Check domes
    if (m_Dome && m_Dome->getDeviceName() == name)
    {
        m_Dome->disconnect(this);
        m_Dome = nullptr;
    }

    // Check rotators
    if (m_Rotator && m_Rotator->getDeviceName() == name)
    {
        m_Rotator->disconnect(this);
        m_Rotator = nullptr;
    }

    // Check cameras
    if (m_Camera && m_Camera->getDeviceName() == name)
    {
        m_Camera->disconnect(this);
        m_Camera = nullptr;

        QTimer::singleShot(1000, this, &Align::checkCamera);
    }

    // Check Remote Astrometry
    if (m_RemoteParserDevice && m_RemoteParserDevice->getDeviceName() == name)
    {
        m_RemoteParserDevice.clear();
    }

    // Check Filter Wheels
    if (m_FilterWheel && m_FilterWheel->getDeviceName() == name)
    {
        m_FilterWheel->disconnect(this);
        m_FilterWheel = nullptr;

        QTimer::singleShot(1000, this, &Align::checkFilter);
    }

    // Check DustCap
    if (m_DustCap && m_DustCap->getDeviceName() == name)
    {
        disconnect(m_DustCap, &ISD::DustCap::newStatus, this, &Ekos::Align::onDustCapStatusChanged);
        m_DustCap = nullptr;
        appendLogText(i18n("Dustcap device %1 removed.", name));
    }
}

bool Align::syncTelescopeInfo()
{
    if (m_Mount == nullptr || m_Mount->isConnected() == false)
        return false;

    if (m_isRateSynced == false)
    {
        auto speed = m_Settings["pAHMountSpeed"];
        auto slewRates = m_Mount->slewRates();
        if (speed.isValid())
        {
            RUN_PAH(syncMountSpeed(speed.toString()));
        }
        else if (!slewRates.isEmpty())
        {
            RUN_PAH(syncMountSpeed(slewRates.last()));
        }

        m_isRateSynced = !slewRates.empty();
    }

    canSync = m_Mount->canSync();

    if (canSync == false && syncR->isEnabled())
    {
        slewR->setChecked(true);
        appendLogText(i18n("Mount does not support syncing."));
    }

    syncR->setEnabled(canSync);

    if (m_FocalLength == -1 || m_Aperture == -1)
        return false;

    if (m_CameraPixelWidth != -1 && m_CameraPixelHeight != -1)
    {
        calculateFOV();
        return true;
    }

    return false;
}

void Align::syncCameraInfo()
{
    if (!m_Camera)
        return;

    auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
    Q_ASSERT(targetChip);

    // Get Maximum resolution and pixel size
    uint8_t bit_depth = 8;
    targetChip->getImageInfo(m_CameraWidth, m_CameraHeight, m_CameraPixelWidth, m_CameraPixelHeight, bit_depth);

    setWCSEnabled(Options::astrometrySolverWCS());

    int binx = 1, biny = 1;
    alignBinning->setEnabled(targetChip->canBin());
    if (targetChip->canBin())
    {
        alignBinning->blockSignals(true);

        targetChip->getMaxBin(&binx, &biny);
        alignBinning->clear();

        for (int i = 0; i < binx; i++)
            alignBinning->addItem(QString("%1x%2").arg(i + 1).arg(i + 1));

        auto binning = m_Settings["alignBinning"];
        if (binning.isValid())
            alignBinning->setCurrentText(binning.toString());

        alignBinning->blockSignals(false);
    }

    // In case ROI is different (smaller) than maximum resolution, let's use that.
    // N.B. 2022.08.14 JM: We must account for binning since this value is used for FOV calculations.
    int roiW = 0, roiH = 0;
    targetChip->getFrameMinMax(nullptr, nullptr, nullptr, nullptr, nullptr, &roiW, nullptr, &roiH);
    roiW *= binx;
    roiH *= biny;
    if ( (roiW > 0 && roiW < m_CameraWidth) || (roiH > 0 && roiH < m_CameraHeight))
    {
        m_CameraWidth = roiW;
        m_CameraHeight = roiH;
    }

    if (m_CameraPixelWidth > 0 && m_CameraPixelHeight > 0 && m_FocalLength > 0 && m_Aperture > 0)
    {
        calculateFOV();
    }
}

void Align::syncCameraControls()
{
    if (m_Camera == nullptr)
        return;

    auto targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    if (targetChip == nullptr || (targetChip && targetChip->isCapturing()))
        return;

    auto isoList = targetChip->getISOList();
    alignISO->clear();

    if (isoList.isEmpty())
    {
        alignISO->setEnabled(false);
    }
    else
    {
        alignISO->setEnabled(true);
        alignISO->addItems(isoList);
        alignISO->setCurrentIndex(targetChip->getISOIndex());
    }

    // Gain Check
    if (m_Camera->hasGain())
    {
        double min, max, step, value;
        m_Camera->getGainMinMaxStep(&min, &max, &step);

        // Allow the possibility of no gain value at all.
        alignGainSpecialValue = min - step;
        alignGain->setRange(alignGainSpecialValue, max);
        alignGain->setSpecialValueText(i18n("--"));
        alignGain->setEnabled(true);
        alignGain->setSingleStep(step);
        m_Camera->getGain(&value);

        auto gain = m_Settings["alignGain"];
        // Set the custom gain if we have one
        // otherwise it will not have an effect.
        if (gain.isValid())
            TargetCustomGainValue = gain.toDouble();
        if (TargetCustomGainValue > 0)
            alignGain->setValue(TargetCustomGainValue);
        else
            alignGain->setValue(alignGainSpecialValue);

        alignGain->setReadOnly(m_Camera->getGainPermission() == IP_RO);

        connect(alignGain, &QDoubleSpinBox::editingFinished, this, [this]()
        {
            if (alignGain->value() > alignGainSpecialValue)
                TargetCustomGainValue = alignGain->value();
        });
    }
    else
        alignGain->setEnabled(false);
}

bool Align::setFilterWheel(ISD::FilterWheel * device)
{
    if (m_FilterWheel && m_FilterWheel == device)
    {
        checkFilter();
        return false;
    }

    if (m_FilterWheel)
        m_FilterWheel->disconnect(this);

    m_FilterWheel = device;

    if (m_FilterWheel)
    {
        connect(m_FilterWheel, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            FilterPosLabel->setEnabled(true);
            alignFilter->setEnabled(true);
        });
        connect(m_FilterWheel, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            FilterPosLabel->setEnabled(false);
            alignFilter->setEnabled(false);
        });
    }

    auto isConnected = m_FilterWheel && m_FilterWheel->isConnected();
    FilterPosLabel->setEnabled(isConnected);
    alignFilter->setEnabled(isConnected);

    checkFilter();
    return true;
}

QString Align::filterWheel()
{
    if (m_FilterWheel)
        return m_FilterWheel->getDeviceName();

    return QString();
}

bool Align::setFilter(const QString &filter)
{
    if (m_FilterWheel)
    {
        alignFilter->setCurrentText(filter);
        return true;
    }

    return false;
}

QString Align::filter()
{
    return alignFilter->currentText();
}

void Align::checkFilter()
{
    alignFilter->clear();

    if (!m_FilterWheel)
    {
        FilterPosLabel->setEnabled(false);
        alignFilter->setEnabled(false);
        return;
    }

    auto isConnected = m_FilterWheel->isConnected();
    FilterPosLabel->setEnabled(isConnected);
    alignFilter->setEnabled(alignUseCurrentFilter->isChecked() == false);

    setupFilterManager();

    alignFilter->addItems(m_FilterManager->getFilterLabels());
    currentFilterPosition = m_FilterManager->getFilterPosition();

    if (alignUseCurrentFilter->isChecked())
    {
        // use currently selected filter
        alignFilter->setCurrentIndex(currentFilterPosition - 1);
    }
    else
    {
        // use the fixed filter
        auto filter = m_Settings["alignFilter"];
        if (filter.isValid())
            alignFilter->setCurrentText(filter.toString());
    }
}

void Align::setRotator(ISD::Rotator * Device)
{
    if ((Manager::Instance()->existRotatorController()) && (!m_Rotator || !(Device == m_Rotator)))
    {
        rotatorB->setEnabled(false);
        if (m_Rotator)
        {
            m_Rotator->disconnect(this);
            m_RotatorControlPanel->close();
        }
        m_Rotator = Device;
        if (m_Rotator)
        {
            if (Manager::Instance()->getRotatorController(m_Rotator->getDeviceName(), m_RotatorControlPanel))
            {
                connect(m_Rotator, &ISD::Rotator::propertyUpdated, this, &Ekos::Align::updateProperty, Qt::UniqueConnection);
                connect(rotatorB, &QPushButton::clicked, this, [this]()
                {
                    m_RotatorControlPanel->show();
                    m_RotatorControlPanel->raise();
                });
                rotatorB->setEnabled(true);
            }
        }
    }
}

void Align::setWCSEnabled(bool enable)
{
    if (!m_Camera)
        return;

    auto wcsControl = m_Camera->getSwitch("WCS_CONTROL");

    if (!wcsControl)
        return;

    auto wcs_enable  = wcsControl.findWidgetByName("WCS_ENABLE");
    auto wcs_disable = wcsControl.findWidgetByName("WCS_DISABLE");

    if (!wcs_enable || !wcs_disable)
        return;

    if ((wcs_enable->getState() == ISS_ON && enable) || (wcs_disable->getState() == ISS_ON && !enable))
        return;

    wcsControl.reset();
    if (enable)
    {
        appendLogText(i18n("World Coordinate System (WCS) is enabled."));
        wcs_enable->setState(ISS_ON);
    }
    else
    {
        appendLogText(i18n("%1: World Coordinate System (WCS) is disabled.", Align::camera()));
        wcs_disable->setState(ISS_ON);
        m_wcsSynced    = false;
    }

    auto clientManager = m_Camera->getDriverInfo()->getClientManager();
    if (clientManager)
        clientManager->sendNewProperty(wcsControl);
}

void Align::checkCameraExposureProgress(ISD::CameraChip * targetChip, double remaining, IPState state)
{
    INDI_UNUSED(targetChip);
    INDI_UNUSED(remaining);

    if (state == IPS_ALERT)
    {
        if (++m_CaptureErrorCounter == 3 && !matchPAHStage(PolarAlignmentAssistant::PAH_REFRESH))
        {
            appendLogText(i18n("Capture error. Aborting..."));
            abort();
            return;
        }

        appendLogText(i18n("Restarting capture attempt #%1", m_CaptureErrorCounter));
        setAlignTableResult(ALIGN_RESULT_FAILED);
        captureAndSolve(false);
    }
}

void Align::setExposure(double value)
{
    alignExposure->setValue(value);
}

void Align::setBinningIndex(int binIndex)
{
    // If sender is not our combo box, then we need to update the combobox itself
    if (dynamic_cast<QComboBox *>(sender()) != alignBinning)
    {
        alignBinning->blockSignals(true);
        alignBinning->setCurrentIndex(binIndex);
        alignBinning->blockSignals(false);
    }

    // Need to calculate FOV and args for APP
    if (Options::astrometryImageScaleUnits() == SSolver::ARCSEC_PER_PIX)
        calculateFOV();
}

void Align::setFocusStatus(Ekos::FocusState state)
{
    m_FocusState = state;
}

void Align::setCaptureStatus(CaptureState newState)
{
    switch (newState)
    {
        case CAPTURE_ALIGNING:
            if (m_Mount && m_Mount->hasAlignmentModel() && Options::resetMountModelAfterMeridian())
            {
                qCDebug(KSTARS_EKOS_ALIGN) << "Post meridian flip mount model reset" << (m_Mount->clearAlignmentModel() ?
                                           "successful." : "failed.");
            }
            if (alignSettlingTime->value() >= DELAY_THRESHOLD_NOTIFY)
                appendLogText(i18n("Settling..."));
            m_resetCaptureTimeoutCounter = true; // Enable rotator time frame estimate in 'captureandsolve()'
            m_CaptureTimer.start(alignSettlingTime->value());
            break;
        // Is this needed anymore with new flip policy? (sb 2023-10-20)
        // On meridian flip, reset Target Position Angle to fully rotated value
        // expected after MF so that we do not end up with reversed camera rotation
        case CAPTURE_MERIDIAN_FLIP:
            if (std::isnan(m_TargetPositionAngle) == false)
                m_TargetPositionAngle = KSUtils::rangePA(m_TargetPositionAngle + 180.0);
            break;
        default:
            break;
    }

    m_CaptureState = newState;
}

void Align::setAstrometryDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    m_RemoteParserDevice = device;

    remoteSolverR->setEnabled(true);
    if (remoteParser.get() != nullptr)
    {
        remoteParser->setAstrometryDevice(m_RemoteParserDevice);
        connect(remoteParser.get(), &AstrometryParser::solverFinished, this, &Ekos::Align::solverFinished, Qt::UniqueConnection);
        connect(remoteParser.get(), &AstrometryParser::solverFailed, this, &Ekos::Align::solverFailed, Qt::UniqueConnection);
    }
}

void Align::setMountStatus(ISD::Mount::Status newState)
{
    switch (newState)
    {
        case ISD::Mount::MOUNT_PARKED:
            solveB->setEnabled(false);
            loadSlewB->setEnabled(false);
            break;
        case ISD::Mount::MOUNT_IDLE:
            solveB->setEnabled(true);
            loadSlewB->setEnabled(true);
            break;
        case ISD::Mount::MOUNT_PARKING:
            solveB->setEnabled(false);
            loadSlewB->setEnabled(false);
            break;
        case ISD::Mount::MOUNT_SLEWING:
        case ISD::Mount::MOUNT_MOVING:
            solveB->setEnabled(false);
            loadSlewB->setEnabled(false);
            break;

        default:
            if (state != ALIGN_PROGRESS)
            {
                solveB->setEnabled(true);
                if (matchPAHStage(PAA::PAH_IDLE))
                {
                    loadSlewB->setEnabled(true);
                }
            }
            break;
    }

    RUN_PAH(setMountStatus(newState));
}

}

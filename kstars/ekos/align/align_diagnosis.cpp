/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "align.h"
#include "align_p.h"
#include "indexfiles.h"
#include "opsastrometryindexfiles.h"
#include "Options.h"

#include "ekos/auxiliary/diagnosismanager.h"
#include "ekos/manager.h"
#include "ekos/mount/mount.h"
#include "fitsviewer/fitsdata.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "kstarsdata.h"

#include <ekos_align_debug.h>

#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>

#include <memory>

namespace Ekos
{

using PAA = PolarAlignmentAssistant;
using ApplyResult = DiagnosisManager::ApplyResult;
using Outcome = DiagnosisManager::Outcome;

namespace
{
const QString MODULE = QStringLiteral("align");

bool isSolveReason(AlignFailReason reason)
{
    return reason == AlignFailReason::SolverFailed || reason == AlignFailReason::SolverTimeout ||
           reason == AlignFailReason::PahSolverFailed || reason == AlignFailReason::FovOutOfBounds;
}

QString downloadErrorText(int error)
{
    switch (error)
    {
        case IndexFiles::NetworkError:
            return i18n("The download failed.");
        case IndexFiles::WriteError:
            return i18n("The index file could not be written.");
        case IndexFiles::PermissionError:
            return i18n("The index file folder is not writable.");
        case IndexFiles::Timeout:
            return i18n("The download timed out.");
        case IndexFiles::Cancelled:
            return i18n("The download was cancelled.");
        case IndexFiles::ServerUnavailable:
            return i18n("The index file server could not be reached.");
        case IndexFiles::NoWritableDirectory:
            return i18n("No writable index file folder was found.");
        default:
            return QString();
    }
}

ApplyResult failed(const QString &message)
{
    ApplyResult result;
    result.outcome = Outcome::Failed;
    result.message = message;
    return result;
}

ApplyResult done(const QString &message = QString())
{
    ApplyResult result;
    result.outcome = Outcome::Done;
    result.message = message;
    return result;
}

/**
 * Connect a device and finish the item when it reports connected.
 */
ApplyResult connectDevice(DiagnosisManager *manager, ISD::ConcreteDevice *device, const Diagnosis::Report &report,
                          const Diagnosis::Item &item)
{
    if (device == nullptr)
        return failed(i18n("The device is not available."));
    if (device->isConnected())
        return done();

    QPointer<DiagnosisManager> guard(manager);
    const QString diagnosisId = report.id;
    const QString itemId = item.id;
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(device, &ISD::ConcreteDevice::Connected, manager, [guard, diagnosisId, itemId,
                                   connection]()
    {
        QObject::disconnect(*connection);
        if (guard)
            guard->finishItem(diagnosisId, itemId, true);
    }, Qt::QueuedConnection);

    device->Connect();

    ApplyResult result;
    result.outcome = Outcome::Running;
    result.timeoutMs = 30000;
    return result;
}
}

DiagnosisManager *Align::diagnosisManager() const
{
    return m_DiagnosisManager.data();
}

bool Align::failCaptureAndSolve(AlignFailReason reason)
{
    m_CaptureTimer.stop();
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);
    loadSlewB->setEnabled(true);
    m_SolveFromFile = false;

    setState(ALIGN_FAILED);
    Q_EMIT newStatus(state);

    recordFailure(reason);

    // Reset after the diagnosis read them
    m_CaptureErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;
    m_SlewErrorCounter = 0;
    resetBlindSolving();
    return false;
}

void Align::recordFailure(AlignFailReason reason, const std::optional<AlignFacts> &facts)
{
    m_FailReason = reason;
    qCInfo(KSTARS_EKOS_ALIGN) << "Align failure reason:" << alignFailReasonKey(reason);

    if (!AlignDiagnosis::isDiagnosable(reason) || !m_DiagnosisManager)
        return;

    // Measure the failed frame first, the rules use its stars and their shapes
    if (!facts && isSolveReason(reason) && solverModeButtonGroup->checkedId() == SOLVER_LOCAL && m_ImageData)
    {
        measureFailedFrame(reason);
        return;
    }

    submitDiagnosis(facts ? *facts : collectDiagnosisFacts());
}

void Align::measureFailedFrame(AlignFailReason reason)
{
    // The analysis takes about 0.06 s on a large frame: run it in the background, then submit the diagnosis.
    // The frame is shared with the worker, so it stays valid if a new one is captured meanwhile.
    auto data = m_ImageData;
    auto watcher = new QFutureWatcher<ImageStats>(this);
    connect(watcher, &QFutureWatcher<ImageStats>::finished, this, [this, watcher, reason]()
    {
        watcher->deleteLater();
        // A new alignment started while the frame was measured
        if (m_FailReason != reason || state != ALIGN_FAILED)
            return;

        const ImageStats stats = watcher->result();
        if (stats.verdict != ImageVerdict::Unknown || stats.measuredStars > 0)
        {
            m_FailedImageStats = stats;
            qCInfo(KSTARS_EKOS_ALIGN) << "Failed frame:" << imageVerdictKey(stats.verdict)
                                      << "stars" << stats.stars << "profiled" << stats.profiledStars
                                      << "HFR" << stats.hfr << "concentration" << stats.concentration
                                      << "centre dip" << stats.centreDip
                                      << "elongation" << stats.elongation << "coherence" << stats.coherence
                                      << "trail" << stats.trailLength
                                      << "double" << stats.doubleOffset << stats.doubleFraction;
        }
        else
            qCInfo(KSTARS_EKOS_ALIGN) << "Failed frame could not be measured.";

        submitDiagnosis(collectDiagnosisFacts());
    });
    watcher->setFuture(QtConcurrent::run([data]()
    {
        return ImageAnalysis::analyse(*data);
    }));
}

void Align::submitDiagnosis(const AlignFacts &facts)
{
    if (!m_DiagnosisManager)
        return;

    m_LastDiagnosisFacts = facts;
    auto report = AlignDiagnosis::diagnose(facts, m_DiagnosisManager->tried(MODULE, facts.train),
                                           m_DiagnosisManager->dismissed(MODULE));
    report.status = getAlignStatusString(static_cast<AlignState>(facts.state), false);
    m_DiagnosisManager->submit(report);
}

AlignFacts Align::collectDiagnosisFacts() const
{
    AlignFacts facts;
    facts.state = state;
    facts.reason = m_FailReason;
    facts.train = opticalTrainCombo->currentText();

    // Devices
    facts.hasCamera = m_Camera != nullptr;
    if (m_Camera)
    {
        facts.cameraConnected = m_Camera->isConnected();
        facts.cameraName = m_Camera->getDeviceName();
        auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        if (targetChip)
            facts.canBin = targetChip->canBin();
    }

    facts.hasMount = m_Mount != nullptr;
    facts.mountCoordsValid = m_TelescopeCoord.isValid();
    if (m_Mount)
    {
        facts.mountConnected = m_Mount->isConnected();
        facts.mountParked = m_Mount->isParked();
        facts.canSync = m_Mount->canSync();

        QDateTime reported, received;
        if (m_Mount->genericDevice() && m_Mount->genericDevice()->reportedTime(reported, received))
        {
            facts.mountUtc = reported;
            facts.mountUtcReceived = received;
        }
    }
    facts.gotoMode = m_CurrentGotoMode;
    facts.hasFilterWheel = m_FilterWheel != nullptr;
    facts.filterWheelConnected = m_FilterWheel && m_FilterWheel->isConnected();
    facts.hasRotator = m_Rotator != nullptr;

    // Counters
    facts.captureErrors = m_CaptureErrorCounter;
    facts.captureTimeouts = m_CaptureTimeoutCounter;
    facts.driverRestarts = m_DeviceRestartCounter;
    facts.slewErrors = m_SlewErrorCounter;

    // Optics and camera
    facts.focalLength = m_FocalLength;
    facts.aperture = m_Aperture;
    facts.reducer = m_Reducer;
    facts.pixelW = m_CameraPixelWidth;
    facts.pixelH = m_CameraPixelHeight;
    facts.width = m_CameraWidth;
    facts.height = m_CameraHeight;
    facts.fovW = m_FOVWidth;
    facts.fovH = m_FOVHeight;
    facts.exposure = alignExposure->value();
    facts.gain = alignGain->isEnabled() ? alignGain->value() : -1;
    facts.binIndex = alignBinning->currentIndex();
    facts.binCount = alignBinning->count();

    // Solver
    facts.localSolver = solverModeButtonGroup->checkedId() == SOLVER_LOCAL;
    facts.solverType = static_cast<int>(Options::solverType());
    if (facts.localSolver)
    {
        if (Options::solverType() == SSolver::SOLVER_ASTAP)
            facts.externalSolverMissing = !QFileInfo::exists(Options::aSTAPExecutable());
        else if (Options::solverType() == SSolver::SOLVER_WATNEYASTROMETRY)
            facts.externalSolverMissing = !QFileInfo::exists(Options::watneyBinary());
    }
    facts.profile = selectedSolverProfile().listName;
    facts.useScale = Options::astrometryUseImageScale();
    facts.usePosition = Options::astrometryUsePosition();
    facts.timeoutSec = Options::astrometryTimeout();
    facts.attempts = m_Attempts;
    facts.image = m_FailedImageStats;

    // Index files, only for the solvers that read them
    facts.indexSolver = facts.localSolver && (Options::solverType() == SSolver::SOLVER_STELLARSOLVER ||
                        Options::solverType() == SSolver::SOLVER_LOCALASTROMETRY);
    if (facts.indexSolver && m_FOVWidth > 0 && m_FOVHeight > 0)
    {
        facts.requiredSizes = IndexFiles::requiredIndexSizes(std::max(m_FOVWidth, m_FOVHeight));
        facts.installedSizes = IndexFiles::installedIndexSizes(
                                   IndexFiles::installedIndexSeries(Options::astrometryIndexFolderList()));
    }

    // Mount limits and clock
    if (auto mountModule = Manager::Instance()->mountModule())
    {
        facts.altLimitsEnabled = mountModule->altitudeLimitsEnabled();
        const auto limits = mountModule->altitudeLimits();
        if (limits.size() == 2)
        {
            facts.altMin = limits[0];
            facts.altMax = limits[1];
        }
    }
    if (m_TargetCoord.ra().degree() >= 0)
    {
        SkyPoint target = m_TargetCoord;
        target.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
        facts.targetAlt = target.alt().Degrees();
        facts.declination = m_TargetCoord.dec().Degrees();
    }
    else if (m_TelescopeCoord.isValid())
        facts.declination = m_TelescopeCoord.dec().Degrees();
    if (m_DiagnosisManager)
        facts.clientUtc = m_DiagnosisManager->clientUtc();
    facts.systemUtc = QDateTime::currentDateTimeUtc();
    facts.kstarsUtc = KStarsData::Instance()->ut();
    facts.timeUpdate = Options::useTimeUpdate();
    facts.timeSource = Options::timeSource();

    facts.rotatorThresholdArcmin = Options::astrometryRotatorThreshold();

    return facts;
}

void Align::registerDiagnosis(DiagnosisManager *manager)
{
    m_DiagnosisManager = manager;
    if (!manager)
        return;

    QPointer<Align> guard(this);

    DiagnosisManager::ModuleHooks hooks;
    hooks.name = i18n("Align");
    hooks.retry = [guard](const QString &)
    {
        return guard && guard->captureAndSolve();
    };
    hooks.log = [guard](const QString & text)
    {
        if (guard)
            guard->appendLogText(text);
    };
    hooks.refresh = [guard](const Diagnosis::Report & report) -> std::optional<Diagnosis::Report>
    {
        if (!guard || !guard->m_DiagnosisManager)
            return std::nullopt;
        // A check-up looks at the setup as it is now
        if (Diagnosis::isCheckup(report))
        {
            auto facts = guard->collectDiagnosisFacts();
            if (facts.train != report.train)
                return std::nullopt;
            auto rebuilt = AlignDiagnosis::checkup(facts, guard->m_DiagnosisManager->dismissed(MODULE));
            rebuilt.status = report.status;
            return rebuilt;
        }
        auto facts = guard->m_LastDiagnosisFacts;
        if (facts.train != report.train || static_cast<int>(facts.reason) != report.reason)
            return std::nullopt;
        facts.clientUtc = guard->m_DiagnosisManager->clientUtc();
        facts.systemUtc = QDateTime::currentDateTimeUtc();
        facts.kstarsUtc = KStarsData::Instance()->ut();
        auto rebuilt = AlignDiagnosis::diagnose(facts, report.tried, guard->m_DiagnosisManager->dismissed(MODULE));
        rebuilt.status = report.status;
        return rebuilt;
    };
    hooks.checkup = [guard](const QString & train) -> std::optional<Diagnosis::Report>
    {
        if (!guard || !guard->m_DiagnosisManager)
            return std::nullopt;
        // Align checks the train it uses now
        auto facts = guard->collectDiagnosisFacts();
        if (!train.isEmpty() && facts.train != train)
            return std::nullopt;
        auto report = AlignDiagnosis::checkup(facts, guard->m_DiagnosisManager->dismissed(MODULE));
        report.status = getAlignStatusString(static_cast<AlignState>(guard->state), false);
        return report;
    };
    manager->registerModule(MODULE, hooks);

    // Devices
    manager->registerAction(MODULE, "camera_connect", [guard, manager](const Diagnosis::Item & item,
                            const Diagnosis::Report & report)
    {
        return connectDevice(manager, guard ? guard->m_Camera : nullptr, report, item);
    });
    manager->registerAction(MODULE, "mount_connect", [guard, manager](const Diagnosis::Item & item,
                            const Diagnosis::Report & report)
    {
        return connectDevice(manager, guard ? guard->m_Mount : nullptr, report, item);
    });
    manager->registerAction(MODULE, "filter_connect", [guard, manager](const Diagnosis::Item & item,
                            const Diagnosis::Report & report)
    {
        return connectDevice(manager, guard ? guard->m_FilterWheel : nullptr, report, item);
    });

    manager->registerAction(MODULE, "mount_unpark", [guard, manager](const Diagnosis::Item & item,
                            const Diagnosis::Report & report)
    {
        if (!guard || !guard->m_Mount)
            return failed(i18n("No mount is available."));
        auto mount = guard->m_Mount;
        if (!mount->isParked())
            return done();

        QPointer<DiagnosisManager> managerGuard(manager);
        const QString diagnosisId = report.id;
        const QString itemId = item.id;
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(mount, &ISD::Mount::newParkStatus, manager, [managerGuard, diagnosisId, itemId,
                                       connection](ISD::ParkStatus status)
        {
            if (status != ISD::PARK_UNPARKED && status != ISD::PARK_ERROR)
                return;
            QObject::disconnect(*connection);
            if (managerGuard)
                managerGuard->finishItem(diagnosisId, itemId, status == ISD::PARK_UNPARKED,
                                         status == ISD::PARK_ERROR ? i18n("The mount reported an error while unparking.") : QString());
        }, Qt::QueuedConnection);

        if (!mount->unpark())
        {
            QObject::disconnect(*connection);
            return failed(i18n("The mount rejected the unpark command."));
        }

        ApplyResult result;
        result.outcome = Outcome::Running;
        result.timeoutMs = 180000;
        return result;
    });

    manager->registerAction(MODULE, "camera_enable_blob", [guard](const Diagnosis::Item &, const Diagnosis::Report &)
    {
        if (!guard || !guard->m_Camera)
            return failed(i18n("No camera is available."));
        auto clientManager = guard->m_Camera->getDriverInfo()->getClientManager();
        if (!clientManager)
            return failed(i18n("The camera driver is not connected."));
        const QByteArray device = guard->m_Camera->getDeviceName().toLatin1();
        clientManager->setBLOBMode(B_ONLY, device.constData(), "CCD1");
        clientManager->setBLOBMode(B_ONLY, device.constData(), "CCD2");
        return done();
    });

    // Sends the KStars clock to the mount, as GenericDevice::updateTime() does when the
    // time source is KStars. The time of the client is never applied.
    auto sendTime = [](ISD::Mount * mount) -> bool
    {
        auto timeUTC = mount->getText("TIME_UTC");
        auto utc = timeUTC ? timeUTC.findWidgetByName("UTC") : nullptr;
        auto offset = timeUTC ? timeUTC.findWidgetByName("OFFSET") : nullptr;
        if (!utc || !offset)
            return false;
        const QString isoTS = KStarsData::Instance()->ut().addMSecs(500).toString(Qt::ISODate).remove('Z');
        utc->setText(isoTS.toLatin1().constData());
        offset->setText(QString::number(KStarsData::Instance()->geo()->TZ(), 'g', 2).toLatin1().constData());
        mount->sendNewProperty(timeUTC);
        return true;
    };

    manager->registerAction(MODULE, "mount_time_sync", [guard, sendTime](const Diagnosis::Item &, const Diagnosis::Report &)
    {
        if (!guard || !guard->m_Mount)
            return failed(i18n("No mount is available."));
        if (!sendTime(guard->m_Mount))
            return failed(i18n("The mount does not accept a time."));
        return done();
    });

    // KStars becomes the time source and keeps the mount clock right from now on
    manager->registerAction(MODULE, "mount_time_source_kstars", [guard, sendTime](const Diagnosis::Item &,
                            const Diagnosis::Report &)
    {
        if (!guard || !guard->m_Mount)
            return failed(i18n("No mount is available."));
        const bool oldUpdate = Options::useTimeUpdate();
        const QString oldSource = Options::timeSource();
        Options::setUseTimeUpdate(true);
        Options::setTimeSource(QStringLiteral("KStars"));
        if (!sendTime(guard->m_Mount))
        {
            Options::setUseTimeUpdate(oldUpdate);
            Options::setTimeSource(oldSource);
            return failed(i18n("The mount does not accept a time."));
        }
        auto result = done();
        result.oldValue = oldUpdate ? oldSource : i18n("none");
        result.newValue = QStringLiteral("KStars");
        result.revert = [oldUpdate, oldSource]()
        {
            Options::setUseTimeUpdate(oldUpdate);
            Options::setTimeSource(oldSource);
        };
        return result;
    });

    // Index files
    manager->registerAction(MODULE, "index_install_", [guard, manager](const Diagnosis::Item & item,
                            const Diagnosis::Report & report)
    {
        bool ok = false;
        const int size = item.id.mid(QStringLiteral("index_install_").size()).toInt(&ok);
        const QString name = ok ? IndexFiles::installName(size) : QString();
        if (name.isEmpty())
            return failed(i18n("Unknown index file size."));
        if (!guard || !guard->opsAstrometryIndexFiles)
            return failed(i18n("The index file manager is not available."));

        QPointer<DiagnosisManager> managerGuard(manager);
        const QString diagnosisId = report.id;
        const QString itemId = item.id;
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(guard.data(), &Align::indexDownloadFinished, manager, [managerGuard, diagnosisId, itemId,
                                       name, connection](const QString & series, bool success, int error)
        {
            if (series != name)
                return;
            QObject::disconnect(*connection);
            if (managerGuard)
                managerGuard->finishItem(diagnosisId, itemId, success, downloadErrorText(error));
        }, Qt::QueuedConnection);

        guard->opsAstrometryIndexFiles->installIndexFile(name);

        ApplyResult result;
        result.outcome = Outcome::Running;
        result.timeoutMs = 60 * 60 * 1000;
        return result;
    }, true);

    // Settings, with revert
    manager->registerAction(MODULE, "align_exposure_5s", [guard](const Diagnosis::Item &, const Diagnosis::Report &)
    {
        if (!guard)
            return failed(QString());
        const double oldValue = guard->alignExposure->value();
        guard->setAllSettings({{"alignExposure", AlignDiagnosis::SUGGESTED_EXPOSURE}});
        auto result = done();
        result.oldValue = oldValue;
        result.newValue = AlignDiagnosis::SUGGESTED_EXPOSURE;
        result.revert = [guard, oldValue]()
        {
            if (guard)
                guard->setAllSettings({{"alignExposure", oldValue}});
        };
        return result;
    });

    manager->registerAction(MODULE, "align_bin_max", [guard](const Diagnosis::Item &, const Diagnosis::Report &)
    {
        if (!guard || guard->alignBinning->count() < 2)
            return failed(i18n("The camera cannot bin."));
        const QString oldValue = guard->alignBinning->currentText();
        const QString newValue = guard->alignBinning->itemText(guard->alignBinning->count() - 1);
        guard->setAllSettings({{"alignBinning", newValue}});
        auto result = done();
        result.oldValue = oldValue;
        result.newValue = newValue;
        result.revert = [guard, oldValue]()
        {
            if (guard)
                guard->setAllSettings({{"alignBinning", oldValue}});
        };
        return result;
    });

    manager->registerAction(MODULE, "solver_timeout_raise", [](const Diagnosis::Item &, const Diagnosis::Report &)
    {
        const uint oldValue = Options::astrometryTimeout();
        const uint newValue = std::min<uint>(oldValue * 2, static_cast<uint>(AlignDiagnosis::MAX_SOLVER_TIMEOUT));
        if (newValue <= oldValue)
            return failed(i18n("The solver timeout is already at its limit."));
        Options::setAstrometryTimeout(newValue);
        auto result = done();
        result.oldValue = oldValue;
        result.newValue = newValue;
        result.revert = [oldValue]()
        {
            Options::setAstrometryTimeout(oldValue);
        };
        return result;
    });

    manager->registerAction(MODULE, "solver_profile_default", [guard](const Diagnosis::Item &, const Diagnosis::Report &)
    {
        if (!guard)
            return failed(QString());
        int index = -1;
        for (int i = 0; i < guard->m_StellarSolverProfiles.size(); i++)
        {
            if (guard->m_StellarSolverProfiles.at(i).listName == AlignDiagnosis::defaultProfileName())
            {
                index = i;
                break;
            }
        }
        if (index < 0)
            return failed(i18n("The default solver profile was not found."));
        const uint oldValue = Options::solveOptionsProfile();
        Options::setSolveOptionsProfile(index);
        auto result = done();
        result.oldValue = oldValue;
        result.newValue = index;
        result.revert = [oldValue]()
        {
            Options::setSolveOptionsProfile(oldValue);
        };
        return result;
    });
}

}

/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guide.h"
#include "internalguide/internalguider.h"
#include "internalguide/calibration.h"

#include "ekos/auxiliary/diagnosisactions.h"
#include "ekos/auxiliary/diagnosismanager.h"
#include "indi/indiguider.h"
#include "Options.h"

#include <ekos_guide_debug.h>

#include <QFutureWatcher>
#include <QtConcurrent>

namespace Ekos
{

using ApplyResult = DiagnosisManager::ApplyResult;
using DiagnosisActions::connectDevice;
using DiagnosisActions::done;
using DiagnosisActions::failed;

namespace
{
const QString MODULE = QStringLiteral("guide");
}

GuideInterface::FailInfo Guide::takeFailInfo()
{
    GuideInterface::FailInfo info = m_PendingFailInfo;
    if (info.reason == GuideFailReason::None && m_GuiderInstance)
        info = m_GuiderInstance->failInfo();
    clearFailInfo();
    return info;
}

void Guide::clearFailInfo()
{
    m_PendingFailInfo = GuideInterface::FailInfo();
    if (m_GuiderInstance)
        m_GuiderInstance->clearFailReason();
}

void Guide::recordFailure(const GuideInterface::FailInfo &info, GuideState state)
{
    m_DiagnosisFailInfo = info;
    m_DiagnosisFailState = state;
    m_FailedImageStats.reset();
    qCInfo(KSTARS_EKOS_GUIDE) << "Guide failure reason:" << guideFailReasonKey(info.reason) << info.detail;

    if (!GuideDiagnosis::isDiagnosable(info.reason) || !m_DiagnosisManager)
        return;

    // The frame Guide shows, unless the user hid it
    auto data = m_ImageData;
    if (!GuideDiagnosis::isFrameReason(info.reason) || !data)
    {
        submitDiagnosis(collectDiagnosisFacts());
        return;
    }

    const auto reason = info.reason;
    auto watcher = new QFutureWatcher<ImageStats>(this);
    connect(watcher, &QFutureWatcher<ImageStats>::finished, this, [this, watcher, reason]()
    {
        watcher->deleteLater();
        // Another failure was recorded meanwhile
        if (m_DiagnosisFailInfo.reason != reason)
            return;

        const ImageStats stats = watcher->result();
        if (stats.verdict != ImageVerdict::Unknown || stats.measuredStars > 0)
        {
            m_FailedImageStats = stats;
            qCInfo(KSTARS_EKOS_GUIDE) << "Failed guide frame:" << imageVerdictKey(stats.verdict) << "stars" << stats.stars
                                      << "HFR" << stats.hfr << "elongation" << stats.elongation;
        }
        submitDiagnosis(collectDiagnosisFacts());
    });
    watcher->setFuture(QtConcurrent::run([data]()
    {
        return ImageAnalysis::analyse(*data);
    }));
}

void Guide::submitDiagnosis(const GuideFacts &facts)
{
    if (!m_DiagnosisManager)
        return;

    m_LastDiagnosisFacts = facts;
    auto report = GuideDiagnosis::diagnose(facts, m_DiagnosisManager->tried(MODULE, facts.train),
                                           m_DiagnosisManager->dismissed(MODULE));
    report.status = getGuideStatusString(static_cast<GuideState>(facts.state), false);
    m_DiagnosisManager->submit(report);
}

GuideFacts Guide::collectDiagnosisFacts() const
{
    GuideFacts facts;
    facts.state = m_DiagnosisFailState;
    facts.reason = m_DiagnosisFailInfo.reason;
    facts.detail = m_DiagnosisFailInfo.detail;
    facts.failPulse = m_DiagnosisFailInfo.pulse;
    facts.failDrift = m_DiagnosisFailInfo.drift;
    facts.train = opticalTrainCombo->currentText();
    facts.guiderType = static_cast<int>(guiderType);

    // Devices
    facts.hasCamera = m_Camera != nullptr;
    if (m_Camera)
    {
        facts.cameraConnected = m_Camera->isConnected();
        facts.cameraName = m_Camera->getDeviceName();
        auto chip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        if (chip)
            facts.canBin = chip->canBin();
    }
    facts.hasGuider = m_Guider != nullptr;
    if (m_Guider)
    {
        facts.guiderConnected = m_Guider->isConnected();
        facts.guiderName = m_Guider->getDeviceName();
        facts.guideViaCamera = m_Camera && m_Guider->getDeviceName() == m_Camera->getDeviceName();
    }
    facts.hasMount = m_Mount != nullptr;
    facts.mountConnected = m_Mount && m_Mount->isConnected();

    // Optics
    facts.focalLength = m_FocalLength;
    facts.aperture = m_Aperture;
    facts.pixelW = ccdPixelSizeX;
    facts.pixelH = ccdPixelSizeY;

    // Settings
    facts.exposure = guideExposure->value();
    facts.binIndex = guideBinning->currentIndex();
    facts.binCount = guideBinning->count();
    facts.squareSize = guideSquareSize->currentText().toInt();
    if (guideSquareSize->currentIndex() + 1 < guideSquareSize->count())
        facts.largerSquareSize = guideSquareSize->itemText(guideSquareSize->currentIndex() + 1).toInt();
    facts.calibrationPulse = Options::calibrationPulseDuration();
    facts.decBacklashRemoval = Options::guideCalibrationBacklash();
    facts.lostStarTimeout = Options::guideLostStarTimeout();
    facts.ditherMaxIterations = Options::ditherMaxIterations();
    facts.ditherFailAborts = Options::ditherFailAbortsAutoGuide();

    // The last calibration of the internal guider
    if (internalGuider)
    {
        // A copy: its accessors are not const
        Calibration calibration = internalGuider->getCalibration();
        if (calibration.isInitialized() && calibration.raPulseMillisecondsPerArcsecond() > 0
                && calibration.decPulseMillisecondsPerArcsecond() > 0)
        {
            facts.raRate = 1000.0 / calibration.raPulseMillisecondsPerArcsecond();
            facts.decRate = 1000.0 / calibration.decPulseMillisecondsPerArcsecond();
        }
    }

    facts.image = m_FailedImageStats;
    return facts;
}

void Guide::registerDiagnosis(DiagnosisManager *manager)
{
    m_DiagnosisManager = manager;
    if (!manager)
        return;

    QPointer<Guide> guard(this);

    // Guiding is busy with something: never start it again nor change its settings under it
    auto isBusy = [guard]()
    {
        switch (guard->m_State)
        {
            case GUIDE_IDLE:
            case GUIDE_ABORTED:
            case GUIDE_CONNECTED:
            case GUIDE_DISCONNECTED:
            case GUIDE_CALIBRATION_ERROR:
            case GUIDE_DITHERING_ERROR:
                return false;
            default:
                return true;
        }
    };

    DiagnosisManager::ModuleHooks hooks;
    hooks.name = i18n("Guide");
    hooks.retry = [guard, isBusy](const QString &)
    {
        if (!guard || isBusy())
            return false;
        return guard->guide();
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
        if (Diagnosis::isCheckup(report))
        {
            auto facts = guard->collectDiagnosisFacts();
            if (facts.train != report.train)
                return std::nullopt;
            auto rebuilt = GuideDiagnosis::checkup(facts, guard->m_DiagnosisManager->dismissed(MODULE));
            rebuilt.status = report.status;
            return rebuilt;
        }
        const auto &facts = guard->m_LastDiagnosisFacts;
        if (facts.train != report.train || static_cast<int>(facts.reason) != report.reason)
            return std::nullopt;
        auto rebuilt = GuideDiagnosis::diagnose(facts, report.tried, guard->m_DiagnosisManager->dismissed(MODULE));
        rebuilt.status = report.status;
        return rebuilt;
    };
    hooks.checkup = [guard](const QString & train) -> std::optional<Diagnosis::Report>
    {
        if (!guard || !guard->m_DiagnosisManager)
            return std::nullopt;
        auto facts = guard->collectDiagnosisFacts();
        if (!train.isEmpty() && facts.train != train)
            return std::nullopt;
        auto report = GuideDiagnosis::checkup(facts, guard->m_DiagnosisManager->dismissed(MODULE));
        report.status = getGuideStatusString(guard->m_State, false);
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

    // Settings, with revert, never while guiding uses them
    auto changeOption = [guard, isBusy](const QVariant & oldValue, const QVariant & newValue,
                                        const std::function<void(const QVariant &)> &set)
    {
        if (!guard)
            return failed(QString());
        if (isBusy())
            return failed(i18n("Guide is busy. Try again when it has stopped."));
        if (!newValue.isValid() || newValue == oldValue)
            return failed(i18n("The setting is already at its limit."));
        set(newValue);
        auto result = done();
        result.oldValue = oldValue;
        result.newValue = newValue;
        result.revert = [set, oldValue]()
        {
            set(oldValue);
        };
        return result;
    };
    // A widget of the Guide tab, through its settings
    auto widgetSetter = [guard](const QString & name)
    {
        return [guard, name](const QVariant & value)
        {
            if (guard)
                guard->setAllSettings({{name, value}});
        };
    };
    auto valid = [](auto value)
    {
        return value > 0 ? QVariant(value) : QVariant();
    };

    manager->registerAction(MODULE, "guide_exposure_raise", [guard, changeOption, widgetSetter, valid](const Diagnosis::Item &,
                            const Diagnosis::Report &)
    {
        if (!guard)
            return failed(QString());
        const double exposure = guard->guideExposure->value();
        return changeOption(exposure, valid(GuideDiagnosis::raisedExposure(exposure)), widgetSetter("guideExposure"));
    });
    manager->registerAction(MODULE, "guide_bin_2x2", [guard, changeOption, widgetSetter](const Diagnosis::Item &,
                            const Diagnosis::Report &)
    {
        if (!guard || guard->guideBinning->count() < 2)
            return failed(i18n("The camera cannot bin."));
        return changeOption(guard->guideBinning->currentText(), guard->guideBinning->itemText(1), widgetSetter("guideBinning"));
    });
    manager->registerAction(MODULE, "guide_square_larger", [guard, changeOption, widgetSetter](const Diagnosis::Item &,
                            const Diagnosis::Report &)
    {
        if (!guard)
            return failed(QString());
        const int index = guard->guideSquareSize->currentIndex();
        if (index + 1 >= guard->guideSquareSize->count())
            return failed(i18n("The guide square is already at its largest."));
        return changeOption(guard->guideSquareSize->currentText(), guard->guideSquareSize->itemText(index + 1),
                            widgetSetter("guideSquareSize"));
    });
    manager->registerAction(MODULE, "guide_pulse_raise", [changeOption, valid](const Diagnosis::Item &,
                            const Diagnosis::Report &)
    {
        const uint pulse = Options::calibrationPulseDuration();
        return changeOption(pulse, valid(static_cast<uint>(GuideDiagnosis::raisedPulse(pulse))), [](const QVariant & value)
        {
            Options::setCalibrationPulseDuration(value.toUInt());
        });
    });
    manager->registerAction(MODULE, "guide_pulse_lower", [changeOption, valid](const Diagnosis::Item &,
                            const Diagnosis::Report &)
    {
        const uint pulse = Options::calibrationPulseDuration();
        return changeOption(pulse, valid(static_cast<uint>(GuideDiagnosis::loweredPulse(pulse))), [](const QVariant & value)
        {
            Options::setCalibrationPulseDuration(value.toUInt());
        });
    });
    manager->registerAction(MODULE, "guide_dec_backlash_on", [changeOption](const Diagnosis::Item &, const Diagnosis::Report &)
    {
        return changeOption(Options::guideCalibrationBacklash(), true, [](const QVariant & value)
        {
            Options::setGuideCalibrationBacklash(value.toBool());
        });
    });
    manager->registerAction(MODULE, "guide_lost_star_timeout_raise", [changeOption, valid](const Diagnosis::Item &,
                            const Diagnosis::Report &)
    {
        const uint timeout = Options::guideLostStarTimeout();
        return changeOption(timeout, valid(static_cast<uint>(GuideDiagnosis::raisedLostStarTimeout(timeout))), [](
                                const QVariant & value)
        {
            Options::setGuideLostStarTimeout(value.toUInt());
        });
    });
    manager->registerAction(MODULE, "dither_iterations_raise", [changeOption, valid](const Diagnosis::Item &,
                            const Diagnosis::Report &)
    {
        const uint iterations = Options::ditherMaxIterations();
        return changeOption(iterations, valid(static_cast<uint>(GuideDiagnosis::raisedDitherIterations(iterations))), [](
                                const QVariant & value)
        {
            Options::setDitherMaxIterations(value.toUInt());
        });
    });
    manager->registerAction(MODULE, "dither_continue", [changeOption](const Diagnosis::Item &, const Diagnosis::Report &)
    {
        return changeOption(Options::ditherFailAbortsAutoGuide(), false, [](const QVariant & value)
        {
            Options::setDitherFailAbortsAutoGuide(value.toBool());
        });
    });
}

}

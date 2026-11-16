/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focus.h"
#include "focusmodule.h"
#include "focusadvisor.h"
#include "focusfitsview.h"
#include "opsfocusmechanics.h"
#include "opsfocusprocess.h"

#include "ekos/auxiliary/diagnosisactions.h"
#include "ekos/auxiliary/diagnosismanager.h"

#include <ekos_focus_debug.h>

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
const QString MODULE = QStringLiteral("focus");

/** Autofocus runs that a capture sequence or the scheduler started, and that handle their failure themselves. */
bool isInSequence(AutofocusReason reason)
{
    switch (reason)
    {
        case FOCUS_FILTER:
        case FOCUS_TIME:
        case FOCUS_TEMPERATURE:
        case FOCUS_HFR_CHECK:
        case FOCUS_MERIDIAN_FLIP:
        case FOCUS_SCHEDULER:
        case FOCUS_USER_REQUEST:
            return true;
        default:
            return false;
    }
}
}

void Focus::setDiagnosisManager(DiagnosisManager *manager)
{
    m_DiagnosisManager = manager;
}

void Focus::noteFailReason(AutofocusFailReason failCode, const QString &failCodeInfo)
{
    if (!FocusDiagnosis::isDiagnosable(failCode))
        return;
    m_NotedFailReason = failCode;
    m_NotedFailInfo = failCodeInfo;
}

void Focus::recordFailure(AutofocusFailReason failCode, const QString &failCodeInfo)
{
    // After the retries, the run ends with a forced abort: report why the attempts failed
    const AutofocusFailReason reported = FocusDiagnosis::reportedReason(failCode, m_NotedFailReason);
    const QString info = reported == failCode ? failCodeInfo : m_NotedFailInfo;
    failCode = reported;
    m_NotedFailReason = FOCUS_FAIL_NONE;
    m_NotedFailInfo.clear();

    m_DiagnosisFailReason = failCode;
    m_DiagnosisFailInfo = info;
    m_FailedImageStats.reset();
    qCInfo(KSTARS_EKOS_FOCUS) << "Autofocus failure reason:" << focusFailReasonKey(failCode);

    if (!FocusDiagnosis::isDiagnosable(failCode) || !m_DiagnosisManager)
        return;

    if (FocusDiagnosis::isFrameReason(failCode) && m_ImageData)
    {
        measureFailedFrame(failCode);
        return;
    }

    submitDiagnosis(collectDiagnosisFacts());
}

void Focus::measureFailedFrame(AutofocusFailReason reason)
{
    // The frame is shared with the worker, so it stays valid if a new one is captured meanwhile
    auto data = m_ImageData;
    auto watcher = new QFutureWatcher<ImageStats>(this);
    connect(watcher, &QFutureWatcher<ImageStats>::finished, this, [this, watcher, reason]()
    {
        watcher->deleteLater();
        // A new autofocus run started while the frame was measured
        if (m_DiagnosisFailReason != reason || inAutoFocus)
            return;

        const ImageStats stats = watcher->result();
        if (stats.verdict != ImageVerdict::Unknown || stats.measuredStars > 0)
        {
            m_FailedImageStats = stats;
            qCInfo(KSTARS_EKOS_FOCUS) << "Failed frame:" << imageVerdictKey(stats.verdict) << "stars" << stats.stars
                                      << "HFR" << stats.hfr << "concentration" << stats.concentration
                                      << "centre dip" << stats.centreDip << "elongation" << stats.elongation;
        }
        else
            qCInfo(KSTARS_EKOS_FOCUS) << "Failed frame could not be measured.";

        submitDiagnosis(collectDiagnosisFacts());
    });
    watcher->setFuture(QtConcurrent::run([data]()
    {
        return ImageAnalysis::analyse(*data);
    }));
}

void Focus::submitDiagnosis(const FocusFacts &facts)
{
    if (!m_DiagnosisManager)
        return;

    m_LastDiagnosisFacts = facts;
    auto report = FocusDiagnosis::diagnose(facts, m_DiagnosisManager->tried(MODULE, facts.train),
                                           m_DiagnosisManager->dismissed(MODULE));
    report.status = getFocusStatusString(static_cast<FocusState>(facts.state), false);
    m_DiagnosisManager->submit(report);
}

FocusFacts Focus::collectDiagnosisFacts() const
{
    FocusFacts facts;
    facts.state = state();
    facts.reason = m_DiagnosisFailReason;
    facts.reasonInfo = m_DiagnosisFailInfo;
    facts.train = opticalTrain();
    facts.inSequence = isInSequence(m_DiagnosisAutofocusReason);

    // Devices
    facts.hasCamera = !m_Camera.isNull();
    if (m_Camera)
    {
        facts.cameraConnected = m_Camera->isConnected();
        facts.cameraName = m_Camera->getDeviceName();
        auto targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
        if (targetChip)
            facts.canBin = targetChip->canBin();
    }
    facts.hasFocuser = !m_Focuser.isNull();
    if (m_Focuser)
    {
        facts.focuserConnected = m_Focuser->isConnected();
        facts.focuserName = m_Focuser->getDeviceName();
    }
    facts.canAbsMove = canAbsMove;
    facts.canRelMove = canRelMove;
    facts.canTimerMove = canTimerMove;
    facts.focuserMin = absMotionMin;
    facts.focuserMax = absMotionMax;
    facts.startPosition = initialFocuserAbsPosition;
    facts.position = currentPosition;
    facts.hasFilterWheel = !m_FilterWheel.isNull();
    facts.filterWheelConnected = m_FilterWheel && m_FilterWheel->isConnected();
    facts.hasDustCap = !m_DustCap.isNull();

    // Settings
    facts.exposure = focusExposure->value();
    facts.gain = focusGain->isEnabled() ? focusGain->value() : -1;
    facts.binIndex = focusBinning->currentIndex();
    facts.binCount = focusBinning->count();
    facts.algorithm = static_cast<int>(m_FocusAlgorithm);
    facts.stepSize = m_OpsFocusMechanics->focusTicks->value();
    facts.maxTravel = m_OpsFocusMechanics->focusMaxTravel->value();
    facts.maxSingleStep = m_OpsFocusMechanics->focusMaxSingleStep->value();
    facts.donutBuster = m_OpsFocusProcess->focusDonut->isChecked();

    // The failed frame
    if (m_ImageData && m_FocusView)
    {
        const auto frame = m_FocusView->lastFrame();
        facts.stars = static_cast<int>(frame.numStars);
        if (frame.hfr > 0)
            facts.hfr = frame.hfr;
    }
    facts.image = m_FailedImageStats;

    return facts;
}

void FocusModule::registerDiagnosis(DiagnosisManager *manager)
{
    m_DiagnosisManager = manager;
    for (auto &focuser : m_Focusers)
        focuser->setDiagnosisManager(manager);
    if (!manager)
        return;

    QPointer<FocusModule> guard(this);

    // Each Focus tab focuses one optical train: a report names the train, a check-up may not
    auto focuserFor = [guard](const QString & train) -> Focus *
    {
        if (!guard || guard->m_Focusers.isEmpty())
            return nullptr;
        if (train.isEmpty())
            return guard->m_Focusers.first().get();
        const int id = guard->findFocuser(train, false);
        return id >= 0 ? guard->m_Focusers[id].get() : nullptr;
    };

    auto isBusy = [](Focus * focus)
    {
        return focus->inAutoFocus || focus->inFocusLoop || focus->inAdjustFocus || focus->inBuildOffsets
               || focus->inAFOptimise || focus->inScanStartPos || focus->focusAdvisor->inFocusAdvisor();
    };

    DiagnosisManager::ModuleHooks hooks;
    hooks.name = i18n("Focus");
    hooks.retry = [focuserFor, isBusy](const QString & train)
    {
        auto focus = focuserFor(train);
        if (!focus || isBusy(focus))
            return false;
        focus->runAutoFocus(FOCUS_MANUAL, "");
        return true;
    };
    hooks.log = [guard](const QString & text)
    {
        if (guard)
            guard->appendLogText(text);
    };
    hooks.refresh = [focuserFor, manager](const Diagnosis::Report & report) -> std::optional<Diagnosis::Report>
    {
        auto focus = focuserFor(report.train);
        if (!focus)
            return std::nullopt;
        // A check-up looks at the setup as it is now
        if (Diagnosis::isCheckup(report))
        {
            auto rebuilt = FocusDiagnosis::checkup(focus->collectDiagnosisFacts(), manager->dismissed(MODULE));
            rebuilt.status = report.status;
            return rebuilt;
        }
        const auto &facts = focus->m_LastDiagnosisFacts;
        if (facts.train != report.train || static_cast<int>(facts.reason) != report.reason)
            return std::nullopt;
        auto rebuilt = FocusDiagnosis::diagnose(facts, report.tried, manager->dismissed(MODULE));
        rebuilt.status = report.status;
        return rebuilt;
    };
    hooks.checkup = [focuserFor, manager](const QString & train) -> std::optional<Diagnosis::Report>
    {
        auto focus = focuserFor(train);
        if (!focus)
            return std::nullopt;
        auto report = FocusDiagnosis::checkup(focus->collectDiagnosisFacts(), manager->dismissed(MODULE));
        report.status = getFocusStatusString(focus->state(), false);
        return report;
    };
    manager->registerModule(MODULE, hooks);

    // Devices
    manager->registerAction(MODULE, "camera_connect", [focuserFor, manager](const Diagnosis::Item & item,
                            const Diagnosis::Report & report)
    {
        auto focus = focuserFor(report.train);
        return connectDevice(manager, focus ? focus->m_Camera.data() : nullptr, report, item);
    });
    manager->registerAction(MODULE, "focuser_connect", [focuserFor, manager](const Diagnosis::Item & item,
                            const Diagnosis::Report & report)
    {
        auto focus = focuserFor(report.train);
        return connectDevice(manager, focus ? focus->m_Focuser.data() : nullptr, report, item);
    });
    manager->registerAction(MODULE, "filter_connect", [focuserFor, manager](const Diagnosis::Item & item,
                            const Diagnosis::Report & report)
    {
        auto focus = focuserFor(report.train);
        return connectDevice(manager, focus ? focus->m_FilterWheel.data() : nullptr, report, item);
    });

    // Settings, with revert. Never while a focus run uses them.
    auto changeSetting = [focuserFor, isBusy](const Diagnosis::Report & report, const QString & name,
                         const std::function<QVariant(Focus *)> &newValueOf, const std::function<QVariant(Focus *)> &oldValueOf)
    {
        auto focus = focuserFor(report.train);
        if (!focus)
            return failed(i18n("Focus is not available."));
        if (isBusy(focus))
            return failed(i18n("Focus is busy. Try again when it has finished."));

        const QVariant oldValue = oldValueOf(focus);
        const QVariant newValue = newValueOf(focus);
        if (!newValue.isValid() || newValue == oldValue)
            return failed(i18n("The setting is already at its limit."));

        QVariantMap settings{{name, newValue}};
        focus->setAllSettings(settings);

        auto result = done();
        result.oldValue = oldValue;
        result.newValue = newValue;
        QPointer<Focus> focusGuard(focus);
        result.revert = [focusGuard, name, oldValue]()
        {
            if (!focusGuard)
                return;
            QVariantMap settings{{name, oldValue}};
            focusGuard->setAllSettings(settings);
        };
        return result;
    };

    manager->registerAction(MODULE, "focus_exposure_raise", [changeSetting](const Diagnosis::Item &,
                            const Diagnosis::Report & report)
    {
        return changeSetting(report, "focusExposure", [](Focus * focus)
        {
            return QVariant(FocusDiagnosis::suggestedExposure(focus->focusExposure->value()));
        }, [](Focus * focus)
        {
            return QVariant(focus->focusExposure->value());
        });
    });

    manager->registerAction(MODULE, "focus_bin_2x2", [changeSetting](const Diagnosis::Item &, const Diagnosis::Report & report)
    {
        return changeSetting(report, "focusBinning", [](Focus * focus)
        {
            return focus->focusBinning->count() > 1 ? QVariant(focus->focusBinning->itemText(1)) : QVariant();
        }, [](Focus * focus)
        {
            return QVariant(focus->focusBinning->currentText());
        });
    });

    manager->registerAction(MODULE, "focus_step_raise", [changeSetting](const Diagnosis::Item &,
                            const Diagnosis::Report & report)
    {
        return changeSetting(report, "focusTicks", [](Focus * focus)
        {
            const int step = FocusDiagnosis::suggestedStepSize(focus->collectDiagnosisFacts());
            return step > 0 ? QVariant(step) : QVariant();
        }, [](Focus * focus)
        {
            return QVariant(focus->m_OpsFocusMechanics->focusTicks->value());
        });
    });

    manager->registerAction(MODULE, "focus_step_min", [changeSetting](const Diagnosis::Item &, const Diagnosis::Report & report)
    {
        return changeSetting(report, "focusTicks", [](Focus *)
        {
            return QVariant(FocusDiagnosis::SUGGESTED_PULSE_STEP);
        }, [](Focus * focus)
        {
            return QVariant(focus->m_OpsFocusMechanics->focusTicks->value());
        });
    });

    manager->registerAction(MODULE, "focus_max_travel_raise", [changeSetting](const Diagnosis::Item &,
                            const Diagnosis::Report & report)
    {
        return changeSetting(report, "focusMaxTravel", [](Focus * focus)
        {
            const double travel = FocusDiagnosis::suggestedMaxTravel(focus->collectDiagnosisFacts());
            return travel > 0 ? QVariant(travel) : QVariant();
        }, [](Focus * focus)
        {
            return QVariant(focus->m_OpsFocusMechanics->focusMaxTravel->value());
        });
    });

    manager->registerAction(MODULE, "focus_donut_buster", [changeSetting](const Diagnosis::Item &,
                            const Diagnosis::Report & report)
    {
        return changeSetting(report, "focusDonut", [](Focus *)
        {
            return QVariant(true);
        }, [](Focus * focus)
        {
            return QVariant(focus->m_OpsFocusProcess->focusDonut->isChecked());
        });
    });
}

}

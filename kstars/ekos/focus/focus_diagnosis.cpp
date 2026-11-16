/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focus.h"
#include "focusmodule.h"
#include "focusadvisor.h"
#include "focusalgorithms.h"
#include "focusfitsview.h"
#include "opsfocusmechanics.h"
#include "opsfocusprocess.h"

#include "ekos/auxiliary/diagnosisactions.h"
#include "ekos/auxiliary/diagnosismanager.h"
#include "ekos/auxiliary/filtermanager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"

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

FocusRunCheck Focus::checkLinearRun(bool r2Failed)
{
    QVector<int> positions;
    QVector<double> values, weights;
    QVector<bool> outliers;
    linearFocuser->getPass1Measurements(&positions, &values, &weights, &outliers);
    // With Donut Buster the outliers are donuts, whose HFR reads too low. Otherwise they come from a curve fit that is
    // itself wrong when the run has no V, e.g. the lowest point far from focus: they are kept.
    const bool dropDonuts = m_OpsFocusProcess->focusDonut->isChecked();
    QList<FocusPoint> points;
    for (int i = 0; i < positions.size() && i < values.size(); i++)
        points << FocusPoint{positions[i], values[i], dropDonuts && i < outliers.size() && outliers[i]};

    // The references are HFRs: runs with another star measure are not compared with them
    double reference = -1;
    if (m_StarMeasure == FOCUS_STAR_HFR)
    {
        const double altitude = (mountAlt >= 0 && mountAlt <= 90) ? mountAlt : -1;
        reference = FocusDiagnosis::referenceFor(loadFocusReferences(), altitude, focusBinning->currentIndex() + 1,
                    QDateTime::currentDateTimeUtc());
    }

    const auto check = FocusDiagnosis::checkRun(points, reference, r2Failed);
    m_LastRunCheck = check;
    qCInfo(KSTARS_EKOS_FOCUS) << "Autofocus run check: points" << check.points << "positions" << check.lowPosition << "-"
                              << check.highPosition << "lowest" << check.lowest << "at" << check.lowestPosition
                              << "reference" << check.reference << "ratio" << check.ratio << "R2 failed" << r2Failed
                              << "slope t" << check.slopeT << "one-sided" << check.oneSided << "end rise" << check.endRise
                              << "direction" << check.direction
                              << "V" << check.hasV << "no minimum" << check.noMinimum;
    return check;
}

bool Focus::failIfNoMinimum(bool r2Failed)
{
    if (m_FocusAlgorithm != FOCUS_LINEAR1PASS || m_AutofocusReason == FOCUS_FOCUS_ADVISOR)
        return false;
    const auto check = checkLinearRun(r2Failed);
    if (!check.noMinimum)
        return false;

    appendLogText(i18n("Autofocus never reached focus: the smallest stars had an HFR of %1, %2 times the %3 of "
                       "recent good runs, and the run had no V.", QString::number(check.lowest, 'f', 2),
                       QString::number(check.ratio, 'f', 1), QString::number(check.reference, 'f', 2)));
    const QString failCodeInfo = i18n("HFR %1 = %2x in focus", QString::number(check.lowest, 'f', 2),
                                      QString::number(check.ratio, 'f', 1));
    m_R2Retries = 0;
    completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_NO_MINIMUM, failCodeInfo, false);
    return true;
}

void Focus::addFocusReference()
{
    if (m_StarMeasure != FOCUS_STAR_HFR || lastFrame().hfr <= 0)
        return;
    // Only a Linear 1 Pass run that went through focus: one that stopped away from it would spoil the reference
    if (!m_LastRunCheck || !FocusDiagnosis::isReferenceRun(*m_LastRunCheck))
    {
        qCInfo(KSTARS_EKOS_FOCUS) << "Autofocus run had no clear V, its HFR is not kept as a reference.";
        return;
    }

    const int id = OpticalTrainManager::Instance()->id(opticalTrain());
    if (id < 0)
        return;
    FocusReference reference;
    reference.hfr = lastFrame().hfr;
    reference.binning = focusBinning->currentIndex() + 1;
    reference.altitude = (m_LastSourceAutofocusAlt >= 0 && m_LastSourceAutofocusAlt <= 90) ? m_LastSourceAutofocusAlt : -1;
    reference.filter = focusFilter->currentIndex() != -1 ? focusFilter->currentText() : QString();
    reference.when = QDateTime::currentDateTimeUtc();

    // Kept with the Focus metadata of the train, the REFERENCE_RUNS most recent runs
    QVariantList list;
    for (const auto &one : FocusDiagnosis::addReference(loadFocusReferences(), reference))
        list << QVariantMap{{"hfr", one.hfr}, {"bin", one.binning}, {"altitude", one.altitude},
        {"when", one.when.toString(Qt::ISODate)}, {"filter", one.filter}};
    setTrainMetadata(id, REFERENCE_METADATA, list);
    qCInfo(KSTARS_EKOS_FOCUS) << "Autofocus reference HFR" << reference.hfr << "binning" << reference.binning
                              << "altitude" << reference.altitude << "filter" << reference.filter;
}

QList<FocusReference> Focus::loadFocusReferences() const
{
    QList<FocusReference> references;
    const int id = OpticalTrainManager::Instance()->id(opticalTrain());
    for (const auto &entry : trainMetadata(id, REFERENCE_METADATA).toList())
    {
        const QVariantMap map = entry.toMap();
        FocusReference reference;
        reference.hfr = map.value("hfr").toDouble();
        reference.binning = map.value("bin", 1).toInt();
        reference.altitude = map.value("altitude", -1).toDouble();
        reference.when = QDateTime::fromString(map.value("when").toString(), Qt::ISODate);
        reference.filter = map.value("filter").toString();
        if (reference.hfr > 0 && reference.when.isValid())
            references << reference;
    }
    return references;
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
    facts.outSteps = static_cast<int>(m_OpsFocusMechanics->focusOutSteps->value());
    facts.afOverscan = m_OpsFocusMechanics->focusAFOverscan->value();
    facts.driverBacklashSupported = m_OpsFocusMechanics->focusBacklash->isEnabled();
    facts.driverBacklash = m_OpsFocusMechanics->focusBacklash->value();
    facts.run = m_LastRunCheck;

    // Where the last successful autofocus with this filter ended, as the Filter Manager keeps it
    facts.filter = focusFilter->currentText();
    if (m_FilterManager && focusFilter->currentIndex() != -1)
    {
        int position = -1;
        double temperature = INVALID_VALUE, altitude = INVALID_VALUE;
        if (m_FilterManager->getFilterAbsoluteFocusDetails(facts.filter, position, temperature, altitude) && position > 0)
        {
            facts.lastGoodPosition = position;
            if (temperature > INVALID_VALUE)
                facts.lastGoodTemperature = temperature;
            if (altitude >= 0 && altitude <= 90)
                facts.lastGoodAltitude = altitude;
            // Adaptive Focus moves focus with these since the last good run
            facts.ticksPerTemp = m_FilterManager->getFilterTicksPerTemp(facts.filter);
            facts.ticksPerAlt = m_FilterManager->getFilterTicksPerAlt(facts.filter);
        }
    }
    if (currentTemperatureSourceElement)
        facts.temperature = currentTemperatureSourceElement->value;
    if (mountAlt >= 0 && mountAlt <= 90)
        facts.altitude = mountAlt;

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

    manager->registerAction(MODULE, "focus_max_travel_fit", [changeSetting](const Diagnosis::Item &,
                            const Diagnosis::Report & report)
    {
        return changeSetting(report, "focusMaxTravel", [](Focus * focus)
        {
            auto facts = focus->collectDiagnosisFacts();
            double travel = FocusDiagnosis::neededMaxTravel(facts);
            if (facts.focuserMax > facts.focuserMin)
                travel = std::min(travel, facts.focuserMax - facts.focuserMin);
            return travel > focus->m_OpsFocusMechanics->focusMaxTravel->value() ? QVariant(travel) : QVariant();
        }, [](Focus * focus)
        {
            return QVariant(focus->m_OpsFocusMechanics->focusMaxTravel->value());
        });
    });

    // Moves the focuser to where the last successful autofocus with the current filter ended
    manager->registerAction(MODULE, "focus_goto_last_good", [focuserFor, isBusy](const Diagnosis::Item &,
                            const Diagnosis::Report & report)
    {
        auto focus = focuserFor(report.train);
        if (!focus || !focus->m_Focuser || !focus->m_Focuser->isConnected())
            return failed(i18n("The focuser is not connected."));
        if (isBusy(focus))
            return failed(i18n("Focus is busy. Try again when it has finished."));
        // Where focus should be now: the last good position moved by the filter's Adaptive Focus settings
        const auto facts = focus->collectDiagnosisFacts();
        const int target = FocusDiagnosis::lastGoodTarget(facts);
        if (!facts.canAbsMove || target < 0)
            return failed(i18n("No good focus position is known for this filter."));
        if (facts.focuserMax > facts.focuserMin && (target < facts.focuserMin || target > facts.focuserMax))
            return failed(i18n("The position %1 is outside the focuser range.", target));
        if (!focus->m_Focuser->moveAbs(target))
            return failed(i18n("The focuser rejected the move."));
        return done(i18n("Moving the focuser to %1.", target));
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

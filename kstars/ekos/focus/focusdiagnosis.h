/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "focusutils.h"
#include "ekos/auxiliary/imageanalysis.h"
#include "ekos/auxiliary/diagnosis.h"

#include <QString>

#include <optional>

namespace Ekos
{

/**
 * Stable machine name of an autofocus failure reason, e.g. "no_stars". The keys go out on D-Bus and EkosLive.
 */
QString focusFailReasonKey(AutofocusFailReason reason);

/** Everything the Focus rules read. Filled by Focus::collectDiagnosisFacts(). */
struct FocusFacts
{
    int state {0};
    AutofocusFailReason reason {FOCUS_FAIL_NONE};
    /// Detail Focus gave with the reason, e.g. "R2=0.52 < Limit=0.80"
    QString reasonInfo;
    QString train;
    /// Autofocus ran for a capture sequence or the scheduler, which handle the failure themselves
    bool inSequence {false};

    // Devices
    bool hasCamera {false};
    bool cameraConnected {false};
    QString cameraName;
    bool canBin {false};
    bool hasFocuser {false};
    bool focuserConnected {false};
    QString focuserName;
    bool canAbsMove {false};
    bool canRelMove {false};
    bool canTimerMove {false};
    /// Range of an absolute focuser, steps
    double focuserMin {0};
    double focuserMax {0};
    /// Position at the start of the autofocus run, -1 if unknown
    int startPosition {-1};
    int position {0};
    bool hasFilterWheel {false};
    bool filterWheelConnected {false};
    bool hasDustCap {false};

    // Settings
    double exposure {0};        ///< seconds
    double gain {-1};           ///< -1 if the camera has no gain
    int binIndex {0};
    int binCount {1};
    int algorithm {0};          ///< Focus::Algorithm
    int stepSize {0};           ///< initial step size, steps or ms
    double maxTravel {0};       ///< steps
    int maxSingleStep {0};      ///< steps, 0 if not limited
    bool donutBuster {false};

    // The failed frame
    int stars {-1};             ///< stars Focus found in the last frame, -1 if unknown
    double hfr {-1};            ///< last measured HFR, -1 if none
    std::optional<ImageStats> image;
};

/**
 * @class FocusDiagnosis
 * @short Turns FocusFacts into a Diagnosis::Report. Pure: no devices, no widgets, no options.
 */
class FocusDiagnosis
{
    public:
        /// Same values as Focus::Algorithm
        static constexpr int ALGORITHM_LINEAR1PASS = 3;
        /// The exposure items stop raising the exposure here, seconds.
        static constexpr double MAX_SUGGESTED_EXPOSURE = 10;
        /// The step the pulse item sets for a focuser that moves by time, ms, as Focus suggests when it fails.
        static constexpr int SUGGESTED_PULSE_STEP = FOCUS_MINIMUM_PULSE_TIMER * 5;

        /** True if a failure with this reason gets a diagnosis: not an abort, a rerun or a skipped run. */
        static bool isDiagnosable(AutofocusFailReason reason);

        /** True for failures that the frame can explain, so it is worth measuring. */
        static bool isFrameReason(AutofocusFailReason reason);

        /**
         * @brief reportedReason The reason to report for an autofocus run that ended with failCode. After its retries
         * a run ends with FOCUS_FAIL_FORCE_ABORT, which says nothing about the cause: the reason of the last failed
         * attempt is reported instead.
         * @param failCode the reason the run ended with
         * @param attemptReason the reason the last failed attempt of the run ended with, FOCUS_FAIL_NONE if none
         */
        static AutofocusFailReason reportedReason(AutofocusFailReason failCode, AutofocusFailReason attemptReason);

        /** The exposure the exposure item sets: twice the current one, at least 1 s, at most MAX_SUGGESTED_EXPOSURE. */
        static double suggestedExposure(double exposure);

        /** The step size the step item sets: twice the current one, within the largest single step. 0 if it can't grow. */
        static int suggestedStepSize(const FocusFacts &facts);

        /** The maximum travel the travel item sets: twice the current one, within the focuser range. 0 if it can't grow. */
        static double suggestedMaxTravel(const FocusFacts &facts);

        /**
         * @brief diagnose Build the report of an autofocus failure.
         * @param facts what Focus knew at the failure
         * @param tried item ids already applied in this failure chain; they are not offered again
         * @param dismissed item ids the user dismissed; they are not offered
         * @return the report, with id and created left empty for the caller to fill
         */
        static Diagnosis::Report diagnose(const FocusFacts &facts, const QStringList &tried = QStringList(),
                                          const QStringList &dismissed = QStringList());

        /**
         * @brief checkup Check the Focus setup when nothing failed: the camera, the focuser, the filter wheel and
         * the step size of a focuser that moves by time. Its items never retry Focus by themselves.
         * @param facts what Focus knows now; the reason is ignored
         * @param dismissed item ids the user dismissed; they are not offered
         * @return the report, with the reason key "checkup" and no items when everything looks right
         */
        static Diagnosis::Report checkup(const FocusFacts &facts, const QStringList &dismissed = QStringList());
};

}

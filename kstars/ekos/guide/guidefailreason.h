/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace Ekos
{

/**
 * @brief Why calibration, guiding or a dither failed. The values are stable: they go out on D-Bus and EkosLive.
 * Use guideFailReasonKey() for a stable machine name.
 *
 * A guider sets the reason where it decides to fail, before it reports the error state. An error state without a
 * reason, e.g. GUIDE_ABORTED after the user pressed Stop, gets no diagnosis.
 */
enum class GuideFailReason
{
    None = 0,
    // Calibration
    CalibrationRaDriftTooShort,    ///< The star barely moved while pulsing RA out
    CalibrationDecDriftTooShort,   ///< The star barely moved while pulsing DEC out
    CalibrationDriftTooShort,      ///< The measured drift or rate was unusable, axis unknown
    CalibrationRaNoReturn,         ///< The star did not come back to the start while pulsing RA in
    CalibrationDecNoReturn,        ///< The star did not come back to the start while pulsing DEC in
    CalibrationStarLost,           ///< The guide star was lost during calibration
    CalibrationStartFailed,        ///< Calibration could not start, e.g. the optics or pixel size are missing
    // Guiding
    NoGuideStar,                   ///< No guide star could be selected
    StarLost,                      ///< The guide star was lost while guiding and not found again
    CaptureTimeout,                ///< Guide exposures timed out, also after restarting the driver
    CameraLost,                    ///< The guide camera did not come back after a driver restart
    DitherFailed,                  ///< A dither did not settle, and the options abort guiding then
    // PHD2
    Phd2CalibrationFailed          ///< PHD2 reported a failed calibration
};

/** Stable machine name of a reason, e.g. "calibration_ra_drift_too_short". */
QString guideFailReasonKey(GuideFailReason reason);

}

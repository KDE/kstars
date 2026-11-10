/*
    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "flatbrightnesscalibration.h"

#include <cstdint>

namespace Ekos
{

/**
 * @short Decisions on how a flat frame reaches its target ADU.
 *
 * A flat calibration either adjusts the brightness of the light box at the exposure
 * of the sequence, or the camera exposure. The functions here make those decisions
 * from plain values, without any device, so that they can be tested on their own.
 */
namespace FlatCalibrationMethod
{

/**
 * @brief Which quantity is adjusted to reach the target ADU.
 */
typedef enum
{
    METHOD_EXPOSURE,   /*!< adjust the camera exposure, the light source stays as it is */
    METHOD_BRIGHTNESS  /*!< adjust the light box brightness, the exposure stays as it is */
} Method;

/**
 * @brief select choose the method when a job starts its flat calibration.
 *
 * The light box brightness is only adjusted if the light box is the flat source,
 * which is the case when its light is on, and if its brightness can be adjusted.
 * Everything else, e.g. wall flats, sky flats or a light box without brightness
 * control, adjusts the exposure.
 * @param skyFlat the job captures sky flats
 * @param lightBoxOn a light box is present and its light is on
 * @param brightnessControllable the light box offers a brightness that can be adjusted
 */
Method select(bool skyFlat, bool lightBoxOn, bool brightnessControllable);

/**
 * @brief fallsBackToExposure whether a brightness calibration that ended with
 * @p result continues by adjusting the exposure.
 *
 * That is the case when the brightness range of the light box cannot reach the
 * target. A result that points to a problem with the light source itself, such as no
 * signal at all, does not fall back.
 */
bool fallsBackToExposure(FlatBrightnessCalibration::Result result);

/** Factor applied to the exposure after a saturated frame, as the exposure calibration does. */
constexpr double SATURATED_EXPOSURE_FACTOR = 0.1;

/**
 * Largest factor by which the first exposure after the fallback may grow. A panel that
 * gives no light still shows the bias of the camera, which would otherwise scale the
 * exposure by the ratio of the target to the bias, easily 30 to 50 times. The exposure
 * calibration continues from the capped step, and stops if the ADU does not follow.
 */
constexpr double MAX_FALLBACK_EXPOSURE_FACTOR = 10.0;

/**
 * @brief fallbackExposure first exposure after falling back from brightness control.
 *
 * The exposure is scaled by the ratio of the target to the ADU measured at the
 * brightness the light box keeps, at most by @see MAX_FALLBACK_EXPOSURE_FACTOR, and
 * clamped to the range of the camera. A saturated frame carries no usable ADU, so the
 * exposure is reduced by @see SATURATED_EXPOSURE_FACTOR instead. The exposure
 * calibration takes over from there.
 * @param exposure exposure of the frame that was measured, in seconds
 * @param measuredADU ADU measured in that frame
 * @param saturated that frame is saturated
 * @param targetADU ADU that should be reached
 * @param minExposure shortest exposure the camera accepts
 * @param maxExposure longest exposure the camera accepts
 * @return the next exposure, or @p exposure clamped to the range if no ratio can be formed
 */
double fallbackExposure(double exposure, double measuredADU, bool saturated, double targetADU, double minExposure,
                        double maxExposure);

/**
 * @brief Which exposure limit of the camera keeps the exposure calibration from going on.
 */
typedef enum
{
    LIMIT_NONE,     /*!< the next exposure can be taken */
    LIMIT_MINIMUM,  /*!< still too bright at the shortest exposure */
    LIMIT_MAXIMUM   /*!< still too dark at the longest exposure */
} ExposureLimit;

/**
 * @brief exposureLimit whether the exposure calibration is stuck at a limit of the camera.
 *
 * That is the case when the frame was already taken at a limit and the next exposure
 * lies beyond it: clamping it would repeat the same frame forever.
 * @param exposure exposure of the frame that was measured, in seconds
 * @param nextExposure exposure the calibration asks for next, before clamping
 * @param minExposure shortest exposure the camera accepts
 * @param maxExposure longest exposure the camera accepts
 */
ExposureLimit exposureLimit(double exposure, double nextExposure, double minExposure, double maxExposure);

/**
 * @brief What to do with a brightness calibration frame, depending on whether the light
 * box kept its brightness while the frame was taken.
 */
typedef enum
{
    FRAME_USE,      /*!< the brightness stayed the same, measure the frame */
    FRAME_RETAKE,   /*!< the brightness changed during the frame, take it again */
    FRAME_GIVE_UP   /*!< the brightness keeps changing, stop the calibration */
} FrameCheck;

/** Number of frames in a row that may be taken again because the brightness changed. */
constexpr int MAX_BRIGHTNESS_RETAKES = 3;

/**
 * @brief checkFrameBrightness whether a brightness calibration frame can be measured.
 *
 * The light box may change its brightness on its own while a frame is taken. An INDI
 * light box applies its per-filter preset when the filter wheel changes, and Capture
 * changes the filter right before the exposure of a frame. Such a frame is taken
 * partly at the old brightness and partly at the new one, so its ADU belongs to
 * neither and could even put the search on the wrong side of the target. It is taken
 * again instead, at most @see MAX_BRIGHTNESS_RETAKES times in a row.
 * @param brightnessAtStart brightness the light box reported when the wait before the
 *        frame ended, -1 if unknown
 * @param brightnessNow brightness the light box reports now that the frame is in
 * @param retakesSoFar frames taken again in a row before this one
 */
FrameCheck checkFrameBrightness(int brightnessAtStart, int brightnessNow, int retakesSoFar);

/**
 * @brief What to do with the light box brightness when a flat job ends or is aborted.
 */
typedef enum
{
    RESTORE_NONE,  /*!< nothing to restore: the brightness is unknown or unchanged */
    RESTORE_SET,   /*!< set the light box back to the brightness it had before the job */
    RESTORE_KEEP   /*!< keep the current brightness, the one before the job would switch the light off */
} RestoreAction;

/**
 * @brief restoreBrightness how to restore the light box brightness after a flat job.
 *
 * The light box returns to the brightness it had before the job, unless that is below
 * the lowest brightness the search asks for, @see FlatBrightnessCalibration::searchMinimum().
 * A brightness of 0 switches some light boxes off, and they ignore other brightness
 * values until their light is turned on again. A flat job that follows right away could
 * start before the light box reports its light off, and then run in the dark.
 * @param startBrightness brightness before the job, -1 if unknown
 * @param currentBrightness brightness the light box reports now
 * @param minBrightness lowest brightness the light box accepts
 * @param maxBrightness highest brightness the light box accepts
 */
RestoreAction restoreBrightness(int startBrightness, int currentBrightness, int minBrightness, int maxBrightness);

/** Fraction of the full range of the frame that an automatic target ADU aims at. */
constexpr double AUTO_TARGET_FRACTION = 0.4;
/** Tolerance of an automatic target ADU, as a fraction of that target. */
constexpr double AUTO_TOLERANCE_FRACTION = 0.05;

/**
 * @brief autoTargetADU target ADU of a job whose target is set to Auto.
 *
 * The target is @see AUTO_TARGET_FRACTION of the full range of the frame, e.g. 26214
 * for 16-bit and 102 for 8-bit data. Other bit depths, such as floating point data,
 * have no range to derive a target from.
 * @param bitsPerPixel bit depth of the captured frame
 * @return the target, or 0 if no target can be derived from @p bitsPerPixel
 */
double autoTargetADU(uint32_t bitsPerPixel);

/**
 * @brief autoTolerance tolerance of an automatic target, @see AUTO_TOLERANCE_FRACTION
 * of @p targetADU rounded to whole ADU.
 */
double autoTolerance(double targetADU);

/**
 * @short Detects an exposure calibration whose ADU no longer follows the exposure.
 *
 * The ADU of a flat follows the exposure almost proportionally. When it hardly moves
 * although the exposure changed considerably, and the frame got no closer to the
 * target, the target cannot be reached that way: the frame is clipped below the
 * target, e.g. unscaled 14-bit data in a 16-bit frame, or the frame holds hardly
 * anything but the bias, e.g. a target below the bias level or a flat source that is
 * off. The exposure calibration would chase such a target forever, so it stops after
 * @see STALLED_STEPS such steps in a row.
 *
 * Both conditions are needed. A dim flat source also gives frames that are mostly
 * bias, so its ADU changes little in relation to the exposure even when a long
 * exposure brings it close to the target.
 */
class StallDetector
{
    public:
        /** A step counts if the exposure changed by at least this fraction, */
        static constexpr double MIN_EXPOSURE_CHANGE = 0.10;
        /** the relative ADU change is less than this fraction of the relative exposure change, */
        static constexpr double MIN_RESPONSE = 0.10;
        /** and the frame closed less than this fraction of the distance to the target. */
        static constexpr double MIN_PROGRESS = 0.10;
        /** Number of such steps in a row after which the calibration stops. */
        static constexpr int STALLED_STEPS = 3;

        /** @brief reset forget all frames, e.g. when a new calibration starts. */
        void reset();

        /**
         * @brief update feed the frame that was just measured.
         * @param exposure exposure of that frame, in seconds
         * @param measuredADU ADU measured in that frame
         * @param targetADU ADU that should be reached
         * @return true if the ADU no longer follows the exposure
         */
        bool update(double exposure, double measuredADU, double targetADU);

    private:
        double m_Exposure { -1 };
        double m_ADU { -1 };
        int m_StalledSteps { 0 };
};

} // namespace FlatCalibrationMethod

} // namespace Ekos

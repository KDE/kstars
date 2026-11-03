/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Based on the Alt-Az derotation work by Toni Schriber (MR !1490).

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace Ekos
{

/**
 * @class FieldDerotation
 * @brief Field derotation for cameras on a rotator carried by an Alt-Az mount.
 *
 * On an Alt-Az mount the camera keeps its orientation relative to the zenith, so its position
 * angle on the sky changes with the parallactic angle q (the position angle of the zenith,
 * measured east of north). With an unchanged rotator angle, the camera PA changes by exactly
 * the change of q, whatever the optics (mirrors flip the direction a rotator turns the image,
 * which RotatorUtils handles through its parity setting, but not the field rotation itself).
 *
 * This class keeps the decision logic free of devices so it can be tested on its own:
 * the caller feeds it a Sample for every mount coordinate update and commands the rotator
 * to the returned position angle.
 */
class FieldDerotation
{
    public:
        /**
         * @brief parallacticAngle Position angle of the zenith as seen from a target, east of north.
         * @param hourAngle hour angle of the target in degrees (positive west of the meridian)
         * @param declination declination of the target in degrees (of date)
         * @param latitude geographic latitude of the observer in degrees
         * @return parallactic angle in degrees, in (-180, 180]
         */
        static double parallacticAngle(double hourAngle, double declination, double latitude);

        /**
         * @brief framingPA Position angle the camera would have at another pointing of the same Alt-Az mount.
         * The camera keeps its angle to the zenith, so its PA changes by the change of the parallactic angle.
         * @param cameraPA current camera position angle in degrees
         * @param cameraParallacticAngle parallactic angle of the current pointing in degrees
         * @param parallacticAngle parallactic angle of the other pointing in degrees
         * @return position angle in degrees, in (-180, 180]
         */
        static double framingPA(double cameraPA, double cameraParallacticAngle, double parallacticAngle);

        /** Mount and rotator state for one update. All angles in degrees. */
        struct Sample
        {
            /// Current camera position angle as computed from the rotator angle.
            double cameraPA { 0 };
            /// The mount is tracking (not slewing, parked or idle).
            bool tracking { false };
            /// The rotator is not moving and not in an error state.
            bool rotatorIdle { false };
            /// Mount pointing (of date).
            double ra { 0 };
            double dec { 0 };
        };

        typedef enum
        {
            /// Nothing to do.
            NONE,
            /// Rotate the camera to targetPA.
            ROTATE,
            /// The error exceeded the maximum correction. The current PA is held instead.
            LIMIT_EXCEEDED
        } Action;

        struct Decision
        {
            Action action { NONE };
            double targetPA { 0 };
            /// Camera PA minus held PA, in (-180, 180].
            double error { 0 };
        };

        /**
         * @brief setThreshold Smallest PA error that is corrected, in degrees.
         */
        void setThreshold(double threshold)
        {
            m_Threshold = threshold;
        }
        double threshold() const
        {
            return m_Threshold;
        }

        /**
         * @brief setMaximumCorrection Largest PA error that is corrected while tracking, in degrees.
         * Larger errors (e.g. a target passing very close to the zenith) are not chased: the
         * current PA is held instead, so the rotator never makes a sudden large turn.
         */
        void setMaximumCorrection(double maximum)
        {
            m_MaximumCorrection = maximum;
        }

        /**
         * @brief hold Keep the camera at this position angle, e.g. after a rotation requested by
         * the user, Align or Capture.
         * @param pa position angle to hold
         * @param ra mount pointing (of date) when the PA was requested
         * @param dec mount pointing (of date) when the PA was requested
         */
        void hold(double pa, double ra, double dec);

        /**
         * @brief clearHold Forget the held PA. The next tracking sample holds its current PA.
         */
        void clearHold();

        bool hasHold() const
        {
            return m_HasHold;
        }
        double heldPA() const
        {
            return m_HeldPA;
        }

        /**
         * @brief update Process a new sample.
         *
         * - Nothing is commanded while the mount is not tracking.
         * - When tracking starts after a slew, the PA reached by the slew is held if the mount
         *   moved more than newTargetDistance from where the PA was held. After a short slew
         *   (e.g. re-centering on the same target) the previous PA is kept and restored.
         * - Nothing is commanded while the rotator is moving.
         * - The error is measured against the held PA, never accumulated, so errors below the
         *   threshold are not lost.
         */
        Decision update(const Sample &sample);

        /** Slews longer than this (in degrees) select a new target and hold the new PA. */
        static constexpr double newTargetDistance = 1.0;

    private:
        double m_Threshold { 0.1 };
        double m_MaximumCorrection { 10.0 };
        bool m_HasHold { false };
        double m_HeldPA { 0 };
        double m_HeldRA { 0 };
        double m_HeldDec { 0 };
        bool m_WasTracking { false };
};

}

/*
    KStars UI tests for field derotation with a rotator on an Alt-Az mount

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_capture_helper.h"

#if defined(HAVE_INDI)

#include "ekos/capture/rotatorsettings.h"

#include <QObject>

/**
 * @brief Tests field derotation end to end with the INDI simulators.
 *
 * The telescope simulator runs as an Alt-Az mount, and the CCD simulator turns the star field by the
 * parallactic angle (it needs the SIM_FIELD_ROTATION property, which INDI 2.2.6 does not have yet).
 * Plate solving measures the camera position angle in the rendered frames.
 *
 * Field rotation is slow in real time, so the tests step the KStars clock. KStars sends every clock
 * change to the simulators as TIME_UTC, so they jump together and then run on at the normal rate.
 * The test field is Kocab one hour before its upper culmination, where it turns about 12° in 30 minutes.
 */
class TestEkosFieldDerotation : public QObject
{
        Q_OBJECT

    public:
        explicit TestEkosFieldDerotation(QObject *parent = nullptr);

    protected:
        /**
         * @brief Set the mount type of the telescope simulator and wait until KStars follows it
         * @param altAz true for ALTAZ, false for EQ_GEM
         */
        bool setMountType(bool altAz);
        /**
         * @brief Set the clock so the test field is at the given hour angle
         */
        bool setHourAngle(double hours);
        /**
         * @brief Step the clock and wait until the simulators and the mount follow
         */
        bool advanceTime(int seconds);
        /**
         * @brief Wait until the simulators run on the KStars time
         */
        bool waitForSimulatorTime();
        /**
         * @brief Slew to the test field and wait until the mount tracks
         */
        bool slewToTestField();
        /**
         * @brief Wait until the mount tracks the test field
         */
        bool waitForTracking();
        /**
         * @brief Wait until KStars used the current mount position for its parallactic angle
         */
        bool waitForFieldRotation();
        /**
         * @brief Wait until the rotator holds the given position angle
         */
        bool waitForDerotation(double heldPA);
        /**
         * @brief Enable or disable derotation, and when enabled, return the position angle it holds
         */
        bool setDerotation(bool enabled, double *heldPA = nullptr);
        /**
         * @brief Capture and solve once, and wait until alignment completes
         */
        bool captureAndSolve();
        /**
         * @brief Wait for the next coordinate update of the mount
         */
        bool waitForCoordinates(int count = 1);

        /** @brief Parallactic angle of the current mount position at the KStars time */
        double expectedParallacticAngle() const;
        /** @brief Parallactic angle the CCD simulator used for the last frame */
        double simulatorParallacticAngle() const;
        /** @brief Camera position angle KStars expects from the rotator angle */
        double cameraPA() const;
        /** @brief Raw angle of the rotator */
        double rotatorAngle() const;
        /** @brief Check if the derotation log contains a message */
        bool derotationLogContains(const QString &text) const;

    protected Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

    private Q_SLOTS:
        /**
         * @brief Without derotation, the field turns by the change of the parallactic angle, and KStars
         *        predicts the new camera position angle before any new solve.
         */
        void testFieldRotation();

        /**
         * @brief With derotation, the rotator takes out the field rotation and the solved position angle
         *        stays where it was.
         */
        void testDerotationHoldsPA();

        /**
         * @brief A rotation beyond the correction limit is not corrected. Derotation holds the new
         *        position angle instead.
         */
        void testDerotationLimit();

    private:
        TestEkosCaptureHelper *m_CaptureHelper = nullptr;

        QSharedPointer<RotatorSettings> m_RotatorSettings;
        bool m_WasRealTime { true };

        Ekos::AlignState m_AlignStatus { Ekos::ALIGN_IDLE };
        QStringList m_AlignLog;
        QStringList m_DerotationLog;

        // last solution
        int m_SolverCount { 0 };
        double m_SolvedPA { 0 };

        void alignStatusChanged(Ekos::AlignState status);
        void alignLogReceived(const QString &text);
        void solverResultReceived(double orientation, double ra, double dec, double pixscale);
};

#endif // HAVE_INDI

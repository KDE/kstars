/*
    KStars UI tests for blind plate solving in alignment

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_capture_helper.h"

#if defined(HAVE_INDI)

#include <QObject>

/**
 * @brief Tests how alignment recovers when the solver hints are wrong.
 *
 * Align solves with the mount position and the image scale from the optical train as
 * constraints. If the solver fails, it retries without those constraints. These tests
 * make the hints wrong on purpose with the CCD simulator and check that the retries
 * find the true pointing:
 * - The mount reports a different position than where the camera looks
 *   (CCD simulator SIM_POINTING_OFFSET, INDI 2.1.x or later).
 * - The image scale is wrong because the optical train has the wrong reducer or barlow.
 *   The simulator keeps rendering with the true focal length.
 *
 * Align only solves ("Nothing" action), since syncing cannot remove a constant simulator offset.
 */
class TestEkosAlignBlind : public QObject
{
        Q_OBJECT
    public:
        explicit TestEkosAlignBlind(QObject *parent = nullptr);

    protected:
        /**
         * @brief Slew to the test field and wait until the mount tracks
         */
        bool slewToTestField();

        /**
         * @brief Set the CCD simulator pointing offset
         * @param raOffset RA offset in degrees
         * @param deOffset DE offset in degrees
         */
        bool setPointingOffset(double raOffset, double deOffset);

        /**
         * @brief Set the reducer of the primary train that Align sees, while the simulator keeps
         *        rendering with the true focal length
         */
        bool setTrainReducer(double reducer);

        /**
         * @brief Capture and solve once, and wait until alignment completes or fails
         */
        bool captureAndSolve();

        /**
         * @brief Verify the last solution against the true pointing of the camera
         * @param raOffset RA offset in degrees between the mount position and the camera pointing
         * @param deOffset DE offset in degrees between the mount position and the camera pointing
         */
        bool verifySolution(double raOffset, double deOffset);

        /**
         * @brief Check if the align log contains a message
         */
        bool logContains(const QString &text) const;

    protected Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

    private Q_SLOTS:
        /**
         * @brief The mount reports a wrong position. Within the solver search radius the
         *        position hint still works, beyond it Align must retry without position.
         */
        void testPointingOffset_data();
        void testPointingOffset();

        /**
         * @brief The optical train has a wrong reducer or barlow. Align must retry without
         *        the image scale and then learn the effective FOV.
         */
        void testWrongFOV_data();
        void testWrongFOV();

        /**
         * @brief Both the pointing and the image scale are wrong. Only a solve without
         *        any constraint succeeds.
         */
        void testWrongFOVAndPointingOffset();

        /**
         * @brief Solving with an external solver (ASTAP) leaves no temporary image files behind,
         *        whether or not the solve succeeds.
         */
        void testExternalSolverTempFiles();

    private:
        TestEkosCaptureHelper *m_CaptureHelper = nullptr;

        // true optics of the primary train, used by the simulator to render
        double m_TrueFocalLength { 0 };
        double m_TrueAperture { 0 };

        Ekos::AlignState m_AlignStatus { Ekos::ALIGN_IDLE };
        QStringList m_AlignLog;

        // last solution: J2000 RA and DE in degrees, pixel scale in arcsec/pixel
        int m_SolverCount { 0 };
        double m_SolvedRA { 0 };
        double m_SolvedDE { 0 };
        double m_SolvedPixScale { 0 };

        void alignStatusChanged(Ekos::AlignState status);
        void alignLogReceived(const QString &text);
        void solverResultReceived(double orientation, double ra, double dec, double pixscale);
};

#endif // HAVE_INDI

/*
    KStars UI tests for alignment

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_helper.h"

#if defined(HAVE_INDI)

#include <QObject>

#define KTRY_CHECK_ALIGNMENTS(i) \
    qCInfo(KSTARS_EKOS_TEST) << "Alignment finished," << image_count << "captures," \
    << solver_count << "successful plate solves."; \
    QVERIFY(image_count == i); \
    QVERIFY(solver_count == i)


class TestEkosAlign : public QObject
{
    Q_OBJECT
public:
    explicit TestEkosAlign(QObject *parent = nullptr);

protected:
    // sequence of alignment states that are expected
    QQueue<Ekos::AlignState> expectedAlignStates;
    // sequence of telescope states that are expected
    QQueue<ISD::Telescope::Status> expectedTelescopeStates;
    // sequence of capture states that are expected
    QQueue<Ekos::CaptureState> expectedCaptureStates;

    /**
     * @brief Track a single alignment
     * @param lastPoint true iff last alignment point
     * @param moveMount if true, an intermediate mount move is issued
     * @return true iff the alignment succeeded
     */
    bool trackSingleAlignment(bool lastPoint, bool moveMount);

protected slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void prepareTestCase();

    bool prepareMountModel(int points);

    /**
     * @brief Execute a mount model creation
     * @param points number of alignment points
     * @param moveMount move the mount during image capturing
     * @return true iff the mount model creation succeeded
     */
    bool runMountModelTool(int points, bool moveMount);

private:
    TestEkosHelper *m_test_ekos_helper;

    // current alignment status
    Ekos::AlignState m_AlignStatus { Ekos::ALIGN_IDLE };

    // current scope status
    ISD::Telescope::Status m_TelescopeStatus { ISD::Telescope::MOUNT_IDLE };

    // current capture status
    Ekos::CaptureState m_CaptureStatus { Ekos::CAPTURE_IDLE };

    /**
     * @brief Slot to track the align status of the mount
     * @param status new align state
     */
    void alignStatusChanged(Ekos::AlignState status);

    /**
     * @brief Slot to track the mount status
     * @param status new mount state
     */
    void telescopeStatusChanged(ISD::Telescope::Status status);

    /**
     * @brief Slot to track the capture status
     * @param status new capture status
     */
    void captureStatusChanged(Ekos::CaptureState status);

    /**
     * @brief slot to track captured images from the align process
     * @param view
     */
    void imageReceived(FITSView *view);

    // counter for images taken in a single test run
    int image_count;

    /**
     * @brief slot to track solver results from the align process
     * @param orientation image orientation
     * @param ra RA position
     * @param dec DEC position
     * @param pixscale image scale of a single image pixel
     */
    void solverResultReceived(double orientation, double ra, double dec, double pixscale);

    // counter of solver results in a single test run
    int solver_count;

private slots:
    /** @brief Test if aligning gets suspended when a slew occurs and recovers when
     *         the scope returns to tracking mode */
    void testAlignRecoverFromSlewing();

    /** @brief Test the mount model */
    void testMountModel();

    /** @brief Test if a slew event during the mount model creation is handled correctly
     *         by suspending when a slew occurs and recovers when the scope returns
     *         to tracking mode */
    void testMountModelToolRecoverFromSlewing();
};

#endif // HAVE_INDI

/*
    KStars UI tests for alignment

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_capture_helper.h"

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
    QQueue<ISD::Mount::Status> expectedTelescopeStates;
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

    /**
     * @brief Slew to a named target
     * @param target target sky position
     */
    bool slewToTarget(SkyPoint *target);

    /**
     * @brief Execute a single alignment and check if the alignment target matches the given position
     * @param targetObject expected target
     * @return true iff the alignment was successful
     */
    bool executeAlignment(SkyObject *targetObject);

    /**
     * @brief Verify if the alignment target matches the expected coordinates
     * @param targetObject
     * @return true iff the alignment target coordinates match those of the target object
     */
    bool verifyAlignmentTarget(SkyObject *targetObject);

    /**
     * @brief Helper function to test alignment with the scheduler running
     * @param targetObject pointer to the target object
     * @param fitsTarget path to a FITS image. Leave null if none should be used
     * @return true iff the alignment target coordinates match those of the target object
     */
    bool alignWithScheduler(SkyObject *targetObject, QString fitsTarget = nullptr);

    /**
     * @brief find a target by the name
     * @param name target name
     * @return target with JNow and J2000 coordinates set
     */
    SkyObject *findTargetByName(QString name);

    /**
     * @brief update J2000 coordinates
     */
    void updateJ2000Coordinates(SkyPoint *target);

private:
    // helper class
    TestEkosCaptureHelper *m_CaptureHelper = nullptr;

    // test directory
    QTemporaryDir *testDir;

    // current alignment status
    Ekos::AlignState m_AlignStatus { Ekos::ALIGN_IDLE };

    // current scope status
    ISD::Mount::Status m_TelescopeStatus { ISD::Mount::MOUNT_IDLE };

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
    void telescopeStatusChanged(ISD::Mount::Status status);

    /**
     * @brief Slot to track the capture status
     * @param status new capture status
     */
    void captureStatusChanged(Ekos::CaptureState status);

    /**
     * @brief slot to track captured images from the align process
     * @param view
     */
    void imageReceived(const QSharedPointer<FITSView> &view);

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
    /**
     * @brief Test a simple slew and a subsequent alignment
     */
    void testSlewAlign();

    /**
     * @brief Test alignment for a sky position in an empty sky segment
     */
    void testEmptySkyAlign();

    /**
     * @brief Test if only syncs have been used the alignment uses the latest
     *        sync as target.
     */
    void testSyncOnlyAlign();

    /**
     * @brief Test if guide drifting leaves the alignment target untouched
     */
    void testSlewDriftAlign();

    /**
     * @brief Test aligning to target where the target definition comes from a FITS image.
     */
    void testFitsAlign();

    /**
     * @brief Test alignment to the target of a scheduled job
     */
    void testAlignTargetScheduledJob();

    /**
     * @brief Test alignment to a FITS target of a scheduled job
     */
    void testFitsAlignTargetScheduledJob();

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

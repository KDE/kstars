/*
    Tests for meridian flip state machine.

    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/manager/meridianflipstate.h"
#include "test_ekos_helper.h"
#include "indi/indimount.h"

#include <QTest>
#include <QObject>

class MountSimulator;

class TestEkosMeridianFlipState : public QObject
{
    Q_OBJECT
public:
    explicit TestEkosMeridianFlipState(QObject *parent = nullptr) : QObject{parent} {}

protected slots:
    void init();
    /**
     * @brief Cleanup after the a single test case has finished.
     */
    void cleanup();
    /**
     * @brief Initialization at beginning of the run of the entire test suite.
     */
    void initTestCase();
    /**
     * @brief Cleanup after the entire test suite has finished.
     */
    void cleanupTestCase();
    /**
     * @brief Initialisation before each single test case.
     */

private slots:
    /**
     * @brief Test case for a simple meridian flip
     */
    void testMeridianFlip();

    /** @brief Test data for {@see testMeridianFlip()} */
    void testMeridianFlip_data();

private:
    // helper functions
    TestEkosHelper m_Helper;
    // The state machine
    Ekos::MeridianFlipState *m_stateMachine { nullptr };
    // the test adapter simulating the EKOS environment
    MountSimulator *m_Mount { nullptr };

    // current mount position
    Ekos::MeridianFlipState::MountPosition m_currentPosition;
    // current mount status
    ISD::Mount::Status m_MountStatus { ISD::Mount::MOUNT_IDLE };
    // sequence of mount states that are expected
    QQueue<ISD::Mount::Status> expectedMountStates;

    // current  meridian flip mount status
    Ekos::MeridianFlipState::MeridianFlipMountState m_MeridianFlipMountStatus { Ekos::MeridianFlipState::MOUNT_FLIP_NONE };
    // sequence of meridian flip mount states that are expected
    QQueue<Ekos::MeridianFlipState::MeridianFlipMountState> expectedMeridianFlipMountStates;
    // initial meridian flip delay
    int secs_to_mf = -1;

    // forward signals to the sequence job
    void connectAdaptor();
    void disconnectAdaptor();

    /**
     * @brief Determine the target close to the meridian
     * @param secsToMF seconds to meridian flip
     * @param upper set to true if close to an upper meridian flip
     * @return position secsToMF seconds close to the meridian
     */
    SkyPoint findMFTestTarget(int secsToMF, bool upper = true);

    /**
     * @brief Slot for receiving a new telescope position
     * @param position new position
     * @param pierSide current pier side
     * @param ha current hour angle
     */
    void updateTelescopeCoord(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha);

    /**
     * @brief Slot to track the mount status
     * @param status new mount status
     */
    void mountStatusChanged(ISD::Mount::Status status);

    /**
     * @brief Slot to track the meridian flip state of the mount
     * @param status new meridian flip mount state
     */
    void meridianFlipMountStatusChanged(Ekos::MeridianFlipState::MeridianFlipMountState status);
};

/* *********************************************************************************
 * Test adapter
 * ********************************************************************************* */

class MountSimulator : public QObject
{
    Q_OBJECT

public:
    explicit MountSimulator(){}

    void init(const SkyPoint &position);
    void shutdown();
    void Slew(const SkyPoint &position);
    void Sync(const SkyPoint &position);

signals:
    void newCoords(const SkyPoint &position, const ISD::Mount::PierSide pierside, const dms &ha);
    void newStatus(ISD::Mount::Status status);

private:
    SkyPoint m_position;
    ISD::Mount::PierSide m_pierside { ISD::Mount::PIER_UNKNOWN};

    void updatePosition();
    dms calcHA(const SkyPoint &pos);
    ISD::Mount::PierSide calcPierSide(const SkyPoint &pos);
};

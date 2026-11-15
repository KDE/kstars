/*
    KStars UI tests for automatic polar alignment correction with the PAC simulator

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_helper.h"

#if defined(HAVE_INDI)

#include <QElapsedTimer>
#include <QObject>

/**
 * @brief Runs the Polar Alignment Assistant with automatic PAC correction end to end.
 *
 * The test does not start an INDI server. It expects one on 127.0.0.1 (port from PAC_TEST_PORT, default 7630)
 * with the telescope, CCD and alignment correction simulators connected, and uses a remote Ekos profile,
 * so it never stops other INDI servers on the machine.
 *
 * The telescope simulator snoops PAC_MANUAL_ADJUSTMENT and moves its polar axis (MM_MA, MM_ME) by every
 * completed correction, so the plate-solve refresh sees the corrections take effect.
 */
class TestEkosPACSim : public QObject
{
        Q_OBJECT

    public:
        explicit TestEkosPACSim(QObject *parent = nullptr);

    protected:
        bool setProperty(const QString &property, const QString &value);
        QString getProperty(const QString &property);
        bool logContains(const QString &text) const;
        void dumpLog() const;

        /** @brief Run the three-point PAA measurement and return the measured Az/Alt error in arcminutes. */
        bool measurePolarError(int ma, int me, double *az, double *alt);
        bool stopPAA();

    private slots:
        void initTestCase();
        void cleanupTestCase();

        /** @brief An axis whose error is inside the deadband is not sent to the PAC. */
        void testDeadband();

        /** @brief The refresh loop corrects the polar error. */
        void testAutoCorrection();
        /** @brief A correction that the driver completes without reporting Busy still completes in Ekos. */
        void testZeroCorrectionCompletes();

        void alignLogReceived(const QString &text);

    private:
        TestEkosHelper *m_Helper { nullptr };
        int m_Port { 7630 };
        QStringList m_AlignLog;
        QElapsedTimer m_LogClock;
};

#endif // HAVE_INDI

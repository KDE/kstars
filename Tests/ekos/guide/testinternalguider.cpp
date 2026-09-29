/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/guide/internalguide/internalguider.h"
#include "ekos/guide/guideview.h"
#include "ekos/ekos.h"

#include "../../testhelpers.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif
#include <QSignalSpy>

class TestInternalGuider : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void testSuspendResume();
        void testRestartWhileSuspended();

    private:
        QSharedPointer<GuideView> m_View;
};

void TestInternalGuider::initTestCase()
{
    KTEST_BEGIN();
    m_View.reset(new GuideView());
}

void TestInternalGuider::cleanupTestCase()
{
    m_View.reset();
    KTEST_END();
}

// The regular capture-driven path: suspend for a non-light frame, then resume.
void TestInternalGuider::testSuspendResume()
{
    Ekos::InternalGuider guider;
    guider.setGuideView(m_View);

    QVERIFY(guider.guide());
    QVERIFY(!guider.isProcessingSuspended());

    QVERIFY(guider.suspend());
    QVERIFY(guider.isProcessingSuspended());

    QVERIFY(guider.resume());
    QVERIFY(!guider.isProcessingSuspended());

    guider.abort();
}

// Bug 526397: when a scheduler job repeats and its sequence ended with flats, guiding is still
// suspended, so SchedulerProcess::startGuiding() calls connectGuider() and then guide() over D-Bus.
// The restarted session reported GUIDE_GUIDING but kept skipping every frame (no corrections and no
// guide stats) because the suspension was never cleared.
void TestInternalGuider::testRestartWhileSuspended()
{
    Ekos::InternalGuider guider;
    guider.setGuideView(m_View);
    QSignalSpy statusSpy(&guider, &Ekos::GuideInterface::newStatus);

    QVERIFY(guider.guide());
    QVERIFY(guider.suspend());
    QVERIFY(guider.isProcessingSuspended());

    // What Guide::connectGuider() followed by Guide::guide() does to the internal guider.
    QVERIFY(guider.Connect());
    statusSpy.clear();
    QVERIFY(guider.guide());

    QVERIFY(!statusSpy.isEmpty());
    QCOMPARE(statusSpy.last().at(0).value<Ekos::GuideState>(), Ekos::GUIDE_GUIDING);
    QVERIFY(!guider.isProcessingSuspended());

    guider.abort();
}

QTEST_MAIN(TestInternalGuider)

#include "testinternalguider.moc"

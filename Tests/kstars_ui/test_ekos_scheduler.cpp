/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "test_ekos_scheduler.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"

TestEkosScheduler::TestEkosScheduler(QObject *parent) : QObject(parent)
{
}

void TestEkosScheduler::init()
{
}

void TestEkosScheduler::cleanup()
{
}

void TestEkosScheduler::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_EKOS_START_SIMULATORS();

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();
}

void TestEkosScheduler::cleanupTestCase()
{
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosScheduler::testScheduleManipulation_data()
{
#if 0
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<QString>("RA");
    QTest::addColumn<QString>("DEC");

    // Altitude computation taken from SchedulerJob::findAltitude
    GeoLocation * const geo = KStarsData::Instance()->geo();
    KStarsDateTime const now(KStarsData::Instance()->lt());
    KSNumbers const numbers(now.djd());
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(now).gst());

    std::list<char const *> Objects = { "Polaris", "Mizar", "M 51", "M 13", "M 47", "Vega", "NGC 2238", "M 81" };
    size_t count = 0;

    foreach (char const *name, Objects)
    {
        SkyObject const * const so = KStars::Instance()->data()->objectNamed(name);
        if (so != nullptr)
        {
            SkyObject o(*so);
            o.updateCoordsNow(&numbers);
            o.EquatorialToHorizontal(&LST, geo->lat());
            if (10.0 < o.alt().Degrees())
            {
                QTest::addRow("%s", name)
                        << name
                        << o.ra().toHMSString()
                        << o.dec().toDMSString();
                count++;
            }
            else QWARN(QString("Fixture '%1' altitude is '%2' degrees, discarding.").arg(name).arg(so->alt().Degrees()).toStdString().c_str());
        }
    }

    if (!count)
        QSKIP("No usable fixture objects, bypassing test.");
#endif
#endif
}

void TestEkosScheduler::testScheduleManipulation()
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    // Wait for Scheduler to come up, switch to Scheduler tab
    QTRY_VERIFY_WITH_TIMEOUT(ekos->schedulerModule() != nullptr, 5000);
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget);
    toolsWidget->setCurrentWidget(ekos->schedulerModule());
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), ekos->schedulerModule(), 1000);

    KTRY_SCHEDULER_GADGET(QLineEdit, nameEdit);
    KTRY_SCHEDULER_GADGET(dmsBox, raBox);
    KTRY_SCHEDULER_GADGET(dmsBox, decBox);
    KTRY_SCHEDULER_GADGET(QLineEdit, sequenceEdit);
    KTRY_SCHEDULER_GADGET(QPushButton, addToQueueB);
    KTRY_SCHEDULER_GADGET(QPushButton, removeFromQueueB);

    // Computation taken from SchedulerJob::findAltitude
    GeoLocation * const geo = KStarsData::Instance()->geo();
    KStarsDateTime const now(KStarsData::Instance()->lt());
    KSNumbers const numbers(now.djd());
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(now).gst());

    raBox->setText("1h 2' 3\"");
    decBox->setText("1° 2' 3\"");
    sequenceEdit->setText(QString("%1%201x1s_Lum.esq").arg(QDir::current().currentPath()).arg(QDir::separator())); // %20 to retain the next '1'

    QEXPECT_FAIL("", "The sequence file editbox cannot be edited directly, and changing its text does not allow to add a job", Continue);
    QTRY_VERIFY_WITH_TIMEOUT(addToQueueB->isEnabled(), 200);

    const int count = 20;
    QStringList seqs;
    seqs << QString("%1%201x1s_Lum.esq").arg(QDir::current().currentPath()).arg(QDir::separator()); // %20 to retain the next '1'
    seqs << QString("%1%201x1s_RGBLumRGB.esq").arg(QDir::current().currentPath()).arg(QDir::separator()); // %20 to retain the next '1'

    KTRY_SCHEDULER_GADGET(QTableWidget, queueTable);

    // Add objects to the schedule
    for (int i = 0; i < count; i++)
    {
        QCOMPARE(queueTable->rowCount(), i);

        nameEdit->setText(QString("Object-%1").arg(i));

        SkyObject o(SkyObject::TYPE_UNKNOWN, LST.radians() - (double)i/10 + (double)count/2, 45.0);
        raBox->setText(o.ra().toHMSString());
        QVERIFY(abs(raBox->createDms(false).Hours() - o.ra().Hours()) <= 15.0/3600.0);
        decBox->setText(o.dec().toDMSString());
        QVERIFY(abs(decBox->createDms(true).Degrees() - o.dec().Degrees()) <= 1.0/3600.0);
        sequenceEdit->setText(seqs[i % seqs.count()]);

        Ekos::Manager::Instance()->schedulerModule()->addObject(&o);
        Ekos::Manager::Instance()->schedulerModule()->setSequence(sequenceEdit->text());
        nameEdit->setText(QString("Object-%1").arg(i));

        // Add object -- note adding can be quite slow
        KTRY_SCHEDULER_CLICK(addToQueueB);
        QTRY_COMPARE_WITH_TIMEOUT(queueTable->rowCount(), i + 1, 5000);
    }

    // Verify each row
    for (int i = 0; i < count; i++)
    {
        queueTable->selectRow(i % queueTable->rowCount());
        int const index = count - i - 1;
        QCOMPARE(qPrintable(nameEdit->text()), qPrintable(QString("Object-%1").arg(index)));
        QCOMPARE(qPrintable(sequenceEdit->text()), qPrintable(seqs[index % seqs.count()]));
        SkyObject o(SkyObject::TYPE_UNKNOWN, LST.radians() - (double)index/10 + (double)count/2, 45.0);
        QCOMPARE(qPrintable(dms::fromString(raBox->text(), false).toHMSString()), qPrintable(o.ra().toHMSString()));
        QCOMPARE(qPrintable(dms::fromString(decBox->text(), true).toDMSString()), qPrintable(o.dec().toDMSString()));
    }

    // Select a job, add a new one, remove the old one, no change expected
    // This verifies the fix to the issue causing sequence files to be messed up when pasting jobs
    for (int i = 1; i < count - 1; i++)
    {
        queueTable->selectRow(i % queueTable->rowCount());

        KTRY_SCHEDULER_CLICK(removeFromQueueB);
        KTRY_SCHEDULER_CLICK(addToQueueB);
        queueTable->selectRow(0);
        queueTable->selectRow(i % queueTable->rowCount());

        int const index = count - i - 1;
        QCOMPARE(qPrintable(nameEdit->text()), qPrintable(QString("Object-%1").arg(index)));
        QCOMPARE(qPrintable(sequenceEdit->text()), qPrintable(seqs[index % seqs.count()]));
        SkyObject o(SkyObject::TYPE_UNKNOWN, LST.radians() - (double)index/10 + (double)count/2, 45.0);
        QCOMPARE(qPrintable(dms::fromString(raBox->text(), false).toHMSString()), qPrintable(o.ra().toHMSString()));
        QCOMPARE(qPrintable(dms::fromString(decBox->text(), true).toDMSString()), qPrintable(o.dec().toDMSString()));
    }

    // Remove objects from the schedule
    for (int i = 0; i < count; i++)
    {
        QCOMPARE(queueTable->rowCount(), count - i);

        // Clear selection, no removal possible
        queueTable->setCurrentIndex(QModelIndex());
        QTRY_COMPARE_WITH_TIMEOUT(queueTable->currentRow(), -1, 500);
        QEXPECT_FAIL("", "Removal button is not disabled when there is no line selection.", Continue);
        QVERIFY(!removeFromQueueB->isEnabled());
        KTRY_SCHEDULER_CLICK(removeFromQueueB);
        //QEXPECT_FAIL("", "Removal button is ineffective when there is no line selection.", Continue);
        //QTRY_COMPARE_WITH_TIMEOUT(queueTable->rowCount(), count - i, 5000);

        // Select a line, remove job - note removal can be quite slow
        queueTable->selectRow(i % queueTable->rowCount());
        //queueTable->selectionModel()->select(queueTable->model()->index(i % queueTable->model()->rowCount(), 0), QItemSelectionModel::SelectCurrent);
        KTRY_SCHEDULER_CLICK(removeFromQueueB);
        QTRY_COMPARE_WITH_TIMEOUT(queueTable->rowCount(), count - i - 1, 5000);
    }

    QCOMPARE(queueTable->rowCount(), 0);
}

QTEST_KSTARS_MAIN(TestEkosScheduler)

#endif // HAVE_INDI

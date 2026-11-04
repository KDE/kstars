/*
    Tests for the defaults new flat jobs get from the calibration settings.

    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Options.h"
#include "ui_calibrationoptions.h"
#include "ekos/capture/sequencejob.h"
#include "kstarsdata.h"

#include "../../testhelpers.h"

#include <lilxml.h>

#include <QDialog>
#include <QTest>

/**
 * @brief New flat jobs reach the flat target ADU by default, and that target is Auto,
 * while sequence files keep the flat duration they were saved with.
 */
class TestFlatCalibrationDefaults : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        /** @brief The calibration dialog selects ADU with an Auto target. */
        void testDialogDefaults();
        /** @brief The global setting that the capture module applies to the dialog selects ADU as well. */
        void testGlobalSettingDefaults();
        /** @brief A job from a sequence file keeps its flat duration, a job without one stays Manual. */
        void testSequenceFileKeepsItsDuration_data();
        void testSequenceFileKeepsItsDuration();
};

void TestFlatCalibrationDefaults::initTestCase()
{
    KTEST_BEGIN();
    // the file name of a job is derived with the help of the KStars data
    QVERIFY(KStarsData::Create() != nullptr);
}

void TestFlatCalibrationDefaults::cleanupTestCase()
{
    KTEST_END();
}

void TestFlatCalibrationDefaults::testDialogDefaults()
{
    QDialog dialog;
    Ui::Calibration ui;
    ui.setupUi(&dialog);

    QVERIFY(ui.captureCalibrationUseADU->isChecked());
    QVERIFY(ui.captureCalibrationDurationManual->isChecked() == false);
    QCOMPARE(ui.captureCalibrationADUValue->value(), 0);
    QCOMPARE(ui.captureCalibrationADUValue->text(), QString("Auto"));
}

void TestFlatCalibrationDefaults::testGlobalSettingDefaults()
{
    // The capture module sets each dialog widget from the setting of the same name, if
    // there is one. Only the ADU button has one, so its default decides for a new train.
    QVERIFY(Options::self()->findItem("CaptureCalibrationDurationManual") == nullptr);

    auto useADU = Options::self()->findItem("CaptureCalibrationUseADU");
    QVERIFY(useADU != nullptr);
    useADU->setDefault();
    QVERIFY(Options::captureCalibrationUseADU());

    auto target = Options::self()->findItem("CaptureCalibrationADUValue");
    QVERIFY(target != nullptr);
    target->setDefault();
    QCOMPARE(Options::captureCalibrationADUValue(), 0.0);
}

void TestFlatCalibrationDefaults::testSequenceFileKeepsItsDuration_data()
{
    QTest::addColumn<QString>("flatDuration");
    QTest::addColumn<int>("duration");
    QTest::addColumn<double>("targetADU");

    QTest::newRow("none") << "" << int(DURATION_MANUAL) << 0.0;
    QTest::newRow("manual") << "<FlatDuration dark='false'><Type>Manual</Type></FlatDuration>"
                            << int(DURATION_MANUAL) << 0.0;
    QTest::newRow("ADU") << "<FlatDuration dark='false'><Type>ADU</Type><Value>20000</Value>"
                  "<Tolerance>1000</Tolerance><SkyFlat>false</SkyFlat></FlatDuration>"
                         << int(DURATION_ADU) << 20000.0;
    QTest::newRow("ADU, Auto") << "<FlatDuration dark='false'><Type>ADU</Type><Value>0</Value>"
                  "<Tolerance>1000</Tolerance><SkyFlat>false</SkyFlat></FlatDuration>"
                               << int(DURATION_ADU) << 0.0;
}

void TestFlatCalibrationDefaults::testSequenceFileKeepsItsDuration()
{
    QFETCH(QString, flatDuration);
    QFETCH(int, duration);
    QFETCH(double, targetADU);

    const QByteArray xml = QString("<Job><Exposure>1</Exposure><Type>Flat</Type><Calibration>%1</Calibration></Job>")
                           .arg(flatDuration).toLatin1();
    LilXML *parser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    for (const char c : xml)
    {
        root = readXMLEle(parser, c, errmsg);
        if (root)
            break;
    }
    delLilXML(parser);
    QVERIFY2(root != nullptr, errmsg);

    Ekos::SequenceJob job(root, QString());
    delXMLEle(root);

    QCOMPARE(int(job.getFlatFieldDuration()), duration);
    QCOMPARE(job.getCoreProperty(Ekos::SequenceJob::SJ_TargetADU).toDouble(), targetADU);
}

QTEST_MAIN(TestFlatCalibrationDefaults)

#include "test_flatcalibrationdefaults.moc"

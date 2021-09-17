/*
    SPDX-FileCopyrightText: 2012 Rishab Arora <ra.rishab@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testfwparser.h"

#include <QDir>
#include <QTemporaryFile>

TestFWParser::TestFWParser() : QObject()
{
}

void TestFWParser::initTestCase()
{
    test_cases_.append("this is an exam ple of 256 cases being tested -3.14       times\n");
    test_cases_.append("                                                               \n");
    test_cases_.append("this is an ex\n\n");

    QTemporaryFile temp_file;
    //temp_file.setPrefix(QDir::tempPath() + QDir::separator());
    //temp_file.setSuffix(".txt");
    temp_file.setAutoRemove(false);
    QVERIFY(temp_file.open());
    test_file_name_ = temp_file.fileName();
    QTextStream out_stream(&temp_file);
    foreach (const QString &test_case, test_cases_)
        out_stream << test_case;
    temp_file.close();

    //Building the sequence to be used. Includes all available types.
    sequence_.clear();
    sequence_.append(qMakePair(QString("field1"), KSParser::D_QSTRING));
    sequence_.append(qMakePair(QString("field2"), KSParser::D_QSTRING));
    sequence_.append(qMakePair(QString("field3"), KSParser::D_QSTRING));
    sequence_.append(qMakePair(QString("field4"), KSParser::D_QSTRING));
    sequence_.append(qMakePair(QString("field5"), KSParser::D_QSTRING));
    sequence_.append(qMakePair(QString("field6"), KSParser::D_INT));
    sequence_.append(qMakePair(QString("field7"), KSParser::D_QSTRING));
    sequence_.append(qMakePair(QString("field8"), KSParser::D_QSTRING));
    sequence_.append(qMakePair(QString("field9"), KSParser::D_QSTRING));
    sequence_.append(qMakePair(QString("field10"), KSParser::D_FLOAT));
    sequence_.append(qMakePair(QString("field11"), KSParser::D_QSTRING));
    sequence_.append(qMakePair(QString("field12"), KSParser::D_QSTRING));
    widths_.append(5);
    widths_.append(3);
    widths_.append(3);
    widths_.append(9);
    widths_.append(3);
    widths_.append(4);
    widths_.append(6);
    widths_.append(6);
    widths_.append(7);
    widths_.append(6);
    widths_.append(6); //'repeatedly' doesn't need a width

    test_parser_ = new KSParser(test_file_name_, '#', sequence_, widths_);
}

void TestFWParser::cleanupTestCase()
{
    delete test_parser_;
}

void TestFWParser::MixedInputs()
{
    /*
     * Test 1: Checks all conversions are working as expected
    */
    QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();

    QCOMPARE(row_content["field1"].toString(), QString("this"));
    QCOMPARE(row_content["field2"].toString(), QString("is"));
    QCOMPARE(row_content["field3"].toString(), QString("an"));
    QCOMPARE(row_content["field4"].toString(), QString("exam ple"));
    QCOMPARE(row_content["field5"].toString(), QString("of"));
    QCOMPARE(row_content["field6"].toInt(), 256);
    QCOMPARE(row_content["field7"].toString(), QString("cases"));
    QCOMPARE(row_content["field8"].toString(), QString("being"));
    QCOMPARE(row_content["field9"].toString(), QString("tested"));
    QVERIFY(row_content["field10"].toFloat() + 3.141 < 0.1);
    QCOMPARE(row_content["field11"].toString(), QString(""));
    QCOMPARE(row_content["field12"].toString(), QString("times"));
}

void TestFWParser::OnlySpaceRow()
{
    /*
     * Test 2: Checks what happens in case of reading an empty space row
    */
    QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();

    QCOMPARE(row_content["field1"].toString(), QString(""));
    QCOMPARE(row_content["field2"].toString(), QString(""));
    QCOMPARE(row_content["field3"].toString(), QString(""));
    QCOMPARE(row_content["field4"].toString(), QString(""));
    QCOMPARE(row_content["field5"].toString(), QString(""));
    QCOMPARE(row_content["field6"].toInt(), 0);
    QCOMPARE(row_content["field7"].toString(), QString(""));
    QCOMPARE(row_content["field8"].toString(), QString(""));
    QCOMPARE(row_content["field9"].toString(), QString(""));
    QCOMPARE(row_content["field10"].toFloat(), float(0.0));
    QCOMPARE(row_content["field11"].toString(), QString(""));
    QCOMPARE(row_content["field12"].toString(), QString(""));
}

void TestFWParser::NoRow()
{
    /*
     *  Test 3:
     *  This test also tests what happens if we have a partial row or a
     *  truncated row. It is simply skipped.
     *
     * It then reaches a point where the file ends.
     * We attempt reading a file after EOF 20 times
    */
    QHash<QString, QVariant> row_content;
    qDebug() << row_content["field12"];

    for (int times = 0; times < 20; times++)
    {
        row_content = test_parser_->ReadNextRow();
        QCOMPARE(row_content["field1"].toString(), QString("Null"));
        QCOMPARE(row_content["field2"].toString(), QString("Null"));
        QCOMPARE(row_content["field3"].toString(), QString("Null"));
        QCOMPARE(row_content["field4"].toString(), QString("Null"));
        QCOMPARE(row_content["field5"].toString(), QString("Null"));
        QCOMPARE(row_content["field6"].toInt(), 0);
        QCOMPARE(row_content["field7"].toString(), QString("Null"));
        QCOMPARE(row_content["field8"].toString(), QString("Null"));
        QCOMPARE(row_content["field9"].toString(), QString("Null"));
        QCOMPARE(row_content["field10"].toFloat(), float(0.0));
        QCOMPARE(row_content["field11"].toString(), QString("Null"));
        QCOMPARE(row_content["field12"].toString(), QString("Null"));
    }
}

void TestFWParser::FWReadMissingFile()
{
    /*
     * Test 4:
     * This tests how the parser reacts if there is no file with the
     * given path.
    */
    QFile::remove(test_file_name_);

    KSParser missing_parser(test_file_name_, '#', sequence_, widths_);
    QHash<QString, QVariant> row_content = missing_parser.ReadNextRow();

    for (int times = 0; times < 20; times++)
    {
        row_content = missing_parser.ReadNextRow();
        QCOMPARE(row_content["field1"].toString(), QString("Null"));
        QCOMPARE(row_content["field2"].toString(), QString("Null"));
        QCOMPARE(row_content["field3"].toString(), QString("Null"));
        QCOMPARE(row_content["field4"].toString(), QString("Null"));
        QCOMPARE(row_content["field5"].toString(), QString("Null"));
        QCOMPARE(row_content["field6"].toInt(), 0);
        QCOMPARE(row_content["field7"].toString(), QString("Null"));
        QCOMPARE(row_content["field8"].toString(), QString("Null"));
        QCOMPARE(row_content["field9"].toString(), QString("Null"));
        QCOMPARE(row_content["field10"].toFloat(), float(0.0));
        QCOMPARE(row_content["field11"].toString(), QString("Null"));
        QCOMPARE(row_content["field12"].toString(), QString("Null"));
    }
}

QTEST_GUILESS_MAIN(TestFWParser)

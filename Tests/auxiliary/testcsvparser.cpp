/*
    SPDX-FileCopyrightText: 2012 Rishab Arora <ra.rishab@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
  * Justification for not testing CombineQuoteParts separately:
  * Changing the structure of KSParser would solve this (by removing the
  * segments to be tested into separate classes) but would unnecessarily
  * complicate things resulting in a large number of small classes.
  * This is good from an OOD perspective, but would make the code unmanageable
*/

#include "testcsvparser.h"

#include <QDir>
#include <QTemporaryFile>

TestCSVParser::TestCSVParser() : QObject()
{
}

void TestCSVParser::initTestCase()
{
    /*
     * Justification for doing this instead of simply creating a file:
     * To add/change tests, we'll need to modify 2 places. The file and this class.
     * So we write the file from the class.
     */
    test_cases_.append("\n");
    test_cases_.append(QString(","
                               "isn't,"
                               "it,"
                               "\"amusing\","
                               "how,"
                               "3,"
                               "\"isn't, pi\","
                               "and,"
                               "\"\","
                               "-3.141,"
                               "isn't,"
                               "either\n"));
    test_cases_.append(QString(","
                               "isn't,"
                               "it,"
                               "\"amusing\","
                               "how,"
                               "3,"
                               "\"isn't\"(, )\"pi\","
                               "and,"
                               "\"\","
                               "-3.141,"
                               "isn't,"
                               "either\n"));
    test_cases_.append(QString(","
                               "isn't,"
                               "it,"
                               "\"amusing\","
                               "how,"
                               "3,"
                               "\"isn't, pi\","
                               "and,"
                               "\"\",")); // less than required fields
    test_cases_.append(QString(","
                               "isn't,"
                               "it,"
                               "\"amusing\","
                               "how,"
                               "3,"
                               "\"isn't, pi\","
                               "and,"
                               "\"," // no matching "
                               "-3.141,"
                               "isn't,"
                               "either\n"));
    test_cases_.append(",,,,,,,,,,,\n");
    test_cases_.append("\n");
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

    test_parser_ = new KSParser(test_file_name_, '#', sequence_);
}

void TestCSVParser::cleanupTestCase()
{
    delete test_parser_;
}

/*
 * The following tests checks for the following cases for CSV files
 *  1. Mixed inputs (See test case for description)
 *  1b. Quoteception
 *  2. Empty Row
 *  3. No row (only a newline character)
 *  4. Truncated row
 *  5. Row with no matching quote
 *  6. Attempt to read missing file
 *
*/

void TestCSVParser::CSVMixedInputs()
{
    /*
     * Test 1. Includes input of the form:
     *
     * It starts with a newline char which should be skipped by virtue
     * of the design of the parser
     *
     * Then a row with the following types of inputs:
     * 1. empty column
     * 2. simple single word
     * 3. single word in quotes
     * 4. multiple words with , in quotes
     * 5. integer
     * 6. float
    */
    QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();
    qDebug() << row_content["field1"];
    QCOMPARE(row_content["field1"].toString(), QString(""));
    QCOMPARE(row_content["field2"].toString(), QString("isn't"));
    QCOMPARE(row_content["field3"].toString(), QString("it"));
    QCOMPARE(row_content["field4"].toString(), QString("amusing"));
    QCOMPARE(row_content["field5"].toString(), QString("how"));
    QCOMPARE(row_content["field6"].toInt(), 3);
    QCOMPARE(row_content["field7"].toString(), QString("isn't, pi"));
    QCOMPARE(row_content["field8"].toString(), QString("and"));
    QCOMPARE(row_content["field9"].toString(), QString(""));
    QVERIFY(row_content["field10"].toFloat() + 3.141 < 0.1);
    QCOMPARE(row_content["field11"].toString(), QString("isn't"));
    QCOMPARE(row_content["field12"].toString(), QString("either"));
}

void TestCSVParser::CSVQuotesInQuotes()
{
    /*
     * Test 1b. Identical to 1 except quotes in quotes
     * in Field 7
     *
     */
    QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();
    qDebug() << row_content["field7"];
    QCOMPARE(row_content["field1"].toString(), QString(""));
    QCOMPARE(row_content["field2"].toString(), QString("isn't"));
    QCOMPARE(row_content["field3"].toString(), QString("it"));
    QCOMPARE(row_content["field4"].toString(), QString("amusing"));
    QCOMPARE(row_content["field5"].toString(), QString("how"));
    QCOMPARE(row_content["field6"].toInt(), 3);
    QCOMPARE(row_content["field7"].toString(), QString("isn't\"(, )\"pi"));
    QCOMPARE(row_content["field8"].toString(), QString("and"));
    QCOMPARE(row_content["field9"].toString(), QString(""));
    QVERIFY(row_content["field10"].toFloat() + 3.141 < 0.1);
    QCOMPARE(row_content["field11"].toString(), QString("isn't"));
    QCOMPARE(row_content["field12"].toString(), QString("either"));
}

void TestCSVParser::CSVEmptyRow()
{
    /* Test 2. Row with less rows than required (to be skipped)
     * Test 3. Row with truncated \" i.e. no matching "
     *         (should be skipped)
    */
    /*
     * Test 4. Attempt to read an empty but valid row
     *
     * Also includes test for:
     * 1. missing integer
     * 2. missing float
     * 3. missing string
    */
    QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();
    qDebug() << row_content["field1"];
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

void TestCSVParser::CSVNoRow()
{
    /*
     * Test 3. Attempt to read a newline char instead of a row
     * The parser is designed to skip an empty row so we can
     * test this for a boundary case. i.e. newline at the end.
    */
    QHash<QString, QVariant> row_content = test_parser_->ReadNextRow();
    qDebug() << row_content["field1"];
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

void TestCSVParser::CSVIgnoreHasNextRow()
{
    QHash<QString, QVariant> row_content;
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

void TestCSVParser::CSVReadMissingFile()
{
    /*
     * Test 6. Attempt to read a missing file repeatedly
    */
    QFile::remove(test_file_name_);

    KSParser missing_parser(test_file_name_, '#', sequence_);
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

QTEST_GUILESS_MAIN(TestCSVParser)

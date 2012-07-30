/***************************************************************************
                 TestKSParser.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/23/07
    copyright            : (C) 2012 by Rishab Arora
    email                : ra.rishab@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*
  * Justification for not testing CombineQuoteParts separately:
  * Changing the structure of KSParser would solve this (by removing the
  * segments to be tested into separate classes) but would unnecessarily
  * complicate things resulting in a large number of small classes.
  * This is good from an OOD perspective, but would make the code unmanageable
*/

#include "testksparser.h"

TestKSParser::TestKSParser(): QObject() {
  /*
   * Justification for doing this instead of simply creating a file:
   * To add/change tests, we'll need to modify 2 places. The file and this class.
   * So we write the file from the class.
   */
  csv_test_cases_.append(QString(","
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
  csv_test_cases_.append("\n");
  csv_test_cases_.append(",,,,,,,,\n");
  QFile test_csv_file;
  QString file_name("TestCSV.txt");
  file_name = KStandardDirs::locateLocal("appdata",file_name);
  if (!file_name.isNull()) {
        test_csv_file.setFileName(file_name);
        if (!test_csv_file.open(QIODevice::WriteOnly)) {
          kWarning() << QString("Couldn't open(%1)").arg(file_name);
        }
  }
  QTextStream out_stream(&test_csv_file);
  foreach(const QString &test_case, csv_test_cases_)
    out_stream << test_case;
  test_csv_file.close();
}


void TestKSParser::stringParse() {
  QList< QPair<QString, KSParser::DataTypes> > sequence;
  sequence.append(qMakePair(QString("field1"), KSParser::D_QSTRING));
  sequence.append(qMakePair(QString("field2"), KSParser::D_QSTRING));
  sequence.append(qMakePair(QString("field3"), KSParser::D_QSTRING));
  sequence.append(qMakePair(QString("field4"), KSParser::D_QSTRING));
  sequence.append(qMakePair(QString("field5"), KSParser::D_QSTRING));
  sequence.append(qMakePair(QString("field6"), KSParser::D_INT));
  sequence.append(qMakePair(QString("field7"), KSParser::D_QSTRING));
  sequence.append(qMakePair(QString("field8"), KSParser::D_QSTRING));
  sequence.append(qMakePair(QString("field9"), KSParser::D_QSTRING));
  sequence.append(qMakePair(QString("field10"), KSParser::D_FLOAT));
  sequence.append(qMakePair(QString("field11"), KSParser::D_QSTRING));
  sequence.append(qMakePair(QString("field12"), KSParser::D_QSTRING));
  KSParser test_parser(QString("TestCSV.txt"), '#', sequence);
  QHash<QString, QVariant> row_content;
  row_content = test_parser.ReadNextRow();
  QVERIFY(row_content["field1"] == QString(""));
  QVERIFY(row_content["field2"] == QString("isn't"));
  QVERIFY(row_content["field3"] == QString("it"));
  QVERIFY(row_content["field4"] == QString("amusing"));
  QVERIFY(row_content["field5"] == QString("how"));
  QVERIFY(row_content["field6"].toInt() == 3);
  QVERIFY(row_content["field7"] == QString("isn't, pi"));
  QVERIFY(row_content["field8"] == QString("and"));
  QVERIFY(row_content["field9"] == QString(""));
  QVERIFY(row_content["field10"].toFloat() + 3.141 < 0.1);
  QVERIFY(row_content["field11"] == QString("isn't"));
  QVERIFY(row_content["field12"] == QString("either"));
}
QTEST_MAIN(TestKSParser)

#include "testksparser.moc"

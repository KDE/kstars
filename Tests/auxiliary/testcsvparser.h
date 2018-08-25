/***************************************************************************
                 TestCSVParser.h  -  K Desktop Planetarium
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

#pragma once

#include "ksparser.h"

#include <QtTest>

class TestCSVParser : public QObject
{
    Q_OBJECT
  public:
    TestCSVParser();
    ~TestCSVParser() override = default;

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void CSVMixedInputs();
    void CSVQuotesInQuotes();
    void CSVEmptyRow();
    void CSVNoRow();
    void CSVIgnoreHasNextRow();
    void CSVReadMissingFile();

  private:
    QStringList test_cases_;
    QList<QPair<QString, KSParser::DataTypes>> sequence_;
    QString test_file_name_;
    KSParser *test_parser_ { nullptr };
};

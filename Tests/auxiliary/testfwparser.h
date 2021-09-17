/*
    SPDX-FileCopyrightText: 2012 Rishab Arora <ra.rishab@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksparser.h"

#include <QDebug>
#include <QtTest>

class TestFWParser : public QObject
{
    Q_OBJECT
  public:
    TestFWParser();
    ~TestFWParser() override = default;
  private slots:
    void initTestCase();
    void cleanupTestCase();
    void MixedInputs();
    void OnlySpaceRow();
    void NoRow();
    void FWReadMissingFile();

  private:
    QStringList test_cases_;
    QList<int> widths_;
    QList<QPair<QString, KSParser::DataTypes>> sequence_;
    QString test_file_name_;
    KSParser *test_parser_ { nullptr };
};

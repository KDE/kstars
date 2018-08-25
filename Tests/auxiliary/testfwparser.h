/***************************************************************************
                          KSParser.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/15/08
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

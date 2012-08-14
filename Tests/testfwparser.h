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

#ifndef TESTFWPARSER_H
#define TESTFWPARSER_H
#include <QtTest/QtTest>
#include <KDebug>
#include "datahandlers/ksparser.h"
#include "kstars/ksfilereader.h"

// TODO(spacetime): documentation
class TestFWParser: public QObject {
  Q_OBJECT
 public:
  TestFWParser();
  ~TestFWParser();
 private slots:
//   void FWMixedInputs();
//   void FWEmptyRow();
//   void FWNoRow();
//   void FWReadMissingFile();

 private:
  QStringList fw_test_cases_;
  QList<int> fw_widths_;
  QList< QPair<QString, KSParser::DataTypes> > sequence_;
  QFile test_fw_file_;
  KSParser *test_parser_;
};

#endif  // TESTFWPARSER_H

/***************************************************************************
                 TestKSParser.h  -  K Desktop Planetarium
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


#ifndef TESTKSPARSER_H
#define TESTKSPARSER_H
#include <QtTest/QtTest>
#include <KDebug>
#include "datahandlers/ksparser.h"
#include "kstars/ksfilereader.h"


class TestKSParser: public QObject {
  Q_OBJECT
 public:
  TestKSParser();
 private slots:
  void stringParse();
 private:
  QStringList csv_test_cases_;
};

#endif  // TESTKSPARSER_H

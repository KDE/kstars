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


#include "testksparser.h"

TestKSParser::TestKSParser(): QObject()
{

}


void TestKSParser::stringParse()
{
  QString str = "hello, world";
  bool test= true;
  QVERIFY(test);
}
QTEST_MAIN( TestKSParser )

#include "testksparser.moc"
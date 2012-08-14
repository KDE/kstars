/***************************************************************************
                          KSParser.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/24/06
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

#include "testfwparser.h"

TestFWParser::TestFWParser(): QObject() {
  fw_test_cases_.append(
    "this is an example of 256 cases being tested 3.141 times repeatedly");
  fw_widths_.append(5);
  fw_widths_.append(3);
  fw_widths_.append(3);
  fw_widths_.append(8);
  fw_widths_.append(3);
  fw_widths_.append(4);
  fw_widths_.append(6);
  fw_widths_.append(6);
  fw_widths_.append(7);
  fw_widths_.append(6);
  fw_widths_.append(6);  //'repeatedly' doesn't need a width
}

TestFWParser::~TestFWParser()
{

}

QTEST_MAIN(TestFWParser)

#include "testfwparser.moc"
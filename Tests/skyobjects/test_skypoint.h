/***************************************************************************
                     test_skypoint.h  -  KStars Planetarium
                             -------------------
    begin                : Tue 27 Sep 2016 20:51:21 CDT
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TEST_SKYPOINT_H
#define TEST_SKYPOINT_H

#include <QtTest/QtTest>
#include <QDebug>

#define UNIT_TEST

#include "skyobjects/skypoint.h"

/**
 * @class TestSkyPoint
 * @short Tests for some SkyPoint operations
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 */

class TestSkyPoint : public QObject
{
    Q_OBJECT

  public:
    TestSkyPoint() : QObject(){};
    ~TestSkyPoint(){};

  private slots:
    void testPrecession();
};

#endif

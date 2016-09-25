/***************************************************************************
                           testcachingdms.h  -  
                             -------------------
    begin                : Sun 25 Sep 2016 03:56:35 CDT
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



#ifndef TESTCACHINGDMS_H
#define TESTCACHINGDMS_H

#include <QtTest/QtTest>
#include <QDebug>

#include "auxiliary/cachingdms.h"

/**
 * @class TestCachingDms
 * @short Tests for CachingDms
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 */

class TestCachingDms : public QObject {

    Q_OBJECT

public:

    /**
     * @short Constructor
     */
    TestCachingDms();

    /**
     * @short Destructor
     */
    ~TestCachingDms();

private slots:
   void defaultCtor();
   void explicitSexigesimalCtor();
   void angleCtor();
   void stringCtor();
   void setUsing_atan2();
   void setUsing_asin();
   void setUsing_acos();
   void additionOperator();
   void subtractionOperator();
   void unaryMinusOperator();
};

#endif

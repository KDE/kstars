/*  KStars UI tests
    Copyright (C) 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "config-kstars.h"

#include <QMutex>
#include <QObject>

class KStars;

class KStarsUiTests : public QObject
{
    Q_OBJECT

  public:
    KStarsUiTests();
    virtual ~KStarsUiTests() = default;

  private slots:
    void initTestCase();
    void cleanupTestCase();
#if defined(HAVE_INDI)
    void openEkos();
    void addEkosProfile();
    void verifyEkosProfile();
    void removeEkosProfile();
#endif
};

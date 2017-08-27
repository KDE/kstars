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

class KStarsLiteUiTests : public QObject
{
    Q_OBJECT

  public:
    KStarsLiteUiTests();
    virtual ~KStarsLiteUiTests() = default;

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void openToolbars();
};

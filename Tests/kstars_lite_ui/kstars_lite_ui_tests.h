/*
    SPDX-FileCopyrightText: 2017 Csaba Kertesz <csaba.kertesz@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

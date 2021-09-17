/*  KStars UI tests
    SPDX-FileCopyrightText: 2021 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTKSPATHS_H
#define TESTKSPATHS_H

#include "config-kstars.h"

#include <QObject>

class TestKSPaths: public QObject
{
    Q_OBJECT
public:
    explicit TestKSPaths(QObject * parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testStandardPaths_data();
    void testStandardPaths();

    void testKStarsInstallation_data();
    void testKStarsInstallation();
};

#endif // TESTKSPATHS_H

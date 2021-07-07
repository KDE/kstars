/*  KStars UI tests
    Copyright (C) 2021
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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

/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TESTCOLORSCHEME_H
#define TESTCOLORSCHEME_H

#include <QtTest>
#include <QObject>

class TestColorScheme : public QObject
{
    Q_OBJECT
public:
    explicit TestColorScheme(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testSchemeColors_data();
    void testSchemeColors();
};

#endif // TESTCOLORSCHEME_H

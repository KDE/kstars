/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTCOLORSCHEME_H
#define TESTCOLORSCHEME_H

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif
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

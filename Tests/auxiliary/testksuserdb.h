/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTKSUSERDB_H
#define TESTKSUSERDB_H

#include <QtTest>
#include <QObject>

class TestKSUserDB : public QObject
{
    Q_OBJECT
public:
    explicit TestKSUserDB(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testInitializeDB();

    void testCreateScopes_data();
    void testCreateScopes();
    void testCreateEyepieces_data();
    void testCreateEyepieces();
    void testCreateLenses_data();
    void testCreateLenes();
    void testCreateFilters_data();
    void testCreateFilters();
    void testCreateProfiles_data();
    void testCreateProfilees();
    void testCreateDatabase();
    void testCoordinates();
};

#endif // TESTKSUSERDB_H

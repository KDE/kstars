/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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

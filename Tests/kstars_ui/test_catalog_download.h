/*  KStars UI tests
    Copyright (C) 2021
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TEST_CATALOG_DOWNLOAD_H
#define TEST_CATALOG_DOWNLOAD_H

#include "config-kstars.h"
#include <QObject>

class TestCatalogDownload: public QObject
{
    Q_OBJECT
public:
    explicit TestCatalogDownload(QObject* parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testCatalogDownloadWhileUpdating();
};

#endif // TEST_CATALOG_DOWNLOAD_H

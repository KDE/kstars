/*  KStars UI tests
    SPDX-FileCopyrightText: 2021 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

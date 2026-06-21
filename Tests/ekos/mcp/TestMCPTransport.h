/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

class TestMCPTransport : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testStartStop();
        void testPostRequest();
        void testInvalidMethod();
        void testUnauthorized();
        void testAuthorized();
        void testSSEConnection();
        void testLargeBody();
        void testRateLimit();
};

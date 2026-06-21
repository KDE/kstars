/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

class TestMCPServer : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testInitialize();
        void testToolsList();
        void testToolsCallUnknown();
        void testToolsCallModuleUnavailable();
        void testInvalidJSON();
        void testMissingMethod();
        void testInvalidJsonRpcVersion();
        void testTokenRegeneration();
        void testAnnotationsEmitted();
        void testReadOnlyModeBlocks();
        void testReadOnlyTokenGates();
};

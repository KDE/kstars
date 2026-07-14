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
        // Disable MCP keychain persistence for the whole run so these tests never
        // read or overwrite the user's real mcp_token / mcp_readonly_token.
        void initTestCase();
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
        void testDisabledTools();
};

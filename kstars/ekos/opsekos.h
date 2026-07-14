/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsekos.h"

class KConfigDialog;

/**
 * @class OpsEkos
 *
 * Enables the user to set remote connection devices and options in addition to online and offline astrometry.net settings.
 * The user can also select to enable or disable audiable alarms upon capturing FITS or when an operation is completed.
 *
 * @author Jasem Mutlaq
 */
class OpsEkos : public QTabWidget, public Ui::OpsEkos
{
        Q_OBJECT

    public:
        explicit OpsEkos();
        ~OpsEkos() = default;

    public slots:
        void setMCPState(bool listening, quint16 port, const QString &error = QString());
        void refreshMCPTokens();
        // Populate the "Available tools" tree from the live MCP::ToolRegistry.
        // Groups by family prefix (mount_*, capture_*, ...). Tool annotations
        // (read-only / destructive / idempotent / openWorld) shown as icons +
        // tooltips. Each leaf carries a checkbox (family roots are tristate)
        // to enable/disable the tool; unchecked names persist in the
        // MCPDisabledTools option. Called on initial dialog construction and
        // whenever the MCP server is (re)created.
        void populateMCPTools();

    private slots:
        // Persist checkbox changes from the tools tree into MCPDisabledTools.
        void onMCPToolItemChanged();

    private:
        KConfigDialog *m_ConfigDialog;
        // Coalesces the burst of itemChanged signals a family toggle emits.
        bool m_MCPToolsSavePending { false };
};

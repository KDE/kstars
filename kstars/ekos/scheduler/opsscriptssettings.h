/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsscriptssettings.h"

class KConfigDialog;

namespace Ekos
{

class OpsScriptsSettings : public QWidget, public Ui::OpsScriptsSettings
{
        Q_OBJECT

    public:
        explicit OpsScriptsSettings();
        virtual ~OpsScriptsSettings() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

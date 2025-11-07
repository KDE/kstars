/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsweathersettings.h"

class KConfigDialog;

namespace Ekos
{

class OpsWeatherSettings : public QFrame, public Ui::OpsWeatherSettings
{
        Q_OBJECT

    public:
        explicit OpsWeatherSettings();
        virtual ~OpsWeatherSettings() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

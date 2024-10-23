/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opscleanupsettings.h"
#include <QFrame>

class KConfigDialog;

namespace Ekos
{

class OpsCleanupSettings : public QFrame, public Ui::OpsCleanupSettings
{
        Q_OBJECT

    public:
        explicit OpsCleanupSettings();
        virtual ~OpsCleanupSettings() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

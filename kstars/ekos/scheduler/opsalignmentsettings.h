/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsalignmentsettings.h"

class KConfigDialog;

namespace Ekos
{

class OpsAlignmentSettings : public QFrame, public Ui::OpsAlignmentSettings
{
        Q_OBJECT

    public:
        explicit OpsAlignmentSettings();
        virtual ~OpsAlignmentSettings() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

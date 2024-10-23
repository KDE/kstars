/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsoffsetsettings.h"

class KConfigDialog;

namespace Ekos
{

class OpsOffsetSettings : public QFrame, public Ui::OpsOffsetSettings
{
        Q_OBJECT

    public:
        explicit OpsOffsetSettings();
        virtual ~OpsOffsetSettings() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

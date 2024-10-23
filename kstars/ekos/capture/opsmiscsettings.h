/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsmiscsettings.h"

class KConfigDialog;

namespace Ekos
{

class OpsMiscSettings : public QFrame, public Ui::OpsMiscSettings
{
        Q_OBJECT

    public:
        explicit OpsMiscSettings();
        virtual ~OpsMiscSettings() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

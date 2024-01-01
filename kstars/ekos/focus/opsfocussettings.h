/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsfocussettings.h"

class KConfigDialog;

namespace Ekos
{

class OpsFocusSettings : public QFrame, public Ui::OpsFocusSettings
{
        Q_OBJECT

    public:
        explicit OpsFocusSettings();
        virtual ~OpsFocusSettings() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

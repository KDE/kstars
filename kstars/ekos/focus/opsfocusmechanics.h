/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsfocusmechanics.h"

class KConfigDialog;

namespace Ekos
{

class OpsFocusMechanics : public QFrame, public Ui::OpsFocusMechanics
{
        Q_OBJECT

    public:
        explicit OpsFocusMechanics();
        virtual ~OpsFocusMechanics() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

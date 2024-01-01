/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsfocusprocess.h"

class KConfigDialog;

namespace Ekos
{

class OpsFocusProcess : public QFrame, public Ui::OpsFocusProcess
{
        Q_OBJECT

    public:
        explicit OpsFocusProcess();
        virtual ~OpsFocusProcess() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

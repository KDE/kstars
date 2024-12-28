/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opsfocusbase.h"
#include "ui_opsfocusprocess.h"

namespace Ekos
{

class OpsFocusProcess : public OpsFocusBase, public Ui::OpsFocusProcess
{
        Q_OBJECT

    public:
        explicit OpsFocusProcess(const QString name);
        virtual ~OpsFocusProcess() override = default;
};
}

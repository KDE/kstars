/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opsfocusbase.h"
#include "ui_opsfocusmechanics.h"

namespace Ekos
{

class OpsFocusMechanics : public OpsFocusBase, public Ui::OpsFocusMechanics
{
        Q_OBJECT

    public:
        explicit OpsFocusMechanics(const QString name);
        virtual ~OpsFocusMechanics() override = default;
};
}

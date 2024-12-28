/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opsfocusbase.h"
#include "ui_opsfocussettings.h"

class KConfigDialog;

namespace Ekos
{

class OpsFocusSettings : public OpsFocusBase, public Ui::OpsFocusSettings
{
        Q_OBJECT

    public:
        explicit OpsFocusSettings(const QString name);
        virtual ~OpsFocusSettings() override = default;
};
}

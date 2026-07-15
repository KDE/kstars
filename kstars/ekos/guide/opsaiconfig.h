/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsaiconfig.h"
#include <QFrame>

namespace Ekos
{

class OpsAIConfig : public QFrame, public Ui::OpsAIConfig
{
        Q_OBJECT

    public:
        explicit OpsAIConfig();
        virtual ~OpsAIConfig() override = default;
};

}

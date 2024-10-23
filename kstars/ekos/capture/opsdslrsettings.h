/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsdslrsettings.h"
#include <QFrame>

class KConfigDialog;

namespace Ekos
{

class OpsDslrSettings : public QFrame, public Ui::OpsDslrSettings
{
        Q_OBJECT

    public:
        explicit OpsDslrSettings();
        virtual ~OpsDslrSettings() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

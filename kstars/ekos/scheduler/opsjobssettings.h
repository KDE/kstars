/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsjobssettings.h"
#include <QFrame>

class KConfigDialog;

namespace Ekos
{

class OpsJobsSettings : public QFrame, public Ui::OpsJobsSettings
{
        Q_OBJECT

    public:
        explicit OpsJobsSettings();
        virtual ~OpsJobsSettings() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
}

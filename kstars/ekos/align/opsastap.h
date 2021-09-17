/*
    SPDX-FileCopyrightText: 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsastap.h"

#include <QWidget>

class KConfigDialog;

namespace Ekos
{
class Align;

class OpsASTAP : public QWidget, public Ui::OpsASTAP
{
        Q_OBJECT

    public:
        explicit OpsASTAP(Align *parent);
        virtual ~OpsASTAP() override = default;

    private slots:
        void slotApply();
        void slotSelectExecutable();

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
        Align *alignModule { nullptr };


};
}

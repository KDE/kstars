/*  ASTAP Options Editor
    Copyright (C) 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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

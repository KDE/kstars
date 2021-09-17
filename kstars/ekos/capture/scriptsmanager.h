/*
    SPDX-FileCopyrightText: 2020 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include "ekos/ekos.h"

namespace Ui
{
class ScriptsManager;
}

namespace Ekos
{
class ScriptsManager : public QDialog
{
        Q_OBJECT

    public:
        explicit ScriptsManager(QWidget *parent = nullptr);
        ~ScriptsManager();

        void setScripts(const QMap<ScriptTypes, QString> &scripts);
        QMap<ScriptTypes, QString> getScripts();

    private:
        void handleSelectScripts();
        Ui::ScriptsManager *ui;
};

}


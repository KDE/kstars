/*  Script Manager
    Copyright (C) 2020 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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


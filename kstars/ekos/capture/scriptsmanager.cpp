/*
    SPDX-FileCopyrightText: 2020 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFileDialog>
#include <QFileInfo>

#include "scriptsmanager.h"
#include "ui_scriptsmanager.h"
#include "auxiliary/ksnotification.h"

#include "scriptsmanager.h"
#include "ui_scriptsmanager.h"

namespace Ekos
{
ScriptsManager::ScriptsManager(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ScriptsManager)
{
    ui->setupUi(this);

    connect(ui->preJobB, &QPushButton::clicked, this, &ScriptsManager::handleSelectScripts);
    connect(ui->postJobB, &QPushButton::clicked, this, &ScriptsManager::handleSelectScripts);
    connect(ui->preCaptureB, &QPushButton::clicked, this, &ScriptsManager::handleSelectScripts);
    connect(ui->postCaptureB, &QPushButton::clicked, this, &ScriptsManager::handleSelectScripts);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ScriptsManager::handleSelectScripts()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button)
        return;

    QString title;
    if (button == ui->preJobB)
        title = i18nc("@title:window", "Pre Job Script");
    else if (button == ui->postJobB)
        title = i18nc("@title:window", "Post Job Script");
    else if (button == ui->postCaptureB)
        title = i18nc("@title:window", "Post Capture Script");
    else if (button == ui->preCaptureB)
        title = i18nc("@title:window", "Pre Capture Script");

    QUrl url = QFileDialog::getOpenFileUrl(this, title);
    if (!url.isValid())
        return;

    QString localFile = url.toLocalFile();
    QFileInfo info(localFile);

    if (info.permission(QFileDevice::ExeOwner) == false)
    {
        KSNotification::error(i18n("File %1 is not executable.", localFile));
        return;
    }

    if (button == ui->preJobB)
        ui->preJobScript->setText(localFile);
    else if (button == ui->postJobB)
        ui->postJobScript->setText(localFile);
    else if (button == ui->postCaptureB)
        ui->postCaptureScript->setText(localFile);
    else if (button == ui->preCaptureB)
        ui->preCaptureScript->setText(localFile);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ScriptsManager::setScripts(const QMap<ScriptTypes, QString> &scripts)
{
    if (scripts.contains(SCRIPT_PRE_JOB))
        ui->preJobScript->setText(scripts[SCRIPT_PRE_JOB]);

    if (scripts.contains(SCRIPT_POST_JOB))
        ui->postJobScript->setText(scripts[SCRIPT_POST_JOB]);

    if (scripts.contains(SCRIPT_PRE_CAPTURE))
        ui->preCaptureScript->setText(scripts[SCRIPT_PRE_CAPTURE]);

    if (scripts.contains(SCRIPT_POST_CAPTURE))
        ui->postCaptureScript->setText(scripts[SCRIPT_POST_CAPTURE]);
}

QMap<ScriptTypes, QString> ScriptsManager::getScripts()
{
    QMap<ScriptTypes, QString> scripts;

    scripts[SCRIPT_PRE_JOB] = ui->preJobScript->text();
    scripts[SCRIPT_POST_JOB] = ui->postJobScript->text();
    scripts[SCRIPT_PRE_CAPTURE] = ui->preCaptureScript->text();
    scripts[SCRIPT_POST_CAPTURE] = ui->postCaptureScript->text();

    return scripts;
}

ScriptsManager::~ScriptsManager()
{
    delete ui;
}

}

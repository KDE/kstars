/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "opsalign.h"

#include "align.h"
#include "fov.h"
#include "kstars.h"
#include "ksnotification.h"
#include "Options.h"

#include <KConfigDialog>
#include <QProcess>

namespace Ekos
{
OpsAlign::OpsAlign(Align *parent) : QWidget(KStars::Instance())
{
    setupUi(this);

    alignModule = parent;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("alignsettings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
    connect(SetupPython, SIGNAL(clicked()), this, SLOT(setupPython()));

#ifdef Q_OS_OSX
    connect(kcfg_AstrometrySolverIsInternal, SIGNAL(clicked()), this, SLOT(toggleSolverInternal()));
    kcfg_AstrometrySolverIsInternal->setToolTip(i18n("Internal or External Plate Solver?"));
    if (Options::astrometrySolverIsInternal())
        kcfg_AstrometrySolverBinary->setEnabled(false);

    connect(kcfg_AstrometryConfFileIsInternal, SIGNAL(clicked()), this, SLOT(toggleConfigInternal()));
    kcfg_AstrometryConfFileIsInternal->setToolTip(i18n("Internal or External astrometry.cfg?"));
    if (Options::astrometryConfFileIsInternal())
        kcfg_AstrometryConfFile->setEnabled(false);

    connect(kcfg_AstrometryWCSIsInternal, SIGNAL(clicked()), this, SLOT(toggleWCSInternal()));
    kcfg_AstrometryWCSIsInternal->setToolTip(i18n("Internal or External wcsinfo?"));
    if (Options::astrometryWCSIsInternal())
        kcfg_AstrometryWCSInfo->setEnabled(false);

    connect(kcfg_UseDefaultPython, SIGNAL(clicked()), this, SLOT(togglePythonDefault()));
    kcfg_PythonExecPath->setVisible(!Options::useDefaultPython());
    SetupPython->setVisible(Options::useDefaultPython());

#else
    kcfg_AstrometrySolverIsInternal->setVisible(false);
    kcfg_AstrometryConfFileIsInternal->setVisible(false);
    kcfg_AstrometryWCSIsInternal->setVisible(false);

    kcfg_UseDefaultPython->setVisible(false);
    pythonLabel->setVisible(false);
    SetupPython->setVisible(false);
    kcfg_PythonExecPath->setVisible(false);

#endif

#ifdef Q_OS_WIN
    kcfg_AstrometrySolverBinary->setEnabled(false);
    kcfg_AstrometryWCSInfo->setEnabled(false);
    kcfg_AstrometryConfFile->setEnabled(false);
#endif
}

void OpsAlign::setupPython()
{
    if(brewInstalled() && pythonInstalled() && astropyInstalled())
    {
        KSNotification::info(i18n("Homebrew, python, and astropy are already installed"));
        return;
    }

    if (KMessageBox::questionYesNo(nullptr, i18n("This installer will install the following requirements for astrometry.net if they are not installed:\nHomebrew -an OS X Unix Program Package Manager\nPython3 -A Powerful Scripting Language \nAstropy -Python Modules for Astronomy \n Do you wish to continue?"),
                                   i18n("Install and Configure Python")) == KMessageBox::Yes)
    {
        QProcess* install = new QProcess(this);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString path            = env.value("PATH", "");
        env.insert("PATH", "/usr/local/opt/python/libexec/bin:/usr/local/bin:" + path);
        install->setProcessEnvironment(env);

        if(!brewInstalled())
        {
            KSNotification::info(i18n("Homebrew is not installed.  \nA Terminal window will pop up for you to install Homebrew.  \n When you are all done, then you can close the Terminal and click the setup button again."));
            QStringList installArgs;
            QString homebrewInstallScript =
                "tell application \"Terminal\"\n"
                "    do script \"/usr/bin/ruby -e \\\"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)\\\"\"\n"
                "end tell\n";
            QString bringToFront =
                "tell application \"Terminal\"\n"
                "    activate\n"
                "end tell\n";

            QStringList processArguments;
            processArguments << "-l" << "AppleScript";
            install->start("/usr/bin/osascript", processArguments);
            install->write(homebrewInstallScript.toUtf8());
            install->write(bringToFront.toUtf8());
            install->closeWriteChannel();
            install->waitForFinished();
            return;
        }
        if(!pythonInstalled())
        {
            KSNotification::info(i18n("Homebrew installed \nPython3 will install when you click Ok \nAstropy waiting . . . \n (Note: this might take a few minutes, please be patient.)"));
            install->start("/usr/local/bin/brew", QStringList() << "install" << "python3");
            install->waitForFinished();
            if(!pythonInstalled())
            {
                KSNotification::info(i18n("Python install failure"));
                return;
            }
        }
        if(!astropyInstalled())
        {
            KSNotification::info(i18n("Homebrew installed \nPython3 installed \nAstropy will install when you click Ok \n (Note: this might take a few minutes, please be patient.)"));
            install->start("/usr/local/bin/pip3", QStringList() << "install" << "astropy");
            install->waitForFinished();
            if(!astropyInstalled())
            {
                KSNotification::info(i18n("Astropy install failure"));
                return;
            }
        }
        KSNotification::info(i18n("All installations are complete and ready to use."));
    }
}

bool OpsAlign::brewInstalled()
{
    return QFileInfo("/usr/local/bin/brew").exists();
}
bool OpsAlign::pythonInstalled()
{
    return QFileInfo("/usr/local/bin/python3").exists();
}
bool OpsAlign::astropyInstalled()
{
    QProcess testAstropy;
    testAstropy.start("/usr/local/bin/pip3 list");
    testAstropy.waitForFinished();
    QString listPip(testAstropy.readAllStandardOutput());
    qDebug() << listPip;
    return listPip.contains("astropy", Qt::CaseInsensitive);
}

void OpsAlign::toggleSolverInternal()
{
    kcfg_AstrometrySolverBinary->setEnabled(!kcfg_AstrometrySolverIsInternal->isChecked());
    if (kcfg_AstrometrySolverIsInternal->isChecked())
        kcfg_AstrometrySolverBinary->setText("*Internal Solver*");
    else
        kcfg_AstrometrySolverBinary->setText(KSUtils::getDefaultPath("AstrometrySolverBinary"));
}

void OpsAlign::toggleConfigInternal()
{
    kcfg_AstrometryConfFile->setEnabled(!kcfg_AstrometryConfFileIsInternal->isChecked());
    if (kcfg_AstrometryConfFileIsInternal->isChecked())
        kcfg_AstrometryConfFile->setText("*Internal astrometry.cfg*");
    else
        kcfg_AstrometryConfFile->setText(KSUtils::getDefaultPath("AstrometryConfFile"));
}

void OpsAlign::toggleWCSInternal()
{
    kcfg_AstrometryWCSInfo->setEnabled(!kcfg_AstrometryWCSIsInternal->isChecked());
    if (kcfg_AstrometryWCSIsInternal->isChecked())
        kcfg_AstrometryWCSInfo->setText("*Internal wcsinfo*");
    else
        kcfg_AstrometryWCSInfo->setText(KSUtils::getDefaultPath("AstrometryWCSInfo"));
}

void OpsAlign::togglePythonDefault()
{
    bool defaultPython = kcfg_UseDefaultPython->isChecked();
    kcfg_PythonExecPath->setVisible(!defaultPython);
    SetupPython->setVisible(defaultPython);
}

void OpsAlign::slotApply()
{
    emit settingsUpdated();
}
}

/*  Astrometry.net Parser - Remote
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "astapastrometryparser.h"

#include "align.h"
#include "ekos_align_debug.h"
#include "Options.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/guimanager.h"
#include "indi/indidevice.h"

#include <indicom.h>

#include <KMessageBox>

namespace Ekos
{
ASTAPAstrometryParser::ASTAPAstrometryParser() : AstrometryParser()
{
}

bool ASTAPAstrometryParser::init()
{
    return QFile::exists(Options::aSTAPExecutable());
}

void ASTAPAstrometryParser::verifyIndexFiles(double, double)
{
}

bool ASTAPAstrometryParser::startSovler(const QString &filename, const QStringList &args, bool generated)
{
    INDI_UNUSED(generated);

    QStringList solverArgs = args;

    solverArgs << "-f" << filename;

    QString solutionFile = QDir::tempPath() + "/solution";
    solverArgs << "-o" << solutionFile;

    solver.clear();
    solver = new QProcess(this);

    connect(solver, static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus exitStatus)>(&QProcess::finished),
            this, &ASTAPAstrometryParser::solverComplete);
    //    solver->setProcessChannelMode(QProcess::MergedChannels);
    //    connect(solver, SIGNAL(readyReadStandardOutput()), this, SLOT(logSolver()));
#if QT_VERSION > QT_VERSION_CHECK(5, 6, 0)
    connect(solver.data(), &QProcess::errorOccurred, this, [&]()
    {
        align->appendLogText(i18n("Error starting solver: %1", solver->errorString()));
        emit solverFailed();
    });
#else
    connect(solver, SIGNAL(error(QProcess::ProcessError)), this, SIGNAL(solverFailed()));
#endif

    solverTimer.start();

    QString solverPath = Options::aSTAPExecutable();

    solver->start(solverPath, solverArgs);

    align->appendLogText(i18n("Starting solver..."));

    if (Options::alignmentLogging())
    {
        QString command = solverPath + ' ' + solverArgs.join(' ');
        align->appendLogText(command);
    }

    return true;
}

void ASTAPAstrometryParser::solverComplete(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    // TODO

}

bool ASTAPAstrometryParser::stopSolver()
{
    // TODO
    return true;
}
}

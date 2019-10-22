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

    QFile solution(QDir::tempPath() + "/solution.ini");

    if (!solution.open(QIODevice::ReadOnly))
    {
        qCritical(KSTARS_EKOS_ALIGN) << "Failed to open solution file" << QDir::tempPath() + "/solution.ini";
        emit solverFailed();
        return;
    }

    QTextStream in(&solution);
    QString line = in.readLine();

    QStringList ini = line.split("=");
    if (ini[1] == "F")
    {
        align->appendLogText(i18n("Solver failed. Try again."));
        emit solverFailed();
        return;
    }

    double ra = 0, dec = 0, orientation = 0, pixscale = 0;
    bool ok[4] = {false};

    line = in.readLine();
    while (!line.isNull())
    {
        QStringList ini = line.split("=");
        if (ini[0] == "CRVAL1")
            ra = ini[1].trimmed().toDouble(&ok[0]);
        else if (ini[0] == "CRVAL2")
            dec = ini[1].trimmed().toDouble(&ok[1]);
        else if (ini[0] == "CDELT1")
            pixscale = ini[1].trimmed().toDouble(&ok[2]) * 3600.0;
        else if (ini[0] == "CROTA1")
            orientation = ini[1].trimmed().toDouble(&ok[3]);

        line = in.readLine();
    }

    if (ok[0] && ok[1] && ok[2] && ok[3])
    {
        int elapsed = static_cast<int>(round(solverTimer.elapsed() / 1000.0));
        align->appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));
        emit solverFinished(orientation, ra, dec, pixscale);
    }
    else
    {
        align->appendLogText(i18n("Solver failed. Try again."));
        emit solverFailed();
    }
}

bool ASTAPAstrometryParser::stopSolver()
{
    if (solver.isNull() == false)
    {
        solver->terminate();
        solver->disconnect();
    }

    return true;
}
}

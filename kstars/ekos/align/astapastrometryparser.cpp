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

    align->appendLogText(i18n("Starting ASTAP solver..."));
    solverTimer.start();

    return true;
}

bool ASTAPAstrometryParser::stopSolver()
{   
    solverRunning = false;
    return true;
}
}

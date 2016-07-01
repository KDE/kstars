/*  Astrometry.net Parser - Remote
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <QDir>

#include <KMessageBox>
#include <KLocalizedString>
#include "Options.h"

#include <basedevice.h>
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"

#include "remoteastrometryparser.h"
#include "align.h"


namespace Ekos
{

RemoteAstrometryParser::RemoteAstrometryParser() : AstrometryParser()
{
    currentCCD = NULL;
    solverRunning=false;
}

RemoteAstrometryParser::~RemoteAstrometryParser()
{
}

bool RemoteAstrometryParser::init()
{    
    return true;
}

void RemoteAstrometryParser::verifyIndexFiles(double, double)
{
}

bool RemoteAstrometryParser::startSovler(const QString &filename,  const QStringList &args, bool generated)
{
    INDI_UNUSED(filename);
    INDI_UNUSED(generated);

    solverRunning = true;

    ITextVectorProperty *solverSettings = currentCCD->getBaseDevice()->getText("ASTROMETRY_SETTINGS");
    ISwitchVectorProperty *solverSwitch = currentCCD->getBaseDevice()->getSwitch("ASTROMETRY_SOLVER");

    if (solverSettings == NULL || solverSwitch == NULL)
    {
        align->appendLogText(i18n("CCD does not support remote solver."));
        emit solverFailed();
        return false;
    }

    for (int i=0; i < solverSettings->ntp; i++)
    {
        if (!strcmp(solverSettings->tp[i].name, "ASTROMETRY_SETTINGS_BINARY"))
            IUSaveText(&solverSettings->tp[i], Options::astrometrySolver().toLatin1().constData());
        else if (!strcmp(solverSettings->tp[i].name, "ASTROMETRY_SETTINGS_OPTIONS"))
            IUSaveText(&solverSettings->tp[i], args.join(" ").toLatin1().constData());
    }

    currentCCD->getDriverInfo()->getClientManager()->sendNewText(solverSettings);

    ISwitch *enableSW = IUFindSwitch(solverSwitch, "ASTROMETRY_SOLVER_ENABLE");
    if (enableSW->s == ISS_OFF)
    {
        IUResetSwitch(solverSwitch);
        enableSW->s = ISS_ON;
        currentCCD->getDriverInfo()->getClientManager()->sendNewSwitch(solverSwitch);
    }

    align->appendLogText(i18n("Starting remote solver..."));
    solverTimer.start();

    return true;
}

bool RemoteAstrometryParser::stopSolver()
{
    // Disable solver
    ISwitchVectorProperty *svp = currentCCD->getBaseDevice()->getSwitch("ASTROMETRY_SOLVER");
    if (!svp)
        return false;

    ISwitch *disableSW = IUFindSwitch(svp, "ASTROMETRY_SOLVER_DISABLE");
    if (disableSW->s == ISS_OFF)
    {
        IUResetSwitch(svp);
        disableSW->s = ISS_ON;
        currentCCD->getDriverInfo()->getClientManager()->sendNewSwitch(svp);
    }

    solverRunning=false;

    return true;

}

void RemoteAstrometryParser::setCCD(ISD::CCD *ccd)
{
    if (ccd == currentCCD)
        return;

    currentCCD = ccd;

    currentCCD->disconnect(this);

    connect(currentCCD, SIGNAL(switchUpdated(ISwitchVectorProperty*)), this, SLOT(checkCCDStatus(ISwitchVectorProperty*)));
    connect(currentCCD, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(checkCCDResults(INumberVectorProperty*)));
}

void RemoteAstrometryParser::checkCCDStatus(ISwitchVectorProperty *svp)
{
    if (solverRunning == false || strcmp(svp->name, "ASTROMETRY_SOLVER"))
        return;

    if (svp->s == IPS_ALERT)
    {
        stopSolver();
        align->appendLogText(i18n("Solver failed. Try again."));
        emit solverFailed();
        return;
    }
}

void RemoteAstrometryParser::checkCCDResults(INumberVectorProperty * nvp)
{
    if (solverRunning == false || strcmp(nvp->name, "ASTROMETRY_RESULTS") || nvp->s != IPS_OK)
        return;

    double pixscale, ra, de, orientation;
    pixscale=ra=de=orientation=-1000;

    for (int i=0; i < nvp->nnp; i++)
    {
        if (!strcmp(nvp->np[i].name, "ASTROMETRY_RESULTS_PIXSCALE"))
            pixscale = nvp->np[i].value;
        else if (!strcmp(nvp->np[i].name, "ASTROMETRY_RESULTS_ORIENTATION"))
            orientation = nvp->np[i].value;
        else if (!strcmp(nvp->np[i].name, "ASTROMETRY_RESULTS_RA"))
            ra = nvp->np[i].value;
        else if (!strcmp(nvp->np[i].name, "ASTROMETRY_RESULTS_DE"))
            de = nvp->np[i].value;
    }

    if (pixscale != -1000 && ra != -1000 && de != -1000 && orientation != -1000)
    {
        int elapsed = (int) round(solverTimer.elapsed()/1000.0);
        align->appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));
        stopSolver();
        emit solverFinished(orientation, ra, de, pixscale);
    }
}

}




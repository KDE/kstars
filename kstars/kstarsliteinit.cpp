/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kstarslite.h"
#include "skymaplite.h"
#include "kstarsdata.h"

#include "skycomponents/skymapcomposite.h"

#include "Options.h"

void KStarsLite::datainitFinished()
{
    //Time-related connections
    connect(data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(updateTime()));
    connect(data()->clock(), SIGNAL(timeChanged()), this, SLOT(updateTime()));

    //Add GUI elements to main window
    //buildGUI();

    connect(data()->clock(), SIGNAL(scaleChanged(float)), map(), SLOT(slotClockSlewing()));

    connect(data(), SIGNAL(skyUpdate(bool)), map(), SLOT(forceUpdateNow()));
    connect(this, SIGNAL(scaleChanged(float)), data(), SLOT(setTimeDirection(float)));
    connect(this, SIGNAL(scaleChanged(float)), data()->clock(), SLOT(setClockScale(float)));
    //connect( this, SIGNAL(scaleChanged(float)), map(),  SLOT(setFocus()) ); Why did we need this connection in KStars?

    //Do not start the clock if "--paused" specified on the cmd line
    if (StartClockRunning)
        data()->clock()->start();

    //Propagate config settings
    applyConfig(false);

    //Initialize focus
    initFocus();

    data()->setFullTimeUpdate();
    updateTime();

    //If this is the first startup, show the wizard
    if (Options::runStartupWizard())
    {
    }

    //DEBUG
    qDebug() << "The current Date/Time is: " << KStarsDateTime::currentDateTime().toString();

    //Notify Splash in QML and LocationDialogLite that loading of data is finished
    dataLoadFinished();
    map()->forceUpdate();

    //Default options
    Options::setShowEquator(true);
    Options::setShowHorizon(true);
    Options::setShowEcliptic(true);
    Options::setAutoSelectGrid(false);
}

void KStarsLite::initFocus()
{
    //Case 1: tracking on an object
    if (Options::isTracking() && Options::focusObject() != i18n("nothing"))
    {
        SkyObject *oFocus;
        if (Options::focusObject() == i18n("star"))
        {
            SkyPoint p(Options::focusRA(), Options::focusDec());
            double maxrad = 1.0;

            oFocus = data()->skyComposite()->starNearest(&p, maxrad);
        }
        else
        {
            oFocus = data()->objectNamed(Options::focusObject());
        }

        if (oFocus)
        {
            map()->setFocusObject(oFocus);
            map()->setClickedObject(oFocus);
            map()->setFocusPoint(oFocus);
        }
        else
        {
            qWarning() << "Cannot center on " << Options::focusObject() << ": no object found." << endl;
        }

        //Case 2: not tracking, and using Alt/Az coords.  Set focus point using
        //FocusRA as the Azimuth, and FocusDec as the Altitude
    }
    else if (!Options::isTracking() && Options::useAltAz())
    {
        SkyPoint pFocus;
        pFocus.setAz(Options::focusRA());
        pFocus.setAlt(Options::focusDec());
        pFocus.HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
        map()->setFocusPoint(&pFocus);

        //Default: set focus point using FocusRA as the RA and
        //FocusDec as the Dec
    }
    else
    {
        SkyPoint pFocus(Options::focusRA(), Options::focusDec());
        pFocus.EquatorialToHorizontal(data()->lst(), data()->geo()->lat());
        map()->setFocusPoint(&pFocus);
    }
    data()->setSnapNextFocus();
    map()->setDestination(*map()->focusPoint());
    map()->setFocus(map()->destination());

    //map()->showFocusCoords();

    //Check whether initial position is below the horizon.
    if (Options::useAltAz() && Options::showGround() && map()->focus()->alt().Degrees() <= SkyPoint::altCrit)
    {
        QString caption = i18n("Initial Position is Below Horizon");
        QString message =
            i18n("The initial position is below the horizon.\nWould you like to reset to the default position?");
        map()->setClickedObject(nullptr);
        map()->setFocusObject(nullptr);
        Options::setIsTracking(false);

        data()->setSnapNextFocus(true);

        SkyPoint DefaultFocus;
        DefaultFocus.setAz(180.0);
        DefaultFocus.setAlt(45.0);
        DefaultFocus.HorizontalToEquatorial(data()->lst(), data()->geo()->lat());
        map()->setDestination(DefaultFocus);
    }

    //If there is a focusObject() and it is a SS body, add a temporary Trail
    /*if ( map()->focusObject() && map()->focusObject()->isSolarSystem()
                && Options::useAutoTrail() ) {
            ((KSPlanetBase*)map()->focusObject())->addToTrail();
            data()->temporaryTrail = true;
        }*/
}

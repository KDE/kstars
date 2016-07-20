/***************************************************************************
                          opsadvanced.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 14 Mar 2004
    copyright            : (C) 2004 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "opsadvanced.h"
#include "config-kstars.h"

#include <QLabel>
#include <QCheckBox>
#include <QRadioButton>

#include "Options.h"
#include "kstars.h"
#include "ksutils.h"
#include "widgets/timestepbox.h"
#include "kspaths.h"

OpsAdvanced::OpsAdvanced()
        : QFrame(KStars::Instance())
{
    setupUi( this );

    #ifdef HAVE_CFITSIO
    FITSViewerGroup->setEnabled(true);
    #endif

    //Initialize the timestep value
    SlewTimeScale->tsbox()->changeScale( Options::slewTimeScale() );

    connect( SlewTimeScale, SIGNAL( scaleChanged( float ) ), this, SLOT( slotChangeTimeScale( float ) ) );

    connect( kcfg_HideOnSlew, SIGNAL( clicked() ), this, SLOT( slotToggleHideOptions() ) );

    connect (kcfg_VerboseLogging, SIGNAL(toggled(bool)), this, SLOT(slotToggleVerbosityOptions()));

    connect(kcfg_LogToFile, SIGNAL(toggled(bool)), this, SLOT(slotToggleOutputOptions()));

    foreach(QAbstractButton *b, modulesGroup->buttons())
        b->setEnabled(kcfg_VerboseLogging->isChecked());
}

OpsAdvanced::~OpsAdvanced() {}

void OpsAdvanced::slotChangeTimeScale( float newScale ) {
    Options::setSlewTimeScale( newScale );
}

void OpsAdvanced::slotToggleHideOptions() {
    textLabelHideTimeStep->setEnabled( kcfg_HideOnSlew->isChecked() );
    SlewTimeScale->setEnabled( kcfg_HideOnSlew->isChecked() );
    HideBox->setEnabled( kcfg_HideOnSlew->isChecked() );
}

void OpsAdvanced::slotToggleVerbosityOptions()
{
    if (kcfg_DisableLogging->isChecked())
        KSUtils::Logging::Disable();

    foreach(QAbstractButton *b, modulesGroup->buttons())
    {
        b->setEnabled(kcfg_VerboseLogging->isChecked());
        // If verbose is not checked, CLEAR all selections
        b->setChecked(kcfg_VerboseLogging->isChecked() ? b->isChecked() : false);
    }
}

void OpsAdvanced::slotToggleOutputOptions()
{
    if (kcfg_LogToDefault->isChecked())
    {        

        if (kcfg_DisableLogging->isChecked() == false)
            KSUtils::Logging::UseDefault();
    }
    else    
            KSUtils::Logging::UseFile();
}


/*  INDI Group
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    JM Changelog
    2004-16-1:	Start
   
 */

#include "indigroup.h"
#include "indiproperty.h"
#include "indidevice.h"
#include "devicemanager.h"
#include "indimenu.h"

#include <klocale.h>
#include <kdialog.h>

#include <QFrame>
#include <QTimer>
#include <QTabWidget>
#include <QVBoxLayout>

/*******************************************************************
** INDI Group: a tab widget for common properties. All properties
** belong to a group, whether they have one or not but how the group
** is displayed differs
*******************************************************************/
INDI_G::INDI_G(INDI_D *parentDevice, const QString &inName)
{
    dp = parentDevice;

    name = inName;

    //propertyContainer = new QFrame(dp->groupContainer);
    propertyContainer = new QFrame();
    propertyLayout    = new QVBoxLayout(propertyContainer);
    propertyLayout->setMargin(20);
    propertyLayout->setSpacing(KDialog::spacingHint());
    VerticalSpacer    = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

    propertyLayout->addItem(VerticalSpacer);

    dp->groupContainer->addTab(propertyContainer, name);
}

INDI_G::~INDI_G()
{
    while ( ! pl.isEmpty() ) delete pl.takeFirst();

    delete(propertyContainer);
}

void INDI_G::addProperty(INDI_P *pp)
{
    propertyLayout->addLayout(pp->PHBox);
    propertyLayout->addItem(VerticalSpacer);

    pl.append(pp);

    // Registering the property should be the last thing
    dp->registerProperty(pp);

}

bool INDI_G::removeProperty(INDI_P *pp)
{
    int i = pl.indexOf( pp );
    if ( i != -1 ) {
        delete pl.takeAt(i);
        return true;
    } else {
        return false;
    }
}

/*  Driver Alias
    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "driveralias.h"

#include "Options.h"

#include "kstars.h"
#include "driverinfo.h"
#include "kspaths.h"
#include "kstarsdata.h"

DriverAlias::DriverAlias(QWidget *parent, const QList<DriverInfo *> &driversList) : QDialog(parent), m_DriversList(driversList)
{
    KStarsData::Instance()->userdb()->GetAllDriverAliass(driverAliases);

    setupUi(this);

    for (const DriverInfo *oneDriver : driversList)
    {
        // MDPD drivers CANNOT have aliases.
        if (oneDriver->getAuxInfo().value("mdpd", false).toBool() == true)
            continue;

        QString label = oneDriver->getTreeLabel();
        if (label.isEmpty())
            continue;

        driverCombo->addItem(label);
    }
}

DriverAlias::~DriverAlias()
{   
}

void DriverAlias::refreshFromDB()
{
    KStarsData::Instance()->userdb()->GetAllDriverAliass(driverAliases);
}

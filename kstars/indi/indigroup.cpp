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

#include <QLocale>
#include <QDialog>

#include <QFrame>
#include <QTimer>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDebug>
#include <QScrollArea>

#include <KLocalizedString>

/*******************************************************************
** INDI Group: a tab widget for common properties. All properties
** belong to a group, whether they have one or not but how the group
** is displayed differs
*******************************************************************/
INDI_G::INDI_G(INDI_D *idv, const QString &inName)
{
    dp = idv;
    name = (inName.isEmpty()) ? i18n("Unknown") : inName;

    propertyContainer = new QFrame(idv);
    propertyLayout    = new QVBoxLayout(propertyContainer);
    propertyLayout->setMargin(20);
    VerticalSpacer    = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );

    propertyLayout->addItem(VerticalSpacer);
    propertyLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    scrollArea = new QScrollArea;
    scrollArea->setWidget(propertyContainer);
    scrollArea->setMinimumSize(idv->size());
}

INDI_G::~INDI_G()
{
   while ( ! propList.isEmpty() ) delete propList.takeFirst();

   delete(propertyContainer);

   delete (scrollArea);
}

bool INDI_G::addProperty(INDI::Property *prop)
{
    QString propName(prop->getName());

    INDI_P * pp = getProperty(propName);

    if (pp)
        return false;

    pp = new INDI_P(this, prop);
    propList.append(pp);

    propertyLayout->removeItem(VerticalSpacer);
    propertyLayout->addLayout(pp->getContainer());
    propertyLayout->addItem(VerticalSpacer);

    return true;
}

bool INDI_G::removeProperty(const QString &probName)
{
    foreach(INDI_P * pp, propList)
    {
        if (pp->getName() == probName)
        {
            propList.removeOne(pp);
            propertyLayout->removeItem(pp->getContainer());
            //qDebug() << "Removing GUI property " << probName << " from gorup " << name << " with size " << propList.size() << " and count " << propList.count() << endl;
            delete (pp);
            return true;
        }
    }


    return false;
}

INDI_P * INDI_G::getProperty(const QString & propName)
{

    foreach(INDI_P *pp, propList)
    {
        if (pp->getName() == propName )
            return pp;
    }

    return NULL;

}

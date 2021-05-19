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
    dp   = idv;
    name = (inName.isEmpty()) ? i18n("Unknown") : inName;

    m_PropertiesContainer = new QFrame(idv);
    m_PropertiesLayout    = new QVBoxLayout;
    m_PropertiesLayout->setContentsMargins(20, 20, 20, 20);
    m_VerticalSpacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);
    m_PropertiesLayout->addItem(m_VerticalSpacer);
    m_PropertiesLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_PropertiesContainer->setLayout(m_PropertiesLayout);

    m_ScrollArea = new QScrollArea;
    m_ScrollArea->setWidget(m_PropertiesContainer);
    m_ScrollArea->setMinimumSize(idv->size());
}

INDI_G::~INDI_G()
{
    while (!m_PropertiesList.isEmpty())
        delete m_PropertiesList.takeFirst();
    delete (m_PropertiesContainer);
    delete (m_ScrollArea);
}

bool INDI_G::addProperty(INDI::Property newProperty)
{
    if (!newProperty.getRegistered())
        return false;

    QString name(newProperty.getName());

    // No duplicates
    if (getProperty(name))
        return false;

    if (m_Dirty)
    {
        resetLayout();
        m_Dirty = false;
    }

    INDI_P *property = new INDI_P(this, newProperty);
    m_PropertiesList.append(property);

    m_PropertiesLayout->removeItem(m_VerticalSpacer);
    m_PropertiesLayout->addLayout(property->getContainer());
    m_PropertiesLayout->addItem(m_VerticalSpacer);
    m_PropertiesLayout->invalidate();

    return true;
}

bool INDI_G::removeProperty(const QString &name)
{
    INDI_P *oneProp = getProperty(name);
    if (oneProp)
    {
        m_PropertiesList.removeOne(oneProp);
        delete (oneProp);
        m_Dirty = true;
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////
/// Reset the layout and remove ALL properties then re-add them
/// This is a brute-force way of fixing a long standing bug with Qt
/// If a property is removed, and then another property is added, it makes
/// the tab completely inaccessible to mouse clicks. Users can only navigate the controls
/// through use of tab key.
/////////////////////////////////////////////////////////////////////
void INDI_G::resetLayout()
{
    QList<INDI::Property> existingProps;

    // Get all existing properties
    for (auto &oneProp : m_PropertiesList)
        existingProps.append(oneProp->getProperty());

    // Remove all properties
    qDeleteAll(m_PropertiesList);
    m_PropertiesList.clear();

    // Remove all containers
    delete (m_PropertiesLayout);

    // Init all containers again
    // N.B. m_VerticalSpacer and m_PropertiesContainer would be automatically deleted by Qt.
    m_PropertiesContainer = new QFrame(dp);
    m_PropertiesLayout    = new QVBoxLayout;
    m_PropertiesLayout->setContentsMargins(20, 20, 20, 20);
    m_VerticalSpacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);
    m_PropertiesLayout->addItem(m_VerticalSpacer);
    m_PropertiesLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_PropertiesContainer->setLayout(m_PropertiesLayout);
    m_ScrollArea->setWidget(m_PropertiesContainer);
    m_ScrollArea->setMinimumSize(dp->size());

    // Re-add all properties
    for (const auto &oneINDIProp : existingProps)
        addProperty(oneINDIProp);

}

INDI_P *INDI_G::getProperty(const QString &name) const
{
    auto pos = std::find_if(m_PropertiesList.begin(), m_PropertiesList.end(), [name](INDI_P * oneProperty)
    {
        return oneProperty->getName() == name;
    });
    if (pos != m_PropertiesList.end())
        return *pos;
    else
        return nullptr;
}

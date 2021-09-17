/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
INDI_G::INDI_G(INDI_D *idv, const QString &inName) : QScrollArea(idv)
{
    dp   = idv;
    name = (inName.isEmpty()) ? i18n("Unknown") : inName;

    m_PropertiesContainer = new QFrame(this);
    m_PropertiesLayout    = new QVBoxLayout;
    m_PropertiesLayout->setContentsMargins(20, 20, 20, 20);
    m_VerticalSpacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);
    m_PropertiesLayout->addItem(m_VerticalSpacer);
    m_PropertiesLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_PropertiesContainer->setLayout(m_PropertiesLayout);

    setWidget(m_PropertiesContainer);
}

bool INDI_G::addProperty(INDI::Property newProperty)
{
    if (!newProperty.isValid())
        return false;

    QString name(newProperty.getName());

    // No duplicates
    if (getProperty(name))
        return false;

    INDI_P *property = new INDI_P(this, newProperty);
    m_PropertiesList.append(property);

    m_PropertiesLayout->removeItem(m_VerticalSpacer);
    m_PropertiesLayout->addWidget(property);
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

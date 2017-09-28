/*  Ekos Filter Manager
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "filtermanager.h"

#include <QSqlTableModel>
#include <QSqlDatabase>

#include <basedevice.h>

#include "kstarsdata.h"
#include "kstars.h"
#include "auxiliary/kspaths.h"

#include "indi_debug.h"

#include "Options.h"

FilterManager::FilterManager() : QDialog(KStars::Instance())
{
    setupUi(this);

    refreshFilterData();
}

void FilterManager::refreshFilterData()
{
    if (m_currentFilterDevice == nullptr)
        return;

    delete (filterModel);
    filterModel = new QSqlTableModel(this, KStarsData::Instance()->userdb()->GetDatabase());
    filterModel->setTable("filter");
    filterModel->select();

    filterView->setModel(filterModel);

    // Get all OAL equipment filter list
    KStarsData::Instance()->userdb()->GetAllFilters(m_FilterList);
    m_ActiveFilters.clear();

    // Get all filters for the current filter device
    QString currentFilterDevice = QString(m_currentFilterDevice->getDeviceName());

    for (OAL::Filter *oneFilter : m_FilterList)
    {
        if (oneFilter->vendor() == currentFilterDevice)
            m_ActiveFilters.append(oneFilter);
    }
}

void FilterManager::addFilter(ISD::GDInterface *filter)
{
    m_filterDevices.append(filter);
    m_currentFilterDevice = filter;
}

void FilterManager::setCurrentFilter(ISD::GDInterface *filter)
{
    m_currentFilterDevice = filter;
    m_currentFilterList.clear();

    m_currentFilterList = getFilterLabelsForDevice(filter);
    m_currentFilterPosition = getFilterPositionForDevice(filter);

    m_currentFilterName = filter->getBaseDevice()->getText("FILTER_NAME");
    m_currentFilterSlot = filter->getBaseDevice()->getNumber("FILTER_SLOT");

    for (ISD::GDInterface *oneFilter : m_FilterList)
        oneFilter->disconnect(this);

    connect(filter, SIGNAL(textUpdated(ITextVectorProperty*)), this, SLOT(processText(ITextVectorProperty*)));
    connect(filter, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processNumber(INumberVectorProperty*)));
}

void FilterManager::addFocuser(ISD::GDInterface *focuser)
{
    m_focuserDevices.append(dynamic_cast<ISD::Focuser*>(focuser));
    m_currentFocuserDevice = dynamic_cast<ISD::Focuser*>(focuser);
}

void FilterManager::setCurrentFocuser(ISD::GDInterface *focuser)
{
    m_currentFocuserDevice = dynamic_cast<ISD::Focuser*>(focuser);
}

QStringList FilterManager::getFilterLabels(ISD::GDInterface *filter)
{
    if (filter == nullptr)
        return m_currentFilterList;

    return getFilterLabelsForDevice(filter);
}

QStringList FilterManager::getFilterLabelsForDevice(ISD::GDInterface *filter)
{
    if (filter == m_currentFilterDevice && m_currentFilterList.isEmpty() == false)
        return m_currentFilterList;

    ITextVectorProperty *name = filter->getBaseDevice()->getText("FILTER_NAME");
    INumberVectorProperty *slot = filter->getBaseDevice()->getNumber("FILTER_SLOT");

    QStringList filterList;

    QStringList filterAlias = Options::filterAlias();

    for (int i = 0; i < slot->np[0].max; i++)
    {
        QString item;

        if (name != nullptr && (i < name->ntp))
            item = name->tp[i].text;
        else if (i < filterAlias.count() && filterAlias[i].isEmpty() == false)
            item = filterAlias.at(i);
        else
            item = QString("Filter_%1").arg(i + 1);

        filterList.append(item);
    }

    return filterList;
}

int FilterManager::getFilterPosition(ISD::GDInterface *filter)
{
    if (filter == nullptr)
        return m_currentFilterPosition;

    return getFilterPositionForDevice(filter);
}

int FilterManager::getFilterPositionForDevice(ISD::GDInterface *filter)
{
    if (filter == m_currentFilterDevice)
        return static_cast<int>(m_currentFilterSlot->np[0].value);

    INumberVectorProperty *slot = filter->getBaseDevice()->getNumber("FILTER_SLOT");
    if (slot == nullptr)
    {
        qCWarning(KSTARS_INDI) << "Unable to find FILTER_SLOT in" << filter->getBaseDevice();
        return -1;
    }

    return static_cast<int>(slot->np[0].value);
}

bool FilterManager::setFilterPosition(uint8_t position, ISD::GDInterface *filter)
{
    if (filter == nullptr)
        return m_currentFilterDevice->runCommand(INDI_SET_FILTER, &position);

    return filter->runCommand(INDI_SET_FILTER, &position);
}

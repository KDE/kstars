/*  Ekos Filter Manager
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "filtermanager.h"

#include "indi_debug.h"
#include "kstarsdata.h"
#include "kstars.h"
#include "Options.h"
#include "auxiliary/kspaths.h"
#include "ekos/auxiliary/filterdelegate.h"


#include <QSqlTableModel>
#include <QSqlDatabase>
#include <QSqlRecord>

#include <basedevice.h>

#include <algorithm>

namespace Ekos
{

FilterManager::FilterManager() : QDialog(KStars::Instance())
{
    #ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    #endif

    setupUi(this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(close()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    //QSqlDatabase::removeDatabase("filter_db");
    QSqlDatabase userdb = QSqlDatabase::cloneDatabase(KStarsData::Instance()->userdb()->GetDatabase(), "filter_db");
    userdb.open();

    //delete (filterModel);

    filterModel = new QSqlTableModel(this, userdb);
    filterView->setModel(filterModel);

    // No Edit delegate
    noEditDelegate = new NotEditableDelegate(filterView);
    filterView->setItemDelegateForColumn(4, noEditDelegate);

    // Exposure delegate
    exposureDelegate = new ExposureDelegate(filterView);
    filterView->setItemDelegateForColumn(5, exposureDelegate);

    // Offset delegate
    offsetDelegate = new OffsetDelegate(filterView);
    filterView->setItemDelegateForColumn(6, offsetDelegate);

    // Auto Focus delegate
    useAutoFocusDelegate = new UseAutoFocusDelegate(filterView);
    filterView->setItemDelegateForColumn(7, useAutoFocusDelegate);

    // Set Delegates
    lockDelegate = new LockDelegate(m_currentFilterLabels, filterView);
    filterView->setItemDelegateForColumn(8, lockDelegate);

    // Absolute Focus Position
    filterView->setItemDelegateForColumn(9, noEditDelegate);

    connect(filterModel, &QSqlTableModel::dataChanged, [this](const QModelIndex &topLeft, const QModelIndex &, const QVector<int> &)
    {
        reloadFilters();
        if (topLeft.column() == 5)
            emit exposureChanged(filterModel->data(topLeft).toDouble());
    });
}

void FilterManager::refreshFilterModel()
{
    if (m_currentFilterDevice == nullptr || m_currentFilterLabels.empty())
        return;

    QString vendor(m_currentFilterDevice->getDeviceName());

    //QSqlDatabase::removeDatabase("filter_db");
    //QSqlDatabase userdb = QSqlDatabase::cloneDatabase(KStarsData::Instance()->userdb()->GetDatabase(), "filter_db");
    //userdb.open();

    //delete (filterModel);

    //filterModel = new QSqlTableModel(this, userdb);
    filterModel->setTable("filter");
    filterModel->setFilter(QString("vendor='%1'").arg(vendor));
    filterModel->select();
    filterModel->setEditStrategy(QSqlTableModel::OnFieldChange);

    // If we have an existing table but it doesn't match the number of current filters
    // then we remove it.
    if (filterModel->rowCount() > 0 && filterModel->rowCount() != m_currentFilterLabels.count())
    {
        for (int i=0; i < filterModel->rowCount(); i++)
            filterModel->removeRow(i);

        filterModel->select();
    }

    // If it is first time, let's populate data
    if (filterModel->rowCount() == 0)
    {
        for (QString &filter : m_currentFilterLabels)
            KStarsData::Instance()->userdb()->AddFilter(vendor, "", "", filter, 0, 1.0, false, "--", 0);

        filterModel->select();
        // Seems ->select() is not enough, have to create a new model.
        /*delete (filterModel);
        filterModel = new QSqlTableModel(this, userdb);
        filterModel->setTable("filter");
        filterModel->setFilter(QString("vendor='%1'").arg(m_currentFilterDevice->getDeviceName()));
        filterModel->select();
        filterModel->setEditStrategy(QSqlTableModel::OnManualSubmit);*/
    }
    // Make sure all the filter colors match DB. If not update model to sync with INDI filter values
    else
    {
        for (int i = 0; i < filterModel->rowCount(); ++i)
        {
            QModelIndex index = filterModel->index(i, 4);
            if (filterModel->data(index).toString() != m_currentFilterLabels[i])
            {
                filterModel->setData(index, m_currentFilterLabels[i]);
            }
        }
    }

    lockDelegate->setCurrentFilterList(m_currentFilterLabels);

    filterModel->setHeaderData(4, Qt::Horizontal, i18n("Filter"));

    filterModel->setHeaderData(5, Qt::Horizontal, i18n("Filter exposure time during focus"), Qt::ToolTipRole);
    filterModel->setHeaderData(5, Qt::Horizontal, i18n("Exposure"));

    filterModel->setHeaderData(6, Qt::Horizontal, i18n("Relative offset in steps"), Qt::ToolTipRole);
    filterModel->setHeaderData(6, Qt::Horizontal, i18n("Offset"));

    filterModel->setHeaderData(7, Qt::Horizontal, i18n("Start Auto Focus when filter is activated"), Qt::ToolTipRole);
    filterModel->setHeaderData(7, Qt::Horizontal, i18n("Auto Focus"));

    filterModel->setHeaderData(8, Qt::Horizontal, i18n("Lock specific filter when running Auto Focus"), Qt::ToolTipRole);
    filterModel->setHeaderData(8, Qt::Horizontal, i18n("Lock Filter"));

    filterModel->setHeaderData(9, Qt::Horizontal, i18n("Flat frames are captured at this focus position. It is updated automatically by focus process if enabled."), Qt::ToolTipRole);
    filterModel->setHeaderData(9, Qt::Horizontal, i18n("Flat Focus Position"));

    filterView->hideColumn(0);
    filterView->hideColumn(1);
    filterView->hideColumn(2);
    filterView->hideColumn(3);

    reloadFilters();
}

void FilterManager::reloadFilters()
{
    qDeleteAll(m_ActiveFilters);
    currentFilter = nullptr;
    targetFilter = nullptr;    
    m_ActiveFilters.clear();
    operationQueue.clear();

    for (int i = 0; i < filterModel->rowCount(); ++i)
    {
        QSqlRecord record = filterModel->record(i);
        QString id        = record.value("id").toString();
        QString vendor    = record.value("Vendor").toString();
        QString model     = record.value("Model").toString();
        QString type      = record.value("Type").toString();
        QString color     = record.value("Color").toString();
        double exposure   = record.value("Exposure").toDouble();
        int offset        = record.value("Offset").toInt();
        QString lockedFilter  = record.value("LockedFilter").toString();
        bool useAutoFocus = record.value("UseAutoFocus").toInt() == 1;
        int absFocusPos   = record.value("AbsoluteFocusPosition").toInt();
        OAL::Filter *o    = new OAL::Filter(id, model, vendor, type, color, exposure, offset, useAutoFocus, lockedFilter, absFocusPos);
        m_ActiveFilters.append(o);
    }
}

void FilterManager::setCurrentFilterWheel(ISD::GDInterface *filter)
{
    if (m_currentFilterDevice == filter)
        return;
    else
        m_currentFilterDevice->disconnect(this);

    filterNameLabel->setText(filter->getDeviceName());

    m_currentFilterDevice = filter;
    m_currentFilterLabels.clear();

    m_FilterNameProperty = filter->getBaseDevice()->getText("FILTER_NAME");
    m_FilterPositionProperty = filter->getBaseDevice()->getNumber("FILTER_SLOT");

    m_currentFilterPosition = getFilterPosition(true);
    m_currentFilterLabels = getFilterLabels(true);

    connect(filter, SIGNAL(textUpdated(ITextVectorProperty*)), this, SLOT(processText(ITextVectorProperty*)));
    connect(filter, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processNumber(INumberVectorProperty*)));
    connect(filter, SIGNAL(switchUpdated(ISwitchVectorProperty*)), this, SLOT(processSwitch(ISwitchVectorProperty*)));
    connect(filter, &ISD::GDInterface::Disconnected, [&]() {
        m_currentFilterLabels.clear();
        m_currentFilterPosition = -1;
        m_currentFilterDevice = nullptr;
        m_FilterNameProperty = nullptr;
        m_FilterPositionProperty = nullptr;
    });

    if (m_currentFilterLabels.isEmpty() == false)
        refreshFilterModel();

    if (m_ActiveFilters.count() >= m_currentFilterPosition)
        lastFilterOffset = m_ActiveFilters[m_currentFilterPosition-1]->offset();
}

QStringList FilterManager::getFilterLabels(bool forceRefresh)
{
    if (forceRefresh == false || m_FilterNameProperty == nullptr)
        return m_currentFilterLabels;

    QStringList filterList;

    QStringList filterAlias = Options::filterAlias();

    for (int i = 0; i < m_FilterPositionProperty->np[0].max; i++)
    {
        QString item;

        if (m_FilterNameProperty != nullptr && (i < m_FilterNameProperty->ntp))
            item = m_FilterNameProperty->tp[i].text;
        else if (i < filterAlias.count() && filterAlias[i].isEmpty() == false)
            item = filterAlias.at(i);

        filterList.append(item);
    }

    return filterList;
}

int FilterManager::getFilterPosition(bool forceRefresh)
{
    if (forceRefresh == false)
        return m_currentFilterPosition;

    return static_cast<int>(m_FilterPositionProperty->np[0].value);
}

bool FilterManager::setFilterPosition(uint8_t position, FilterPolicy policy)
{
    // Position 1 to Max
    if (position > m_ActiveFilters.count())
        return false;

    m_Policy = policy;
    currentFilter= m_ActiveFilters[m_currentFilterPosition - 1];
    targetFilter = m_ActiveFilters[position-1];

    if (currentFilter == targetFilter)
    {
        emit ready();
        return true;
    }

    buildOperationQueue(FILTER_CHANGE);

    executeOperationQueue();

    return true;
}

void FilterManager::processNumber(INumberVectorProperty *nvp)
{
    if (nvp->s != IPS_OK || strcmp(nvp->name, "FILTER_SLOT") || m_currentFilterDevice == nullptr || strcmp(nvp->device, m_currentFilterDevice->getDeviceName()))
        return;

    m_FilterPositionProperty = nvp;

    if (m_currentFilterPosition != static_cast<int>(m_FilterPositionProperty->np[0].value))
    {
        m_currentFilterPosition = static_cast<int>(m_FilterPositionProperty->np[0].value);
        emit positionChanged(m_currentFilterPosition);
    }

    if (state == FILTER_CHANGE)
        executeOperationQueue();
    // If filter is changed externally, record its current offset as the starting offset.
    else if (state == FILTER_IDLE && m_ActiveFilters.count() >= m_currentFilterPosition)
        lastFilterOffset = m_ActiveFilters[m_currentFilterPosition-1]->offset();

    // Check if we have to apply Focus Offset
    // Focus offsets are always applied first



    // Check if we have to start Auto Focus
    // If new filter position changed, and new filter policy is to perform auto-focus then autofocus is initiated.

    // Capture Module
    // 3x L ---> 3x HA ---> 3x L
    // Capture calls setFilterPosition("L").
    // 0. Change filter to desired filter "L"
    // 1. Is there any offset from last offset that needs to be applied?
    // 1.1 Yes --> Apply focus offset and wait until position is changed:
    // 1.1.1 Position complete, now check for useAutoFocus policy (#2).
    // 1.1.2 Position failed, retry?
    // 1.2 No  --> Go to #2
    // 2. Is there autofocus policy for current filter?
    // 2.1. Yes --> Check filter lock policy
    // 2.1.1 If filter lock is another filter --> Change filter
    // 2.1.2 If filter lock is same filter --> proceed to 2.3
    // 2.2 No --> Process to 2.3
    // 2.3 filter lock policy filter is applied, start autofocus.
    // 2.4 Autofocus complete. Check filter lock policy
    // 2.4.1 If filter lock policy was applied --> revert filter
    // 2.4.1.1 If filter offset policy is applicable --> Apply offset
    // 2.4.1.2 If no filter offset policy is applicable --> Go to 2.5
    // 2.4.2 No filter lock policy, go to 2.5
    // 2.5 All complete, emit ready()

    // Example. Current filter L. setFilterPosition("HA"). AutoFocus = YES. HA lock policy: L, HA offset policy: +100 with respect to L
    // Operation Stack. offsetDiff = 100
    // If AutoFocus && current filter = lock policy filter
    // AUTO_FOCUS (on L)
    // CHANGE_FILTER (to HA)
    // APPLY_OFFSET: +100

    // Example. Current filter L. setFilterPosition("HA"). AutoFocus = No. HA lock policy: L, HA offset policy: +100 with respect to L
    // Operation Stack. offsetDiff = 100
    // CHANGE_FILTER (to HA)
    // APPLY_OFFSET: +100




    // Example. Current filter R. setFilterPosition("B"). AutoFocus = YES. B lock policy: "--", B offset policy: +70 with respect to L
    // R offset = -50 with respect to L
    // FILTER_CHANGE (to B)
    // FILTER_OFFSET (+120)
    // AUTO_FOCUS

    // Example. Current filter R. setFilterPosition("HA"). AutoFocus = YES. HA lock policy: L, HA offset policy: +100 with respect to L
    // R offset = -50 with respect to L
    // Operation Stack. offsetDiff = +150
    // CHANGE_FILTER (to L)
    // APPLY_OFFSET: +50 (L - R)
    // AUTO_FOCUS
    // CHANGE_FILTER (HA)
    // APPLY_OFFSET: +100



    // Example. Current filter R. setFilterPosition("HA"). AutoFocus = No. HA lock policy: L, HA offset policy: +100 with respect to L
    // R offset = -50 with respect to L
    // Operation Stack. offsetDiff = +150
    // CHANGE_FILTER (to HA)
    // APPLY_OFFSET: +150 (HA - R)



    // Example. Current filter L. setFilterPosition("R"). AutoFocus = Yes. R lock policy: R, R offset policy: -50 with respect to L
    // Operation Stack. offsetDiff = -50
    // CHANGE_FILTER (to R)
    // APPLY_OFFSET: -50 (R - L)
    // AUTO_FOCUS


}

void FilterManager::processText(ITextVectorProperty *tvp)
{
    if (strcmp(tvp->name, "FILTER_NAME") || m_currentFilterDevice == nullptr || strcmp(tvp->device, m_currentFilterDevice->getDeviceName())            )
        return;    

    m_FilterNameProperty = tvp;

    QStringList newFilterLabels = getFilterLabels(true);

    if (newFilterLabels != m_currentFilterLabels)
    {
        m_currentFilterLabels = newFilterLabels;

        refreshFilterModel();

        emit labelsChanged(newFilterLabels);
    }
}

void FilterManager::processSwitch(ISwitchVectorProperty *svp)
{
    if (m_currentFilterDevice == nullptr || strcmp(svp->device, m_currentFilterDevice->getDeviceName()))
        return;

}

void FilterManager::buildOperationQueue(FilterState operation)
{
    operationQueue.clear();    
    m_useTargetFilter = false;

    switch (operation)
    {
    case FILTER_CHANGE:
    {
        if ( (m_Policy & CHANGE_POLICY) && targetFilter != currentFilter)
            m_useTargetFilter = true;

        if (m_useTargetFilter)
        {            
            operationQueue.enqueue(FILTER_CHANGE);
            if (m_FocusReady && (m_Policy & OFFSET_POLICY))
                operationQueue.enqueue(FILTER_OFFSET);
        }

        if (m_FocusReady && (m_Policy & AUTOFOCUS_POLICY) && targetFilter->useAutoFocus())
            operationQueue.enqueue(FILTER_AUTOFOCUS);
    }
        break;

    default:
        break;
    }
}

bool FilterManager::executeOperationQueue()
{
    if (operationQueue.isEmpty())
    {
        state = FILTER_IDLE;
        emit newStatus(state);
        emit ready();
        return false;
    }

    FilterState nextOperation = operationQueue.dequeue();

    bool actionRequired = true;

    switch (nextOperation)
    {
    case FILTER_CHANGE:
    {
        state = FILTER_CHANGE;
        if (m_useTargetFilter)
            targetFilterPosition = m_ActiveFilters.indexOf(targetFilter) + 1;
        m_currentFilterDevice->runCommand(INDI_SET_FILTER, &targetFilterPosition);
        emit newStatus(state);
    }
        break;

    case FILTER_OFFSET:
    {
        state = FILTER_OFFSET;
        if (m_useTargetFilter)
        {
            targetFilterOffset = targetFilter->offset() - lastFilterOffset;
            lastFilterOffset   = targetFilter->offset();
            currentFilter = targetFilter;
            m_useTargetFilter = false;
        }
        if (targetFilterOffset == 0)
            actionRequired = false;
        else
        {
            emit newFocusOffset(targetFilterOffset, false);
            emit newStatus(state);
        }
    }
        break;

    case FILTER_AUTOFOCUS:
        state = FILTER_AUTOFOCUS;
        emit newStatus(state);
        emit checkFocus(0.01);
        break;

    default:
        break;
    }

    // If an additional action is required, return return and continue later
    if (actionRequired)
        return true;
    // Othereise, continue processing the queue
    else
        return executeOperationQueue();
}

bool FilterManager::executeOneOperation(FilterState operation)
{
    bool actionRequired = false;

    switch (operation)
    {
    default:
        break;
    }

    return actionRequired;
}

void FilterManager::setFocusOffsetComplete()
{
    if (state == FILTER_OFFSET)
        executeOperationQueue();
}

double FilterManager::getFilterExposure(const QString &name) const
{
    if (m_currentFilterLabels.empty())
        return 1;

    QString color = name;
    if (color.isEmpty())
        color = m_currentFilterLabels[m_currentFilterPosition-1];
    // Search for locked filter by filter color name
    auto pos = std::find_if(m_ActiveFilters.begin(), m_ActiveFilters.end(), [color](OAL::Filter *oneFilter)
    {return (oneFilter->color() == color);});

    if (pos != m_ActiveFilters.end())
        return (*pos)->exposure();

    // Default value
    return 1;
}

bool FilterManager::setFilterExposure(int index, double exposure)
{
    if (m_currentFilterLabels.empty())
        return false;

     QString color = m_currentFilterLabels[index];
     for (int i=0; i < m_ActiveFilters.count(); i++)
     {
         if (color == m_ActiveFilters[i]->color())
         {
             filterModel->setData(filterModel->index(i, 5), exposure);
             filterModel->submitAll();
             refreshFilterModel();
             return true;
         }
     }

     return false;
}

bool FilterManager::setFilterAbsoluteFocusPosition(int index, int absFocusPos)
{
    if (index < 0 || index >= m_currentFilterLabels.count())
        return false;

    QString color = m_currentFilterLabels[index];
    for (int i=0; i < m_ActiveFilters.count(); i++)
    {
        if (color == m_ActiveFilters[i]->color())
        {
            filterModel->setData(filterModel->index(i, 9), absFocusPos);
            filterModel->submitAll();
            refreshFilterModel();
            return true;
        }
    }

    return false;
}

QString FilterManager::getFilterLock(const QString &name) const
{
    // Search for locked filter by filter color name
    auto pos = std::find_if(m_ActiveFilters.begin(), m_ActiveFilters.end(), [name](OAL::Filter *oneFilter)
    {return (oneFilter->color() == name);});

    if (pos != m_ActiveFilters.end())
        return (*pos)->lockedFilter();

    // Default value
    return "--";
}

void FilterManager::removeDevice(ISD::GDInterface *device)
{    
    if (device == m_currentFilterDevice)
    {
        m_FilterNameProperty = nullptr;
        m_FilterPositionProperty = nullptr;
        m_currentFilterDevice = nullptr;
        m_currentFilterLabels.clear();
        m_currentFilterPosition = 0;
        qDeleteAll(m_ActiveFilters);
        m_ActiveFilters.clear();
        delete(filterModel);
        filterModel = nullptr;
    }
}

void FilterManager::setFocusStatus(Ekos::FocusState focusState)
{
    if (state == FILTER_AUTOFOCUS)
    {
        switch (focusState)
        {
            case FOCUS_COMPLETE:
            executeOperationQueue();
            break;

            case FOCUS_FAILED:
            if (++retries == 3)
            {
                retries = 0;
                emit failed();
                return;
            }
            // Restart again
            emit checkFocus(0.01);
            break;

        default:
            break;

        }
    }
}

bool FilterManager::syncAbsoluteFocusPosition(int index)
{
    if (index < 0 || index > m_ActiveFilters.count())
    {
        qCWarning(KSTARS_INDI) << __FUNCTION__ << "index" << index << "is out of bounds.";
        return false;
    }

    int absFocusPos = m_ActiveFilters[index]->absoluteFocusPosition();

    if (m_FocusAbsPosition == absFocusPos)
    {
        m_FocusAbsPositionPending = false;
        return true;
    }
    else if (m_FocusAbsPositionPending == false)
    {
        m_FocusAbsPositionPending = true;
        emit newFocusOffset(absFocusPos, true);
    }

    return false;
}
}

/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "buildfilteroffsets.h"
#include <kstars_debug.h>

#include "indi_debug.h"
#include "kstarsdata.h"
#include "kstars.h"
#include "ekos/focus/focus.h"
#include "ekos/manager.h"
#include "Options.h"
#include "auxiliary/kspaths.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/auxiliary/tabledelegate.h"

#include <QTimer>
#include <QSqlTableModel>
#include <QSqlDatabase>
#include <QSqlRecord>

#include <basedevice.h>

#include <algorithm>

namespace Ekos
{

FilterManager::FilterManager(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_MACOS
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setupUi(this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(close()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    connect(buildOffsetsButton, &QPushButton::clicked, this, &FilterManager::buildFilterOffsets);

    kcfg_FlatSyncFocus->setChecked(Options::flatSyncFocus());
    connect(kcfg_FlatSyncFocus, &QCheckBox::toggled, this, [this]()
    {
        Options::setFlatSyncFocus(kcfg_FlatSyncFocus->isChecked());
    });

    // 30 second timeout for filter change
    m_FilterChangeTimeout.setSingleShot(true);
    m_FilterChangeTimeout.setInterval(30000);
    connect(&m_FilterChangeTimeout, &QTimer::timeout, this, &FilterManager::checkFilterChangeTimeout);

    createFilterModel();

    // No Edit delegate
    noEditDelegate = new NotEditableDelegate(m_FilterView);
    m_FilterView->setItemDelegateForColumn(FM_LABEL, noEditDelegate);

    // Exposure delegate
    exposureDelegate = new DoubleDelegate(m_FilterView, 0.001, 3600, 1);
    m_FilterView->setItemDelegateForColumn(FM_EXPOSURE, exposureDelegate);

    // Offset delegate
    offsetDelegate = new IntegerDelegate(m_FilterView, -10000, 10000, 1);
    m_FilterView->setItemDelegateForColumn(FM_OFFSET, offsetDelegate);

    // Auto Focus delegate
    useAutoFocusDelegate = new ToggleDelegate(m_FilterView);
    m_FilterView->setItemDelegateForColumn(FM_AUTO_FOCUS, useAutoFocusDelegate);

    // Set Delegates
    lockDelegate = new ComboDelegate(m_FilterView);
    m_FilterView->setItemDelegateForColumn(FM_LOCK_FILTER, lockDelegate);
    lockDelegate->setValues(getLockDelegates());

    // Last AF solution delegate. Set by Autofocus but make editable in case bad data
    // corrections need to be made
    lastAFSolutionDelegate = new IntegerDelegate(m_FilterView, 0, 1000000, 1);
    m_FilterView->setItemDelegateForColumn(FM_LAST_AF_SOLUTION, lastAFSolutionDelegate);

    // Last AF solution temperature delegate
    lastAFTempDelegate = new DoubleDelegate(m_FilterView, -60.0, 60.0, 1.0);
    m_FilterView->setItemDelegateForColumn(FM_LAST_AF_TEMP, lastAFTempDelegate);

    // Last AF solution altitude delegate
    lastAFAltDelegate = new DoubleDelegate(m_FilterView, 0.0, 90.0, 1.0);
    m_FilterView->setItemDelegateForColumn(FM_LAST_AF_ALT, lastAFAltDelegate);

    // Last AF solution datetime delegate
    lastAFDTDelegate = new DatetimeDelegate(m_FilterView, DATETIME_FORMAT, "2025-01-01", "2100-01-01", false);
    m_FilterView->setItemDelegateForColumn(FM_LAST_AF_DATETIME, lastAFDTDelegate);

    // Ticks / °C delegate
    ticksPerTempDelegate = new DoubleDelegate(m_FilterView, -10000.0, 10000.0, 1.0);
    m_FilterView->setItemDelegateForColumn(FM_TICKS_PER_TEMP, ticksPerTempDelegate);

    // Ticks / °Altitude delegate
    ticksPerAltDelegate = new DoubleDelegate(m_FilterView, -10000.0, 10000.0, 1.0);
    m_FilterView->setItemDelegateForColumn(FM_TICKS_PER_ALT, ticksPerAltDelegate);

    // Wavelength delegate
    wavelengthDelegate = new IntegerDelegate(m_FilterView, 200, 1000, 50);
    m_FilterView->setItemDelegateForColumn(FM_WAVELENGTH, wavelengthDelegate);
}

void FilterManager::createFilterModel()
{
    QSqlDatabase userdb = QSqlDatabase::database(KStarsData::Instance()->userdb()->connectionName());

    m_FilterModel = new QSqlTableModel(this, userdb);
    m_FilterView->setModel(m_FilterModel);
    m_FilterModel->setTable("filter");
    m_FilterModel->setEditStrategy(QSqlTableModel::OnFieldChange);

    m_FilterModel->setHeaderData(FM_LABEL, Qt::Horizontal, i18n("Filter"));

    m_FilterModel->setHeaderData(FM_EXPOSURE, Qt::Horizontal, i18n("Filter exposure time during focus"), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_EXPOSURE, Qt::Horizontal, i18n("Exposure"));

    m_FilterModel->setHeaderData(FM_OFFSET, Qt::Horizontal, i18n("Relative offset in steps"), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_OFFSET, Qt::Horizontal, i18n("Offset"));

    m_FilterModel->setHeaderData(FM_AUTO_FOCUS, Qt::Horizontal, i18n("Start Auto Focus when filter is activated"),
                                 Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_AUTO_FOCUS, Qt::Horizontal, i18n("Auto Focus"));

    m_FilterModel->setHeaderData(FM_LOCK_FILTER, Qt::Horizontal, i18n("Lock specific filter when running Auto Focus"),
                                 Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_LOCK_FILTER, Qt::Horizontal, i18n("Lock Filter"));

    m_FilterModel->setHeaderData(FM_LAST_AF_SOLUTION, Qt::Horizontal,
                                 i18n("Last AF solution. Updated automatically by the autofocus process."), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_LAST_AF_SOLUTION, Qt::Horizontal, i18n("Last AF Solution"));

    m_FilterModel->setHeaderData(FM_LAST_AF_TEMP, Qt::Horizontal,
                                 i18n("The temperature of the last AF solution. Updated automatically by the autofocus process."), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_LAST_AF_TEMP, Qt::Horizontal, i18n("Last AF Temp (°C)"));

    m_FilterModel->setHeaderData(FM_LAST_AF_ALT, Qt::Horizontal,
                                 i18n("The altitude of the last AF solution. Updated automatically by the autofocus process."), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_LAST_AF_ALT, Qt::Horizontal, i18n("Last AF Alt (°Alt)"));

    m_FilterModel->setHeaderData(FM_LAST_AF_DATETIME, Qt::Horizontal,
                                 i18n("The datetime of the last AF solution. Updated automatically by the autofocus process."), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_LAST_AF_DATETIME, Qt::Horizontal, i18n("Last AF Datetime"));

    m_FilterModel->setHeaderData(FM_TICKS_PER_TEMP, Qt::Horizontal,
                                 i18n("The number of ticks per °C increase in temperature. +ve for outward focuser movement"), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_TICKS_PER_TEMP, Qt::Horizontal, i18n("Ticks / °C"));

    m_FilterModel->setHeaderData(FM_TICKS_PER_ALT, Qt::Horizontal,
                                 i18n("The number of ticks per degree increase in altitude. +ve for outward focuser movement"), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_TICKS_PER_ALT, Qt::Horizontal, i18n("Ticks / °Alt"));

    m_FilterModel->setHeaderData(FM_WAVELENGTH, Qt::Horizontal, i18n("Mid-point wavelength of filter in nm"), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_WAVELENGTH, Qt::Horizontal, i18n("Wavelength"));

    connect(m_FilterModel, &QSqlTableModel::dataChanged, this, &FilterManager::updated);

    connect(m_FilterModel, &QSqlTableModel::dataChanged, this, [this](const QModelIndex & topLeft, const QModelIndex &,
            const QVector<int> &)
    {
        reloadFilters();
        if (topLeft.column() == FM_EXPOSURE)
            emit exposureChanged(m_FilterModel->data(topLeft).toDouble());
        else if (topLeft.column() == FM_LOCK_FILTER)
        {
            // Don't allow the lock filter to be set to the current filter - circular dependancy
            // Don't allow the lock to be set if this filter is a lock for another filter - nested dependancy
            QString lock = m_FilterModel->data(m_FilterModel->index(topLeft.row(), topLeft.column())).toString();
            QString filter = m_FilterModel->data(m_FilterModel->index(topLeft.row(), FM_LABEL)).toString();
            bool alreadyALock = false;
            for (int i = 0; i < m_ActiveFilters.count(); i++)
            {
                if (m_ActiveFilters[i]->lockedFilter() == filter)
                {
                    alreadyALock = true;
                    break;
                }
            }
            if (alreadyALock || (lock == filter))
            {
                m_FilterModel->setData(m_FilterModel->index(topLeft.row(), topLeft.column()), NULL_FILTER);
            }
            // Update the acceptable values in the lockDelegate
            lockDelegate->setValues(getLockDelegates());
        }
        else if (topLeft.column() == FM_TICKS_PER_TEMP)
            emit ticksPerTempChanged();
        else if (topLeft.column() == FM_TICKS_PER_ALT)
            emit ticksPerAltChanged();
        else if (topLeft.column() == FM_WAVELENGTH)
            emit wavelengthChanged();
    });
}

void FilterManager::refreshFilterModel()
{
    if (m_FilterWheel == nullptr || m_currentFilterLabels.empty())
        return;

    // In case filter model was cleared due to a device disconnect
    if (m_FilterModel == nullptr)
        createFilterModel();

    QString vendor(m_FilterWheel->getDeviceName());
    m_FilterModel->setFilter(QString("vendor='%1'").arg(vendor));
    m_FilterModel->select();

    m_FilterView->hideColumn(0);
    m_FilterView->hideColumn(1);
    m_FilterView->hideColumn(2);
    m_FilterView->hideColumn(3);

    // If we have an existing table but it doesn't match the number of current filters
    // then we remove it.
    if (m_FilterModel->rowCount() > 0 && m_FilterModel->rowCount() != m_currentFilterLabels.count())
    {
        for (int i = 0; i < m_FilterModel->rowCount(); i++)
            m_FilterModel->removeRow(i);

        m_FilterModel->select();
    }

    // If it is first time, let's populate data
    if (m_FilterModel->rowCount() == 0)
    {
        filterProperties *fp = new filterProperties(vendor, "", "", "");
        for (QString &filter : m_currentFilterLabels)
        {
            fp->color = filter;
            KStarsData::Instance()->userdb()->AddFilter(fp);
        }

        m_FilterModel->select();
    }
    // Make sure all the filter colors match DB. If not update model to sync with INDI filter values
    else
    {
        for (int i = 0; i < m_FilterModel->rowCount(); ++i)
        {
            QModelIndex index = m_FilterModel->index(i, FM_LABEL);
            if (m_FilterModel->data(index).toString() != m_currentFilterLabels[i])
            {
                m_FilterModel->setData(index, m_currentFilterLabels[i]);
            }
        }
    }

    lockDelegate->setValues(getLockDelegates());

    reloadFilters();
    resizeDialog();
}

void FilterManager::resizeDialog()
{
    // Resize the columns to the data and then the dialog to the rows and columns
    m_FilterView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    int width = m_FilterView->horizontalHeader()->length() + 50;
    int height = label->height() + m_FilterView->verticalHeader()->length() + label_2->height() + buttonBox->height() + 100;
    this->resize(width, height);
}
// This function processes the list of active filters and returns a list of filters that could be lock filters
// i.e. that don't themselves have locks.
QStringList FilterManager::getLockDelegates()
{
    QStringList lockDelegates;

    for (int i = 0; i < m_ActiveFilters.count(); i++)
    {
        if (m_ActiveFilters[i]->lockedFilter() == NULL_FILTER)
            lockDelegates.append(m_ActiveFilters[i]->color());
    }
    return lockDelegates;
}

void FilterManager::reloadFilters()
{
    qDeleteAll(m_ActiveFilters);
    currentFilter = nullptr;
    targetFilter = nullptr;
    m_ActiveFilters.clear();
    operationQueue.clear();

    filterProperties *fp = new filterProperties("", "", "", "");

    for (int i = 0; i < m_FilterModel->rowCount(); ++i)
    {
        QSqlRecord record     = m_FilterModel->record(i);
        QString id            = record.value("id").toString();

        fp->vendor            = record.value("Vendor").toString();
        fp->model             = record.value("Model").toString();
        fp->type              = record.value("Type").toString();
        fp->color             = record.value("Color").toString();
        fp->exposure          = record.value("Exposure").toDouble();
        fp->offset            = record.value("Offset").toInt();
        fp->lockedFilter      = record.value("LockedFilter").toString();
        fp->useAutoFocus      = record.value("UseAutoFocus").toInt() == 1;
        fp->absFocusPos       = record.value("AbsoluteFocusPosition").toInt();
        fp->focusTemperature  = record.value("FocusTemperature").toDouble();
        fp->focusAltitude     = record.value("FocusAltitude").toDouble();
        fp->focusDatetime     = record.value("FocusDatetime").toString();
        fp->focusTicksPerTemp = record.value("FocusTicksPerTemp").toDouble();
        fp->focusTicksPerAlt  = record.value("FocusTicksPerAlt").toDouble();
        fp->wavelength        = record.value("Wavelength").toInt();
        OAL::Filter *o        = new OAL::Filter(id, fp);
        m_ActiveFilters.append(o);
    }
}

void FilterManager::setFilterWheel(ISD::FilterWheel *filter)
{
    // Return if same device and we already initialized the properties.
    if (m_FilterWheel == filter && m_FilterNameProperty && m_FilterPositionProperty)
        return;
    else if (m_FilterWheel)
        m_FilterWheel->disconnect(this);

    m_FilterWheel = filter;

    m_FilterNameProperty = nullptr;
    m_FilterPositionProperty = nullptr;
    m_FilterConfirmSet = nullptr;

    if (!m_FilterWheel)
        return;

    connect(m_FilterWheel, &ISD::ConcreteDevice::propertyUpdated, this, &FilterManager::updateProperty);
    connect(m_FilterWheel, &ISD::ConcreteDevice::Disconnected, this, &FilterManager::processDisconnect);

    refreshFilterProperties();
}

void FilterManager::refreshFilterProperties()
{
    if (m_FilterNameProperty && m_FilterPositionProperty)
    {
        if (m_FilterConfirmSet == nullptr)
            m_FilterConfirmSet = m_FilterWheel->getSwitch("CONFIRM_FILTER_SET");

        // All filters are synced up?
        if (m_currentFilterLabels.count() == m_FilterNameProperty->ntp)
            return;
    }

    filterNameLabel->setText(m_FilterWheel->getDeviceName());

    m_currentFilterLabels.clear();

    m_FilterNameProperty = m_FilterWheel->getText("FILTER_NAME");
    m_FilterPositionProperty = m_FilterWheel->getNumber("FILTER_SLOT");
    m_FilterConfirmSet = m_FilterWheel->getSwitch("CONFIRM_FILTER_SET");

    refreshFilterLabels();
    refreshFilterPosition();

    if (m_currentFilterPosition >= 1 && m_currentFilterPosition <= m_ActiveFilters.count())
        lastFilterOffset = m_ActiveFilters[m_currentFilterPosition - 1]->offset();
}

QStringList FilterManager::getFilterLabels(bool forceRefresh)
{
    if ((!m_currentFilterLabels.empty() && forceRefresh == false) || !m_FilterNameProperty || !m_FilterPositionProperty)
        return m_currentFilterLabels;

    QStringList filterList;

    for (int i = 0; i < m_FilterPositionProperty->np[0].max; i++)
    {
        if (m_FilterNameProperty != nullptr && (i < m_FilterNameProperty->ntp))
            filterList.append(m_FilterNameProperty->tp[i].text);
    }

    return filterList;
}

int FilterManager::getFilterPosition(bool forceRefresh)
{
    if (forceRefresh == false || m_FilterPositionProperty == nullptr)
        return m_currentFilterPosition;

    return static_cast<int>(m_FilterPositionProperty->np[0].value);
}

void FilterManager::refreshFilterLabels()
{
    QList filters = getFilterLabels(true);

    if (filters != m_currentFilterLabels)
    {
        m_currentFilterLabels = filters;
        refreshFilterModel();

        emit labelsChanged(filters);

        // refresh position after filter changes
        refreshFilterPosition();
    }
}

void FilterManager::refreshFilterPosition()
{

    int pos = getFilterPosition(true);
    if (pos != m_currentFilterPosition)
    {
        m_currentFilterPosition = pos;
        emit positionChanged(pos);
    }
}

bool FilterManager::setFilterPosition(uint8_t position, FilterPolicy policy)
{
    // Position 1 to Max
    if (position > m_ActiveFilters.count() || m_currentFilterPosition < 1 || position < 1)
        return false;

    m_Policy = policy;
    currentFilter = m_ActiveFilters[m_currentFilterPosition - 1];
    targetFilter = m_ActiveFilters[position - 1];

    buildOperationQueue(FILTER_CHANGE);

    executeOperationQueue();

    return true;
}


void FilterManager::updateProperty(INDI::Property prop)
{
    if (m_FilterWheel == nullptr || m_FilterWheel->getDeviceName() != prop.getDeviceName())
        return;

    if (prop.isNameMatch("FILTER_NAME"))
    {
        auto tvp = prop.getText();
        m_FilterNameProperty = tvp;

        refreshFilterLabels();
    }
    else if (prop.isNameMatch("FILTER_SLOT"))
    {
        m_FilterChangeTimeout.stop();

        auto nvp = prop.getNumber();
        // If filter fails to change position while we request that
        // fail immediately.
        if (state == FILTER_CHANGE && nvp->s == IPS_ALERT)
        {
            emit failed();
            return;
        }

        if (nvp->s != IPS_OK)
            return;

        m_FilterPositionProperty = nvp;
        refreshFilterPosition();

        if (state == FILTER_CHANGE)
            executeOperationQueue();
        // If filter is changed externally, record its current offset as the starting offset.
        else if (state == FILTER_IDLE && m_ActiveFilters.count() >= m_currentFilterPosition)
            lastFilterOffset = m_ActiveFilters[m_currentFilterPosition - 1]->offset();
    }
}


void FilterManager::processDisconnect()
{
    m_currentFilterLabels.clear();
    m_currentFilterPosition = -1;
    m_FilterNameProperty = nullptr;
    m_FilterPositionProperty = nullptr;
}

void FilterManager::buildOperationQueue(FilterState operation)
{
    operationQueue.clear();
    m_useTargetFilter = false;

    switch (operation)
    {
        case FILTER_CHANGE:
        {
            // Setup whether we'll run an Autofocus after the Filter Change. The normal reason is that a Filter
            // Change has been requested but there is also a condition whereby a previous filter change worked but
            // the associated Autofocus failed. In the latter case, the currentFilter == targetFilter so no need to
            // change the filter but we'll still need to retry Autofocus.
            bool runAF = false;
            if (m_FocusReady && (m_Policy & AUTOFOCUS_POLICY) && targetFilter->useAutoFocus())
                if ((targetFilter != currentFilter) || (m_LastAFFailed.value(targetFilter->color(), FALSE)))
                    runAF = true;

            if ( (m_Policy & CHANGE_POLICY) && targetFilter != currentFilter)
                m_useTargetFilter = true;

            if (m_useTargetFilter)
            {
                operationQueue.enqueue(FILTER_CHANGE);
                if (m_FocusReady && (m_Policy & OFFSET_POLICY))
                    operationQueue.enqueue(FILTER_OFFSET);
                else
                {
                    // Keep track of filter and offset either here or after the offset has been processed
                    lastFilterOffset = targetFilter->offset();
                    currentFilter = targetFilter;
                }

            }

            if (runAF)
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
            m_FilterChangeTimeout.stop();

            if (m_ConfirmationPending)
                return true;

            state = FILTER_CHANGE;
            if (m_useTargetFilter)
                targetFilterPosition = m_ActiveFilters.indexOf(targetFilter) + 1;
            m_FilterWheel->setPosition(targetFilterPosition);

            emit newStatus(state);

            if (m_FilterConfirmSet)
            {
                connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
                {
                    KSMessageBox::Instance()->disconnect(this);
                    m_ConfirmationPending = false;
                    m_FilterWheel->confirmFilter();
                });
                connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
                {
                    KSMessageBox::Instance()->disconnect(this);
                    m_ConfirmationPending = false;
                });

                m_ConfirmationPending = true;

                KSMessageBox::Instance()->questionYesNo(i18n("Set filter to %1. Is filter set?", targetFilter->color()),
                                                        i18n("Confirm Filter"));
            }
            // If automatic filter change with filter wheel, we start operation timeout
            else m_FilterChangeTimeout.start();
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
            qCDebug(KSTARS) << "FilterManager.cpp is triggering autofocus.";
            emit newStatus(state);
            emit runAutoFocus(AutofocusReason::FOCUS_FILTER, "");
            break;

        default:
            break;
    }

    // If an additional action is required, return return and continue later
    if (actionRequired)
        return true;
    // Otherwise, continue processing the queue
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
    auto filterDetails = getFilterByName(name);
    if (filterDetails)
        return filterDetails->exposure();

    // Default value
    return 1;
}

bool FilterManager::setFilterExposure(int index, double exposure)
{
    if (m_currentFilterLabels.empty())
        return false;

    QString color = m_currentFilterLabels[index];
    for (int i = 0; i < m_ActiveFilters.count(); i++)
    {
        if (color == m_ActiveFilters[i]->color())
        {
            m_FilterModel->setData(m_FilterModel->index(i, FM_EXPOSURE), exposure);
            m_FilterModel->submitAll();
            refreshFilterModel();
            return true;
        }
    }

    return false;
}

int FilterManager::getFilterOffset(const QString &name) const
{
    int offset = INVALID_VALUE;
    auto filterDetails = getFilterByName(name);
    if (filterDetails)
        offset = filterDetails->offset();

    return offset;
}

bool FilterManager::setFilterOffset(QString color, int offset)
{
    if (m_currentFilterLabels.empty())
        return false;

    for (int i = 0; i < m_ActiveFilters.count(); i++)
    {
        if (color == m_ActiveFilters[i]->color())
        {
            m_FilterModel->setData(m_FilterModel->index(i, FM_OFFSET), offset);
            m_FilterModel->submitAll();
            refreshFilterModel();
            return true;
        }
    }

    return false;
}

bool FilterManager::getFilterAbsoluteFocusDetails(const QString &name, int &focusPos, double &focusTemp,
        double &focusAlt) const
{
    auto filterDetails = getFilterByName(name);
    if (filterDetails)
    {
        focusPos = filterDetails->absoluteFocusPosition();
        focusTemp = filterDetails->focusTemperature();
        focusAlt = filterDetails->focusAltitude();
        return true;
    }

    return false;
}

bool FilterManager::setFilterAbsoluteFocusDetails(int index, int focusPos, double focusTemp, double focusAlt)
{
    if (index < 0 || index >= m_currentFilterLabels.count())
        return false;

    QString color = m_currentFilterLabels[index];
    for (int i = 0; i < m_ActiveFilters.count(); i++)
    {
        if (color == m_ActiveFilters[i]->color())
        {
            m_FilterModel->setData(m_FilterModel->index(i, FM_LAST_AF_SOLUTION), focusPos);
            m_FilterModel->setData(m_FilterModel->index(i, FM_LAST_AF_TEMP), focusTemp);
            m_FilterModel->setData(m_FilterModel->index(i, FM_LAST_AF_ALT), focusAlt);
            m_FilterModel->setData(m_FilterModel->index(i, FM_LAST_AF_DATETIME),
                                   KStarsData::Instance()->lt().toString(DATETIME_FORMAT));
            m_FilterModel->submitAll();
            refreshFilterModel();
            return true;
        }
    }

    return false;
}

bool FilterManager::getAFDatetime(const QString &name, QDateTime &datetime) const
{
    auto filterDetails = getFilterByName(name);
    if (filterDetails)
    {
        datetime = filterDetails->focusDatetime();
        return true;
    }
    return false;
}

QString FilterManager::getFilterLock(const QString &name) const
{
    auto filterDetails = getFilterByName(name);
    if (filterDetails)
        return filterDetails->lockedFilter();

    // Default value
    return NULL_FILTER;
}

bool FilterManager::setFilterLock(int index, QString name)
{
    if (m_currentFilterLabels.empty())
        return false;

    QString color = m_currentFilterLabels[index];
    for (int i = 0; i < m_ActiveFilters.count(); i++)
    {
        if (color == m_ActiveFilters[i]->color())
        {
            m_FilterModel->setData(m_FilterModel->index(i, FM_LOCK_FILTER), name);
            m_FilterModel->submitAll();
            refreshFilterModel();
            return true;
        }
    }

    return false;
}

int FilterManager::getFilterWavelength(const QString &name) const
{
    auto filterDetails = getFilterByName(name);
    if (filterDetails)
        return filterDetails->wavelength();

    // Default value
    return 500;
}

double FilterManager::getFilterTicksPerTemp(const QString &name) const
{
    auto filterDetails = getFilterByName(name);
    if (filterDetails)
        return filterDetails->focusTicksPerTemp();

    // Something's wrong so return 0
    return 0.0;
}

double FilterManager::getFilterTicksPerAlt(const QString &name) const
{
    auto filterDetails = getFilterByName(name);
    if (filterDetails)
        return filterDetails->focusTicksPerAlt();

    // Something's wrong so return 0
    return 0.0;
}

OAL::Filter * FilterManager::getFilterByName(const QString &name) const
{
    if (m_currentFilterLabels.empty() ||
            m_currentFilterPosition < 1 ||
            m_currentFilterPosition > m_currentFilterLabels.count())
        return nullptr;

    QString color = name;
    if (color.isEmpty())
        color = m_currentFilterLabels[m_currentFilterPosition - 1];

    auto pos = std::find_if(m_ActiveFilters.begin(), m_ActiveFilters.end(), [color](OAL::Filter * oneFilter)
    {
        return (oneFilter->color() == color);
    });

    if (pos != m_ActiveFilters.end())
        return (*pos);
    else
        return nullptr;
}

void FilterManager::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (m_FilterWheel && (m_FilterWheel->getDeviceName() == device->getDeviceName()))
    {
        m_FilterNameProperty = nullptr;
        m_FilterPositionProperty = nullptr;
        m_FilterWheel = nullptr;
        m_currentFilterLabels.clear();
        m_currentFilterPosition = 0;
        qDeleteAll(m_ActiveFilters);
        m_ActiveFilters.clear();
        delete(m_FilterModel);
        m_FilterModel = nullptr;
    }
}

void FilterManager::setFocusStatus(Ekos::FocusState focusState)
{
    setAutofocusStatus(focusState);

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
                emit runAutoFocus(AutofocusReason::FOCUS_FILTER, "");
                break;

            default:
                break;
        }
    }
}

void FilterManager::setAutofocusStatus(Ekos::FocusState focusState)
{
    if (targetFilter)
    {
        if (focusState == FOCUS_COMPLETE)
            m_LastAFFailed.insert(targetFilter->color(), FALSE);
        else if (focusState == FOCUS_ABORTED || focusState == FOCUS_FAILED)
            m_LastAFFailed.insert(targetFilter->color(), TRUE);
    }
}

bool FilterManager::syncAbsoluteFocusPosition(int index)
{
    if (m_FocusReady == false)
    {
        qCWarning(KSTARS_INDI) << __FUNCTION__ << "No Focuser detected.";
        return true;
    }
    else if (index < 0 || index > m_ActiveFilters.count())
    {
        // We've been asked to set the focus position but something's wrong because
        // the passed in filter index is bad. Give up and return true - returning false
        // just results in an infinite retry loop.
        qCWarning(KSTARS_INDI) << __FUNCTION__ << "index" << index << "is out of bounds.";
        return true;
    }

    // By default filter absolute focus offset is zero
    // JM 2023.07.03: So if it is zero, we return immediately.
    auto absFocusPos = m_ActiveFilters[index]->absoluteFocusPosition();

    if (m_FocusAbsPosition == absFocusPos || absFocusPos <= 0)
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

bool FilterManager::setFilterNames(const QStringList &newLabels)
{
    if (m_FilterWheel == nullptr || m_currentFilterLabels.empty())
        return false;

    m_FilterWheel->setLabels(newLabels);
    return true;
}

QJsonObject FilterManager::toJSON()
{
    if (!m_FilterWheel)
        return QJsonObject();

    QJsonArray filters;

    for (int i = 0; i < m_FilterModel->rowCount(); ++i)
    {
        QJsonObject oneFilter =
        {
            {"index", i},
            {"label", m_FilterModel->data(m_FilterModel->index(i, FM_LABEL)).toString()},
            {"exposure", m_FilterModel->data(m_FilterModel->index(i, FM_EXPOSURE)).toDouble()},
            {"offset", m_FilterModel->data(m_FilterModel->index(i, FM_OFFSET)).toInt()},
            {"autofocus", m_FilterModel->data(m_FilterModel->index(i, FM_AUTO_FOCUS)).toBool()},
            {"lock", m_FilterModel->data(m_FilterModel->index(i, FM_LOCK_FILTER)).toString()},
            {"lastafsolution", m_FilterModel->data(m_FilterModel->index(i, FM_LAST_AF_SOLUTION)).toInt()},
            {"lastaftemp", m_FilterModel->data(m_FilterModel->index(i, FM_LAST_AF_TEMP)).toDouble()},
            {"lastafalt", m_FilterModel->data(m_FilterModel->index(i, FM_LAST_AF_ALT)).toDouble()},
            {"lastafdt", m_FilterModel->data(m_FilterModel->index(i, FM_LAST_AF_DATETIME)).toString()},
            {"tickspertemp", m_FilterModel->data(m_FilterModel->index(i, FM_TICKS_PER_TEMP)).toDouble()},
            {"ticksperalt", m_FilterModel->data(m_FilterModel->index(i, FM_TICKS_PER_ALT)).toDouble()},
            {"wavelength", m_FilterModel->data(m_FilterModel->index(i, FM_WAVELENGTH)).toInt()},
        };

        filters.append(oneFilter);
    }

    QJsonObject data =
    {
        {"device", m_FilterWheel->getDeviceName()},
        {"filters", filters}
    };

    return data;

}

void FilterManager::setFilterData(const QJsonObject &settings)
{
    if (!m_FilterWheel)
        return;

    if (settings["device"].toString() != m_FilterWheel->getDeviceName())
        return;

    QJsonArray filters = settings["filters"].toArray();
    QStringList labels = getFilterLabels();

    for (auto oneFilterRef : filters)
    {
        QJsonObject oneFilter = oneFilterRef.toObject();
        int row = oneFilter["index"].toInt();

        labels[row] = oneFilter["label"].toString();
        m_FilterModel->setData(m_FilterModel->index(row, FM_LABEL), oneFilter["label"].toString());
        m_FilterModel->setData(m_FilterModel->index(row, FM_EXPOSURE), oneFilter["exposure"].toDouble());
        m_FilterModel->setData(m_FilterModel->index(row, FM_OFFSET), oneFilter["offset"].toInt());
        m_FilterModel->setData(m_FilterModel->index(row, FM_AUTO_FOCUS), oneFilter["autofocus"].toBool());
        m_FilterModel->setData(m_FilterModel->index(row, FM_LOCK_FILTER), oneFilter["lock"].toString());
        m_FilterModel->setData(m_FilterModel->index(row, FM_LAST_AF_SOLUTION), oneFilter["lastafsolution"].toInt());
        m_FilterModel->setData(m_FilterModel->index(row, FM_LAST_AF_TEMP), oneFilter["lastaftemp"].toDouble());
        m_FilterModel->setData(m_FilterModel->index(row, FM_LAST_AF_ALT), oneFilter["lastafalt"].toDouble());
        m_FilterModel->setData(m_FilterModel->index(row, FM_LAST_AF_DATETIME), oneFilter["lastafdt"].toString(DATETIME_FORMAT));
        m_FilterModel->setData(m_FilterModel->index(row, FM_TICKS_PER_TEMP), oneFilter["tickspertemp"].toDouble());
        m_FilterModel->setData(m_FilterModel->index(row, FM_TICKS_PER_ALT), oneFilter["ticksperalt"].toDouble());
        m_FilterModel->setData(m_FilterModel->index(row, FM_WAVELENGTH), oneFilter["wavelength"].toInt());
    }

    m_FilterModel->submitAll();
    setFilterNames(labels);

    refreshFilterModel();
}

void FilterManager::buildFilterOffsets()
{
    // Launch the Build Filter Offsets utility. The utility uses a sync call to launch the dialog
    QSharedPointer<FilterManager> filterManager;
    Ekos::Manager::Instance()->getFilterManager(m_FilterWheel->getDeviceName(), filterManager);
    BuildFilterOffsets bfo(filterManager);
}

void FilterManager::signalRunAutoFocus(AutofocusReason autofocusReason, const QString &reasonInfo)
{
    // BuildFilterOffsets signalled runAutoFocus so pass signal to Focus
    emit runAutoFocus(autofocusReason, reasonInfo);
}

void FilterManager::autoFocusComplete(FocusState completionState, int currentPosition, double currentTemperature,
                                      double currentAlt)
{
    // Focus signalled Autofocus completed so pass signal to BuildFilterOffsets
    emit autoFocusDone(completionState, currentPosition, currentTemperature, currentAlt);
}

void FilterManager::signalAbortAutoFocus()
{
    // BuildFilterOffsets signalled abortAutoFocus so pass signal to Focus
    emit abortAutoFocus();
}

void FilterManager::checkFilterChangeTimeout()
{
    if (state == FILTER_CHANGE)
    {
        qCWarning(KSTARS) << "FilterManager.cpp filter change timed out.";
        emit failed();
    }
}
}

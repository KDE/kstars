/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "filtermanager.h"
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
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setupUi(this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(close()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    connect(buildOffsetsButton, &QPushButton::clicked, this, &FilterManager::buildFilterOffsets);

    QSqlDatabase userdb = QSqlDatabase::cloneDatabase(KStarsData::Instance()->userdb()->GetDatabase(),
                          QUuid::createUuid().toString());
    userdb.open();

    kcfg_FlatSyncFocus->setChecked(Options::flatSyncFocus());
    connect(kcfg_FlatSyncFocus, &QCheckBox::toggled, this, [this]()
    {
        Options::setFlatSyncFocus(kcfg_FlatSyncFocus->isChecked());
    });

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

    m_FilterModel->setHeaderData(FM_TICKS_PER_TEMP, Qt::Horizontal,
                                 i18n("The number of ticks per °C increase in temperature. +ve for outward focuser movement"), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_TICKS_PER_TEMP, Qt::Horizontal, i18n("Ticks / °C"));

    m_FilterModel->setHeaderData(FM_TICKS_PER_ALT, Qt::Horizontal,
                                 i18n("The number of ticks per degree increase in altitude. +ve for outward focuser movement"), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_TICKS_PER_ALT, Qt::Horizontal, i18n("Ticks / °Alt"));

    m_FilterModel->setHeaderData(FM_WAVELENGTH, Qt::Horizontal, i18n("Mid-point wavelength of filter in nm"), Qt::ToolTipRole);
    m_FilterModel->setHeaderData(FM_WAVELENGTH, Qt::Horizontal, i18n("Wavelength"));

    connect(m_FilterModel, &QSqlTableModel::dataChanged, this, &FilterManager::updated);

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

    // Ticks / °C delegate
    ticksPerTempDelegate = new DoubleDelegate(m_FilterView, -10000.0, 10000.0, 1.0);
    m_FilterView->setItemDelegateForColumn(FM_TICKS_PER_TEMP, ticksPerTempDelegate);

    // Ticks / °Altitude delegate
    ticksPerAltDelegate = new DoubleDelegate(m_FilterView, -10000.0, 10000.0, 1.0);
    m_FilterView->setItemDelegateForColumn(FM_TICKS_PER_ALT, ticksPerAltDelegate);

    // Wavelength delegate
    wavelengthDelegate = new IntegerDelegate(m_FilterView, 200, 1000, 50);
    m_FilterView->setItemDelegateForColumn(FM_WAVELENGTH, wavelengthDelegate);

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

    m_currentFilterPosition = getFilterPosition(true);
    m_currentFilterLabels = getFilterLabels(true);

    if (m_currentFilterLabels.isEmpty() == false)
        refreshFilterModel();

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

bool FilterManager::setFilterPosition(uint8_t position, FilterPolicy policy)
{
    // Position 1 to Max
    if (position > m_ActiveFilters.count())
        return false;

    m_Policy = policy;
    currentFilter = m_ActiveFilters[m_currentFilterPosition - 1];
    targetFilter = m_ActiveFilters[position - 1];

    if (currentFilter == targetFilter)
    {
        emit ready();
        return true;
    }

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

        QStringList newFilterLabels = getFilterLabels(true);

        if (newFilterLabels != m_currentFilterLabels)
        {
            m_currentFilterLabels = newFilterLabels;

            refreshFilterModel();

            emit labelsChanged(newFilterLabels);
        }
    }
    else if (prop.isNameMatch("FILTER_SLOT"))
    {
        auto nvp = prop.getNumber();
        if (nvp->s != IPS_OK)
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
            emit checkFocus(0.01);
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
            m_FilterModel->submitAll();
            refreshFilterModel();
            return true;
        }
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
        m_FilterModel->setData(m_FilterModel->index(row, FM_TICKS_PER_TEMP), oneFilter["tickspertemp"].toDouble());
        m_FilterModel->setData(m_FilterModel->index(row, FM_TICKS_PER_ALT), oneFilter["ticksperalt"].toDouble());
        m_FilterModel->setData(m_FilterModel->index(row, FM_WAVELENGTH), oneFilter["wavelength"].toInt());
    }

    m_FilterModel->submitAll();
    setFilterNames(labels);

    refreshFilterModel();
}

// buildFilterOffsets sets up a dialog to manage automatic calculation of filter offsets
// by performing autofocus runs on lock and dependant filters and working out the offsets.
void FilterManager::buildFilterOffsets()
{
    initBuildFilterOffsets();
    setupBuildFilterOffsetsTable();

    // Create a layout
    QVBoxLayout *layout = new QVBoxLayout(m_buildOffsetsDialog);
    // Add the table view to the layout
    layout->addWidget(m_BFOView);

    // Create buttons, add to the layout and set enabled state
    m_buildOffsetButtonBox = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Save, m_buildOffsetsDialog);
    m_runButton = m_buildOffsetButtonBox->addButton("Run", QDialogButtonBox::ActionRole);
    m_stopButton = m_buildOffsetButtonBox->addButton("Stop", QDialogButtonBox::ActionRole);
    layout->addWidget(m_buildOffsetButtonBox);

    // Set the buttons' state
    setBuildFilterOffsetsButtons(BFO_INIT);

    // Connect up button callbacks
    connect(m_buildOffsetButtonBox, &QDialogButtonBox::accepted, m_buildOffsetsDialog, &QDialog::accept);
    connect(m_buildOffsetButtonBox, &QDialogButtonBox::rejected, m_buildOffsetsDialog, [this]()
    {
        // Put up an "are you sure" dialog before exiting
        if (KMessageBox::questionYesNo(KStars::Instance(),
                                       i18n("Are you sure you want to quit?")) == KMessageBox::Yes)
        {
            // User wants to close
            m_buildOffsetsDialog->reject();
        }
    });

    connect(m_runButton, &QPushButton::clicked, this, &FilterManager::buildTheOffsets);
    connect(m_stopButton, &QPushButton::clicked, this, &FilterManager::stopProcessing);
    connect(m_buildOffsetButtonBox->button(QDialogButtonBox::Save), &QPushButton::clicked, this,
            &FilterManager::saveTheOffsets);
    connect(m_buildOffsetButtonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &FilterManager::close);

    // Connect cell changed callback
    connect(m_BFOModel, &QStandardItemModel::itemChanged, this, &FilterManager::itemChanged);

    // Add a progress bar
    m_buildOffsetsProgressBar = new QProgressBar(m_buildOffsetsDialog);
    m_buildOffsetsProgressBar->setOrientation(Qt::Horizontal);
    layout->addWidget(m_buildOffsetsProgressBar);

    m_StatusBar = new QStatusBar(m_buildOffsetsDialog);
    m_StatusBar->showMessage(i18n("Idle"));
    layout->addWidget(m_StatusBar);

    // Add the layout to the dialog
    m_buildOffsetsDialog->setLayout(layout);

    // Filter change is signalled by ready().
    connect(this, &FilterManager::ready, this, &FilterManager::buildTheOffsetsTaskComplete);

    // Resize the dialog based on the data
    buildOffsetsDialogResize();

    if (m_buildOffsetsDialog->exec() == QDialog::Accepted)
    {
        disconnect(this, &FilterManager::ready, this, &FilterManager::buildTheOffsetsTaskComplete);
    }
    else
    {
        disconnect(this, &FilterManager::ready, this, &FilterManager::buildTheOffsetsTaskComplete);
    }

    // Set flag off so any stray signals will be ignored after the dialog closes
    m_inBuildOffsets = false;
    // Drain any remaining Q items
    buildOffsetsQItem qItem;
    while (!m_buildOffsetsQ.isEmpty())
    {
        qItem = m_buildOffsetsQ.dequeue();
    }
}

void FilterManager::initBuildFilterOffsets()
{
    m_inBuildOffsets = false;
    m_stopFlag = m_problemFlag = m_abortAFPending = m_tableInEditMode = false;
    m_lockFilters.clear();
    m_dependantFilters.clear();
    m_dependantOffset.clear();
    m_rowIdx = m_colIdx = 0;

    // Drain any old queue items
    buildOffsetsQItem qItem;
    while (!m_buildOffsetsQ.isEmpty())
    {
        qItem = m_buildOffsetsQ.dequeue();
    }

    // Setup the dialog. Since its unlikely to be used more than once, delete it on close
    m_buildOffsetsDialog = new QDialog(this);
    m_buildOffsetsDialog->setAttribute (Qt::WA_DeleteOnClose);
    m_buildOffsetsDialog->setWindowTitle("Build Filter Offsets");
}

void FilterManager::setupBuildFilterOffsetsTable()
{
    // Setup MVC
    m_BFOModel = new QStandardItemModel(m_buildOffsetsDialog);
    m_BFOView = new QTableView(m_buildOffsetsDialog);
    m_BFOView->setModel(m_BFOModel);
    m_BFOView->setSelectionMode(QAbstractItemView::SingleSelection);

    // Setup the table view
    QStringList Headers { i18n("Filter"), i18n("Offset"), i18n("Lock Filter"), i18n("# Focus Runs") };
    m_BFOModel->setColumnCount(Headers.count());
    m_BFOModel->setHorizontalHeaderLabels(Headers);

    // Setup edit delegates for each column
    // No Edit delegates for Filter, Offset and Lock Filter
    NotEditableDelegate *noEditDel = new NotEditableDelegate(m_BFOView);
    m_BFOView->setItemDelegateForColumn(BFO_FILTER, noEditDel);
    m_BFOView->setItemDelegateForColumn(BFO_OFFSET, noEditDel);
    m_BFOView->setItemDelegateForColumn(BFO_LOCK, noEditDel);

    // # Focus Runs delegate
    IntegerDelegate *numRunsDel = new IntegerDelegate(m_BFOView, 0, 10, 1);
    m_BFOView->setItemDelegateForColumn(BFO_NUM_FOCUS_RUNS, numRunsDel);

    // Build the data to display
    for (int i = 0; i < m_ActiveFilters.count(); i++)
    {
        if (m_ActiveFilters[i]->lockedFilter() != NULL_FILTER)
        {
            // Add to the list of lock filters unless we already have it in the list
            if (!m_lockFilters.contains(m_ActiveFilters[i]->lockedFilter()))
                m_lockFilters.push_back(m_ActiveFilters[i]->lockedFilter());

            // Add to the list of dependant filters unless already on the list
            m_dependantFilters.push_back(m_ActiveFilters[i]->color());
            m_dependantOffset.push_back(m_ActiveFilters[i]->offset());
        }
    }

    // Setup the table
    const int numRows = m_lockFilters.count() + m_dependantFilters.count();
    m_BFOModel->setRowCount(numRows);

    // Start with the lock filters
    for (int row = 0 ; row < m_lockFilters.count(); row++ )
    {
        // Filter
        QStandardItem* item0 = new QStandardItem(m_lockFilters[row]);
        m_BFOModel->setItem(row, BFO_FILTER, item0);

        // Offset
        QStandardItem* item1 = new QStandardItem(QString::number(0));
        m_BFOModel->setItem(row, BFO_OFFSET, item1);

        // Lock filter
        QStandardItem* item2 = new QStandardItem(QString(NULL_FILTER));
        m_BFOModel->setItem(row, BFO_LOCK, item2);

        // Number of AF runs to perform
        QStandardItem* item3 = new QStandardItem(QString::number(5));
        m_BFOModel->setItem(row, BFO_NUM_FOCUS_RUNS, item3);
    }

    // Now the dependant filters
    for (int row = 0; row < m_dependantFilters.count(); row++ )
    {
        // Filter
        QStandardItem* item0 = new QStandardItem(m_dependantFilters[row]);
        m_BFOModel->setItem(row + m_lockFilters.count(), BFO_FILTER, item0);

        // Offset
        QStandardItem* item1 = new QStandardItem(QString::number(m_dependantOffset[row]));
        m_BFOModel->setItem(row + m_lockFilters.count(), BFO_OFFSET, item1);

        // Lock Filter
        QStandardItem* item2 = new QStandardItem(getFilterLock(m_dependantFilters[row]));
        m_BFOModel->setItem(row + m_lockFilters.count(), BFO_LOCK, item2);

        // Number of AF runs to perform - editable
        QStandardItem* item3 = new QStandardItem(QString::number(5));
        m_BFOModel->setItem(row + m_lockFilters.count(), BFO_NUM_FOCUS_RUNS, item3);
    }
}

void FilterManager::setBuildFilterOffsetsButtons(BFOButtonState state)
{
    switch (state)
    {
        case BFO_INIT:
            m_runButton->setEnabled(true);
            m_stopButton->setEnabled(false);
            m_buildOffsetButtonBox->button(QDialogButtonBox::Save)->setEnabled(false);
            m_buildOffsetButtonBox->button(QDialogButtonBox::Close)->setEnabled(true);
            break;

        case BFO_RUN:
            m_runButton->setEnabled(false);
            m_stopButton->setEnabled(true);
            m_buildOffsetButtonBox->button(QDialogButtonBox::Save)->setEnabled(false);
            m_buildOffsetButtonBox->button(QDialogButtonBox::Close)->setEnabled(false);
            break;

        case BFO_SAVE:
            m_runButton->setEnabled(false);
            m_stopButton->setEnabled(false);
            m_buildOffsetButtonBox->button(QDialogButtonBox::Save)->setEnabled(true);
            m_buildOffsetButtonBox->button(QDialogButtonBox::Close)->setEnabled(true);
            break;

        case BFO_STOP:
            m_runButton->setEnabled(false);
            m_stopButton->setEnabled(false);
            m_buildOffsetButtonBox->button(QDialogButtonBox::Save)->setEnabled(false);
            m_buildOffsetButtonBox->button(QDialogButtonBox::Close)->setEnabled(false);
            break;

        default:
            break;
    }
}

// Loop through all the filters to process, and for each...
// - set Autofocus to use the filter
// - Loop for the number of runs chosen by the user for that filter
//   - Run AF
//   - Get the focus solution
//   - Load the focus solution into table widget in the appropriate cell
// - Calculate the average AF solution for that filter and display it
void FilterManager::buildTheOffsets()
{
    buildOffsetsQItem qItem;

    // Set the buttons
    setBuildFilterOffsetsButtons(BFO_RUN);

    // Make the Number of runs column not editable
    // No Edit delegates for Filter, Offset and Lock Filter
    QPointer<NotEditableDelegate> noEditDel = new NotEditableDelegate(m_BFOView);
    m_BFOView->setItemDelegateForColumn(BFO_NUM_FOCUS_RUNS, noEditDel);

    // Loop through the work to do and load up Queue and extend tableWidget to record AF answers
    int maxAFRuns = 0;
    // Start with the lock filters
    for (int row = 0; row < m_lockFilters.count(); row++ )
    {
        // Set the filter change
        qItem.color = m_lockFilters[row];
        qItem.changeFilter = true;
        m_buildOffsetsQ.enqueue(qItem);

        // Load up the AF runs based on how many the user requested
        qItem.changeFilter = false;
        int numRuns = m_BFOModel->item(row, BFO_NUM_FOCUS_RUNS)->text().toInt();
        maxAFRuns = std::max(maxAFRuns, numRuns);
        for (int runNum = 1; runNum <= numRuns; runNum++ )
        {
            qItem.numAFRun = runNum;
            m_buildOffsetsQ.enqueue(qItem);
        }
    }

    // Now the dependant filters
    for (int row = 0; row < m_dependantFilters.count(); row++ )
    {
        int numRuns = m_BFOModel->item(row + m_lockFilters.count(), BFO_NUM_FOCUS_RUNS)->text().toInt();
        if (numRuns <= 0)
            continue;

        // We have at least 1 AF run to do so firstly set the filter change
        qItem.color = m_dependantFilters[row];
        qItem.changeFilter = true;
        m_buildOffsetsQ.enqueue(qItem);

        // Load up the AF runs based on how many the user requested
        qItem.changeFilter = false;
        maxAFRuns = std::max(maxAFRuns, numRuns);
        for (int runNum = 1; runNum <= numRuns; runNum++ )
        {
            qItem.numAFRun = runNum;
            m_buildOffsetsQ.enqueue(qItem);
        }
    }

    // Add columns to the Model for AF runs and set the headers. Each AF run result is editable
    // but the calculated average is not.
    int origCols = m_BFOModel->columnCount();
    m_BFOModel->setColumnCount(origCols + maxAFRuns + 3);
    for (int col = 0; col < maxAFRuns; col++)
    {
        QStandardItem *newItem = new QStandardItem(QString("AF Run %1").arg(col + 1));
        m_BFOModel->setHorizontalHeaderItem(origCols + col, newItem);
        m_BFOView->setItemDelegateForColumn(origCols + col, noEditDel);
    }

    // Add 3 more columns for the average of the AF runs, the offset and whether to save the offset
    QStandardItem *averageItem = new QStandardItem(QString(i18n("Average")));
    m_BFOModel->setHorizontalHeaderItem(origCols + maxAFRuns, averageItem);
    m_BFOView->setItemDelegateForColumn(origCols + maxAFRuns, noEditDel);

    QStandardItem *offsetItem = new QStandardItem(QString(i18n("New Offset")));
    m_BFOModel->setHorizontalHeaderItem(origCols + maxAFRuns + 1, offsetItem);
    m_BFOView->setItemDelegateForColumn(origCols + maxAFRuns + 1, noEditDel);

    QPointer<ToggleDelegate> saveDelegate = new ToggleDelegate(m_BFOModel);
    QStandardItem *saveItem = new QStandardItem(QString(i18n("Save")));
    m_BFOModel->setHorizontalHeaderItem(origCols + maxAFRuns + 2, saveItem);
    m_BFOView->setItemDelegateForColumn(origCols + maxAFRuns + 2, saveDelegate);

    // Resize the dialog
    buildOffsetsDialogResize();

    // Set the selected cell to the first AF run
    m_rowIdx = 0;
    m_colIdx = BFO_AF_RUN_1;
    QModelIndex index = m_BFOView->model()->index(m_rowIdx, m_colIdx);
    m_BFOView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);

    // Initialise the progress bar
    m_buildOffsetsProgressBar->reset();
    m_buildOffsetsProgressBar->setRange(0, m_buildOffsetsQ.count());
    m_buildOffsetsProgressBar->setValue(0);

    // The Q has been loaded with all required actions so lets start processing
    m_inBuildOffsets = true;
    runBuildOffsets();
}

// This is signalled when an asynchronous task has been completed, either
// a filter change or an autofocus run
void FilterManager::buildTheOffsetsTaskComplete()
{
    if (m_stopFlag)
    {
        // User hit the stop button, so see what they want to do
        if (KMessageBox::questionYesNo(KStars::Instance(),
                                       i18n("Process stopped. Yes to restart, No to abort")) == KMessageBox::Yes)
        {
            // User wants to restart processing
            m_stopFlag = false;
            setBuildFilterOffsetsButtons(BFO_RUN);

            if (m_abortAFPending)
            {
                // If the in-flight task was aborted then retry - don't take the next task off the Q
                m_abortAFPending = false;
                processQItem(m_qItemInProgress);
            }
            else
            {
                // No tasks were aborted so we can just start the next task in the queue
                m_buildOffsetsProgressBar->setValue(m_buildOffsetsProgressBar->value() + 1);
                runBuildOffsets();
            }
        }
        else
        {
            // User wants to abort
            m_stopFlag = m_abortAFPending = false;
            m_buildOffsetsDialog->done(QDialog::Rejected);
        }

    }
    else if (m_problemFlag)
    {
        // The in flight task had a problem so see what the user wants to do
        if (KMessageBox::questionYesNo(KStars::Instance(),
                                       i18n("A problem occurred. Yes to retry, No to abort")) == KMessageBox::Yes)
        {
            // User wants to retry
            m_problemFlag = false;
            processQItem(m_qItemInProgress);
        }
        else
        {
            // User wants to abort
            m_problemFlag = false;
            m_buildOffsetsDialog->done(QDialog::Rejected);
        }
    }
    else
    {
        // All good so update the progress bar and process the next task
        m_buildOffsetsProgressBar->setValue(m_buildOffsetsProgressBar->value() + 1);
        runBuildOffsets();
    }
}

// Resize the dialog to the data
void FilterManager::buildOffsetsDialogResize()
{
    // Resize the columns to the data
    m_BFOView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    // Resize the dialog to the width and height of the table widget
    int width = m_BFOView->horizontalHeader()->length() + 40;
    int height = m_BFOView->verticalHeader()->length() + m_buildOffsetButtonBox->height() +
                 m_buildOffsetsProgressBar->height() + m_StatusBar->height() + 60;
    m_buildOffsetsDialog->resize(width, height);
}

void FilterManager::runBuildOffsets()
{
    if (m_buildOffsetsQ.isEmpty())
    {
        // All tasks have been actioned so allow the user to edit the results, save them or quit.
        setCellsEditable();
        setBuildFilterOffsetsButtons(BFO_SAVE);
        m_tableInEditMode = true;
        m_StatusBar->showMessage(QString(i18n("Processing complete.")));
    }
    else
    {
        // Take the next item off the queue
        m_qItemInProgress = m_buildOffsetsQ.dequeue();
        processQItem(m_qItemInProgress);
    }
}

void FilterManager::processQItem(buildOffsetsQItem currentItem)
{
    if (currentItem.changeFilter)
    {
        // Need to change filter
        m_StatusBar->showMessage(QString(i18n("Changing filter to %1...").arg(currentItem.color)));

        auto pos = m_currentFilterLabels.indexOf(currentItem.color) + 1;
        if (!setFilterPosition(pos, CHANGE_POLICY))
        {
            // Filter wheel position change failed.
            m_StatusBar->showMessage(QString(i18n("Problem changing filter to %1...").arg(currentItem.color)));
            m_problemFlag = true;
        }
    }
    else
    {
        // Signal an AF run with an arg of "build offsets"
        int run = m_colIdx - BFO_AF_RUN_1 + 1;
        int numRuns = m_BFOModel->item(m_rowIdx, BFO_NUM_FOCUS_RUNS)->text().toInt();
        m_StatusBar->showMessage(QString(i18n("Running Autofocus on %1 (%2/%3)...").arg(currentItem.color).arg(run).arg(numRuns)));
        emit runAutoFocus(m_inBuildOffsets);
    }
}

// This is called at the end of an AF run
void FilterManager::autoFocusComplete(FocusState completionState, int currentPosition)
{
    if (!m_inBuildOffsets)
        return;

    if (completionState != FOCUS_COMPLETE)
    {
        // The AF run has failed. If the user aborted the run then this is an expected signal
        if (!m_abortAFPending)
            // In this case the failure is a genuine problem so set a problem flag
            m_problemFlag = true;
    }
    else
    {
        // AF run was successful so load the result into the table
        // The Model update will trigger further updates
        QStandardItem *posItem = new QStandardItem(QString::number(currentPosition));
        m_BFOModel->setItem(m_rowIdx, m_colIdx, posItem);

        // Now see what's next, another AF run on this filter or are we moving to the next filter
        int numRuns = m_BFOModel->item(m_rowIdx, BFO_NUM_FOCUS_RUNS)->text().toInt();
        if (m_colIdx - 3 < numRuns)
            m_colIdx++;
        else
        {
            // Move the active cell to the next AF run in the table
            // Usually this will be the next row, but if this row has zero AF runs skip to the next
            for (int nextRow = m_rowIdx + 1; nextRow < m_lockFilters.count() + m_dependantFilters.count(); nextRow++)
            {
                if (m_BFOModel->item(nextRow, BFO_NUM_FOCUS_RUNS)->text().toInt() > 0)
                {
                    // Found the next filter to process
                    m_rowIdx = nextRow;
                    m_colIdx = BFO_AF_RUN_1;
                    break;
                }
            }
        }
        // Highlight the next cell...
        QModelIndex index = m_BFOView->model()->index(m_rowIdx, m_colIdx);
        m_BFOView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    }
    // Signal the next processing step
    emit ready();
}

// Called when the user wants to persist the calculated offsets
void FilterManager::saveTheOffsets()
{
    const int saveCol = m_BFOView->horizontalHeader()->count() - 1;
    const int offsetCol = saveCol - 1;
    for (int i = 0; i < m_dependantFilters.count(); i++)
    {
        int row = m_lockFilters.count() + i;
        if (m_BFOModel->item(row, saveCol))
        {
            // Check there's an item set for the current row before accessing
            if (m_BFOModel->item(row, saveCol)->text().toInt())
            {
                // Save item is set so persist the offset
                int offset = m_BFOModel->item(row, offsetCol)->text().toInt();
                if (!setFilterOffset(m_dependantFilters[i], offset))
                    qCDebug(KSTARS) << "Unable to save calculated offset for filter " << m_dependantFilters[i];
            }
        }
    }
}

void FilterManager::close()
{
    /*if (KMessageBox::questionYesNo(KStars::Instance(),
                                   i18n("Close without saving results?")) == KMessageBox::Yes)
    {
        // User wants to close
    }*/
}

// Processing done so make certain cells editable for the user
void FilterManager::setCellsEditable()
{
    // AF results columns
    int maxRuns = m_BFOModel->columnCount() - BFO_AF_RUN_1 - 3;

    // Enable an edit delegate on the AF run result columns so the user can adjust as necessary
    // The delegates operate at the row or column level so some cells need to be manually disabled
    for (int col = BFO_AF_RUN_1; col < BFO_AF_RUN_1 + maxRuns; col++)
    {
        IntegerDelegate *AFDel = new IntegerDelegate(m_BFOView, 0, 1000000, 1);
        m_BFOView->setItemDelegateForColumn(col, AFDel);

        // Disable any cells where for that filter less AF runs were requested
        for (int row = 0; row < m_BFOModel->rowCount(); row++)
        {
            int numRuns = m_BFOModel->item(row, BFO_NUM_FOCUS_RUNS)->text().toInt();
            if ((numRuns > 0) && (col > numRuns + BFO_AF_RUN_1 - 1))
            {
                //QStandardItem *currentItem = m_BFOModel->item(row, col);
                QStandardItem *currentItem = new QStandardItem();
                currentItem->setEditable(false);
                m_BFOModel->setItem(row, col, currentItem);
            }
        }
    }

    // Offset column
    int offsetCol = m_BFOModel->columnCount() - 2;
    IntegerDelegate *offsetDel = new IntegerDelegate(m_BFOView, -10000, 10000, 1);
    m_BFOView->setItemDelegateForColumn(offsetCol, offsetDel);

    // Save column
    int saveCol = offsetCol + 1;
    ToggleDelegate *saveDel = new ToggleDelegate(m_BFOView);
    m_BFOView->setItemDelegateForColumn(saveCol, saveDel);

    // Disable Offset and Save for lock filters
    QStandardItem *item = new QStandardItem();
    item->setEditable(false);
    for (int row = 0; row < m_lockFilters.count(); row++)
    {
        m_BFOModel->setItem(row, m_BFOModel->columnCount() - 2, item);
        m_BFOModel->setItem(row, m_BFOModel->columnCount() - 1, item);
    }
    // Check dependant filters where user requested zero AF runs
    for (int row = 0; row < m_dependantFilters.count(); row++)
    {
        if (m_BFOModel->item(m_lockFilters.count() + row, BFO_NUM_FOCUS_RUNS)->text().toInt() <= 0)
        {
            // Set save checkout off just in case user activated it
            QStandardItem *saveItem = new QStandardItem(QString::number(0));
            m_BFOModel->setItem(m_lockFilters.count() + row, m_BFOModel->columnCount() - 1, saveItem);
            NotEditableDelegate *newDelegate = new NotEditableDelegate(m_BFOView);
            m_BFOView->setItemDelegateForRow(m_lockFilters.count() + row, newDelegate);
        }
    }
}

void FilterManager::stopProcessing()
{
    m_stopFlag = true;
    setBuildFilterOffsetsButtons(BFO_STOP);

    if (m_qItemInProgress.changeFilter)
    {
        // Change filter in progress. Let it run to completion
        m_abortAFPending = false;
    }
    else
    {
        // AF run is currently in progress so signal an abort
        m_StatusBar->showMessage(i18n("Aborting Autofocus..."));
        m_abortAFPending = true;
        emit abortAutoFocus();
    }
}

// Callback when an item in the Model is changed
void FilterManager::itemChanged(QStandardItem *item)
{
    if (item->column() == BFO_NUM_FOCUS_RUNS)
    {
        // Don't allow lock filters to have 0 runs
        if ((item->row() < m_lockFilters.count()) && (m_BFOModel->item(item->row(), item->column())->text().toInt() == 0))
        {
            QStandardItem *updateItem = m_BFOModel->item(item->row(), item->column());
            updateItem->setText(QString::number(1));
            m_BFOModel->setItem(item->row(), item->column(), updateItem);
        }
    }
    else if ((item->column() >= BFO_AF_RUN_1) && (item->column() < m_BFOModel->columnCount() - 3))
    {
        // One of the AF runs has changed so calc the Average and Offset
        calculateAFAverage(item->row(), item->column());
        calculateOffset(item->row());
    }
    else if ((m_tableInEditMode) && (item->column() == m_BFOModel->columnCount() - 1))
    {
        // Save column changed
        if ((item->row() < m_lockFilters.count()) && (m_BFOModel->item(item->row(), item->column())->text().toInt()))
        {
            // Lock filter. Don't allow the checkbox to be set as there's no point in saving the offset
            QStandardItem *saveItem = new QStandardItem(QString::number(0));
            m_BFOModel->setItem(item->row(), item->column(), saveItem);
        }
    }
}

// This routine calculates the average of the AF runs. Given that the number of runs is likely to be low
// a simple average is used. The user may manually adjust the values.
void FilterManager::calculateAFAverage(int row, int col)
{
    int numRuns;
    if (m_tableInEditMode)
        numRuns = m_BFOModel->item(row, BFO_NUM_FOCUS_RUNS)->text().toInt();
    else
        numRuns = col - BFO_AF_RUN_1 + 1;

    // Firstly, the average of the AF runs
    int total = 0;
    int useableRuns = numRuns;
    for(int i = 0; i < numRuns; i++)
    {
        int j = m_BFOModel->item(row, BFO_AF_RUN_1 + i)->text().toInt();
        if (j > 0)
            total += j;
        else
            useableRuns--;
    }

    int average;
    (useableRuns > 0) ? average = total / useableRuns : average = 0;

    // Update the Model with the newly calculated average
    QStandardItem *averageItem = new QStandardItem(QString::number(average));
    int averageCol = m_BFOModel->columnCount() - 3;
    m_BFOModel->setItem(row, averageCol, averageItem);
}

// calculateOffset updates new offsets when AF averages have been calculated. There are 2 posibilities:
// 1. The updated row is a dependent filter so update its offset
// 2. The updated row is a lock filter in which case any dependent filters need to have their offset updated
void FilterManager::calculateOffset(int row)
{
    const int averageCol = m_BFOModel->columnCount() - 3;
    const int offsetCol = averageCol + 1;
    const int saveCol = offsetCol +  1;

    if (m_BFOModel->item(row, BFO_LOCK)->text() == NULL_FILTER)
    {
        // Loop through the dependant filters and update any associated with this lock filter
        // But only if we've already processed the dependent filters.
        if (m_tableInEditMode)
        {
            QString thisFilter = m_BFOModel->item(row, BFO_FILTER)->text();
            for (int i = 0; i < m_dependantFilters.count(); i++)
            {
                if (m_BFOModel->item(m_lockFilters.count() + i, BFO_NUM_FOCUS_RUNS)->text().toInt() > 0
                        && (m_BFOModel->item(m_lockFilters.count() + i, BFO_LOCK)->text() == thisFilter))
                    calculateOffset(m_lockFilters.count() + i);
            }
        }
        else
        {
            // Lock filter so no offset to save so turn off the save checkbox
            QStandardItem *saveItem = new QStandardItem(QString::number(0));
            saveItem->setEditable(false);
            m_BFOModel->setItem(row, saveCol, saveItem);
            // Disable the user from editing the Offset column
            QStandardItem *offsetItem = new QStandardItem();
            offsetItem->setEditable(false);
            m_BFOModel->setItem(row, saveCol - 1, offsetItem);
        }
    }
    else
    {
        // We are dealing with a dependant filter
        const int average = m_BFOModel->item(row, averageCol)->text().toInt();
        const int lockFilter = m_lockFilters.indexOf(m_BFOModel->item(row, BFO_LOCK)->text());
        const int lockFilterAverage = m_BFOModel->item(lockFilter, averageCol)->text().toInt();

        // Calculate the offset and set it in the model
        int offset = average - lockFilterAverage;
        QStandardItem *offsetItem = new QStandardItem(QString::number(offset));
        m_BFOModel->setItem(row, offsetCol, offsetItem);

        // Set the save checkbox
        QStandardItem *saveItem = new QStandardItem(QString::number(1));
        m_BFOModel->setItem(row, saveCol, saveItem);
    }
}

}

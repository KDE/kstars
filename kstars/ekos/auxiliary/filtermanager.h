/*  Ekos Filter Manager
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QDialog>
#include <QSqlDatabase>
#include <QQueue>
#include <QPointer>

#include <indi/indistd.h>
#include <indi/indifocuser.h>
#include <oal/filter.h>

#include "ekos/ekos.h"

#include "ui_filtersettings.h"

class QSqlTableModel;
class LockDelegate;
class NotEditableDelegate;
class ExposureDelegate;
class OffsetDelegate;
class UseAutoFocusDelegate;

namespace Ekos
{

class FilterManager : public QDialog, public Ui::FilterSettings
{
    Q_OBJECT
public:

    typedef enum
    {
        CHANGE_POLICY    = 1 << 0,
        LOCK_POLICY      = 1 << 1,
        OFFSET_POLICY    = 1 << 2,
        AUTOFOCUS_POLICY = 1 << 3,
        ALL_POLICIES     = CHANGE_POLICY | OFFSET_POLICY | LOCK_POLICY | AUTOFOCUS_POLICY
    } FilterPolicy;

    FilterManager();

    void refreshFilterModel();

    QStringList getFilterLabels(bool forceRefresh=false);

    int getFilterPosition(bool forceRefresh=false);

    // The target position and offset
    int getTargetFilterPosition() { return targetFilterPosition; }
    int getTargetFilterOffset() { return targetFilterOffset; }

    /**
     * @brief getFilterExposure Get optimal exposure time for the specified filter
     * @param name filter to obtain exposure time for
     * @return exposure time in seconds.
     */
    double getFilterExposure(const QString &name = QString()) const;
    bool setFilterExposure(int index, double exposure);

    /**
     * @brief getFilterLock Return filter that should be used when running autofocus for the supplied filter
     * For example, "Red" filter can be locked to use "Lum" when doing autofocus. "Green" filter can be locked to "--"
     * which means that no filter change is necessary.
     * @param name filter which we need to query its locked filter.
     * @return locked filter. "--" indicates no locked filter and whatever current filter should be used.
     *
     */
    QString getFilterLock(const QString &name) const;

    void setCurrentFilter(ISD::GDInterface *filter);    


    /**
     * @brief applyFilterFocusPolicies Check if we need to apply any filter policies for focus operations.
     */
    void applyFilterFocusPolicies();

public slots:
    // Position. if applyPolicy is true then all filter offsets and autofocus & lock policies are applied.
    bool setFilterPosition(uint8_t position, FilterPolicy policy = ALL_POLICIES);
    // Offset
    void setFocusOffsetComplete();
    // Remove Device
    void removeDevice(ISD::GDInterface *device);
    // Refresh Filters after model update
    void reloadFilters();
    // Focus Status
    void setFocusStatus(Ekos::FocusState focusState);

signals:
    // Emitted only when there is a change in the filter slot number
    void positionChanged(int);
    // Emitted when filter change operation completed successfully including any focus offsets or auto-focus operation
    void labelsChanged(QStringList);
    // Emitted when filter exposure duration changes
    void exposureChanged(double);
    // Emitted when filter change completed including all required actions    
    void ready();
    // Emitted when operation fails
    void failed();
    // Status signal
    void newStatus(FilterState state);
    // Check Focus
    void checkFocus(double);
    // New Focus offset requested
    void newFocusOffset(int16_t);

private slots:    
    void processText(ITextVectorProperty *tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processSwitch(ISwitchVectorProperty *svp);

private:

    // Filter Wheel Devices
    ISD::GDInterface *m_currentFilterDevice = { nullptr };

    // Position and Labels
    QStringList m_currentFilterLabels;
    int m_currentFilterPosition = { -1 };
    double m_currentFilterExposure = { -1 };

    // Filter Structure
    QList<OAL::Filter *> m_ActiveFilters;
    OAL::Filter *targetFilter = { nullptr };
    OAL::Filter *currentFilter = { nullptr };
    OAL::Filter *lockedFilter = { nullptr };
    bool m_useLockedFilter = { false };
    bool m_useTargetFilter = { false };

    // Autofocus retries
    uint8_t retries = { 0 };

    int16_t lastFilterOffset { 0 };

    // Table model
    QSqlTableModel *filterModel = { nullptr };

    // INDI Properties of current active filter
    ITextVectorProperty *m_FilterNameProperty { nullptr };
    INumberVectorProperty *m_FilterPositionProperty { nullptr };

    // Operation stack
    void buildOperationQueue(FilterState operation);
    bool executeOperationQueue();
    bool executeOneOperation(FilterState operation);

    // Update model
    void syncDBToINDI();

    // Operation Queue
    QQueue<FilterState> operationQueue;

    FilterState state = { FILTER_IDLE };

    int targetFilterPosition = { -1 };
    int targetFilterOffset = { - 1 };

    // Delegates
    QPointer<LockDelegate> lockDelegate;
    QPointer<NotEditableDelegate> noEditDelegate;
    QPointer<ExposureDelegate> exposureDelegate;
    QPointer<OffsetDelegate> offsetDelegate;
    QPointer<UseAutoFocusDelegate> useAutoFocusDelegate;

    // Policies
    FilterPolicy m_Policy = { ALL_POLICIES };
};

}

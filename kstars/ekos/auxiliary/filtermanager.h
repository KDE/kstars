/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_filtersettings.h"
#include "ekos/ekos.h"

#include <indi/indistd.h>
#include <indi/indifocuser.h>
#include <oal/filter.h>

#include <QDialog>
#include <QSqlDatabase>
#include <QQueue>
#include <QPointer>

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
            OFFSET_POLICY    = 1 << 1,
            AUTOFOCUS_POLICY = 1 << 2,
            ALL_POLICIES     = CHANGE_POLICY | OFFSET_POLICY | AUTOFOCUS_POLICY,
            NO_AUTOFOCUS_POLICY = CHANGE_POLICY | OFFSET_POLICY
        } FilterPolicy;

        enum
        {
            FM_LABEL = 4,
            FM_EXPOSURE,
            FM_OFFSET,
            FM_AUTO_FOCUS,
            FM_LOCK_FILTER,
            FM_FLAT_FOCUS
        };

        FilterManager();

        QJsonObject toJSON();
        void setFilterData(const QJsonObject &settings);

        void refreshFilterModel();

        QStringList getFilterLabels(bool forceRefresh = false);

        int getFilterPosition(bool forceRefresh = false);

        // The target position and offset
        int getTargetFilterPosition()
        {
            return targetFilterPosition;
        }
        int getTargetFilterOffset()
        {
            return targetFilterOffset;
        }

        bool setFilterAbsoluteFocusPosition(int index, int absFocusPos);

        // Set absolute focus position, if supported, to the current filter absolute focus position.
        bool syncAbsoluteFocusPosition(int index);

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

        /**
         * @brief setCurrentFilterWheel Set the FilterManager active filter wheel.
         * @param filter pointer to filter wheel device
         */
        void setCurrentFilterWheel(ISD::GDInterface *filter);

        /**
         * @brief setFocusReady Set whether a focuser device is active and in use.
         * @param enabled true if focus is ready, false otherwise.
         */
        void setFocusReady(bool enabled)
        {
            m_FocusReady = enabled;
        }


        /**
         * @brief applyFilterFocusPolicies Check if we need to apply any filter policies for focus operations.
         */
        void applyFilterFocusPolicies();

    public slots:
        // Position. if applyPolicy is true then all filter offsets and autofocus & lock policies are applied.
        bool setFilterPosition(uint8_t position, Ekos::FilterManager::FilterPolicy policy = ALL_POLICIES);
        // Change filter names
        bool setFilterNames(const QStringList &newLabels);
        // Offset Request completed
        void setFocusOffsetComplete();
        // Remove Device
        void removeDevice(ISD::GDInterface *device);
        // Refresh Filters after model update
        void reloadFilters();
        // Focus Status
        void setFocusStatus(Ekos::FocusState focusState);
        // Set absolute focus position
        void setFocusAbsolutePosition(int value)
        {
            m_FocusAbsPosition = value;
        }
        // Inti filter property after connection
        void initFilterProperties();

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
        void newStatus(Ekos::FilterState state);
        // Check Focus
        void checkFocus(double);
        // New Focus offset requested
        void newFocusOffset(int value, bool useAbsoluteOffset);

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
        bool m_useTargetFilter = { false };

        // Autofocus retries
        uint8_t retries = { 0 };

        int16_t lastFilterOffset { 0 };

        // Table model
        QSqlTableModel *filterModel = { nullptr };

        // INDI Properties of current active filter
        ITextVectorProperty *m_FilterNameProperty { nullptr };
        INumberVectorProperty *m_FilterPositionProperty { nullptr };
        ISwitchVectorProperty *m_FilterConfirmSet { nullptr };

        // Operation stack
        void buildOperationQueue(FilterState operation);
        bool executeOperationQueue();
        bool executeOneOperation(FilterState operation);

        // Update model
        void syncDBToINDI();

        // Operation Queue
        QQueue<FilterState> operationQueue;

        FilterState state = { FILTER_IDLE };

        int targetFilterPosition { -1 };
        int targetFilterOffset { - 1 };


        bool m_FocusReady { false };
        bool m_FocusAbsPositionPending { false};
        int m_FocusAbsPosition { -1 };

        // Delegates
        QPointer<LockDelegate> lockDelegate;
        QPointer<NotEditableDelegate> noEditDelegate;
        QPointer<ExposureDelegate> exposureDelegate;
        QPointer<OffsetDelegate> offsetDelegate;
        QPointer<UseAutoFocusDelegate> useAutoFocusDelegate;

        // Policies
        FilterPolicy m_Policy = { ALL_POLICIES };

        bool m_ConfirmationPending { false };
};

}

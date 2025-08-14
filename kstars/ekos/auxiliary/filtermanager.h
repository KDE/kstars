/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_filtersettings.h"
#include "ekos/ekos.h"
#include "ekos/focus/focusutils.h"

#include <indi/indifilterwheel.h>
#include <indi/indifocuser.h>
#include <oal/filter.h>

#include <QQueue>
#include <QPointer>
#include <QStandardItemModel>

class QSqlTableModel;
class ComboDelegate;
class NotEditableDelegate;
class NotEditableDelegate2dp;
class DoubleDelegate;
class IntegerDelegate;
class ToggleDelegate;
class DatetimeDelegate;

namespace Ekos
{

class FilterManager : public QDialog, public Ui::FilterSettings
{
        Q_OBJECT

        // BuildFilterOffsets is a friend class so it can access methods in FilterManager
        friend class BuildFilterOffsets;

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
            FM_LAST_AF_SOLUTION,
            FM_LAST_AF_TEMP,
            FM_LAST_AF_ALT,
            FM_LAST_AF_DATETIME,
            FM_TICKS_PER_TEMP,
            FM_TICKS_PER_ALT,
            FM_WAVELENGTH
        };

        FilterManager(QWidget *parent = nullptr);

        QJsonObject toJSON();
        void setFilterData(const QJsonObject &settings);

        void createFilterModel();
        void refreshFilterModel();

        QStringList getFilterLabels(bool forceRefresh = false);
        int getFilterPosition(bool forceRefresh = false);
        /**
         * @brief refreshFilterLabels Update the filter labels from the device and signal changes.
         */
        void refreshFilterLabels();
        /**
         * @brief refreshFilterPos Update the filter wheel position and signal changes.
         */
        void refreshFilterPosition();

        // The target position and offset
        int getTargetFilterPosition()
        {
            return targetFilterPosition;
        }
        int getTargetFilterOffset()
        {
            return targetFilterOffset;
        }

        /**
         * @brief setFilterAbsoluteFocusDetails set params from successful autofocus run
         * @param index filter index
         * @param focusPos the position of the focus solution
         * @param focusTemp the temperature at the time of the focus run
         * @param focusAlt the altitude at the time of the focus run
         * @return whether function worked or not
         */
        bool setFilterAbsoluteFocusDetails(int index, int focusPos, double focusTemp, double focusAlt);

        /**
         * @brief getFilterAbsoluteFocusDetails get params from the last successful autofocus run
         * @param name filter name
         * @param focusPos the position of the focus solution
         * @param focusTemp the temperature at the time of the focus run
         * @param focusAlt the altitude at the time of the focus run
         * @return whether function worked or not
         */
        bool getFilterAbsoluteFocusDetails(const QString &name, int &focusPos, double &focusTemp, double &focusAlt) const;

        /**
         * @brief getAFDatetime get timestamp of the last successful autofocus run
         * @param name filter name
         * @param datetime
         * @return whether function worked or not
         */
        bool getAFDatetime(const QString &name, QDateTime &datetime) const;

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
         * @brief getFilterOffset returns the offset for the specified filter
         * @param name of the filter
         * @return filter offset (INVALID_VALUE in the case of a problem)
         */
        int getFilterOffset(const QString &name) const;

        /**
         * @brief setFilterOffset set the offset for the specified filter
         * @param color of the filter
         * @param the new filter offset
         * @return whether or not the operation was successful
         */
        bool setFilterOffset(QString color, int offset);

        /**
         * @brief getFilterWavelength Get mid-point wavelength for the specified filter
         * @param name filter to obtain exposure time for
         * @return wavelength in nm.
         */
        int getFilterWavelength(const QString &name = QString()) const;

        /**
         * @brief getFilterTicksPerTemp gets the ticks per degree C
         * @param name filter to obtain exposure time for
         * @return ticks / degree C
         */
        double getFilterTicksPerTemp(const QString &name = QString()) const;

        /**
         * @brief getFilterTicksPerAlt gets the ticks per degree of altitude
         * @param name filter to obtain exposure time for
         * @return ticks / degree Alt
         */
        double getFilterTicksPerAlt(const QString &name = QString()) const;

        /**
         * @brief getFilterLock Return filter that should be used when running autofocus for the supplied filter
         * For example, "Red" filter can be locked to use "Lum" when doing autofocus. "Green" filter can be locked to "--"
         * which means that no filter change is necessary.
         * @param name filter which we need to query its locked filter.
         * @return locked filter. "--" indicates no locked filter and whatever current filter should be used.
         *
         */
        QString getFilterLock(const QString &name) const;
        bool setFilterLock(int index, QString name);

        /**
         * @brief setCurrentFilterWheel Set the FilterManager active filter wheel.
         * @param filter pointer to filter wheel device
         */
        void setFilterWheel(ISD::FilterWheel *filter);
        ISD::FilterWheel *filterWheel() const
        {
            return m_FilterWheel;
        }

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

        /**
         * @brief buildFilterOffsets Launch the Build Filter Offsets utility
         * @param FM pointer to the FilterManager
         */
        void buildFilterOffsets();

    public slots:
        // Position. if applyPolicy is true then all filter offsets and autofocus & lock policies are applied.
        bool setFilterPosition(uint8_t position, Ekos::FilterManager::FilterPolicy policy = ALL_POLICIES);
        // Change filter names
        bool setFilterNames(const QStringList &newLabels);
        // Offset Request completed
        void setFocusOffsetComplete();
        // Remove Device
        void removeDevice(const QSharedPointer<ISD::GenericDevice> &device);
        // Refresh Filters after model update
        void reloadFilters();
        // Resize the dialog to the contents
        void resizeDialog();
        // Focus Status
        void setFocusStatus(Ekos::FocusState focusState);
        // Set absolute focus position
        void setFocusAbsolutePosition(int value)
        {
            m_FocusAbsPosition = value;
        }
        // Inti filter property after connection
        void refreshFilterProperties();
        // Signal from BuildFilterOffsets to run Autofocus. Pass onto Focus
        void signalRunAutoFocus(AutofocusReason autofocusReason, const QString &reasonInfo);
        // Signal from BuildFilterOffsets to abort AF run. Pass onto Focus
        void signalAbortAutoFocus();
        // Signal from Focus that Autofocus has completed - used by BuildFilterOffsets utility
        void autoFocusComplete(FocusState completionState, int currentPosition, double currentTemperature, double currentAlt);

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
        // Run AutoFocus
        void runAutoFocus(AutofocusReason autofocusReason, const QString &reasonInfo);
        // Abort AutoFocus
        void abortAutoFocus();
        // New Focus offset requested
        void newFocusOffset(int value, bool useAbsoluteOffset);
        // database was updated
        void updated();
        // Filter ticks per degree of temperature changed
        void ticksPerTempChanged();
        // Filter ticks per degree of altitude changed
        void ticksPerAltChanged();
        // Filter wavelength changed
        void wavelengthChanged();
        // Pass on Autofocus completed signal to Build Filter Offsets
        void autoFocusDone(FocusState completionState, int currentPosition, double currentTemperature, double currentAlt);

    private slots:
        void updateProperty(INDI::Property prop);
        void processDisconnect();

    private:

        // Filter Wheel Devices
        ISD::FilterWheel *m_FilterWheel = { nullptr };

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
        QSqlTableModel *m_FilterModel = { nullptr };

        // INDI Properties of current active filter
        ITextVectorProperty *m_FilterNameProperty { nullptr };
        INumberVectorProperty *m_FilterPositionProperty { nullptr };
        ISwitchVectorProperty *m_FilterConfirmSet { nullptr };

        // Accessor function to return filter pointer for the passed in name.
        // nullptr is returned if there isn't a match
        OAL::Filter * getFilterByName(const QString &name) const;

        // Operation stack
        void buildOperationQueue(FilterState operation);
        bool executeOperationQueue();
        bool executeOneOperation(FilterState operation);

        // Check Filter Change timeout
        void checkFilterChangeTimeout();

        // Update model
        void syncDBToINDI();

        // Get the list of possible lock filters to set in the combo box.
        // The list excludes filters already setup with a lock to prevent nested dependencies
        QStringList getLockDelegates();

        // Operation Queue
        QQueue<FilterState> operationQueue;

        FilterState state = { FILTER_IDLE };

        int targetFilterPosition { -1 };
        int targetFilterOffset { - 1 };
        QTimer m_FilterChangeTimeout;

        bool m_FocusReady { false };
        bool m_FocusAbsPositionPending { false};
        int m_FocusAbsPosition { -1 };

        // Delegates
        QPointer<ComboDelegate> lockDelegate;
        QPointer<NotEditableDelegate> noEditDelegate;
        QPointer<DoubleDelegate> exposureDelegate;
        QPointer<IntegerDelegate> offsetDelegate;
        QPointer<ToggleDelegate> useAutoFocusDelegate;
        QPointer<IntegerDelegate> lastAFSolutionDelegate;
        QPointer<DoubleDelegate> lastAFTempDelegate;
        QPointer<DoubleDelegate> lastAFAltDelegate;
        QPointer<DatetimeDelegate> lastAFDTDelegate;
        QPointer<DoubleDelegate> ticksPerTempDelegate;
        QPointer<DoubleDelegate> ticksPerAltDelegate;
        QPointer<IntegerDelegate> wavelengthDelegate;

        // Policies
        FilterPolicy m_Policy = { ALL_POLICIES };

        bool m_ConfirmationPending { false };

        // Keep track of Autofocus failures
        void setAutofocusStatus(Ekos::FocusState focusState);
        QMap<QString, bool> m_LastAFFailed;
};

}

/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_buildfilteroffsets.h"
#include "filtermanager.h"
#include "ekos/focus/focusutils.h"

namespace Ekos
{

class BuildFilterOffsets : public QDialog, public Ui::buildOffsetsDialog
{
        Q_OBJECT

    public:

        // AFSolutionDetail is used to store data used to calculate adaptive focus position info
        typedef struct
        {
            int position;
            int adaptedPosition;
            double temperature;
            double deltaTemp;
            double altitude;
            double deltaAlt;
            double ticksPerTemp;
            double ticksPerAlt;
            double deltaTicksTemperature;
            double deltaTicksAltitude;
            double deltaTicksTotal;
        } AFSolutionDetail;

        BuildFilterOffsets(FilterManager *filterManager);
        ~BuildFilterOffsets();

        // ----- GUI -----
        // Show and execute the dialog - reinitializes model/UI each time
        void showDialog();

        // ----- Programmatic API -----
        // Initialize for a new build-filter-offsets session. Clears previous state.
        void initialize();
        // Select which filters to process. If empty or not called, all filters are used.
        void setFilterSelection(const QStringList &filters);
        // Set number of AF runs for a specific filter by name
        void setNumAFRuns(const QString &filter, int runs);
        // Set number of AF runs for all selected filters
        void setNumAFRuns(int runs);
        // Set the reference filter (defaults to first selected filter)
        void setRefFilter(const QString &filter);
        // Start the build-filter-offsets workflow. Emits processingComplete() when done.
        void startProcessing();
        // Stop the in-flight workflow
        void stopProcessing();
        // Persist the calculated offsets to FilterManager / database
        void saveOffsets();

        // ----- Results -----
        // Returns all AF solution details
        QVector<AFSolutionDetail> getAFSolutions() const;
        // Returns map of filter name → calculated offset
        QMap<QString, int> getCalculatedOffsets() const;
        // Returns the list of selected filters
        QStringList getSelectedFilters() const;

        // Used by build filter offsets utility to process the completion of an AF run.
        void autoFocusComplete(FocusState completionState, int currentPosition, double currentTemperature, double currentAlt);

    Q_SIGNALS:
        // Trigger Autofocus
        void runAutoFocus(AutofocusReason autofocusReason, const QString &reasonInfo);
        // User has elected to abort Autofocus, pass on signal to FilterManager
        void abortAutoFocus();
        // New Focus offset requested
        void newFocusOffset(int value, bool useAbsoluteOffset);
        // Emitted when filter change completed including all required actions
        void ready();
        // Emitted when all processing is complete (queue empty, AF runs done)
        void processingComplete();

    private Q_SLOTS:
        void itemChanged(QStandardItem *item);
        void refChanged(QModelIndex index);

    private:

        // BFOColID references the columns in the table
        typedef enum
        {
            BFO_FILTER = 0,
            BFO_OFFSET,
            BFO_LOCK,
            BFO_NUM_FOCUS_RUNS,
            BFO_AF_RUN_1,
            BFO_AVERAGE,
            BFO_NEW_OFFSET,
            BFO_SAVE_CHECK
        } BFOColID;

        // BFOButtonState controls the states used when using the dialog
        typedef enum
        {
            BFO_INIT,
            BFO_RUN,
            BFO_SAVE,
            BFO_STOP
        } BFOButtonState;

        // buildOffsetsQItem is used to queue items for processing
        typedef struct
        {
            QString color;
            bool changeFilter;
            int numAFRun;
        } buildOffsetsQItem;

        // Function to setup signal/slots in and out of Build Filter Offsets
        void setupConnections();
        // Setup the dialog GUI (per-show)
        void setupGUI();
        // Setup the dialog GUI (one-time connections)
        void setupGUIOnce();
        // Function to initialise resources for the build filter offsets dialog
        void initBuildFilterOffsets();
        // Setup the table widget
        void setupBuildFilterOffsetsTable();
        // Set the buttons state
        void setBuildFilterOffsetsButtons(const BFOButtonState state);
        // Function to setup the work required to build the offsets
        void buildTheOffsets();
        // Function to persist the calculated filter offsets (internal, called from UI button)
        void saveTheOffsets();
        // When all automated processing is  complete allow some cells to be editable
        void setCellsEditable();
        // Function to call Autofocus to build the filter offsets
        void runBuildOffsets();
        // Function to process a filter change event
        void buildTheOffsetsTaskComplete();
        // Resize the dialog
        void buildOffsetsDialogResize();
        // Calculate the average of the AF runs
        void calculateAFAverage(const int row, const int col);
        // Calculate the new offset for the filter
        void calculateOffset(const int row);
        // Process the passed in Q item
        void processQItem(const buildOffsetsQItem qitem);
        // Process successful Autofocus data
        void processAFcomplete(const int position, const double temperature, const double altitude);
        // Load the AF position into the table.
        void loadPosition(const bool checked, const int row, const int col);
        // Reload all AF positions processed so far depending on setting of adaptFocus checkbox
        void reloadPositions(const bool checked);
        // Return the column for the passed in ID
        int getColumn(const BFOColID id) const;
        // Get the number of AF runs for the passed in row
        int getNumRuns(const int row);
        // Get the maximum number of AF runs
        int getMaxRuns();

        QStandardItemModel m_BFOModel;

        QVector < QString > m_filters;
        QStringList m_selectedFilters;  // Which filters the user wants to process
        int m_refFilter { -1 };
        double m_refTemperature { INVALID_VALUE };
        double m_refAltitude { INVALID_VALUE };

        QQueue < buildOffsetsQItem > m_buildOffsetsQ;
        buildOffsetsQItem m_qItemInProgress;

        FilterManager *m_filterManager {nullptr};

        bool m_inBuildOffsets { false };
        int m_rowIdx { 0 };
        int m_colIdx { 0 };
        QPushButton *m_runButton;
        QPushButton *m_stopButton;
        bool m_problemFlag { false };
        bool m_stopFlag { false };
        bool m_abortAFPending { false };
        bool m_tableInEditMode {false};
        QVector < AFSolutionDetail > m_AFSolutions;
};

}
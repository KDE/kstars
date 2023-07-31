/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_buildfilteroffsets.h"
#include "filtermanager.h"

namespace Ekos
{

class BuildFilterOffsets : public QDialog, public Ui::buildOffsetsDialog
{
        Q_OBJECT

    public:

        BuildFilterOffsets(QSharedPointer<FilterManager> filterManager);
        ~BuildFilterOffsets();

        // Used by build filter offsets utility to process the completion of an AF run.
        void autoFocusComplete(FocusState completionState, int currentPosition, double currentTemperature, double currentAlt);

    signals:
        // Trigger Autofocus
        void runAutoFocus(bool buildOffsets);
        // User has elected to abort Autofocus, pass on signal to FilterManager
        void abortAutoFocus();
        // New Focus offset requested
        void newFocusOffset(int value, bool useAbsoluteOffset);
        // Emitted when filter change completed including all required actions
        void ready();

    private slots:
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

        // Function to setup signal/slots in and out of Build Filter Offsets
        void setupConnections();
        // Setup the dialog GUI
        void setupGUI();
        // Function to initialise resources for the build filter offsers dialog
        void initBuildFilterOffsets();
        // Setup the table widget
        void setupBuildFilterOffsetsTable();
        // Set the buttons state
        void setBuildFilterOffsetsButtons(const BFOButtonState state);
        // Function to setup the work required to build the offsets
        void buildTheOffsets();
        // Function to stop in-flight processing, e.g AF runs
        void stopProcessing();
        // Function to persist the calculated filter offsets
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
        int getColumn(const BFOColID id);
        // Get the number of AF runs for the passed in row
        int getNumRuns(const int row);
        // Get the maximum number of AF runs
        int getMaxRuns();

        QStandardItemModel m_BFOModel;

        QVector <QString> m_filters;
        int m_refFilter { -1 };
        double m_refTemperature { INVALID_VALUE };
        double m_refAltitude { INVALID_VALUE };

        QQueue<buildOffsetsQItem> m_buildOffsetsQ;
        buildOffsetsQItem m_qItemInProgress;

        QSharedPointer<FilterManager> m_filterManager;

        bool m_inBuildOffsets { false };
        int m_rowIdx { 0 };
        int m_colIdx { 0 };
        QPushButton *m_runButton;
        QPushButton *m_stopButton;
        bool m_problemFlag { false };
        bool m_stopFlag { false };
        bool m_abortAFPending { false };
        bool m_tableInEditMode {false};
        QVector<AFSolutionDetail> m_AFSolutions;
};

}

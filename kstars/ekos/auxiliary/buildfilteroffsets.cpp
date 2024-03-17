/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "buildfilteroffsets.h"
#include <kstars_debug.h>
#include "kstars.h"
#include "ekos/auxiliary/tabledelegate.h"
#include "Options.h"

namespace Ekos
{

// buildFilterOffsets (BFO) sets up a dialog to manage automatic calculation of filter offsets
// by performing autofocus runs on user selected filters and working out the relative offsets.
// The idea is to use the utility once in a while to setup the offsets which them enables the user to avoid having
// to do an autofocus run when swapping filters. The benefit of this might be to avoid focusing on a difficult-to-
// focus filter but focusing on a lock filter and then using the offset; or just to avoid an Autofocus run when
// changing filter.
//
// The utility follows the standard Ekos cpp/h/ui file setup. The basic dialog is contained in the .ui file but
// because the table is constructed from data at runtime some of the GUI is constructed in code in the .cpp.
// BFO uses the MVC design pattern. The BuildFilterOffset object is a friend of FilterManager because BFO is closely
// related to FilterManager. This allows several FilterManager methods to be accessed by BFO. In addition, signals
// with Focus are passed via FilterManager.
//
// On launch, BFO displays a grid containing all the filters in the Filter Settings. Relevant information is copied across
// to BFO and the filters appear in the same order. The 1st filter is suffiixed with a "*", marking it as the reference
// filter which is the one against which all other relevant offsets are measured.
//
// To change the reference filter, the user double clicks another filter.
//
// The user needs to set the number of Autofocus (AF) runs to perform on each filter. The default is 5, the maximum is 10.
// Setting this field to 0, removes the associated filter from further processing.
//
// The user presses Run and the utility moves to the processing stage. Extra columns are displayed, one for each AF run,
// as well as average, new offset and save columns. For each filter the number of requested AF runs is performed. The
// table cell associated with the in-flight AF run is highlighted. As an AF run completes the results are displayed in
// the table. When all AF runs for a filter are complete, processing moves to the next filter.
//
// Normally, if a lock filter is configured for a particular filter, then when focusing, the lock filter is swapped in
// AF runs, and then the original filter is swapped back into position. For BFO this model is inappropriate. When an
// AF run is requested on a filter then AF is always run on that filter so the lock filter policy is not honoured when
// running AF from BFO.
//
// The user can interrupt processing by pressing stop. A confirm dialog then allows the user to stop BFO or resume.
//
// BFO can take a while to complete. For example if 5 AF runs are requested on 8 filters and AF takes, say 2 mins per
// run, then BFO will take over an hour. During this time environmental conditions such as temperature and altitude
// could change the focus point. For this reaons, it it possible Adapt each focus run back to the temp and alt applicable
// during the first AF run for more accurate calculation of the offsets. If Adapt Focus is checked, then the adapted
// vales are used in calculations; if not checked then the raw (unadapted) values are used. The toggle can be used at
// any time. The tooltip on each AF run provides a tabular explanation of the adaptations and how the raw value is
// changed to the adapted value. In order to use Adapt Focus, the ticks per temperature and ticks per altitude fields
// in the Fillter Settings popup need to be filled in appropriately for each filter being processed.
//
// If AF fails then the user is prompted to retry or abort the processing.
//
// When processing completes the user can review the results. Each processed filter's new offset has an associated save
// checkbox allowing all or some values to be saved. Saving persists the new offset values in the Filter Settings popup
// for future use during imaging.
//
// The average AF value for a filter is a simple mean. There are typically not enough sample points taken for robust
// statistical processing to add any value. So the user needs to review the AF values and decide if they want to remove
// any outliers (set the AF value to 0 to exclude from processing, or adjust the number). In addition, it is possible
// to override the offset with a manually entered value.
//
BuildFilterOffsets::BuildFilterOffsets(QSharedPointer<FilterManager> filterManager)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    if (filterManager.isNull())
        return;

    m_filterManager = filterManager;

    setupUi(this);
    setupConnections();
    initBuildFilterOffsets();
    setupBuildFilterOffsetsTable();
    setupGUI();

    // Launch the dialog - synchronous call
    this->exec();
}

BuildFilterOffsets::~BuildFilterOffsets()
{
}

void BuildFilterOffsets::setupConnections()
{
    // Connections to FilterManager
    connect(this, &BuildFilterOffsets::runAutoFocus, m_filterManager.get(), &FilterManager::signalRunAutoFocus);
    connect(this, &BuildFilterOffsets::abortAutoFocus, m_filterManager.get(), &FilterManager::signalAbortAutoFocus);

    // Connections from FilterManager
    connect(m_filterManager.get(), &FilterManager::autoFocusDone, this, &BuildFilterOffsets::autoFocusComplete);
    connect(m_filterManager.get(), &FilterManager::ready, this, &BuildFilterOffsets::buildTheOffsetsTaskComplete);

    // Connections internal to BuildFilterOffsets
    connect(this, &BuildFilterOffsets::ready, this, &BuildFilterOffsets::buildTheOffsetsTaskComplete);
}

void BuildFilterOffsets::setupGUI()
{
    // Add action buttons to the button box
    m_runButton = buildOffsetsButtonBox->addButton("Run", QDialogButtonBox::ActionRole);
    m_stopButton = buildOffsetsButtonBox->addButton("Stop", QDialogButtonBox::ActionRole);

    // Set tooltips for the buttons
    m_runButton->setToolTip("Run Build Filter Offsets utility");
    m_stopButton->setToolTip("Interrupt processing when utility is running");
    buildOffsetsButtonBox->button(QDialogButtonBox::Save)->setToolTip("Save New Offsets");

    // Set the buttons' state
    setBuildFilterOffsetsButtons(BFO_INIT);

    // Connect up button callbacks
    connect(m_runButton, &QPushButton::clicked, this, &BuildFilterOffsets::buildTheOffsets);
    connect(m_stopButton, &QPushButton::clicked, this, &BuildFilterOffsets::stopProcessing);
    connect(buildOffsetsButtonBox->button(QDialogButtonBox::Save), &QPushButton::clicked, this,
            &BuildFilterOffsets::saveTheOffsets);
    connect(buildOffsetsButtonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, [this]()
    {
        // Close the dialog down unless lots of processing has been done. In that case put up an "are you sure" popup
        if (!m_tableInEditMode)
            this->done(QDialog::Rejected);
        else if (KMessageBox::questionYesNo(KStars::Instance(),
                                            i18n("Are you sure you want to quit?")) == KMessageBox::Yes)
            this->done(QDialog::Rejected);
    });

    // Setup the Adapt Focus checkbox
    buildOffsetsAdaptFocus->setChecked(Options::adaptFocusBFO());
    connect(buildOffsetsAdaptFocus, &QCheckBox::toggled, this, [&](bool checked)
    {
        Options::setAdaptFocusBFO(checked);
        reloadPositions(checked);
    });

    // Connect cell changed callback
    connect(&m_BFOModel, &QStandardItemModel::itemChanged, this, &BuildFilterOffsets::itemChanged);

    // Connect double click callback
    connect(buildOffsetsTableView, &QAbstractItemView::doubleClicked, this, &BuildFilterOffsets::refChanged);

    // Display an initial message in the status bar
    buildOffsetsStatusBar->showMessage(i18n("Idle"));

    // Resize the dialog based on the data
    buildOffsetsDialogResize();
}

void BuildFilterOffsets::initBuildFilterOffsets()
{
    m_inBuildOffsets = false;
    m_stopFlag = m_problemFlag = m_abortAFPending = m_tableInEditMode = false;
    m_filters.clear();
    m_refFilter = -1;
    m_rowIdx = m_colIdx = 0;

    // Drain any old queue items
    m_buildOffsetsQ.clear();
}

void BuildFilterOffsets::setupBuildFilterOffsetsTable()
{
    // Setup MVC
    buildOffsetsTableView->setModel(&m_BFOModel);

    // Setup the table view
    QStringList Headers { i18n("Filter"), i18n("Offset"), i18n("Lock Filter"), i18n("# Focus Runs") };
    m_BFOModel.setColumnCount(Headers.count());
    m_BFOModel.setHorizontalHeaderLabels(Headers);

    // Setup tooltips on column headers
    m_BFOModel.setHeaderData(getColumn(BFO_FILTER), Qt::Horizontal,
                             i18n("Filter. * indicates reference filter. Double click to change"),
                             Qt::ToolTipRole);
    m_BFOModel.setHeaderData(getColumn(BFO_NUM_FOCUS_RUNS), Qt::Horizontal, i18n("# Focus Runs. Set per filter. 0 to ignore"),
                             Qt::ToolTipRole);

    // Setup edit delegates for each column
    // No Edit delegates for Filter, Offset and Lock Filter
    NotEditableDelegate *noEditDel = new NotEditableDelegate(buildOffsetsTableView);
    buildOffsetsTableView->setItemDelegateForColumn(getColumn(BFO_FILTER), noEditDel);
    buildOffsetsTableView->setItemDelegateForColumn(getColumn(BFO_OFFSET), noEditDel);
    buildOffsetsTableView->setItemDelegateForColumn(getColumn(BFO_LOCK), noEditDel);

    // # Focus Runs delegate
    IntegerDelegate *numRunsDel = new IntegerDelegate(buildOffsetsTableView, 0, 10, 1);
    buildOffsetsTableView->setItemDelegateForColumn(getColumn(BFO_NUM_FOCUS_RUNS), numRunsDel);

    // Setup the table
    m_BFOModel.setRowCount(m_filterManager->m_ActiveFilters.count());

    // Load the data for each filter
    for (int row = 0 ; row < m_filterManager->m_ActiveFilters.count(); row++)
    {
        // Filter name
        m_filters.push_back(m_filterManager->m_ActiveFilters[row]->color());
        QString str;
        if (row == 0)
        {
            str = QString("%1 *").arg(m_filters[row]);
            m_refFilter = 0;
        }
        else
            str = m_filters[row];

        QStandardItem* item0 = new QStandardItem(str);
        m_BFOModel.setItem(row, getColumn(BFO_FILTER), item0);

        // Offset
        QStandardItem* item1 = new QStandardItem(QString::number(m_filterManager->m_ActiveFilters[row]->offset()));
        m_BFOModel.setItem(row, getColumn(BFO_OFFSET), item1);

        // Lock filter
        QStandardItem* item2 = new QStandardItem(m_filterManager->m_ActiveFilters[row]->lockedFilter());
        m_BFOModel.setItem(row, getColumn(BFO_LOCK), item2);

        // Number of AF runs to perform
        QStandardItem* item3 = new QStandardItem(QString::number(5));
        m_BFOModel.setItem(row, getColumn(BFO_NUM_FOCUS_RUNS), item3);
    }
}

void BuildFilterOffsets::setBuildFilterOffsetsButtons(const BFOButtonState state)
{
    switch (state)
    {
        case BFO_INIT:
            m_runButton->setEnabled(true);
            m_stopButton->setEnabled(false);
            buildOffsetsButtonBox->button(QDialogButtonBox::Save)->setEnabled(false);
            buildOffsetsButtonBox->button(QDialogButtonBox::Close)->setEnabled(true);
            break;

        case BFO_RUN:
            m_runButton->setEnabled(false);
            m_stopButton->setEnabled(true);
            buildOffsetsButtonBox->button(QDialogButtonBox::Save)->setEnabled(false);
            buildOffsetsButtonBox->button(QDialogButtonBox::Close)->setEnabled(false);
            break;

        case BFO_SAVE:
            m_runButton->setEnabled(false);
            m_stopButton->setEnabled(false);
            buildOffsetsButtonBox->button(QDialogButtonBox::Save)->setEnabled(true);
            buildOffsetsButtonBox->button(QDialogButtonBox::Close)->setEnabled(true);
            break;

        case BFO_STOP:
            m_runButton->setEnabled(false);
            m_stopButton->setEnabled(false);
            buildOffsetsButtonBox->button(QDialogButtonBox::Save)->setEnabled(false);
            buildOffsetsButtonBox->button(QDialogButtonBox::Close)->setEnabled(false);
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
void BuildFilterOffsets::buildTheOffsets()
{
    buildOffsetsQItem qItem;

    // Set the buttons
    setBuildFilterOffsetsButtons(BFO_RUN);

    // Make the Number of runs column not editable
    // No Edit delegates for Filter, Offset and Lock Filter
    QPointer<NotEditableDelegate> noEditDel = new NotEditableDelegate(buildOffsetsTableView);
    buildOffsetsTableView->setItemDelegateForColumn(getColumn(BFO_NUM_FOCUS_RUNS), noEditDel);

    // Loop through the work to do and load up Queue and extend tableWidget to record AF answers
    int maxAFRuns = 0;
    int startRow = -1;
    for (int row = 0; row < m_filters.count(); row++ )
    {
        const int numRuns = m_BFOModel.item(row, getColumn(BFO_NUM_FOCUS_RUNS))->text().toInt();
        if (numRuns > 0)
        {
            if (startRow < 0)
                startRow = row;

            // Set the filter change
            qItem.color = m_filters[row];
            qItem.changeFilter = true;
            m_buildOffsetsQ.enqueue(qItem);

            // Load up the AF runs based on how many the user requested
            qItem.changeFilter = false;
            maxAFRuns = std::max(maxAFRuns, numRuns);
            for (int runNum = 1; runNum <= numRuns; runNum++)
            {
                qItem.numAFRun = runNum;
                m_buildOffsetsQ.enqueue(qItem);
            }
        }
    }

    // Add columns to the Model for AF runs and set the headers. Each AF run result is editable
    // but the calculated average is not.
    int origCols = m_BFOModel.columnCount();
    m_BFOModel.setColumnCount(origCols + maxAFRuns + 3);
    for (int col = 0; col < maxAFRuns; col++)
    {
        QStandardItem *newItem = new QStandardItem(i18n("AF Run %1", col + 1));
        m_BFOModel.setHorizontalHeaderItem(origCols + col, newItem);
        m_BFOModel.setHeaderData(origCols + col, Qt::Horizontal,
                                 i18n("AF Run %1. Calculated automatically but can be edited. Set to 0 to exclude from average.", col + 1),
                                 Qt::ToolTipRole);
        buildOffsetsTableView->setItemDelegateForColumn(origCols + col, noEditDel);
    }

    // Add 3 more columns for the average of the AF runs, the offset and whether to save the offset
    QStandardItem *averageItem = new QStandardItem(i18n("Average"));
    m_BFOModel.setHorizontalHeaderItem(origCols + maxAFRuns, averageItem);
    m_BFOModel.setHeaderData(origCols + maxAFRuns, Qt::Horizontal, i18n("AF Average (mean)."), Qt::ToolTipRole);
    buildOffsetsTableView->setItemDelegateForColumn(origCols + maxAFRuns, noEditDel);

    QStandardItem *offsetItem = new QStandardItem(i18n("New Offset"));
    m_BFOModel.setHorizontalHeaderItem(origCols + maxAFRuns + 1, offsetItem);
    m_BFOModel.setHeaderData(origCols + maxAFRuns + 1, Qt::Horizontal,
                             i18n("New Offset. Calculated relative to Filter with *. Can be edited."), Qt::ToolTipRole);
    buildOffsetsTableView->setItemDelegateForColumn(origCols + maxAFRuns + 1, noEditDel);

    QPointer<ToggleDelegate> saveDelegate = new ToggleDelegate(&m_BFOModel);
    QStandardItem *saveItem = new QStandardItem(i18n("Save"));
    m_BFOModel.setHorizontalHeaderItem(origCols + maxAFRuns + 2, saveItem);
    m_BFOModel.setHeaderData(origCols + maxAFRuns + 2, Qt::Horizontal,
                             i18n("Save. Check to save the New Offset for the associated Filter."), Qt::ToolTipRole);
    buildOffsetsTableView->setItemDelegateForColumn(origCols + maxAFRuns + 2, saveDelegate);

    // Resize the dialog
    buildOffsetsDialogResize();

    // Set the selected cell to the first AF run of the ref filter
    m_rowIdx = startRow;
    m_colIdx = getColumn(BFO_AF_RUN_1);
    QModelIndex index = buildOffsetsTableView->model()->index(m_rowIdx, m_colIdx);
    buildOffsetsTableView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);

    // Initialise the progress bar
    buildOffsetsProgressBar->reset();
    buildOffsetsProgressBar->setRange(0, m_buildOffsetsQ.count());
    buildOffsetsProgressBar->setValue(0);

    // The Q has been loaded with all required actions so lets start processing
    m_inBuildOffsets = true;
    runBuildOffsets();
}

// This is signalled when an asynchronous task has been completed, either
// a filter change or an autofocus run
void BuildFilterOffsets::buildTheOffsetsTaskComplete()
{
    if (m_stopFlag)
    {
        // User hit the stop button, so see what they want to do
        if (KMessageBox::warningContinueCancel(KStars::Instance(),
                                               i18n("Are you sure you want to stop Build Filter Offsets?"), i18n("Stop Build Filter Offsets"),
                                               KStandardGuiItem::stop(), KStandardGuiItem::cancel(), "") == KMessageBox::Cancel)
        {
            // User wants to retry processing
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
                buildOffsetsProgressBar->setValue(buildOffsetsProgressBar->value() + 1);
                runBuildOffsets();
            }
        }
        else
        {
            // User wants to abort
            m_stopFlag = m_abortAFPending = false;
            this->done(QDialog::Rejected);
        }
    }
    else if (m_problemFlag)
    {
        // The in flight task had a problem so see what the user wants to do
        if (KMessageBox::warningContinueCancel(KStars::Instance(),
                                               i18n("An unexpected problem occurred.\nStop Build Filter Offsets, or Cancel to retry?"),
                                               i18n("Build Filter Offsets Unexpected Problem"),
                                               KStandardGuiItem::stop(), KStandardGuiItem::cancel(), "") == KMessageBox::Cancel)
        {
            // User wants to retry
            m_problemFlag = false;
            processQItem(m_qItemInProgress);
        }
        else
        {
            // User wants to abort
            m_problemFlag = false;
            this->done(QDialog::Rejected);
        }
    }
    else
    {
        // All good so update the progress bar and process the next task
        buildOffsetsProgressBar->setValue(buildOffsetsProgressBar->value() + 1);
        runBuildOffsets();
    }
}

// Resize the dialog to the data
void BuildFilterOffsets::buildOffsetsDialogResize()
{
    // Resize the columns to the data
    buildOffsetsTableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    // Resize the dialog to the width and height of the table widget
    const int width = buildOffsetsTableView->horizontalHeader()->length() + 40;
    const int height = buildOffsetsTableView->verticalHeader()->length() + buildOffsetsButtonBox->height() +
                       buildOffsetsProgressBar->height() + buildOffsetsStatusBar->height() + 60;
    this->resize(width, height);
}

void BuildFilterOffsets::runBuildOffsets()
{
    if (m_buildOffsetsQ.isEmpty())
    {
        // All tasks have been actioned so allow the user to edit the results, save them or quit.
        setCellsEditable();
        setBuildFilterOffsetsButtons(BFO_SAVE);
        m_tableInEditMode = true;
        buildOffsetsStatusBar->showMessage(i18n("Processing complete."));
    }
    else
    {
        // Take the next item off the queue
        m_qItemInProgress = m_buildOffsetsQ.dequeue();
        processQItem(m_qItemInProgress);
    }
}

void BuildFilterOffsets::processQItem(const buildOffsetsQItem currentItem)
{
    if (currentItem.changeFilter)
    {
        // Need to change filter
        buildOffsetsStatusBar->showMessage(i18n("Changing filter to %1...", currentItem.color));

        auto pos = m_filterManager->m_currentFilterLabels.indexOf(currentItem.color) + 1;
        if (!m_filterManager->setFilterPosition(pos, m_filterManager->CHANGE_POLICY))
        {
            // Filter wheel position change failed.
            buildOffsetsStatusBar->showMessage(i18n("Problem changing filter to %1...", currentItem.color));
            m_problemFlag = true;
        }
    }
    else
    {
        // Signal an AF run with an arg of "build offsets"
        const int run = m_colIdx - getColumn(BFO_AF_RUN_1) + 1;
        const int numRuns = m_BFOModel.item(m_rowIdx, getColumn(BFO_NUM_FOCUS_RUNS))->text().toInt();
        buildOffsetsStatusBar->showMessage(i18n("Running Autofocus on %1 (%2/%3)...", currentItem.color, run, numRuns));
        emit runAutoFocus(AutofocusReason::FOCUS_FILTER_OFFSETS, "");
    }
}

// This is called at the end of an AF run
void BuildFilterOffsets::autoFocusComplete(FocusState completionState, int position, double temperature, double altitude)
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
        // AF run was successful so load the solution results
        processAFcomplete(position, temperature, altitude);

        // Load the result into the table. The Model update will trigger further updates
        loadPosition(buildOffsetsAdaptFocus->isChecked(), m_rowIdx, m_colIdx);

        // Now see what's next, another AF run on this filter or are we moving to the next filter
        if (m_colIdx - getColumn(BFO_NUM_FOCUS_RUNS) < getNumRuns(m_rowIdx))
            m_colIdx++;
        else
        {
            // Move the active cell to the next AF run in the table
            // Usually this will be the next row, but if this row has zero AF runs skip to the next
            for (int nextRow = m_rowIdx + 1; nextRow < m_filters.count(); nextRow++)
            {
                if (getNumRuns(nextRow) > 0)
                {
                    // Found the next filter to process
                    m_rowIdx = nextRow;
                    m_colIdx = getColumn(BFO_AF_RUN_1);
                    break;
                }
            }
        }
        // Highlight the next cell...
        const QModelIndex index = buildOffsetsTableView->model()->index(m_rowIdx, m_colIdx);
        buildOffsetsTableView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    }
    // Signal the next processing step
    emit ready();
}

// Called to store the AF position details. The raw AF position is passed in (from Focus)
// The Adapted position (based on temperature and altitude) is calculated
// The sense of the adaptation is to take the current position and adapt it to the
// conditions (temp and alt) of the reference.
void BuildFilterOffsets::processAFcomplete(const int position, const double temperature, const double altitude)
{
    AFSolutionDetail solution;

    solution.position = position;
    solution.temperature = temperature;
    solution.altitude = altitude;
    solution.ticksPerTemp = m_filterManager->getFilterTicksPerTemp(m_filters[m_rowIdx]);
    solution.ticksPerAlt = m_filterManager->getFilterTicksPerAlt(m_filters[m_rowIdx]);

    if (m_rowIdx == 0 && m_colIdx == getColumn(BFO_AF_RUN_1))
    {
        // Set the reference temp and alt to be the first AF run
        m_refTemperature = temperature;
        m_refAltitude = altitude;
    }

    // Calculate the temperature adaptation
    if (temperature == INVALID_VALUE || m_refTemperature == INVALID_VALUE)
    {
        solution.deltaTemp = 0.0;
        solution.deltaTicksTemperature = 0.0;
    }
    else
    {
        solution.deltaTemp = m_refTemperature - temperature;
        solution.deltaTicksTemperature = solution.ticksPerTemp * solution.deltaTemp;
    }

    // Calculate the altitude adaptation
    if (altitude == INVALID_VALUE || m_refAltitude == INVALID_VALUE)
    {
        solution.deltaAlt = 0.0;
        solution.deltaTicksAltitude = 0.0;
    }
    else
    {
        solution.deltaAlt = m_refAltitude - altitude;
        solution.deltaTicksAltitude = solution.ticksPerAlt * solution.deltaAlt;
    }

    // Calculate the total adaptation
    solution.deltaTicksTotal = static_cast<int>(round(solution.deltaTicksTemperature + solution.deltaTicksAltitude));

    // Calculate the Adapted position
    solution.adaptedPosition = position + solution.deltaTicksTotal;

    m_AFSolutions.push_back(solution);
}

// Load the focus position depending on the setting of the adaptPos checkbox. The Model update will trigger further updates
void BuildFilterOffsets::loadPosition(const bool checked, const int row, const int col)
{
    int idx = 0;
    // Work out the array index for m_AFSolutions
    for (int i = 0; i < row; i++)
        idx += getNumRuns(i);

    idx += col - getColumn(BFO_AF_RUN_1);

    // Check that the passed in row, col has been processed. If not, nothing to do
    if (idx < m_AFSolutions.count())
    {
        // Get the AF position to use based on the setting of 'checked'
        int pos = (checked) ? m_AFSolutions[idx].adaptedPosition : m_AFSolutions[idx].position;

        // Present a tooltip explanation of how the original position is changed by adaptation, e.g.
        //                    Adapt Focus Explainer
        //               Position  Temperature (°C)   Altitude (°Alt)
        // Measured Pos: 36704     T: 0.9°C (ΔT=0.7)  Alt: 40.1° (ΔAlt=-4.2)
        // Adaptations:      4     T: 7.10 ticks      Alt: -3.20 ticks
        // Adapted Pos:  36708
        //
        const QString temp = QString("%1").arg(m_AFSolutions[idx].temperature, 0, 'f', 1);
        const QString deltaTemp = i18n("(ΔT=%1)", QString("%1").arg(m_AFSolutions[idx].deltaTemp, 0, 'f', 1));
        const QString ticksTemp = i18n("(%1 ticks)", QString("%1").arg(m_AFSolutions[idx].deltaTicksTemperature, 0, 'f', 1));
        const QString alt = QString("%1").arg(m_AFSolutions[idx].altitude, 0, 'f', 1);
        const QString deltaAlt = i18n("(ΔAlt=%1)", QString("%1").arg(m_AFSolutions[idx].deltaAlt, 0, 'f', 1));
        const QString ticksAlt = i18n("(%1 ticks)", QString("%1").arg(m_AFSolutions[idx].deltaTicksAltitude, 0, 'f', 1));

        QStandardItem *posItem = new QStandardItem(QString::number(pos));
        const QString toolTip =
            i18nc("Graphics tooltip; colume 1 is a header, column 2 is focus position, column 3 is temperature in °C, colunm 4 is altitude in °Alt"
                  "Row 1 is the headers, row 2 is the measured position, row 3 are the adaptations for temperature and altitude, row 4 is adapted position",
                  "<head><style>"
                  "  th, td, caption {white-space: nowrap; padding-left: 5px; padding-right: 5px;}"
                  "  th { text-align: left;}"
                  "  td { text-align: right;}"
                  "  caption { text-align: center; vertical-align: top; font-weight: bold; margin: 0px; padding-bottom: 5px;}"
                  "</head></style>"
                  "<body><table>"
                  "<caption align=top>Adapt Focus Explainer</caption>"
                  "<tr><th></th><th>Position</th><th>Temperature (°C)</th><th>Altitude (°Alt)</th></tr>"
                  "<tr><th>Measured Pos</th><td>%1</td><td>%2 %3</td><td>%4 %5</td></tr>"
                  "<tr><th>Adaptations</th><td>%6</td><td>%7</td><td>%8</td></tr>"
                  "<tr><th>Adapted Pos</th><td>%9</td></tr>"
                  "</table></body>",
                  m_AFSolutions[idx].position, temp, deltaTemp, alt, deltaAlt,
                  m_AFSolutions[idx].deltaTicksTotal, ticksTemp, ticksAlt,
                  m_AFSolutions[idx].adaptedPosition);

        posItem->setToolTip(toolTip);
        m_BFOModel.setItem(row, col, posItem);
    }
}

// Reload the focus position grid depending on the setting of the adaptPos checkbox.
void BuildFilterOffsets::reloadPositions(const bool checked)
{
    for (int row = 0; row <= m_rowIdx; row++)
    {
        const int numRuns = getNumRuns(row);
        const int maxCol = (row < m_rowIdx) ? numRuns : m_colIdx - getColumn(BFO_AF_RUN_1) + 1;
        for (int col = 0; col < maxCol; col++)
            loadPosition(checked, row, col + getColumn(BFO_AF_RUN_1));
    }
}

// Called when the user wants to persist the calculated offsets
void BuildFilterOffsets::saveTheOffsets()
{
    for (int row = 0; row < m_filters.count(); row++)
    {
        // Check there's an item set for the current row before accessing
        if (m_BFOModel.item(row, getColumn(BFO_SAVE_CHECK))->text().toInt())
        {
            // Save item is set so persist the offset
            const int offset = m_BFOModel.item(row, getColumn(BFO_NEW_OFFSET))->text().toInt();
            if (!m_filterManager->setFilterOffset(m_filters[row], offset))
                qCDebug(KSTARS) << "Unable to save calculated offset for filter " << m_filters[row];
        }
    }
    // All done so close the dialog
    this->done(QDialog::Accepted);
}

// Processing done so make certain cells editable for the user
void BuildFilterOffsets::setCellsEditable()
{
    // Enable an edit delegate on the AF run result columns so the user can adjust as necessary
    // The delegates operate at the row or column level so some cells need to be manually disabled
    for (int col = getColumn(BFO_AF_RUN_1); col < getColumn(BFO_AF_RUN_1) + getMaxRuns(); col++)
    {
        IntegerDelegate *AFDel = new IntegerDelegate(buildOffsetsTableView, 0, 1000000, 1);
        buildOffsetsTableView->setItemDelegateForColumn(col, AFDel);

        // Disable any cells where for that filter less AF runs were requested
        for (int row = 0; row < m_BFOModel.rowCount(); row++)
        {
            const int numRuns = getNumRuns(row);
            if ((numRuns > 0) && (col > numRuns + getColumn(BFO_AF_RUN_1) - 1))
            {
                QStandardItem *currentItem = new QStandardItem();
                currentItem->setEditable(false);
                m_BFOModel.setItem(row, col, currentItem);
            }
        }
    }

    // Offset column
    IntegerDelegate *offsetDel = new IntegerDelegate(buildOffsetsTableView, -10000, 10000, 1);
    buildOffsetsTableView->setItemDelegateForColumn(getColumn(BFO_NEW_OFFSET), offsetDel);

    // Save column
    ToggleDelegate *saveDel = new ToggleDelegate(buildOffsetsTableView);
    buildOffsetsTableView->setItemDelegateForColumn(getColumn(BFO_SAVE_CHECK), saveDel);

    // Check filters where user requested zero AF runs
    for (int row = 0; row < m_filters.count(); row++)
    {
        if (getNumRuns(row) <= 0)
        {
            // Uncheck save just in case user activated it
            QStandardItem *saveItem = new QStandardItem("");
            m_BFOModel.setItem(row, getColumn(BFO_SAVE_CHECK), saveItem);
            NotEditableDelegate *newDelegate = new NotEditableDelegate(buildOffsetsTableView);
            buildOffsetsTableView->setItemDelegateForRow(row, newDelegate);
        }
    }
}

void BuildFilterOffsets::stopProcessing()
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
        buildOffsetsStatusBar->showMessage(i18n("Aborting Autofocus..."));
        m_abortAFPending = true;
        emit abortAutoFocus();
    }
}

// Callback when an item in the Model is changed
void BuildFilterOffsets::itemChanged(QStandardItem *item)
{
    if (item->column() == getColumn(BFO_NUM_FOCUS_RUNS))
    {
        if (item->row() == m_refFilter && m_BFOModel.item(item->row(), item->column())->text().toInt() == 0)
            // If user is trying to set the num AF runs of the ref filter to 0, set to 1
            m_BFOModel.setItem(item->row(), item->column(), new QStandardItem(QString::number(1)));
    }
    else if ((item->column() >= getColumn(BFO_AF_RUN_1)) && (item->column() < getColumn(BFO_AVERAGE)))
    {
        // One of the AF runs has changed so recalc the Average and Offset
        calculateAFAverage(item->row(), item->column());
        calculateOffset(item->row());
    }
}

// Callback when a row in the Model is changed
void BuildFilterOffsets::refChanged(QModelIndex index)
{
    if (m_inBuildOffsets)
        return;

    const int row = index.row();
    const int col = index.column();
    if (col == 0 && row >= 0 && row < m_filters.count() && row != m_refFilter)
    {
        // User double clicked the filter column in a different cell to the current ref filter
        QStandardItem* itemSelect = new QStandardItem(QString("%1 *").arg(m_filters[row]));
        m_BFOModel.setItem(row, getColumn(BFO_FILTER), itemSelect);

        // The ref filter needs to have at least 1 AF run so that there is an average
        // solution to base the offsets of other filters against... so force this condition
        if (getNumRuns(row) == 0)
            m_BFOModel.setItem(row, getColumn(BFO_NUM_FOCUS_RUNS), new QStandardItem(QString::number(1)));

        // Reset the previous selection
        QStandardItem* itemDeselect = new QStandardItem(m_filters[m_refFilter]);
        m_BFOModel.setItem(m_refFilter, getColumn(BFO_FILTER), itemDeselect);

        m_refFilter = row;
        // Resize the cols and dialog
        buildOffsetsDialogResize();
    }
}

// This routine calculates the average of the AF runs. Given that the number of runs is likely to be low
// a simple average is used. The user may manually adjust the values.
void BuildFilterOffsets::calculateAFAverage(const int row, const int col)
{
    int numRuns;
    if (m_tableInEditMode)
        numRuns = getNumRuns(row);
    else
        numRuns = col - getColumn(BFO_AF_RUN_1) + 1;

    // Firstly, the average of the AF runs
    double total = 0;
    int useableRuns = numRuns;
    for(int i = 0; i < numRuns; i++)
    {
        int j = m_BFOModel.item(row, getColumn(BFO_AF_RUN_1) + i)->text().toInt();
        if (j > 0)
            total += j;
        else
            useableRuns--;
    }

    const int average = (useableRuns > 0) ? static_cast<int>(round(total / useableRuns)) : 0;

    // Update the Model with the newly calculated average
    QStandardItem *averageItem = new QStandardItem(QString::number(average));
    m_BFOModel.setItem(row, getColumn(BFO_AVERAGE), averageItem);
}

// calculateOffset updates new offsets when AF averages have been calculated. There are 2 posibilities:
// 1. The updated row is the reference filter in the list so update the offset of other filters
// 2. The updated row is another filter in which case just update its offset
void BuildFilterOffsets::calculateOffset(const int row)
{
    if (row == m_refFilter)
    {
        // The first filter has been changed so loop through the other filters and adjust the offsets
        if (m_tableInEditMode)
        {
            for (int i = 0; i < m_filters.count(); i++)
            {
                if (i != m_refFilter && getNumRuns(i) > 0)
                    calculateOffset(i);
            }
        }
        else
        {
            // If there are some filters higher in the table than ref filter then these can only be
            // proessed now that the ref filter has been processed.
            for (int i = 0; i < m_refFilter; i++)
            {
                if (getNumRuns(i) > 0)
                    calculateOffset(i);
            }
        }
    }

    // If we haven't processed the ref filter yet then skip over
    if (m_rowIdx >= m_refFilter)
    {
        // The ref filter has been processed so we can calculate the offset from it
        const int average = m_BFOModel.item(row, getColumn(BFO_AVERAGE))->text().toInt();
        const int refFilterAverage = m_BFOModel.item(m_refFilter, getColumn(BFO_AVERAGE))->text().toInt();

        // Calculate the offset and set it in the model
        const int offset = average - refFilterAverage;
        QStandardItem *offsetItem = new QStandardItem(QString::number(offset));
        m_BFOModel.setItem(row, getColumn(BFO_NEW_OFFSET), offsetItem);

        // Set the save checkbox
        QStandardItem *saveItem = new QStandardItem(QString::number(1));
        m_BFOModel.setItem(row, getColumn(BFO_SAVE_CHECK), saveItem);
    }
}

// Returns the column in the table for the passed in id. The structure is:
// Col 0 -- BFO_FILTER         -- Filter name
// Col 1 -- BFO_OFFSET         -- Current offset value
// Col 2 -- BFO_LOCK           -- Lock filter name
// Col 3 -- BFO_NUM_FOCUS_RUNS -- Number of AF runs
// Col 4 -- BFO_AF_RUN_1       -- 1st AF run
// Col x -- BFO_AVERAGE        -- Average AF run. User selects the number of AF runs at run time
// Col y -- BFO_NEW_OFFSET     -- New offset.
// Col z -- BFO_SAVE_CHECK     -- Save checkbox
int BuildFilterOffsets::getColumn(const BFOColID id)
{
    switch (id)
    {
        case BFO_FILTER:
        case BFO_OFFSET:
        case BFO_LOCK:
        case BFO_NUM_FOCUS_RUNS:
        case BFO_AF_RUN_1:
            break;

        case BFO_AVERAGE:
            return m_BFOModel.columnCount() - 3;
            break;

        case BFO_NEW_OFFSET:
            return m_BFOModel.columnCount() - 2;
            break;

        case BFO_SAVE_CHECK:
            return m_BFOModel.columnCount() - 1;
            break;

        default:
            break;
    }
    return id;
}

// Get the number of AF runs for the passed in row
int BuildFilterOffsets::getNumRuns(const int row)
{
    return m_BFOModel.item(row, getColumn(BFO_NUM_FOCUS_RUNS))->text().toInt();
}

// Get the maximum number of AF runs
int BuildFilterOffsets::getMaxRuns()
{
    return getColumn(BFO_AVERAGE) - getColumn(BFO_AF_RUN_1);
}

}

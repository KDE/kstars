/*
    SPDX-FileCopyrightText: 2024 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusadvisor.h"
#include "focus.h"
#include "focusalgorithms.h"
#include "Options.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/auxiliary/stellarsolverprofile.h"

#include <QScrollBar>

namespace Ekos
{

const char * FOCUSER_SIMULATOR = "Focuser Simulator";
const int MAXIMUM_FOCUS_ADVISOR_ITERATIONS = 1001;
const int NUM_JUMPS_PER_SECTOR = 10;
const int FIND_STARS_MIN_STARS = 2;
const double TARGET_MAXMIN_HFR_RATIO = 3.0;
const double INITIAL_MAXMIN_HFR_RATIO = 2.0;
const int NUM_STEPS_PRE_AF = 11;

FocusAdvisor::FocusAdvisor(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_MACOS
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setupUi(this);
    m_focus = static_cast<Focus *>(parent);

    processUI();
    setupHelpTable();
}

FocusAdvisor::~FocusAdvisor()
{
}

void FocusAdvisor::processUI()
{
    // Setup the help dialog
    m_helpDialog = new QDialog(this);
    m_helpUI.reset(new Ui::focusAdvisorHelpDialog());
    m_helpUI->setupUi(m_helpDialog);
#ifdef Q_OS_MACOS
    m_helpDialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    m_runButton = focusAdvButtonBox->addButton("Run", QDialogButtonBox::ActionRole);
    m_stopButton = focusAdvButtonBox->addButton("Stop", QDialogButtonBox::ActionRole);
    m_helpButton = focusAdvButtonBox->addButton("Help", QDialogButtonBox::HelpRole);

    // Set tooltips for the buttons
    m_runButton->setToolTip("Run Focus Advisor");
    m_stopButton->setToolTip("Stop Focus Advisor");
    m_helpButton->setToolTip("List parameter settings");

    // Connect up button callbacks
    connect(m_runButton, &QPushButton::clicked, this, &FocusAdvisor::start);
    connect(m_stopButton, &QPushButton::clicked, this, &FocusAdvisor::stop);
    connect(m_helpButton, &QPushButton::clicked, this, &FocusAdvisor::help);

    // Initialise buttons
    setButtons(false);

    // Setup the results table
    setupResultsTable();
}

void FocusAdvisor::setupResultsTable()
{
    focusAdvTable->setColumnCount(RESULTS_MAX_COLS);
    focusAdvTable->setRowCount(0);

    QTableWidgetItem *itemSection = new QTableWidgetItem(i18n ("Section"));
    itemSection->setToolTip(i18n("Section"));
    focusAdvTable->setHorizontalHeaderItem(RESULTS_SECTION, itemSection);

    QTableWidgetItem *itemRunNumber = new QTableWidgetItem(i18n ("Run"));
    itemRunNumber->setToolTip(i18n("Run number"));
    focusAdvTable->setHorizontalHeaderItem(RESULTS_RUN_NUMBER, itemRunNumber);

    QTableWidgetItem *itemStartPosition = new QTableWidgetItem(i18n ("Start Pos"));
    itemStartPosition->setToolTip(i18n("Start position"));
    focusAdvTable->setHorizontalHeaderItem(RESULTS_START_POSITION, itemStartPosition);

    QTableWidgetItem *itemStepSize = new QTableWidgetItem(i18n ("Step/Jump Size"));
    itemStepSize->setToolTip(i18n("Step Size"));
    focusAdvTable->setHorizontalHeaderItem(RESULTS_STEP_SIZE, itemStepSize);

    QTableWidgetItem *itemAFOverscan = new QTableWidgetItem(i18n ("Overscan"));
    itemAFOverscan->setToolTip(i18n("AF Overscan"));
    focusAdvTable->setHorizontalHeaderItem(RESULTS_AFOVERSCAN, itemAFOverscan);

    QTableWidgetItem *itemText = new QTableWidgetItem(i18n ("Comments"));
    itemText->setToolTip(i18n("Additional Text"));
    focusAdvTable->setHorizontalHeaderItem(RESULTS_TEXT, itemText);

    focusAdvTable->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    focusAdvTable->hide();
    resizeDialog();
}


void FocusAdvisor::setupHelpTable()
{
    QTableWidgetItem *itemParameter = new QTableWidgetItem(i18n ("Parameter"));
    itemParameter->setToolTip(i18n("Parameter Name"));
    m_helpUI->table->setHorizontalHeaderItem(HELP_PARAMETER, itemParameter);

    QTableWidgetItem *itemCurrentValue = new QTableWidgetItem(i18n ("Current Value"));
    itemCurrentValue->setToolTip(i18n("Current value of the parameter"));
    m_helpUI->table->setHorizontalHeaderItem(HELP_CURRENT_VALUE, itemCurrentValue);

    QTableWidgetItem *itemProposedValue = new QTableWidgetItem(i18n ("Proposed Value"));
    itemProposedValue->setToolTip(i18n("Focus Advisor proposed value for the parameter"));
    m_helpUI->table->setHorizontalHeaderItem(HELP_NEW_VALUE, itemProposedValue);

    connect(m_helpUI->focusAdvHelpOnlyChanges, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), this, [this]()
    {
        setupParams("");
    });
}

void FocusAdvisor::setButtons(const bool running)
{
    bool canRun = m_focus->m_Focuser && m_focus->m_Focuser->isConnected() && m_focus->m_Focuser->canAbsMove();
    m_runButton->setEnabled(!running && canRun);
    m_stopButton->setEnabled(running);
    m_helpButton->setEnabled(!running);
    focusAdvButtonBox->button(QDialogButtonBox::Close)->setEnabled(!running);
}

bool FocusAdvisor::canFocusAdvisorRun()
{
    // Focus Advisor can only be run if the following...
    return m_focus->m_Focuser && m_focus->m_Focuser->isConnected() && m_focus->m_Focuser->canAbsMove() &&
           m_focus->m_FocusAlgorithm == Focus::FOCUS_LINEAR1PASS &&
           (m_focus->m_StarMeasure == Focus::FOCUS_STAR_HFR || m_focus->m_StarMeasure == Focus::FOCUS_STAR_HFR_ADJ
            || m_focus->m_StarMeasure == Focus::FOCUS_STAR_FWHM) &&
           (m_focus->m_CurveFit == CurveFitting::FOCUS_HYPERBOLA || m_focus->m_CurveFit == CurveFitting::FOCUS_PARABOLA);
}

bool FocusAdvisor::start()
{
    // Reset the results table
    focusAdvTable->setRowCount(0);
    focusAdvTable->sizePolicy();

    if (!m_focus)
        return false;

    if (m_focus->inFocusLoop || m_focus->inAdjustFocus || m_focus->inAutoFocus || m_focus->inBuildOffsets
            || m_focus->inAFOptimise ||
            m_focus->inScanStartPos || inFocusAdvisor())
    {
        m_focus->appendLogText(i18n("Focus Advisor: another focus action in progress. Please try again."));
        return false;
    }

    if (focusAdvUpdateParams->isChecked())
    {
        updateParams();
        focusAdvUpdateParamsLabel->setText(i18n("Done"));
        emit newStage(UpdatingParameters);
    }

    m_inFindStars = focusAdvFindStars->isChecked();
    m_inPreAFAdj = focusAdvCoarseAdj->isChecked();
    m_inAFAdj = focusAdvFineAdj->isChecked();
    if (m_inFindStars || m_inPreAFAdj || m_inAFAdj)
    {
        if (canFocusAdvisorRun())
        {
            // Deselect ScanStartPos to stop interference with Focus Advisor
            m_initialScanStartPos = m_focus->m_OpsFocusProcess->focusScanStartPos->isChecked();
            m_focus->m_OpsFocusProcess->focusScanStartPos->setChecked(false);
            m_focus->runAutoFocus(FOCUS_FOCUS_ADVISOR, "");
        }
        else
        {
            m_focus->appendLogText(i18n("Focus Advisor cannot run with current params."));
            return false;
        }
    }

    return true;
}

void FocusAdvisor::stop()
{
    abort(i18n("Focus Advisor stopped"));
    emit newStage(Idle);
}

// Focus Advisor help popup
void FocusAdvisor::help()
{
    setupParams("");
    m_helpDialog->show();
    m_helpDialog->raise();
}

void FocusAdvisor::addSectionToHelpTable(int &row, const QString &section)
{
    if (++row >= m_helpUI->table->rowCount())
        m_helpUI->table->setRowCount(row + 1);
    QTableWidgetItem *item = new QTableWidgetItem();
    item->setText(section);
    QFont font = item->font();
    font.setUnderline(true);
    font.setPointSize(font.pointSize() + 2);
    item->setFont(font);
    m_helpUI->table->setItem(row, HELP_PARAMETER, item);
}

void FocusAdvisor::addParamToHelpTable(int &row, const QString &parameter, const QString &currentValue,
                                       const QString &newValue)
{
    if (m_helpUI->focusAdvHelpOnlyChanges->isChecked() && newValue == currentValue)
        return;

    if (++row >= m_helpUI->table->rowCount())
        m_helpUI->table->setRowCount(row + 1);
    QTableWidgetItem *itemParameter = new QTableWidgetItem(parameter);
    m_helpUI->table->setItem(row, HELP_PARAMETER, itemParameter);
    QTableWidgetItem *itemCurrentValue = new QTableWidgetItem(currentValue);
    m_helpUI->table->setItem(row, HELP_CURRENT_VALUE, itemCurrentValue);
    QTableWidgetItem *itemNewValue = new QTableWidgetItem(newValue);
    if (newValue != currentValue)
    {
        // Highlight changes
        QFont font = itemNewValue->font();
        font.setBold(true);
        itemNewValue->setFont(font);
    }
    m_helpUI->table->setItem(row, HELP_NEW_VALUE, itemNewValue);
}

// Resize the help dialog to the data
void FocusAdvisor::resizeHelpDialog()
{
    // Resize the columns to the data
    m_helpUI->table->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    // Resize the dialog to the width of the table widget + decoration
    int left, right, top, bottom;
    m_helpUI->verticalLayout->layout()->getContentsMargins(&left, &top, &right, &bottom);

    const int width = m_helpUI->table->horizontalHeader()->length() +
                      m_helpUI->table->verticalHeader()->width() +
                      m_helpUI->table->verticalScrollBar()->width() +
                      m_helpUI->table->frameWidth() * 2 +
                      left + right +
                      m_helpDialog->contentsMargins().left() + m_helpDialog->contentsMargins().right() + 8;

    m_helpDialog->resize(width, m_helpDialog->height());
}

// Action the focus params recommendations
void FocusAdvisor::updateParams()
{
    m_focus->setAllSettings(m_map);
    addResultsTable(i18n("Update Parameters"), -1, -1, -1, -1, "Done");
}

// Load up the Focus Advisor recommendations
void FocusAdvisor::setupParams(const QString &OTName)
{
    // See if there is another OT that can be used to default parameters
    m_map = getOTDefaults(OTName);
    bool noDefaults = m_map.isEmpty();

    if (!m_map.isEmpty())
        // If we have found parameters from another OT that fit then we're done
        return;

    bool longFL = m_focus->m_FocalLength > 1500;
    double imageScale = m_focus->getStarUnits(Focus::FOCUS_STAR_HFR, Focus::FOCUS_UNITS_ARCSEC);
    bool centralObstruction = scopeHasObstruction(m_focus->m_ScopeType);

    // Set the label based on the optical train
    m_helpUI->helpLabel->setText(QString("Recommendations: %1 FL=%2 ImageScale=%3")
                                 .arg(m_focus->m_ScopeType).arg(m_focus->m_FocalLength).arg(imageScale, 0, 'f', 2));

    bool ok;
    // Reset the table
    m_helpUI->table->setRowCount(0);

    // Camera options
    int row = -1;
    addSectionToHelpTable(row, i18n("Camera & Filter Wheel Parameters"));

    // Exposure
    double exposure = longFL ? 4.0 : 2.0;
    processParam(exposure, row, m_map, m_focus->focusExposure, "Exposure");

    // Binning
    QString binning;
    if (m_focus->focusBinning->isEnabled())
    {
        // Only try and update binning if camera supports it (binning field enabled)
        QString binTarget = (imageScale < 1.0) ? "2x2" : "1x1";

        for (int i = 0; i < m_focus->focusBinning->count(); i++)
        {
            if (m_focus->focusBinning->itemText(i) == binTarget)
            {
                binning = binTarget;
                break;
            }
        }
    }
    processParam(binning, row, m_map, m_focus->focusBinning, "Binning");

    // Gain - don't know a generic way to default to Unity gain so use current setting
    processParam(m_focus->focusGain->value(), row, m_map, m_focus->focusGain, "Gain");

    // ISO - default to current value
    processParam(m_focus->focusISO->currentText(), row, m_map, m_focus->focusISO, "ISO");

    // Filter
    processParam(m_focus->focusFilter->currentText(), row, m_map, m_focus->focusFilter, "Filter");

    // Settings
    addSectionToHelpTable(row, i18n("Settings Parameters"));

    // Auto Select Star
    processParam(false, row, m_map, m_focus->m_OpsFocusSettings->focusAutoStarEnabled, "Auto Select Star");

    // Suspend Guiding - leave alone
    processParam(m_focus->m_OpsFocusSettings->focusSuspendGuiding->isChecked(), row, m_map,
                 m_focus->m_OpsFocusSettings->focusSuspendGuiding, "Auto Select Star");

    // Use Dark Frame
    processParam(false, row, m_map, m_focus->m_OpsFocusSettings->useFocusDarkFrame, "Dark Frame");

    // Full Field & Subframe
    processParam(true, row, m_map, m_focus->m_OpsFocusSettings->focusUseFullField, "Full Field");
    processParam(false, row, m_map, m_focus->m_OpsFocusSettings->focusSubFrame, "Sub Frame");

    // Subframe box
    processParam(32, row, m_map, m_focus->m_OpsFocusSettings->focusBoxSize, "Box");

    // Display Units - leave alone
    processParam(m_focus->m_OpsFocusSettings->focusUnits->currentText(), row, m_map, m_focus->m_OpsFocusSettings->focusUnits,
                 "Display Units");

    // Guide Settle - leave alone
    processParam(m_focus->m_OpsFocusSettings->focusGuideSettleTime->value(), row, m_map,
                 m_focus->m_OpsFocusSettings->focusGuideSettleTime, "Guide Settle");

    // AF Optimize - switch off
    processParam(0, row, m_map,
                 m_focus->m_OpsFocusSettings->focusAFOptimize, "AF Optimize");

    // Mask
    QString maskCurrent, maskNew;
    if (m_focus->m_OpsFocusSettings->focusNoMaskRB->isChecked())
        maskCurrent = "Use all stars";
    else if (m_focus->m_OpsFocusSettings->focusRingMaskRB->isChecked())
    {
        double inner = m_focus->m_OpsFocusSettings->focusFullFieldInnerRadius->value();
        double outer = m_focus->m_OpsFocusSettings->focusFullFieldOuterRadius->value();
        maskCurrent = QString("Ring Mask %1%-%2%").arg(inner, 0, 'f', 1).arg(outer, 0, 'f', 1);
    }
    else
    {
        int width = m_focus->m_OpsFocusSettings->focusMosaicTileWidth->value();
        int spacer = m_focus->m_OpsFocusSettings->focusMosaicSpace->value();
        maskCurrent = QString("Mosaic Mask %1% (%2 px)").arg(width).arg(spacer);
    }

    if (noDefaults)
    {
        // Set a Ring Mask 0% - 80%
        m_map.insert("focusNoMaskRB", false);
        m_map.insert("focusRingMaskRB", true);
        m_map.insert("focusMosaicMaskRB", false);
        m_map.insert("focusFullFieldInnerRadius", 0.0);
        m_map.insert("focusFullFieldOuterRadius", 80.0);
        maskNew = QString("Ring Mask %1%-%2%").arg(0.0, 0, 'f', 1).arg(80.0, 0, 'f', 1);
    }
    else
    {
        bool noMask = m_map.value("focusNoMaskRB", false).toBool();
        bool ringMask = m_map.value("focusRingMaskRB", false).toBool();
        bool mosaicMask = m_map.value("focusMosaicMaskRB", false).toBool();
        if (noMask)
            maskNew = "Use all stars";
        else if (ringMask)
        {
            double inner = m_map.value("focusFullFieldInnerRadius", 0.0).toDouble(&ok);
            if (!ok || inner < 0.0 || inner > 100.0)
                inner = 0.0;
            double outer = m_map.value("focusFullFieldOuterRadius", 80.0).toDouble(&ok);
            if (!ok || outer < 0.0 || outer > 100.0)
                outer = 80.0;
            maskNew = QString("Ring Mask %1%-%2%").arg(inner, 0, 'f', 1).arg(outer, 0, 'f', 1);
        }
        else if (mosaicMask)
        {
            int width = m_map.value("focusMosaicTileWidth", 0.0).toInt(&ok);
            if (!ok || width < 0 || width > 100)
                width = 0.0;
            int spacer = m_map.value("focusMosaicSpace", 0.0).toInt(&ok);
            if (!ok || spacer < 0 || spacer > 100)
                spacer = 0.0;
            maskNew = QString("Mosaic Mask %1% (%2 px)").arg(width).arg(spacer);
        }
    }
    addParamToHelpTable(row, i18n("Mask"), maskCurrent, maskNew);

    // Adaptive Focus
    processParam(false, row, m_map, m_focus->m_OpsFocusSettings->focusAdaptive, "Adaptive Focus");

    // Min Move - leave default
    processParam(m_focus->m_OpsFocusSettings->focusAdaptiveMinMove->value(), row, m_map,
                 m_focus->m_OpsFocusSettings->focusAdaptiveMinMove, "Min Move");

    // Adapt Start Pos
    processParam(false, row, m_map, m_focus->m_OpsFocusSettings->focusAdaptStart, "Adapt Start Pos");

    // Max Movement - leave default
    processParam(m_focus->m_OpsFocusSettings->focusAdaptiveMaxMove->value(), row, m_map,
                 m_focus->m_OpsFocusSettings->focusAdaptiveMaxMove, "Max Total Move");

    // Process
    addSectionToHelpTable(row, i18n("Process Parameters"));

    // Detection method
    processParam(QString("SEP"), row, m_map, m_focus->m_OpsFocusProcess->focusDetection, "Detection");

    // SEP profile
    QString profile = centralObstruction ? FOCUS_DEFAULT_DONUT_NAME : FOCUS_DEFAULT_NAME;
    processParam(profile, row, m_map, m_focus->m_OpsFocusProcess->focusSEPProfile, "SEP Profile");

    // Algorithm
    processParam(QString("Linear 1 Pass"), row, m_map, m_focus->m_OpsFocusProcess->focusAlgorithm, "Algorithm");

    // Curve Fit
    processParam(QString("Hyperbola"), row, m_map, m_focus->m_OpsFocusProcess->focusCurveFit, "Curve Fit");

    // Measure
    processParam(QString("HFR"), row, m_map, m_focus->m_OpsFocusProcess->focusStarMeasure, "Measure");

    // Use Weights
    processParam(true, row, m_map, m_focus->m_OpsFocusProcess->focusUseWeights, "Use Weights");

    // R2 limit
    processParam(0.8, row, m_map, m_focus->m_OpsFocusProcess->focusR2Limit, "R² Limit");

    // Refine Curve Fit
    processParam(true, row, m_map, m_focus->m_OpsFocusProcess->focusRefineCurveFit, "Refine Curve Fit");

    // Frames Count
    processParam(1, row, m_map, m_focus->m_OpsFocusProcess->focusFramesCount, "Average Over");

    // HFR Frames Count
    processParam(1, row, m_map, m_focus->m_OpsFocusProcess->focusHFRFramesCount, "Average HFR Check");

    // Donut buster
    processParam(centralObstruction, row, m_map, m_focus->m_OpsFocusProcess->focusDonut, "Donut Buster");
    processParam(1.0, row, m_map, m_focus->m_OpsFocusProcess->focusTimeDilation, "Time Dilation x");
    processParam(0.2, row, m_map, m_focus->m_OpsFocusProcess->focusOutlierRejection, "Outlier Rejection");

    // Scan for Start Position
    processParam(true, row, m_map, m_focus->m_OpsFocusProcess->focusScanStartPos, "Scan for Start Position");
    processParam(false, row, m_map, m_focus->m_OpsFocusProcess->focusScanAlwaysOn, "Always On");
    processParam(5, row, m_map, m_focus->m_OpsFocusProcess->focusScanDatapoints, "Num Datapoints");
    processParam(1.0, row, m_map, m_focus->m_OpsFocusProcess->focusScanStepSizeFactor, "Initial Step Sixe x");

    // Mechanics
    addSectionToHelpTable(row, i18n("Mechanics Parameters"));

    // Walk
    processParam(QString("Fixed Steps"), row, m_map, m_focus->m_OpsFocusMechanics->focusWalk, "Walk");

    // Settle Time
    processParam(1.0, row, m_map, m_focus->m_OpsFocusMechanics->focusSettleTime, "Focuser Settle");

    // Initial Step Size - Sim = 5000 otherwise 20
    int stepSize = m_focus->m_Focuser && m_focus->m_Focuser->getDeviceName() == FOCUSER_SIMULATOR ? 5000 : 20;
    processParam(stepSize, row, m_map, m_focus->m_OpsFocusMechanics->focusTicks, "Initial Step Size");

    // Number of steps
    processParam(11, row, m_map, m_focus->m_OpsFocusMechanics->focusNumSteps, "Number Steps");

    // Max Travel - leave default
    processParam(m_focus->m_OpsFocusMechanics->focusMaxTravel->maximum(), row, m_map,
                 m_focus->m_OpsFocusMechanics->focusMaxTravel, "Max Travel");

    // Backlash - leave default
    processParam(m_focus->m_OpsFocusMechanics->focusBacklash->value(), row, m_map, m_focus->m_OpsFocusMechanics->focusBacklash,
                 "Driver Backlash");

    // AF Overscan - leave default
    processParam(m_focus->m_OpsFocusMechanics->focusAFOverscan->value(), row, m_map,
                 m_focus->m_OpsFocusMechanics->focusAFOverscan, "AF Overscan");

    // Overscan Delay
    processParam(0.0, row, m_map, m_focus->m_OpsFocusMechanics->focusOverscanDelay, "AF Overscan Delay");

    // Capture timeout
    processParam(30, row, m_map, m_focus->m_OpsFocusMechanics->focusCaptureTimeout, "Capture Timeout");

    // Motion timeout
    processParam(30, row, m_map, m_focus->m_OpsFocusMechanics->focusMotionTimeout, "Motion Timeout");

    resizeHelpDialog();
    setButtons(false);
}

QString FocusAdvisor::boolText(const bool flag)
{
    return flag ? "On" : "Off";
}

// Function to set inFocusAdvisor
void FocusAdvisor::setInFocusAdvisor(bool value)
{
    m_inFocusAdvisor = value;
}

// Return a prefix to use when Focus saves off a focus frame
QString FocusAdvisor::getFocusFramePrefix()
{
    QString prefix;
    if (inFocusAdvisor() && m_inFindStars)
        prefix = "_fs_" + QString("%1_%2").arg(m_findStarsRunNum).arg(m_focus->absIterations + 1);
    else if (inFocusAdvisor() && m_inPreAFAdj)
        prefix = "_ca_" + QString("%1_%2").arg(m_preAFRunNum).arg(m_focus->absIterations + 1);
    else if (inFocusAdvisor() && m_focus->inScanStartPos)
        prefix = "_ssp_" + QString("%1_%2").arg(m_focus->lastAFRun()).arg(m_focus->absIterations + 1);
    return prefix;
}

// Find similar OTs to seed defaults
QVariantMap FocusAdvisor::getOTDefaults(const QString &OTName)
{
    QVariantMap map;

    // If a blank OTName is passed in return an empty map
    if (OTName.isEmpty())
        return map;

    for (auto &tName : OpticalTrainManager::Instance()->getTrainNames())
    {
        if (tName == OTName)
            continue;
        auto tFocuser = OpticalTrainManager::Instance()->getFocuser(tName);
        if (tFocuser != m_focus->m_Focuser)
            continue;
        auto tScope = OpticalTrainManager::Instance()->getScope(tName);
        auto tScopeType = tScope["type"].toString();
        if (tScopeType != m_focus->m_ScopeType)
            continue;

        // We have an OT with the same Focuser and scope type so see if we have any parameters
        auto tID = OpticalTrainManager::Instance()->id(tName);
        OpticalTrainSettings::Instance()->setOpticalTrainID(tID);
        auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Focus);
        if (settings.isValid())
        {
            // We have a set of parameters
            map = settings.toJsonObject().toVariantMap();
            // We will adjust the step size here
            // We will use the CFZ. The CFZ scales with f#^2, so adjust step size in the same way
            auto tAperture = tScope["aperture"].toDouble(-1);
            auto tFocalLength = tScope["focal_length"].toDouble(-1);
            auto tFocalRatio = tScope["focal_ratio"].toDouble(-1);
            auto tReducer = OpticalTrainManager::Instance()->getReducer(tName);
            if (tFocalLength > 0.0)
                tFocalLength *= tReducer;

            // Use the adjusted focal length to calculate an adjusted focal ratio
            if (tFocalRatio <= 0.0)
                // For a scope, FL and aperture are specified so calc the F#
                tFocalRatio = (tAperture > 0.001) ? tFocalLength / tAperture : 0.0f;
            // else if (tAperture < 0.0)
            //     // DSLR Lens. FL and F# are specified so calc the aperture
            //     tAperture = tFocalLength / tFocalRatio;

            int stepSize = 5000;
            if (m_focus->m_Focuser && m_focus->m_Focuser->getDeviceName() == FOCUSER_SIMULATOR)
                // The Simulator is a special case so use 5000 as that works well
                stepSize = 5000;
            else
                stepSize = map.value("focusTicks", stepSize).toInt() * pow(m_focus->m_FocalRatio, 2.0) / pow(tFocalRatio, 2.0);
            // Add the value to map if one doesn't exist or update it if it does
            map.insert("focusTicks", stepSize);
            break;
        }
    }
    // Reset Optical Train Manager to the original OT
    auto id = OpticalTrainManager::Instance()->id(OTName);
    OpticalTrainSettings::Instance()->setOpticalTrainID(id);
    return map;
}

// Returns whether or not the passed in scopeType has a central obstruction or not. The scopeTypes
// are defined in the equipmentWriter code. It would be better, probably, if that code included
// a flag for central obstruction, rather than hard coding strings for the scopeType that are compared
// in this routine.
bool FocusAdvisor::scopeHasObstruction(const QString &scopeType)
{
    return (scopeType != "Refractor" && scopeType != "Kutter (Schiefspiegler)");
}

// Focus Advisor control function initialiser
void FocusAdvisor::initControl()
{
    setInFocusAdvisor(true);
    setButtons(true);
    m_initialStepSize = m_focus->m_OpsFocusMechanics->focusTicks->value();
    m_initialBacklash = m_focus->m_OpsFocusMechanics->focusBacklash->value();
    m_initialAFOverscan = m_focus->m_OpsFocusMechanics->focusAFOverscan->value();
    m_initialUseWeights = m_focus->m_OpsFocusProcess->focusUseWeights->isChecked();

    if (m_inFindStars)
        initFindStars(m_focus->currentPosition);
    else if (m_inPreAFAdj)
        initPreAFAdj(m_focus->currentPosition);
    else if (m_inAFAdj)
        initAFAdj(m_focus->currentPosition, false);
}

// Focus Advisor control function
void FocusAdvisor::control()
{
    if (!inFocusAdvisor())
        return;

    if (m_inFindStars)
        findStars();
    else if (m_inPreAFAdj)
        preAFAdj();
    else
        abort(i18n("Focus Advisor aborted due to internal error"));
}

// Prepare the Find Stars algorithm
void FocusAdvisor::initFindStars(const int startPos)
{
    // check whether Focus Advisor can run with the current parameters
    if (!canFocusAdvisorRun())
    {
        abort(i18n("Focus Advisor cannot run with current params"));
        return;
    }

    focusAdvFindStarsLabel->setText(i18n("In progress..."));
    emit newStage(FindingStars);

    // Set the initial position, which we'll fallback to in case of failure
    m_focus->initialFocuserAbsPosition = startPos;
    m_focus->linearRequestedPosition = startPos;

    // Set useWeights off as it makes the v-graph display unnecessarily complex whilst adding nothing
    m_focus->m_OpsFocusProcess->focusUseWeights->setChecked(false);

    m_focus->absIterations = 0;
    m_position.clear();
    m_measure.clear();
    m_focus->clearDataPoints();
    m_jumpsToGo = NUM_JUMPS_PER_SECTOR;
    m_jumpSize = m_focus->m_OpsFocusMechanics->focusTicks->value() * NUM_JUMPS_PER_SECTOR;
    m_findStarsIn = false;
    m_findStarsMaxBound = m_findStarsMinBound = false;
    m_findStarsSector = 0;
    m_findStarsRange = false;
    m_findStarsRangeIn = false;
    m_findStarsFindInEdge = false;
    m_findStarsInRange = -1;
    m_findStarsOutRange = -1;
    m_findStarsRunNum++;

    // Set backlash and Overscan off as its not needed at this stage - we'll calculate later
    m_focus->m_OpsFocusMechanics->focusBacklash->setValue(0);
    m_focus->m_OpsFocusMechanics->focusAFOverscan->setValue(0);

    addResultsTable(i18n("Find Stars"), m_findStarsRunNum, startPos, m_jumpSize,
                    m_focus->m_OpsFocusMechanics->focusAFOverscan->value(), "");
    emit m_focus->setTitle(QString(i18n("Find Stars: Scanning for stars...")), true);
    if (!m_focus->changeFocus(startPos - m_focus->currentPosition))
        abort(i18n("Find Stars: Failed"));
}

// Algorithm to scan the focuser's range of motion to find stars
// The algorithm is seeded with a start value, jump size and number of jumps and it sweeps out by num jumps,
// then sweeps in taking a frame and looking for stars. Once some stars have been found it searches the range
// of motion containing stars to get the central position which is passed as start point to the next stage.
void FocusAdvisor::findStars()
{
    // Plot the datapoint
    emit m_focus->newHFRPlotPosition(static_cast<double>(m_focus->currentPosition), m_focus->getLastMeasure(),
                                     std::pow(m_focus->getLastWeight(), -0.5), false, m_jumpSize, true);

    int offset = 0;
    bool starsExist = starsFound();
    if (m_findStarsRange)
    {
        bool outOfRange = false;
        if (starsExist)
        {
            // We have some stars but check we're not about to run out of range
            if (m_findStarsRangeIn)
                outOfRange = (m_focus->currentPosition - m_jumpSize) < static_cast<int>(m_focus->absMotionMin);
            else
                outOfRange = (m_focus->currentPosition + m_jumpSize) > static_cast<int>(m_focus->absMotionMax);
        }

        if (m_findStarsFindInEdge)
        {
            if (starsExist)
            {
                // We found the inner boundary of stars / no stars
                m_findStarsFindInEdge = false;
                m_findStarsInRange = m_focus->currentPosition - m_jumpSize;
            }
        }
        else if (!starsExist && m_findStarsInRange < 0 && m_findStarsOutRange < 0)
        {
            // We have 1 side of the stars range, but not the other... so reverse motion to get the other side
            // Calculate the offset to get back to the start position where stars were found
            if (m_findStarsRangeIn)
            {
                m_findStarsInRange = m_focus->currentPosition;
                auto max = std::max_element(std::begin(m_position), std::end(m_position));
                offset = *max - m_focus->currentPosition;
            }
            else
            {
                m_findStarsOutRange = m_focus->currentPosition;
                auto min = std::min_element(std::begin(m_position), std::end(m_position));
                offset = *min - m_focus->currentPosition;
            }
            m_findStarsRangeIn = !m_findStarsRangeIn;
        }
        else if (!starsExist || outOfRange)
        {
            // We're reached the other side of the zone in which stars can be detected so we're done
            // A good place to use for the next phase will be the centre of the zone
            m_inFindStars = false;
            if (m_findStarsRangeIn)
                // Range for stars is inwards
                m_findStarsInRange = m_focus->currentPosition;
            else
                // Range for stars is outwards
                m_findStarsOutRange = m_focus->currentPosition;
            const int zoneCentre = (m_findStarsInRange + m_findStarsOutRange) / 2;
            focusAdvFindStarsLabel->setText(i18n("Done"));
            updateResultsTable(i18n("Stars detected, range center %1", QString::number(zoneCentre)));
            // Now move onto the next stage or stop if nothing more to do
            if (m_inPreAFAdj)
                initPreAFAdj(zoneCentre);
            else if (m_inAFAdj)
                initAFAdj(zoneCentre, true);
            else
            {
                m_focus->absTicksSpin->setValue(zoneCentre);
                emit m_focus->setTitle(QString(i18n("Stars detected, range centre %1", zoneCentre)), true);
                complete(false, i18n("Focus Advisor Find Stars completed"));
            }
            return;
        }
    }

    // Log the results
    m_position.push_back(m_focus->currentPosition);
    m_measure.push_back(m_focus->getLastMeasure());

    // Now check if we have any stars
    if (starsExist)
    {
        // We have stars! Now try and find the centre of range where stars exist. We'll be conservative here
        // and set the range to the point where stars disappear.
        if (!m_findStarsRange)
        {
            m_findStarsRange = true;

            // If stars found first position we don't know where we are in the zone so explore both ends
            // Otherwise we know where 1 boundary is so we only need to expore the other
            if (m_position.size() > 1)
            {
                if (m_position.last() < m_position[m_position.size() - 2])
                {
                    // Stars were found whilst moving inwards so we know the outer boundary
                    QVector<int> positionsCopy = m_position;
                    std::sort(positionsCopy.begin(), positionsCopy.end(), std::greater<int>());
                    m_findStarsOutRange = positionsCopy[1];
                    m_findStarsOutRange = m_position[m_position.size() - 2];
                    m_findStarsRangeIn = true;
                }
                else
                {
                    // Stars found whilst moving outwards. Firstly we need to find the inward edge of no stars / stars
                    // so set the position back to the previous max position before the current point.
                    m_findStarsFindInEdge = true;
                    QVector<int> positionsCopy = m_position;
                    std::sort(positionsCopy.begin(), positionsCopy.end(), std::greater<int>());
                    offset = positionsCopy[1] - m_focus->currentPosition;
                }
            }
        }
    }

    // Cap the maximum number of iterations before failing
    if (++m_focus->absIterations > MAXIMUM_FOCUS_ADVISOR_ITERATIONS)
    {
        abort(i18n("Find Stars: exceeded max iterations %1", MAXIMUM_FOCUS_ADVISOR_ITERATIONS));
        return;
    }

    int deltaPos;
    if (m_findStarsRange)
    {
        // Collect more data to find the range of focuser motion with stars
        emit m_focus->setTitle(QString(i18n("Stars detected, centering range")), true);
        updateResultsTable(i18n("Stars detected, centering range"));
        int nextPos;
        if (m_findStarsRangeIn)
            nextPos = std::max(m_focus->currentPosition + offset - m_jumpSize, static_cast<int>(m_focus->absMotionMin));
        else
            nextPos = std::min(m_focus->currentPosition + offset + m_jumpSize, static_cast<int>(m_focus->absMotionMax));
        deltaPos = nextPos - m_focus->currentPosition;
    }
    else if (m_position.size() == 1)
    {
        // No luck with stars at the seed position so jump outwards and start an inward sweep
        deltaPos = m_jumpSize * m_jumpsToGo;
        // Check the proposed move is within bounds
        if (m_focus->currentPosition + deltaPos >= m_focus->absMotionMax)
        {
            deltaPos = m_focus->absMotionMax - m_focus->currentPosition;
            m_jumpsToGo = deltaPos / m_jumpSize + (deltaPos % m_jumpSize != 0);
            m_findStarsMaxBound = true;
        }
        m_findStarsJumpsInSector = m_jumpsToGo;
        m_findStarsSector = 1;
        emit m_focus->setTitle(QString(i18n("Find Stars Run %1: No stars at start %2, scanning...", m_findStarsRunNum,
                                            m_focus->currentPosition)), true);
    }
    else if (m_jumpsToGo > 0)
    {
        // Collect more data in the current sweep
        emit m_focus->setTitle(QString(i18n("Find Stars Run %1 Sector %2: Scanning %3/%4", m_findStarsRunNum, m_findStarsSector,
                                            m_jumpsToGo, m_findStarsJumpsInSector)), true);
        updateResultsTable(i18n("Find Stars Run %1: Scanning Sector %2", m_findStarsRunNum, m_findStarsSector));
        int nextPos = std::max(m_focus->currentPosition - m_jumpSize, static_cast<int>(m_focus->absMotionMin));
        deltaPos = nextPos - m_focus->currentPosition;
    }
    else
    {
        // We've completed the current sweep, but still no stars so check if we still have focuser range to search...
        if(m_findStarsMaxBound && m_findStarsMinBound)
        {
            // We're out of road... covered the entire focuser range of motion but couldn't find any stars
            // halve the step size and go again
            updateResultsTable(i18n("No stars detected"));
            int newStepSize = m_focus->m_OpsFocusMechanics->focusTicks->value() / 2;
            if (newStepSize > 1)
            {
                m_focus->m_OpsFocusMechanics->focusTicks->setValue(newStepSize);
                initFindStars(m_focus->initialFocuserAbsPosition);
            }
            else
                abort(i18n("Find Stars Run %1: failed to find any stars", m_findStarsRunNum));
            return;
        }

        // Setup the next sweep sector...
        m_jumpsToGo = NUM_JUMPS_PER_SECTOR;
        if (m_findStarsIn)
        {
            // We're "inside" the starting point so flip to the outside unless max bound already hit
            if (m_findStarsMaxBound)
            {
                // Ensure deltaPos doesn't go below minimum
                if (m_focus->currentPosition - m_jumpSize < static_cast<int>(m_focus->absMotionMin))
                    deltaPos = static_cast<int>(m_focus->absMotionMin) - m_focus->currentPosition;
                else
                    deltaPos = -m_jumpSize;
            }
            else
            {
                auto max = std::max_element(std::begin(m_position), std::end(m_position));
                int sweepMax = *max + (m_jumpSize * m_jumpsToGo);
                if (sweepMax >= static_cast<int>(m_focus->absMotionMax))
                {
                    sweepMax = static_cast<int>(m_focus->absMotionMax);
                    m_jumpsToGo = (sweepMax - *max) / m_jumpSize + ((sweepMax - *max) % m_jumpSize != 0);
                    m_findStarsMaxBound = true;
                }
                m_findStarsIn = false;
                deltaPos = sweepMax - m_focus->currentPosition;
            }
        }
        else
        {
            // We're "outside" the starting point so continue inwards unless min bound already hit
            if (m_findStarsMinBound)
            {
                auto max = std::max_element(std::begin(m_position), std::end(m_position));
                int sweepMax = *max + (m_jumpSize * m_jumpsToGo);
                if (sweepMax >= static_cast<int>(m_focus->absMotionMax))
                {
                    sweepMax = static_cast<int>(m_focus->absMotionMax);
                    m_jumpsToGo = (sweepMax - *max) / m_jumpSize + ((sweepMax - *max) % m_jumpSize != 0);
                    m_findStarsMaxBound = true;
                }
                deltaPos = sweepMax - m_focus->currentPosition;
            }
            else
            {
                auto min = std::min_element(std::begin(m_position), std::end(m_position));
                int sweepMin = *min - (m_jumpSize * m_jumpsToGo);
                if (sweepMin <= static_cast<int>(m_focus->absMotionMin))
                {
                    // This sweep will hit the min bound
                    m_findStarsMinBound = true;
                    sweepMin = static_cast<int>(m_focus->absMotionMin);
                    m_jumpsToGo = (*min - sweepMin) / m_jumpSize + ((*min - sweepMin) % m_jumpSize != 0);
                }
                // We've already done the most inward point of a sweep at the start so jump to the next point.
                m_findStarsIn = true;
                int nextJumpPos = std::max(*min - m_jumpSize, static_cast<int>(m_focus->absMotionMin));
                deltaPos = nextJumpPos - m_focus->currentPosition;
            }
        }
        m_findStarsSector++;
        m_findStarsJumpsInSector = m_jumpsToGo;
        emit m_focus->setTitle(QString(i18n("Find Stars Run %1 Sector %2: scanning %3/%4", m_findStarsRunNum, m_findStarsSector,
                                            m_jumpsToGo, m_findStarsJumpsInSector)), true);
    }
    if (!m_findStarsRange)
        m_jumpsToGo--;
    m_focus->linearRequestedPosition = m_focus->currentPosition + deltaPos;
    if (!m_focus->changeFocus(deltaPos))
        abort(i18n("Focus Advisor Find Stars run %1: failed to move focuser", m_findStarsRunNum));
}

bool FocusAdvisor::starsFound()
{
    return (m_focus->getLastMeasure() != INVALID_STAR_MEASURE && m_focus->getLastNumStars() > FIND_STARS_MIN_STARS);

}

// Initialise the pre-Autofocus adjustment algorithm
void FocusAdvisor::initPreAFAdj(const int startPos)
{
    // check whether Focus Advisor can run with the current parameters
    if (!canFocusAdvisorRun())
    {
        abort(i18n("Focus Advisor cannot run with current params"));
        return;
    }

    // Check we're not looping
    if (m_preAFRunNum > 50)
    {
        abort(i18n("Focus Advisor Coarse Adjustment aborted after %1 runs", m_preAFRunNum));
        return;
    }

    focusAdvCoarseAdjLabel->setText(i18n("In progress..."));
    emit newStage(CoarseAdjustments);

    m_focus->initialFocuserAbsPosition = startPos;
    m_focus->absIterations = 0;
    m_position.clear();
    m_measure.clear();
    m_preAFInner = 0;
    m_preAFNoStarsOut = m_preAFNoStarsIn = false;
    m_preAFRunNum++;

    // Reset the v-curve - otherwise there's too much data to see what's going on
    m_focus->clearDataPoints();
    emit m_focus->setTitle(QString(i18n("Coarse Adjustment Scan...")), true);

    // Setup a sweep of m_jumpSize either side of startPos
    m_jumpSize = m_focus->m_OpsFocusMechanics->focusTicks->value() * NUM_STEPS_PRE_AF;

    // Check that the sweep can fit into the focuser's motion range
    if (m_jumpSize > m_focus->absMotionMax - m_focus->absMotionMin)
    {
        m_jumpSize = m_focus->absMotionMax - m_focus->absMotionMin;
        m_focus->m_OpsFocusMechanics->focusTicks->setValue(m_jumpSize / NUM_STEPS_PRE_AF);
    }

    int deltaPos = (startPos - m_focus->currentPosition) + m_jumpSize / 2;
    if (m_focus->currentPosition + deltaPos > maxStarsLimit())
        deltaPos = maxStarsLimit() - m_focus->currentPosition;

    m_preAFInner = startPos - m_jumpSize / 2;
    if (m_preAFInner < minStarsLimit())
        m_preAFInner = minStarsLimit();

    // Set backlash and AF Overscan off on first run, and reset useWeights
    if (m_preAFRunNum == 1)
    {
        m_focus->m_OpsFocusMechanics->focusBacklash->setValue(0);
        m_focus->m_OpsFocusMechanics->focusAFOverscan->setValue(0);
        m_focus->m_OpsFocusProcess->focusUseWeights->setChecked(m_initialUseWeights);
    }

    addResultsTable(i18n("Coarse Adjustment"), m_preAFRunNum, startPos,
                    m_focus->m_OpsFocusMechanics->focusTicks->value(),
                    m_focus->m_OpsFocusMechanics->focusAFOverscan->value(), "");

    m_focus->linearRequestedPosition = m_focus->currentPosition + deltaPos;
    if (!m_focus->changeFocus(deltaPos))
        abort(i18n("Focus Advisor Coarse Adjustment failed to move focuser"));
}

// Pre Autofocus coarse adjustment algorithm.
// Move the focuser until we get a reasonable movement in measure (x2)
void FocusAdvisor::preAFAdj()
{
    // Cap the maximum number of iterations before failing
    if (++m_focus->absIterations > MAXIMUM_FOCUS_ADVISOR_ITERATIONS)
    {
        abort(i18n("Focus Advisor Coarse Adjustment: exceeded max iterations %1", MAXIMUM_FOCUS_ADVISOR_ITERATIONS));
        return;
    }

    int deltaPos;
    const int step = m_focus->m_OpsFocusMechanics->focusTicks->value();

    bool starsExist = starsFound();
    if (starsExist)
    {
        // If we have stars, persist the results for later analysis
        m_position.push_back(m_focus->currentPosition);
        m_measure.push_back(m_focus->getLastMeasure());

        if (m_preAFNoStarsOut && (m_position.size() == 0))
        {
            if (m_findStarsOutRange < 0)
                m_findStarsOutRange = m_focus->currentPosition;
            else
                m_findStarsOutRange = std::max(m_findStarsOutRange, m_focus->currentPosition);
        }
    }
    else
    {
        // No stars - record whether this is at the inward or outward. If in the middle, just ignore
        if (m_position.size() < 2)
            m_preAFNoStarsOut = true;
        else if (m_focus->currentPosition - (2 * step) <= m_preAFInner)
        {
            m_preAFNoStarsIn = true;
            if (m_findStarsInRange < 0)
                m_findStarsInRange = m_position.last();
            else
                m_findStarsInRange = std::min(m_findStarsInRange, m_position.last());
        }
    }

    emit m_focus->newHFRPlotPosition(static_cast<double>(m_focus->currentPosition), m_focus->getLastMeasure(),
                                     std::pow(m_focus->getLastWeight(), -0.5), false, step, true);

    // See if we need to extend the sweep
    if (m_focus->currentPosition - step < m_preAFInner && starsExist)
    {
        // Next step would take us beyond our bound, so see if we have enough data or want to extend the sweep
        auto it = std::min_element(std::begin(m_measure), std::end(m_measure));
        auto min = std::distance(std::begin(m_measure), it);
        if (m_position.size() < 5 || (m_position.size() < 2 * NUM_STEPS_PRE_AF && m_position.size() - min < 3))
        {
            // Not much data or minimum is at the edge so continue for a bit, if we can
            if (m_preAFInner - step >= minStarsLimit())
                m_preAFInner -= step;
        }
    }

    if (m_focus->currentPosition - step >= m_preAFInner)
    {
        // Collect more data in the current sweep
        emit m_focus->setTitle(QString(i18n("Coarse Adjustment Run %1 scan...", m_preAFRunNum)), true);
        deltaPos = -step;
    }
    else
    {
        // We've completed the current sweep, so analyse the data...
        if (m_position.size() < 5)
        {
            abort(i18n("Focus Advisor Coarse Adjustment Run %1: insufficient data to proceed", m_preAFRunNum));
            return;
        }
        else
        {
            auto it = std::min_element(std::begin(m_measure), std::end(m_measure));
            auto min = std::distance(std::begin(m_measure), it);
            const double minMeasure = *it;
            int minPos = m_position[min];

            auto it2 = std::max_element(std::begin(m_measure), std::end(m_measure));
            const double maxMeasure = *it2;
            // Get a ratio of max to min scaled to NUM_STEPS_PRE_AF
            const double scaling = static_cast<double>(NUM_STEPS_PRE_AF) / static_cast<double>(m_measure.count());
            const double measureRatio = maxMeasure / minMeasure * scaling;

            // Is the minimum near the centre of the sweep?
            double whereInRange = static_cast<double>(minPos - m_position.last()) / static_cast<double>
                                  (m_position.first() - m_position.last());
            bool nearCenter = whereInRange > 0.3 && whereInRange < 0.7;

            QVector<double> gradients;
            for (int i = 0; i < m_position.size() - 1; i++)
            {
                double gradient = (m_measure[i] - m_measure[i + 1]) / (m_position[i] - m_position[i + 1]);
                gradients.push_back(std::abs(gradient));
            }

            // Average the largest 3 gradients (to stop distortion by a single value). The largest gradients should occur
            // at the furthest out position. Assume smaller gradients at furthest out position are caused by backlash
            QVector<double> gradientsCopy = gradients;
            std::sort(gradientsCopy.begin(), gradientsCopy.end(), std::greater<double>());
            std::vector<double> gradientsMax;
            for (int i = 0; i < 3; i++)
                gradientsMax.push_back(gradientsCopy[i]);

            double gradientsMean = Mathematics::RobustStatistics::ComputeLocation(Mathematics::RobustStatistics::LOCATION_MEAN,
                                   gradientsMax);

            const double threshold = gradientsMean * 0.5;
            int overscan = m_focus->m_OpsFocusMechanics->focusAFOverscan->value();
            for (int i = 0; i < gradients.size(); i++)
            {
                if (gradients[i] <= threshold)
                    overscan += m_position[i] - m_position[i + 1];
                else
                    break;
            }
            overscan = std::max(overscan, step);
            m_focus->m_OpsFocusMechanics->focusAFOverscan->setValue(overscan);

            const bool hitNoStarsRegion = m_preAFNoStarsIn || m_preAFNoStarsOut;

            // Is everything good enough to proceed, or do we need to run again?
            if (nearCenter && maxMinRatioOK(INITIAL_MAXMIN_HFR_RATIO, measureRatio) && !hitNoStarsRegion)
            {
                // We're done with the coarse adjustment step so prepare for the next step
                updateResultsTable(i18n("Max/Min Ratio: %1", QString::number(measureRatio, 'f', 1)));
                m_inPreAFAdj = false;
                focusAdvCoarseAdjLabel->setText(i18n("Done"));
                m_focus->absIterations = 0;
                m_position.clear();
                m_measure.clear();
                if (m_inAFAdj)
                {
                    // Reset the v-curve - otherwise there's too much data to see what's going on
                    m_focus->clearDataPoints();
                    // run Autofocus
                    m_nearFocus = true;
                    initAFAdj(minPos, true);
                }
                else
                {
                    m_focus->absTicksSpin->setValue(minPos);
                    complete(false, i18n("Focus Advisor Coarse Adjustment completed."));
                }
                return;
            }
            else
            {
                // Curve is too flat / steep - so we need to change the range, select a better start position and rerun...
                int startPosition;
                const double expansion = TARGET_MAXMIN_HFR_RATIO / measureRatio;
                int newStepSize = m_focus->m_OpsFocusMechanics->focusTicks->value() * expansion;

                if (m_preAFNoStarsIn && m_preAFNoStarsOut)
                {
                    // We have no stars both inwards and outwards
                    const int range = m_position.first() - m_position.last();
                    newStepSize = range / NUM_STEPS_PRE_AF;
                    startPosition = (m_position.first() + m_position.last()) / 2;
                    m_preAFMaxRange = true;
                    if (newStepSize < 1)
                    {
                        // Looks like data is inconsistent so stop here
                        abort(i18n("Focus Advisor Coarse Adjustment: data quality too poor to continue"));
                        return;
                    }
                }
                else if (m_preAFNoStarsIn)
                {
                    // Shift start position outwards as we had no stars inwards
                    int range = NUM_STEPS_PRE_AF * newStepSize;
                    int jumpPosition = m_position.last() + range;
                    if (jumpPosition > maxStarsLimit())
                    {
                        range = maxStarsLimit() - m_position.last();
                        newStepSize = range / NUM_STEPS_PRE_AF;
                        m_preAFMaxRange = true;
                    }
                    startPosition = m_position.last() + (range / 2);
                }
                else if (m_preAFNoStarsOut)
                {
                    // Shift start position inwards as we had no stars outwards
                    int range = NUM_STEPS_PRE_AF * newStepSize;
                    int endPosition = m_position.first() - range;
                    if (endPosition < minStarsLimit())
                    {
                        range = m_position.first() - minStarsLimit();
                        newStepSize = range / NUM_STEPS_PRE_AF;
                        m_preAFMaxRange = true;
                    }
                    startPosition = m_position.first() - (range / 2);
                }
                else
                    // Set the start position to the previous minimum
                    startPosition = minPos;

                updateResultsTable(i18n("Max/Min Ratio: %1, Next Step Size: %2, Next Overscan: %3", QString::number(measureRatio, 'f', 1),
                                        QString::number(newStepSize), QString::number(overscan)));
                m_focus->m_OpsFocusMechanics->focusTicks->setValue(newStepSize);
                initPreAFAdj(startPosition);
                return;
            }
        }
    }
    m_focus->linearRequestedPosition = m_focus->currentPosition + deltaPos;
    if (!m_focus->changeFocus(deltaPos))
        abort(i18n("Focus Advisor Coarse Adjustment failed to move focuser"));
}

// Check whether the Max / Min star measure ratio is good enough
bool FocusAdvisor::maxMinRatioOK(const double limit, const double maxMinRatio)
{
    if (m_preAFMaxRange)
        // We've hit the maximum focuser range where we have stars so go with what we've got in terms of maxMinRatio
        return true;
    return maxMinRatio >= limit;
}

// Minimum focuser position where stars exist - if unsure about stars return min focuser position
int FocusAdvisor::minStarsLimit()
{
    return std::max(static_cast<int>(m_focus->absMotionMin), m_findStarsInRange);
}

// Maximum focuser position where stars exist - if unsure about stars return max focuser position
int FocusAdvisor::maxStarsLimit()
{
    if (m_findStarsOutRange < 0)
        return m_focus->absMotionMax;
    else
        return std::min(static_cast<int>(m_focus->absMotionMax), m_findStarsOutRange);
}

void FocusAdvisor::initAFAdj(const int startPos, const bool retryOverscan)
{
    // check whether Focus Advisor can run with the current parameters
    if (!canFocusAdvisorRun())
    {
        abort(i18n("Focus Advisor cannot run with current params"));
        return;
    }

    focusAdvFineAdjLabel->setText(i18n("In progress..."));
    emit newStage(FineAdjustments);

    // The preAF routine will have estimated AF Overscan but because its a crude measure its likely to be an overestimate.
    // We will try and refine the estimate by halving the current Overscan
    if (retryOverscan)
    {
        const int newOverscan = m_focus->m_OpsFocusMechanics->focusAFOverscan->value() / 2;
        m_focus->m_OpsFocusMechanics->focusBacklash->setValue(0);
        m_focus->m_OpsFocusMechanics->focusAFOverscan->setValue(newOverscan);
    }

    // Reset useWeights setting
    m_focus->m_OpsFocusProcess->focusUseWeights->setChecked(m_initialUseWeights);

    addResultsTable(i18n("Fine Adjustment"), ++m_AFRunCount, startPos,
                    m_focus->m_OpsFocusMechanics->focusTicks->value(),
                    m_focus->m_OpsFocusMechanics->focusAFOverscan->value(), "");

    startAF(startPos);
}

void FocusAdvisor::startAF(const int startPos)
{
    if (m_nearFocus)
    {
        setInFocusAdvisor(false);
        m_focus->absIterations = 0;
        m_focus->setupLinearFocuser(startPos);
        if (!m_focus->changeFocus(m_focus->linearRequestedPosition - m_focus->currentPosition))
            m_focus->completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_NO_MOVE);
    }
    else
    {
        m_nearFocus = true;
        m_focus->initScanStartPos(true, startPos);
    }
}

bool FocusAdvisor::analyseAF()
{
    if (m_focus->m_FocusAlgorithm != Focus::FOCUS_LINEAR1PASS || !m_focus->linearFocuser || !m_focus->linearFocuser->isDone()
            || m_focus->linearFocuser->solution() == -1)
        return false;

    bool runAgainRatio = false;
    QVector<int> positions;
    QVector<double> measures, weights;
    QVector<bool> outliers;
    m_focus->linearFocuser->getPass1Measurements(&positions, &measures, &weights, &outliers);

    int minPosition = m_focus->linearFocuser->solution();
    double minMeasure = m_focus->linearFocuser->solutionValue();

    int maxPositon = positions[0];
    double maxMeasure = m_focus->curveFitting->f(maxPositon);

    const int stepSize = m_focus->m_OpsFocusMechanics->focusTicks->value();
    int newStepSize = stepSize;
    // Firstly check that the step size is giving a good measureRatio
    double measureRatio = maxMeasure / minMeasure;
    if (measureRatio > 2.5 && measureRatio < 3.5)
        // Sweet spot
        runAgainRatio = false;
    else
    {
        runAgainRatio = true;
        // Adjust the step size to try and get the measureRatio around 3
        int pos = m_focus->curveFitting->fy(minMeasure * TARGET_MAXMIN_HFR_RATIO);
        newStepSize = (pos - minPosition) / ((positions.size() - 1.0) / 2.0);
        // Throttle newStepSize. Usually this stage should be close to good parameters so changes in step size
        // should be small, but if run with poor parameters changes should be throttled to prevent big swings that
        // can lead to Autofocus failure.
        double ratio = static_cast<double>(newStepSize) / static_cast<double>(stepSize);
        ratio = std::max(std::min(ratio, 2.0), 0.5);
        newStepSize = stepSize * ratio;
        m_focus->m_OpsFocusMechanics->focusTicks->setValue(newStepSize);
    }

    // Look at the backlash
    // So assume flatness of curve at the outward point is all due to backlash.
    if (!m_overscanFound)
    {
        double backlashPoints = 0.0;
        for (int i = 0; i < positions.size() / 2; i++)
        {
            double deltaAct = measures[i] - measures[i + 1];
            double deltaExp = m_focus->curveFitting->f(positions[i]) - m_focus->curveFitting->f(positions[i + 1]);
            double delta = std::abs(deltaAct / deltaExp);
            // May have to play around with this threshold
            if (delta > 0.75)
                break;
            if (delta > 0.5)
                backlashPoints += 0.5;
            else
                backlashPoints++;
        }

        const int overscan = m_focus->m_OpsFocusMechanics->focusAFOverscan->value();
        int newOverscan = overscan;

        if (backlashPoints > 0.0)
        {
            // We've found some additional Overscan so we know the current value is too low and now have a reasonable estimate
            newOverscan = overscan + (stepSize * backlashPoints);
            m_overscanFound = true;
        }
        else if (overscan == 0)
            m_overscanFound = true;
        else if (!m_overscanFound)
            // No additional Overscan was detected so the current Overscan estimate may be too high so try reducing it
            newOverscan = overscan <= 2 * stepSize ? 0 : overscan / 2;

        m_focus->m_OpsFocusMechanics->focusAFOverscan->setValue(newOverscan);
    }

    // Try again for a poor R2 - but retry just once (unless something else changes so we don't get stuck in a loop
    if (m_runAgainR2)
        m_runAgainR2 = false;
    else
        m_runAgainR2 = m_focus->R2 < m_focus->m_OpsFocusProcess->focusR2Limit->value();
    bool runAgain = runAgainRatio || m_runAgainR2 || !m_overscanFound;

    updateResultsTable(i18n("Max/Min Ratio: %1, R2: %2, Step Size: %3, Overscan: %4", QString::number(measureRatio, 'f', 1),
                            QString::number(m_focus->R2, 'f', 3), QString::number(newStepSize),
                            QString::number(m_focus->m_OpsFocusMechanics->focusAFOverscan->value())));
    if (!runAgain)
    {
        m_inAFAdj = false;
        focusAdvFineAdjLabel->setText(i18n("Done"));
        complete(true, i18n("Focus Advisor Fine Adjustment completed"));
        emit newStage(Idle);
    }
    return runAgain;
}

// Reset the Focus Advisor
void FocusAdvisor::reset()
{
    m_inFocusAdvisor = false;
    m_initialStepSize = -1;
    m_initialBacklash = -1;
    m_initialAFOverscan = -1;
    m_initialUseWeights = false;
    m_initialScanStartPos = false;
    m_position.clear();
    m_measure.clear();
    m_inFindStars = false;
    m_inPreAFAdj = false;
    m_inAFAdj = false;
    m_jumpsToGo = 0;
    m_jumpSize = 0;
    m_findStarsIn = false;
    m_findStarsMaxBound = false;
    m_findStarsMinBound = false;
    m_findStarsSector = 0;
    m_findStarsRunNum = 0;
    m_findStarsRange = false;
    m_findStarsRangeIn = false;
    m_findStarsFindInEdge = false;
    m_findStarsInRange = -1;
    m_findStarsOutRange = -1;
    m_preAFInner = 0;
    m_preAFNoStarsOut = false;
    m_preAFNoStarsIn = false;
    m_preAFMaxRange = false;
    m_preAFRunNum = 0;
    m_overscanFound = false;
    m_runAgainR2 = false;
    m_nearFocus = false;
    m_AFRunCount = 0;
    setButtons(false);
    focusAdvUpdateParamsLabel->setText("");
    focusAdvFindStarsLabel->setText("");
    focusAdvCoarseAdjLabel->setText("");
    focusAdvFineAdjLabel->setText("");
}

// Abort the Focus Advisor
void FocusAdvisor::abort(const QString &msg)
{
    m_focus->appendLogText(msg);

    // Restore settings to initial value
    resetSavedSettings(false);
    m_focus->processAbort();
}

// Focus Advisor completed successfully
// If Autofocus was run then do the usual processing / notification of success, otherwise treat it as an autofocus fail
void FocusAdvisor::complete(const bool autofocus, const QString &msg)
{
    m_focus->appendLogText(msg);
    // Restore settings to initial value
    resetSavedSettings(true);

    if (!autofocus)
    {
        if (m_initialBacklash > -1) m_focus->m_OpsFocusMechanics->focusBacklash->setValue(m_initialBacklash);
        if (m_initialAFOverscan > -1) m_focus->m_OpsFocusMechanics->focusAFOverscan->setValue(m_initialAFOverscan);
        m_focus->completeFocusProcedure(Ekos::FOCUS_IDLE, Ekos::FOCUS_FAIL_ADVISOR_COMPLETE);
    }
}

void FocusAdvisor::resetSavedSettings(const bool success)
{
    m_focus->m_OpsFocusProcess->focusUseWeights->setChecked(m_initialUseWeights);
    m_focus->m_OpsFocusProcess->focusScanStartPos->setChecked(m_initialScanStartPos);

    if (!success)
    {
        // Restore settings to initial value since Focus Advisor failed
        if (m_initialStepSize > -1) m_focus->m_OpsFocusMechanics->focusTicks->setValue(m_initialStepSize);
        if (m_initialBacklash > -1) m_focus->m_OpsFocusMechanics->focusBacklash->setValue(m_initialBacklash);
        if (m_initialAFOverscan > -1) m_focus->m_OpsFocusMechanics->focusAFOverscan->setValue(m_initialAFOverscan);
    }
}

// Add a new row to the results table
void FocusAdvisor::addResultsTable(QString section, int run, int startPos, int stepSize, int overscan, QString text)
{
    m_focus->appendLogText(i18n("Focus Advisor Result (%1): Run: %2 startPos: %3 stepSize: %4 overscan: %5",
                                section, run, startPos, stepSize, overscan));

    focusAdvTable->insertRow(0);
    QTableWidgetItem *itemSection = new QTableWidgetItem(section);
    focusAdvTable->setItem(0, RESULTS_SECTION, itemSection);
    QString runStr = (run >= 0) ? QString("%1").arg(run) : "N/A";
    QTableWidgetItem *itemRunNumber = new QTableWidgetItem(runStr);
    itemRunNumber->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    focusAdvTable->setItem(0, RESULTS_RUN_NUMBER, itemRunNumber);
    QString startPosStr = (startPos >= 0) ? QString("%1").arg(startPos) : "N/A";
    QTableWidgetItem *itemStartPos = new QTableWidgetItem(startPosStr);
    itemStartPos->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    focusAdvTable->setItem(0, RESULTS_START_POSITION, itemStartPos);
    QString stepSizeStr = (stepSize >= 0) ? QString("%1").arg(stepSize) : "N/A";
    QTableWidgetItem *itemStepSize = new QTableWidgetItem(stepSizeStr);
    itemStepSize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    focusAdvTable->setItem(0, RESULTS_STEP_SIZE, itemStepSize);
    QString overscanStr = (stepSize >= 0) ? QString("%1").arg(overscan) : "N/A";
    QTableWidgetItem *itemAFOverscan = new QTableWidgetItem(overscanStr);
    itemAFOverscan->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    focusAdvTable->setItem(0, RESULTS_AFOVERSCAN, itemAFOverscan);
    QTableWidgetItem *itemText = new QTableWidgetItem(text);
    focusAdvTable->setItem(0, RESULTS_TEXT, itemText);

    emit newMessage(text);

    if (focusAdvTable->rowCount() == 1)
    {
        focusAdvTable->show();
        resizeDialog();
    }
}

// Update text for current row (0) in the results table with the passed in value
void FocusAdvisor::updateResultsTable(QString text)
{
    m_focus->appendLogText(i18n("Focus Advisor Result Update: %1", text));

    QTableWidgetItem *itemText = new QTableWidgetItem(text);
    focusAdvTable->setItem(0, RESULTS_TEXT, itemText);
    focusAdvTable->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void FocusAdvisor::resizeDialog()
{
    focusAdvTable->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    int left, right, top, bottom;
    this->verticalLayout->layout()->getContentsMargins(&left, &top, &right, &bottom);

    int width = left + right;
    for (int i = 0; i < focusAdvTable->horizontalHeader()->count(); i++)
        width += focusAdvTable->columnWidth(i);
    const int height = focusAdvGroupBox->height() + focusAdvTable->height() + focusAdvButtonBox->height();
    this->resize(width, height);
}
}

/*  Ekos Alignment Module
    Copyright (C) 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "align.h"

#include "alignadaptor.h"
#include "alignview.h"
#include "flagcomponent.h"
#include "fov.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "offlineastrometryparser.h"
#include "onlineastrometryparser.h"
#include "astapastrometryparser.h"
#include "opsalign.h"
#include "opsprograms.h"
#include "opsastap.h"
#include "opsastrometry.h"
#include "opsastrometrycfg.h"
#include "opsastrometryindexfiles.h"
#include "Options.h"
#include "remoteastrometryparser.h"
#include "skymap.h"
#include "skymapcomposite.h"
#include "starobject.h"
#include "auxiliary/QProgressIndicator.h"
#include "auxiliary/ksmessagebox.h"
#include "dialogs/finddialog.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitstab.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/indifilter.h"
#include "profileinfo.h"
#include "ksnotification.h"
#include "kspaths.h"

#include <KConfigDialog>
#include <KActionCollection>

#include <basedevice.h>
#include <indicom.h>

#include <memory>

#include <ekos_align_debug.h>

#define PAH_CUTOFF_FOV            10 // Minimum FOV width in arcminutes for PAH to work
#define MAXIMUM_SOLVER_ITERATIONS 10
#define CAPTURE_RETRY_DELAY       10000

#define AL_FORMAT_VERSION 1.0

namespace Ekos
{
// 30 arcminutes RA movement
const double Align::RAMotion = 0.5;
// Sidereal rate, degrees/s
const double Align::SIDRATE = 0.004178;

const QMap<Align::PAHStage, QString> Align::PAHStages =
{
    {PAH_IDLE, I18N_NOOP("Idle")},
    {PAH_FIRST_CAPTURE, I18N_NOOP("First Capture"}),
    {PAH_FIND_CP, I18N_NOOP("Finding CP"}),
    {PAH_FIRST_ROTATE, I18N_NOOP("First Rotation"}),
    {PAH_SECOND_CAPTURE, I18N_NOOP("Second Capture"}),
    {PAH_SECOND_ROTATE, I18N_NOOP("Second Rotation"}),
    {PAH_THIRD_CAPTURE, I18N_NOOP("Third Capture"}),
    {PAH_STAR_SELECT, I18N_NOOP("Select Star"}),
    {PAH_PRE_REFRESH, I18N_NOOP("Select Refresh"}),
    {PAH_REFRESH, I18N_NOOP("Refreshing"}),
    {PAH_ERROR, I18N_NOOP("Error")},
};

Align::Align(ProfileInfo *activeProfile) : m_ActiveProfile(activeProfile)
{
    setupUi(this);

    qRegisterMetaType<Ekos::AlignState>("Ekos::AlignState");
    qDBusRegisterMetaType<Ekos::AlignState>();

    new AlignAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Align", this);

    dirPath = QDir::homePath();

    KStarsData::Instance()->clearTransientFOVs();

    //loadSlewMode = false;
    solverFOV.reset(new FOV());
    solverFOV->setName(i18n("Solver FOV"));
    solverFOV->setObjectName("solver_fov");
    solverFOV->setLockCelestialPole(true);
    solverFOV->setColor(KStars::Instance()->data()->colorScheme()->colorNamed("SolverFOVColor").name());
    solverFOV->setProperty("visible", false);
    KStarsData::Instance()->addTransientFOV(solverFOV);

    sensorFOV.reset(new FOV());
    sensorFOV->setObjectName("sensor_fov");
    sensorFOV->setLockCelestialPole(true);
    sensorFOV->setProperty("visible", Options::showSensorFOV());
    KStarsData::Instance()->addTransientFOV(sensorFOV);

    QAction *a = KStars::Instance()->actionCollection()->action("show_sensor_fov");
    if (a)
        a->setEnabled(true);

    showFITSViewerB->setIcon(
        QIcon::fromTheme("kstars_fitsviewer"));
    showFITSViewerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(showFITSViewerB, &QPushButton::clicked, this, &Ekos::Align::showFITSViewer);

    toggleFullScreenB->setIcon(
        QIcon::fromTheme("view-fullscreen"));
    toggleFullScreenB->setShortcut(Qt::Key_F4);
    toggleFullScreenB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(toggleFullScreenB, &QPushButton::clicked, this, &Ekos::Align::toggleAlignWidgetFullScreen);

    alignView = new AlignView(alignWidget, FITS_ALIGN);
    alignView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    alignView->setBaseSize(alignWidget->size());
    alignView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();

    vlayout->addWidget(alignView);
    alignWidget->setLayout(vlayout);

    connect(solveB, &QPushButton::clicked, [this]()
    {
        syncTargetToMount();
        captureAndSolve();
    });
    connect(stopB, &QPushButton::clicked, this, &Ekos::Align::abort);
    connect(measureAltB, &QPushButton::clicked, this, &Ekos::Align::measureAltError);
    connect(measureAzB, &QPushButton::clicked, this, &Ekos::Align::measureAzError);

    // Effective FOV Edit
    connect(FOVOut, &QLineEdit::editingFinished, this, &Align::syncFOV);

    connect(CCDCaptureCombo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Align::setDefaultCCD);
    connect(CCDCaptureCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Align::checkCCD);

    connect(correctAltB, &QPushButton::clicked, this, &Ekos::Align::correctAltError);
    connect(correctAzB, &QPushButton::clicked, this, &Ekos::Align::correctAzError);
    connect(loadSlewB, &QPushButton::clicked, [&]()
    {
        loadAndSlew();
    });

    FilterDevicesCombo->addItem("--");
    connect(FilterDevicesCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated),
            [ = ](const QString & text)
    {
        syncSettings();
        Options::setDefaultAlignFilterWheel(text);
    });

    connect(FilterDevicesCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Align::checkFilter);

    connect(FilterPosCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            [ = ](int index)
    {
        syncSettings();
        Options::setLockAlignFilterIndex(index);
    }
           );

    connect(PAHSlewRateCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [&](int index)
    {
        Options::setPAHMountSpeedIndex(index);
    });

    PAHFlipVectorC->setChecked(Options::pAHFlipCorrectionVector());
    PAHFlipVectorC2->setChecked(Options::pAHFlipCorrectionVector());
    connect(PAHFlipVectorC2, &QCheckBox::toggled, [&] { PAHFlipVectorC->setChecked(PAHFlipVectorC2->isChecked());});
    connect(PAHFlipVectorC, &QCheckBox::toggled, [&]()
    {
        Options::setPAHFlipCorrectionVector(PAHFlipVectorC->isChecked());
        PAHFlipVectorC2->blockSignals(true);
        PAHFlipVectorC2->setChecked(PAHFlipVectorC->isChecked());
        PAHFlipVectorC2->blockSignals(false);
        syncCorrectionVector();
    });

    gotoModeButtonGroup->setId(syncR, GOTO_SYNC);
    gotoModeButtonGroup->setId(slewR, GOTO_SLEW);
    gotoModeButtonGroup->setId(nothingR, GOTO_NOTHING);

    connect(gotoModeButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this,
            [ = ](int id)
    {
        this->currentGotoMode = static_cast<GotoMode>(id);
    });

    m_CaptureTimer.setSingleShot(true);
    m_CaptureTimer.setInterval(CAPTURE_RETRY_DELAY);
    connect(&m_CaptureTimer, &QTimer::timeout, [&]()
    {
        if (m_CaptureTimeoutCounter++ > 3)
        {
            appendLogText(i18n("Capture timed out."));
            abort();
        }
        else
        {
            ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
            if (targetChip->isCapturing())
            {
                appendLogText(i18n("Capturing still running, Retrying in %1 seconds...", m_CaptureTimer.interval() / 500));
                targetChip->abortExposure();
                m_CaptureTimer.start( m_CaptureTimer.interval() * 2);
            }
            else
                captureAndSolve();
        }
    });

    m_AlignTimer.setSingleShot(true);
    m_AlignTimer.setInterval(Options::astrometryTimeout() * 1000);
    connect(&m_AlignTimer, &QTimer::timeout, this, &Ekos::Align::checkAlignmentTimeout);

    currentGotoMode = static_cast<GotoMode>(Options::solverGotoOption());
    gotoModeButtonGroup->button(currentGotoMode)->setChecked(true);

    KConfigDialog *dialog = new KConfigDialog(this, "alignsettings", Options::self());

#ifdef Q_OS_OSX
    dialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    opsAlign = new OpsAlign(this);
    connect(opsAlign, &OpsAlign::settingsUpdated, this, &Ekos::Align::refreshAlignOptions);
    KPageWidgetItem *page = dialog->addPage(opsAlign, i18n("StellarSolver Options"));
    page->setIcon(QIcon(":/icons/StellarSolverIcon.png"));

    opsPrograms = new OpsPrograms(this);
    page = dialog->addPage(opsPrograms, i18n("External & Online Programs"));
    page->setIcon(QIcon(":/icons/astrometry.svg"));

    opsAstrometry = new OpsAstrometry(this);
    page = dialog->addPage(opsAstrometry, i18n("Scale & Position"));
    page->setIcon(QIcon(":/icons/center_telescope_red.svg"));

    optionsProfileEditor = new StellarSolverProfileEditor(this, Ekos::AlignProfiles, dialog);
    page = dialog->addPage(optionsProfileEditor, i18n("Align Options Profiles Editor"));
    connect(optionsProfileEditor, &StellarSolverProfileEditor::optionsProfilesUpdated, this, [this]()
    {
        if(QFile(savedOptionsProfiles).exists())
            m_StellarSolverProfiles = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
        else
            m_StellarSolverProfiles = getDefaultAlignOptionsProfiles();
        opsAlign->reloadOptionsProfiles();
    });
    page->setIcon(QIcon::fromTheme("configure"));

    connect(opsAlign, &OpsAlign::needToLoadProfile, this, [this, dialog, page](int profile)
    {
        optionsProfileEditor->loadProfile(profile);
        dialog->setCurrentPage(page);
    });

    opsAstrometryIndexFiles = new OpsAstrometryIndexFiles(this);
    page = dialog->addPage(opsAstrometryIndexFiles, i18n("Index Files"));
    page->setIcon(QIcon::fromTheme("map-flat"));

    // opsASTAP = new OpsASTAP(this);
    // page = dialog->addPage(opsASTAP, i18n("ASTAP"));
    // page->setIcon(QIcon(":/icons/astap.ico"));

    appendLogText(i18n("Idle."));

    pi.reset(new QProgressIndicator(this));

    stopLayout->addWidget(pi.get());

    exposureIN->setValue(Options::alignExposure());
    connect(exposureIN, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [&]()
    {
        syncSettings();
    });

    altStage = ALT_INIT;
    azStage  = AZ_INIT;

    rememberSolverWCS = Options::astrometrySolverWCS();
    rememberAutoWCS   = Options::autoWCS();
    //rememberMeridianFlip = Options::executeMeridianFlip();

    solverTypeButtonGroup->setId(localSolverR, SOLVER_LOCAL);
    solverTypeButtonGroup->setId(remoteSolverR, SOLVER_REMOTE);

    localSolverR->setChecked(Options::solverType() == SOLVER_LOCAL);
    remoteSolverR->setChecked(Options::solverType() == SOLVER_REMOTE);
    connect(solverTypeButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this,
            &Align::setSolverType);
    setSolverType(solverTypeButtonGroup->checkedId());
    //        this, &Align::setSolverBackend);
    // solverBackendGroup->setId(astapSolverR, SOLVER_ASTAP);
    //solverBackendGroup->setId(astrometrySolverR, SOLVER_ASTROMETRYNET);

    // JM 2019-11-10: solver type was 3 in previous version (online, offline, remote)
    // But they are now two choices (ASTAP and ASTROMETERY.NET) so we need to accommodate that.
    //  if (Options::solverBackend() > SOLVER_ASTROMETRYNET)
    //  {
    //      Options::setSolverBackend(SOLVER_ASTROMETRYNET);
    //   }
    //
    // solverBackendGroup->button(Options::solverBackend())->setChecked(true);
    // connect(solverBackendGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
    //        this, &Align::setSolverBackend);

    //astrometryTypeCombo->addItem(i18n("Online"));
#ifndef Q_OS_WIN
    // astrometryTypeCombo->addItem(i18n("Offline"));
#endif
    //astrometryTypeCombo->addItem(i18n("Remote"));

    // astrometryTypeCombo->setCurrentIndex(Options::astrometrySolverType());
    // connect(astrometryTypeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
    //         &Ekos::Align::setAstrometrySolverType);

    //setSolverBackend(solverBackendGroup->checkedId());

    // Which telescope info to use for FOV calculations
    FOVScopeCombo->setCurrentIndex(Options::solverScopeType());
    connect(FOVScopeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Align::updateTelescopeType);

    accuracySpin->setValue(Options::solverAccuracyThreshold());
    alignDarkFrameCheck->setChecked(Options::alignDarkFrame());

    delaySpin->setValue(Options::settlingTime());
    connect(delaySpin, &QSpinBox::editingFinished, this, &Ekos::Align::saveSettleTime);

    connect(binningCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Align::setBinningIndex);

    // PAH Connections
    connect(this, &Align::PAHEnabled, [&](bool enabled)
    {
        PAHStartB->setEnabled(enabled);
        directionLabel->setEnabled(enabled);
        PAHDirectionCombo->setEnabled(enabled);
        PAHRotationSpin->setEnabled(enabled);
        PAHSlewRateCombo->setEnabled(enabled);
        PAHManual->setEnabled(enabled);
    });
    connect(PAHStartB, &QPushButton::clicked, this, &Ekos::Align::startPAHProcess);
    // PAH StopB is just a shortcut for the regular stop
    connect(PAHStopB, &QPushButton::clicked, this, &Align::stopPAHProcess);
    connect(PAHCorrectionsNextB, &QPushButton::clicked, this, &Ekos::Align::setPAHCorrectionSelectionComplete);
    connect(PAHRefreshB, &QPushButton::clicked, this, &Ekos::Align::startPAHRefreshProcess);
    connect(PAHDoneB, &QPushButton::clicked, this, &Ekos::Align::setPAHRefreshComplete);
    // done buttons for manual slewing during polar alignment:
    connect(PAHfirstDone, &QPushButton::clicked, this, &Ekos::Align::setPAHSlewDone);
    connect(PAHsecondDone, &QPushButton::clicked, this, &Ekos::Align::setPAHSlewDone);

    //if (solverOptions->text().contains("no-fits2fits"))
    //     appendLogText(i18n(
    //                      "Warning: If using astrometry.net v0.68 or above, remove the --no-fits2fits from the astrometry options."));

    hemisphere = KStarsData::Instance()->geo()->lat()->Degrees() > 0 ? NORTH_HEMISPHERE : SOUTH_HEMISPHERE;

    double accuracyRadius = accuracySpin->value();

    alignPlot->setBackground(QBrush(Qt::black));
    alignPlot->setSelectionTolerance(10);

    alignPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    alignPlot->yAxis->setBasePen(QPen(Qt::white, 1));

    alignPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    alignPlot->yAxis->setTickPen(QPen(Qt::white, 1));

    alignPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    alignPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));

    alignPlot->xAxis->setTickLabelColor(Qt::white);
    alignPlot->yAxis->setTickLabelColor(Qt::white);

    alignPlot->xAxis->setLabelColor(Qt::white);
    alignPlot->yAxis->setLabelColor(Qt::white);

    alignPlot->xAxis->setLabelFont(QFont(font().family(), 10));
    alignPlot->yAxis->setLabelFont(QFont(font().family(), 10));

    alignPlot->xAxis->setLabelPadding(2);
    alignPlot->yAxis->setLabelPadding(2);

    alignPlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    alignPlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    alignPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    alignPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    alignPlot->xAxis->grid()->setZeroLinePen(QPen(Qt::yellow));
    alignPlot->yAxis->grid()->setZeroLinePen(QPen(Qt::yellow));

    alignPlot->xAxis->setLabel(i18n("dRA (arcsec)"));
    alignPlot->yAxis->setLabel(i18n("dDE (arcsec)"));

    alignPlot->xAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    alignPlot->yAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);

    alignPlot->setInteractions(QCP::iRangeZoom);
    alignPlot->setInteraction(QCP::iRangeDrag, true);

    alignPlot->addGraph();
    alignPlot->graph(0)->setLineStyle(QCPGraph::lsNone);
    alignPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::white, 15));

    buildTarget();

    connect(alignPlot, &QCustomPlot::mouseMove, this, &Ekos::Align::handlePointTooltip);
    connect(rightLayout, &QSplitter::splitterMoved, this, &Ekos::Align::handleVerticalPlotSizeChange);
    connect(alignSplitter, &QSplitter::splitterMoved, this, &Ekos::Align::handleHorizontalPlotSizeChange);
    connect(accuracySpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Ekos::Align::buildTarget);

    alignPlot->resize(190, 190);
    alignPlot->replot();

    solutionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    clearAllSolutionsB->setIcon(
        QIcon::fromTheme("application-exit"));
    clearAllSolutionsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    removeSolutionB->setIcon(QIcon::fromTheme("list-remove"));
    removeSolutionB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    exportSolutionsCSV->setIcon(
        QIcon::fromTheme("document-save-as"));
    exportSolutionsCSV->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    autoScaleGraphB->setIcon(QIcon::fromTheme("zoom-fit-best"));
    autoScaleGraphB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.setupUi(&mountModelDialog);
    mountModelDialog.setWindowTitle("Mount Model Tool");
    mountModelDialog.setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    mountModel.alignTable->setColumnWidth(0, 70);
    mountModel.alignTable->setColumnWidth(1, 75);
    mountModel.alignTable->setColumnWidth(2, 130);
    mountModel.alignTable->setColumnWidth(3, 30);

    mountModel.wizardAlignB->setIcon(
        QIcon::fromTheme("tools-wizard"));
    mountModel.wizardAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.clearAllAlignB->setIcon(
        QIcon::fromTheme("application-exit"));
    mountModel.clearAllAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.removeAlignB->setIcon(QIcon::fromTheme("list-remove"));
    mountModel.removeAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.addAlignB->setIcon(QIcon::fromTheme("list-add"));
    mountModel.addAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.findAlignB->setIcon(QIcon::fromTheme("edit-find"));
    mountModel.findAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.alignTable->verticalHeader()->setDragDropOverwriteMode(false);
    mountModel.alignTable->verticalHeader()->setSectionsMovable(true);
    mountModel.alignTable->verticalHeader()->setDragEnabled(true);
    mountModel.alignTable->verticalHeader()->setDragDropMode(QAbstractItemView::InternalMove);
    connect(mountModel.alignTable->verticalHeader(), SIGNAL(sectionMoved(int, int, int)), this,
            SLOT(moveAlignPoint(int, int, int)));

    mountModel.loadAlignB->setIcon(
        QIcon::fromTheme("document-open"));
    mountModel.loadAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.saveAsAlignB->setIcon(
        QIcon::fromTheme("document-save-as"));
    mountModel.saveAsAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.saveAlignB->setIcon(
        QIcon::fromTheme("document-save"));
    mountModel.saveAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.previewB->setIcon(QIcon::fromTheme("kstars_grid"));
    mountModel.previewB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    mountModel.previewB->setCheckable(true);

    mountModel.sortAlignB->setIcon(QIcon::fromTheme("svn-update"));
    mountModel.sortAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.stopAlignB->setIcon(
        QIcon::fromTheme("media-playback-stop"));
    mountModel.stopAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    mountModel.startAlignB->setIcon(
        QIcon::fromTheme("media-playback-start"));
    mountModel.startAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(clearAllSolutionsB, &QPushButton::clicked, this, &Ekos::Align::slotClearAllSolutionPoints);
    connect(removeSolutionB, &QPushButton::clicked, this, &Ekos::Align::slotRemoveSolutionPoint);
    connect(exportSolutionsCSV, &QPushButton::clicked, this, &Ekos::Align::exportSolutionPoints);
    connect(autoScaleGraphB, &QPushButton::clicked, this, &Ekos::Align::slotAutoScaleGraph);
    connect(mountModelB, &QPushButton::clicked, this, &Ekos::Align::slotMountModel);
    connect(solutionTable, &QTableWidget::cellClicked, this, &Ekos::Align::selectSolutionTableRow);

    connect(mountModel.wizardAlignB, &QPushButton::clicked, this, &Ekos::Align::slotWizardAlignmentPoints);
    connect(mountModel.alignTypeBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Align::alignTypeChanged);

    connect(mountModel.starListBox, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Align::slotStarSelected);
    connect(mountModel.greekStarListBox, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
            this,
            &Ekos::Align::slotStarSelected);

    connect(mountModel.loadAlignB, &QPushButton::clicked, this, &Ekos::Align::slotLoadAlignmentPoints);
    connect(mountModel.saveAsAlignB, &QPushButton::clicked, this, &Ekos::Align::slotSaveAsAlignmentPoints);
    connect(mountModel.saveAlignB, &QPushButton::clicked, this, &Ekos::Align::slotSaveAlignmentPoints);
    connect(mountModel.clearAllAlignB, &QPushButton::clicked, this, &Ekos::Align::slotClearAllAlignPoints);
    connect(mountModel.removeAlignB, &QPushButton::clicked, this, &Ekos::Align::slotRemoveAlignPoint);
    connect(mountModel.addAlignB, &QPushButton::clicked, this, &Ekos::Align::slotAddAlignPoint);
    connect(mountModel.findAlignB, &QPushButton::clicked, this, &Ekos::Align::slotFindAlignObject);
    connect(mountModel.sortAlignB, &QPushButton::clicked, this, &Ekos::Align::slotSortAlignmentPoints);

    connect(mountModel.previewB, &QPushButton::clicked, this, &Ekos::Align::togglePreviewAlignPoints);
    connect(mountModel.stopAlignB, &QPushButton::clicked, this, &Ekos::Align::resetAlignmentProcedure);
    connect(mountModel.startAlignB, &QPushButton::clicked, this, &Ekos::Align::startStopAlignmentProcedure);

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    savedOptionsProfiles = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QString("SavedAlignProfiles.ini");
    if(QFile(savedOptionsProfiles).exists())
        m_StellarSolverProfiles = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        m_StellarSolverProfiles = getDefaultAlignOptionsProfiles();
}

Align::~Align()
{
    if (alignWidget->parent() == nullptr)
        toggleAlignWidgetFullScreen();

    // Remove temporary FITS files left before by the solver
    QDir dir(QDir::tempPath());
    dir.setNameFilters(QStringList() << "fits*"
                       << "tmp.*");
    dir.setFilter(QDir::Files);
    for (auto &dirFile : dir.entryList())
        dir.remove(dirFile);
}
void Align::selectSolutionTableRow(int row, int column)
{
    Q_UNUSED(column)

    solutionTable->selectRow(row);
    for (int i = 0; i < alignPlot->itemCount(); i++)
    {
        QCPAbstractItem *abstractItem = alignPlot->item(i);
        if (abstractItem)
        {
            QCPItemText *item = qobject_cast<QCPItemText *>(abstractItem);
            if (item)
            {
                if (i == row)
                {
                    item->setColor(Qt::black);
                    item->setBrush(Qt::yellow);
                }
                else
                {
                    item->setColor(Qt::red);
                    item->setBrush(Qt::white);
                }
            }
        }
    }
    alignPlot->replot();
}

void Align::handleHorizontalPlotSizeChange()
{
    alignPlot->xAxis->setScaleRatio(alignPlot->yAxis, 1.0);
    alignPlot->replot();
}

void Align::handleVerticalPlotSizeChange()
{
    alignPlot->yAxis->setScaleRatio(alignPlot->xAxis, 1.0);
    alignPlot->replot();
}

void Align::resizeEvent(QResizeEvent *event)
{
    if (event->oldSize().width() != -1)
    {
        if (event->oldSize().width() != size().width())
            handleHorizontalPlotSizeChange();
        else if (event->oldSize().height() != size().height())
            handleVerticalPlotSizeChange();
    }
    else
    {
        QTimer::singleShot(10, this, &Ekos::Align::handleHorizontalPlotSizeChange);
    }
}

void Align::handlePointTooltip(QMouseEvent *event)
{
    QCPAbstractItem *item = alignPlot->itemAt(event->localPos());
    if (item)
    {
        QCPItemText *label = qobject_cast<QCPItemText *>(item);
        if (label)
        {
            QString labelText = label->text();
            int point         = labelText.toInt() - 1;

            if (point < 0)
                return;
            QToolTip::showText(event->globalPos(),
                               i18n("<table>"
                                    "<tr>"
                                    "<th colspan=\"2\">Object %1: %2</th>"
                                    "</tr>"
                                    "<tr>"
                                    "<td>RA:</td><td>%3</td>"
                                    "</tr>"
                                    "<tr>"
                                    "<td>DE:</td><td>%4</td>"
                                    "</tr>"
                                    "<tr>"
                                    "<td>dRA:</td><td>%5</td>"
                                    "</tr>"
                                    "<tr>"
                                    "<td>dDE:</td><td>%6</td>"
                                    "</tr>"
                                    "</table>",
                                    point + 1,
                                    solutionTable->item(point, 2)->text(),
                                    solutionTable->item(point, 0)->text(),
                                    solutionTable->item(point, 1)->text(),
                                    solutionTable->item(point, 4)->text(),
                                    solutionTable->item(point, 5)->text()),
                               alignPlot, alignPlot->rect());
        }
    }
}

void Align::buildTarget()
{
    double accuracyRadius = accuracySpin->value();
    if (centralTarget)
    {
        concentricRings->data()->clear();
        redTarget->data()->clear();
        yellowTarget->data()->clear();
        centralTarget->data()->clear();
    }
    else
    {
        concentricRings = new QCPCurve(alignPlot->xAxis, alignPlot->yAxis);
        redTarget       = new QCPCurve(alignPlot->xAxis, alignPlot->yAxis);
        yellowTarget    = new QCPCurve(alignPlot->xAxis, alignPlot->yAxis);
        centralTarget   = new QCPCurve(alignPlot->xAxis, alignPlot->yAxis);
    }
    const int pointCount = 200;
    QVector<QCPCurveData> circleRings(
        pointCount * (5)); //Have to multiply by the number of rings, Rings at : 25%, 50%, 75%, 125%, 175%
    QVector<QCPCurveData> circleCentral(pointCount);
    QVector<QCPCurveData> circleYellow(pointCount);
    QVector<QCPCurveData> circleRed(pointCount);

    int circleRingPt = 0;
    for (int i = 0; i < pointCount; i++)
    {
        double theta = i / static_cast<double>(pointCount) * 2 * M_PI;

        for (double ring = 1; ring < 8; ring++)
        {
            if (ring != 4 && ring != 6)
            {
                if (i % (9 - static_cast<int>(ring)) == 0) //This causes fewer points to draw on the inner circles.
                {
                    circleRings[circleRingPt] = QCPCurveData(circleRingPt, accuracyRadius * ring * 0.25 * qCos(theta),
                                                accuracyRadius * ring * 0.25 * qSin(theta));
                    circleRingPt++;
                }
            }
        }

        circleCentral[i] = QCPCurveData(i, accuracyRadius * qCos(theta), accuracyRadius * qSin(theta));
        circleYellow[i]  = QCPCurveData(i, accuracyRadius * 1.5 * qCos(theta), accuracyRadius * 1.5 * qSin(theta));
        circleRed[i]     = QCPCurveData(i, accuracyRadius * 2 * qCos(theta), accuracyRadius * 2 * qSin(theta));
    }

    concentricRings->setLineStyle(QCPCurve::lsNone);
    concentricRings->setScatterSkip(0);
    concentricRings->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(255, 255, 255, 150), 1));

    concentricRings->data()->set(circleRings, true);
    redTarget->data()->set(circleRed, true);
    yellowTarget->data()->set(circleYellow, true);
    centralTarget->data()->set(circleCentral, true);

    concentricRings->setPen(QPen(Qt::white));
    redTarget->setPen(QPen(Qt::red));
    yellowTarget->setPen(QPen(Qt::yellow));
    centralTarget->setPen(QPen(Qt::green));

    concentricRings->setBrush(Qt::NoBrush);
    redTarget->setBrush(QBrush(QColor(255, 0, 0, 50)));
    yellowTarget->setBrush(
        QBrush(QColor(0, 255, 0, 50))); //Note this is actually yellow.  It is green on top of red with equal opacity.
    centralTarget->setBrush(QBrush(QColor(0, 255, 0, 50)));

    if (alignPlot->size().width() > 0)
        alignPlot->replot();
}

void Align::slotAutoScaleGraph()
{
    double accuracyRadius = accuracySpin->value();
    alignPlot->xAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    alignPlot->yAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);

    alignPlot->xAxis->setScaleRatio(alignPlot->yAxis, 1.0);

    alignPlot->replot();
}

void Align::slotWizardAlignmentPoints()
{
    int points = mountModel.alignPtNum->value();
    if (points <
            2)      //The minimum is 2 because the wizard calculations require the calculation of an angle between points.
        return; //It should not be less than 2 because the minimum in the spin box is 2.

    int minAlt       = mountModel.minAltBox->value();
    KStarsData *data = KStarsData::Instance();
    GeoLocation *geo = data->geo();
    double lat       = geo->lat()->Degrees();

    if (mountModel.alignTypeBox->currentIndex() == OBJECT_FIXED_DEC)
    {
        double decAngle = mountModel.alignDec->value();
        //Dec that never rises.
        if (lat > 0)
        {
            if (decAngle < lat - 90 + minAlt) //Min altitude possible at minAlt deg above horizon
            {
                KSNotification::sorry(i18n("DEC is below the altitude limit"));
                return;
            }
        }
        else
        {
            if (decAngle > lat + 90 - minAlt) //Max altitude possible at minAlt deg above horizon
            {
                KSNotification::sorry(i18n("DEC is below the altitude limit"));
                return;
            }
        }
    }

    //If there are less than 6 points, keep them all in the same DEC,
    //any more, set the num per row to be the sqrt of the points to evenly distribute in RA and DEC
    int numRAperDEC = 5;
    if (points > 5)
        numRAperDEC = qSqrt(points);

    //These calculations rely on modulus and int division counting beginning at 0, but the #s start at 1.
    int decPoints       = (points - 1) / numRAperDEC + 1;
    int lastSetRAPoints = (points - 1) % numRAperDEC + 1;

    double decIncrement = -1;
    double initDEC      = -1;
    SkyPoint spTest;

    if (mountModel.alignTypeBox->currentIndex() == OBJECT_FIXED_DEC)
    {
        decPoints    = 1;
        initDEC      = mountModel.alignDec->value();
        decIncrement = 0;
    }
    else if (decPoints == 1)
    {
        decIncrement = 0;
        spTest.setAlt(
            minAlt); //The goal here is to get the point exactly West at the minAlt so that we can use that DEC
        spTest.setAz(270);
        spTest.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        initDEC = spTest.dec().Degrees();
    }
    else
    {
        spTest.setAlt(
            minAlt +
            10); //We don't want to be right at the minAlt because there would be only 1 point on the dec circle above the alt.
        spTest.setAz(180);
        spTest.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        initDEC = spTest.dec().Degrees();
        if (lat > 0)
            decIncrement = (80 - initDEC) / (decPoints); //Don't quite want to reach NCP
        else
            decIncrement = (initDEC - 80) / (decPoints); //Don't quite want to reach SCP
    }

    for (int d = 0; d < decPoints; d++)
    {
        double initRA      = -1;
        double raPoints    = -1;
        double raIncrement = -1;
        double dec;

        if (lat > 0)
            dec = initDEC + d * decIncrement;
        else
            dec = initDEC - d * decIncrement;

        if (mountModel.alignTypeBox->currentIndex() == OBJECT_FIXED_DEC)
        {
            raPoints = points;
        }
        else if (d == decPoints - 1)
        {
            raPoints = lastSetRAPoints;
        }
        else
        {
            raPoints = numRAperDEC;
        }

        //This computes both the initRA and the raIncrement.
        calculateAngleForRALine(raIncrement, initRA, dec, lat, raPoints, minAlt);

        if (raIncrement == -1 || decIncrement == -1)
        {
            KSNotification::sorry(i18n("Point calculation error."));
            return;
        }

        for (int i = 0; i < raPoints; i++)
        {
            double ra = initRA + i * raIncrement;

            const SkyObject *original = getWizardAlignObject(ra, dec);

            QString ra_report, dec_report, name;

            if (original)
            {
                SkyObject *o = original->clone();
                o->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);
                getFormattedCoords(o->ra0().Hours(), o->dec0().Degrees(), ra_report, dec_report);
                name = o->longname();
            }
            else
            {
                getFormattedCoords(dms(ra).Hours(), dec, ra_report, dec_report);
                name = i18n("Sky Point");
            }

            int currentRow = mountModel.alignTable->rowCount();
            mountModel.alignTable->insertRow(currentRow);

            QTableWidgetItem *RAReport = new QTableWidgetItem();
            RAReport->setText(ra_report);
            RAReport->setTextAlignment(Qt::AlignHCenter);
            mountModel.alignTable->setItem(currentRow, 0, RAReport);

            QTableWidgetItem *DECReport = new QTableWidgetItem();
            DECReport->setText(dec_report);
            DECReport->setTextAlignment(Qt::AlignHCenter);
            mountModel.alignTable->setItem(currentRow, 1, DECReport);

            QTableWidgetItem *ObjNameReport = new QTableWidgetItem();
            ObjNameReport->setText(name);
            ObjNameReport->setTextAlignment(Qt::AlignHCenter);
            mountModel.alignTable->setItem(currentRow, 2, ObjNameReport);

            QTableWidgetItem *disabledBox = new QTableWidgetItem();
            disabledBox->setFlags(Qt::ItemIsSelectable);
            mountModel.alignTable->setItem(currentRow, 3, disabledBox);
        }
    }
    if (previewShowing)
        updatePreviewAlignPoints();
}

void Align::calculateAngleForRALine(double &raIncrement, double &initRA, double initDEC, double lat, double raPoints,
                                    double minAlt)
{
    SkyPoint spEast;
    SkyPoint spWest;

    //Circumpolar dec
    if (fabs(initDEC) > (90 - fabs(lat) + minAlt))
    {
        if (raPoints > 1)
            raIncrement = 360 / (raPoints - 1);
        else
            raIncrement = 0;
        initRA = 0;
    }
    else
    {
        dms AZEast, AZWest;
        calculateAZPointsForDEC(dms(initDEC), dms(minAlt), AZEast, AZWest);

        spEast.setAlt(minAlt);
        spEast.setAz(AZEast.Degrees());
        spEast.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

        spWest.setAlt(minAlt);
        spWest.setAz(AZWest.Degrees());
        spWest.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

        dms angleSep = spEast.ra().deltaAngle(spWest.ra());

        initRA = spWest.ra().Degrees();
        if (raPoints > 1)
            raIncrement = fabs(angleSep.Degrees() / (raPoints - 1));
        else
            raIncrement = 0;
    }
}

void Align::calculateAZPointsForDEC(dms dec, dms alt, dms &AZEast, dms &AZWest)
{
    KStarsData *data = KStarsData::Instance();
    GeoLocation *geo = data->geo();
    double AZRad;

    double sindec, cosdec, sinlat, coslat;
    double sinAlt, cosAlt;

    geo->lat()->SinCos(sinlat, coslat);
    dec.SinCos(sindec, cosdec);
    alt.SinCos(sinAlt, cosAlt);

    double arg = (sindec - sinlat * sinAlt) / (coslat * cosAlt);
    AZRad      = acos(arg);
    AZEast.setRadians(AZRad);
    AZWest.setRadians(2.0 * dms::PI - AZRad);
}

const SkyObject *Align::getWizardAlignObject(double ra, double dec)
{
    double maxSearch = 5.0;
    switch (mountModel.alignTypeBox->currentIndex())
    {
        case OBJECT_ANY_OBJECT:
            return KStarsData::Instance()->skyComposite()->objectNearest(new SkyPoint(dms(ra), dms(dec)), maxSearch);
        case OBJECT_FIXED_DEC:
        case OBJECT_FIXED_GRID:
            return nullptr;

        case OBJECT_ANY_STAR:
            return KStarsData::Instance()->skyComposite()->starNearest(new SkyPoint(dms(ra), dms(dec)), maxSearch);
    }

    //If they want named stars, then try to search for and return the closest Align Star to the requested location

    dms bestDiff = dms(360);
    double index = -1;
    for (int i = 0; i < alignStars.size(); i++)
    {
        const StarObject *star = alignStars.value(i);
        if (star)
        {
            if (star->hasName())
            {
                SkyPoint thisPt(ra / 15.0, dec);
                dms thisDiff = thisPt.angularDistanceTo(star);
                if (thisDiff.Degrees() < bestDiff.Degrees())
                {
                    index    = i;
                    bestDiff = thisDiff;
                }
            }
        }
    }
    if (index == -1)
        return KStarsData::Instance()->skyComposite()->starNearest(new SkyPoint(dms(ra), dms(dec)), maxSearch);
    return alignStars.value(index);
}

void Align::alignTypeChanged(int alignType)
{
    if (alignType == OBJECT_FIXED_DEC)
        mountModel.alignDec->setEnabled(true);
    else
        mountModel.alignDec->setEnabled(false);
}

void Align::slotStarSelected(const QString selectedStar)
{
    for (int i = 0; i < alignStars.size(); i++)
    {
        const StarObject *star = alignStars.value(i);
        if (star)
        {
            if (star->name() == selectedStar || star->gname().simplified() == selectedStar)
            {
                int currentRow = mountModel.alignTable->rowCount();
                mountModel.alignTable->insertRow(currentRow);

                QString ra_report, dec_report;
                getFormattedCoords(star->ra0().Hours(), star->dec0().Degrees(), ra_report, dec_report);

                QTableWidgetItem *RAReport = new QTableWidgetItem();
                RAReport->setText(ra_report);
                RAReport->setTextAlignment(Qt::AlignHCenter);
                mountModel.alignTable->setItem(currentRow, 0, RAReport);

                QTableWidgetItem *DECReport = new QTableWidgetItem();
                DECReport->setText(dec_report);
                DECReport->setTextAlignment(Qt::AlignHCenter);
                mountModel.alignTable->setItem(currentRow, 1, DECReport);

                QTableWidgetItem *ObjNameReport = new QTableWidgetItem();
                ObjNameReport->setText(star->longname());
                ObjNameReport->setTextAlignment(Qt::AlignHCenter);
                mountModel.alignTable->setItem(currentRow, 2, ObjNameReport);

                QTableWidgetItem *disabledBox = new QTableWidgetItem();
                disabledBox->setFlags(Qt::ItemIsSelectable);
                mountModel.alignTable->setItem(currentRow, 3, disabledBox);

                mountModel.starListBox->setCurrentIndex(0);
                mountModel.greekStarListBox->setCurrentIndex(0);
                return;
            }
        }
    }
    if (previewShowing)
        updatePreviewAlignPoints();
}

void Align::generateAlignStarList()
{
    alignStars.clear();
    mountModel.starListBox->clear();
    mountModel.greekStarListBox->clear();

    KStarsData *data = KStarsData::Instance();
    QVector<QPair<QString, const SkyObject *>> listStars;
    listStars.append(data->skyComposite()->objectLists(SkyObject::STAR));
    for (int i = 0; i < listStars.size(); i++)
    {
        QPair<QString, const SkyObject *> pair = listStars.value(i);
        const StarObject *star                 = dynamic_cast<const StarObject *>(pair.second);
        if (star)
        {
            StarObject *alignStar = star->clone();
            alignStar->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);
            alignStars.append(alignStar);
        }
    }

    QStringList boxNames;
    QStringList greekBoxNames;

    for (int i = 0; i < alignStars.size(); i++)
    {
        const StarObject *star = alignStars.value(i);
        if (star)
        {
            if (!isVisible(star))
            {
                alignStars.remove(i);
                i--;
            }
            else
            {
                if (star->hasLatinName())
                    boxNames << star->name();
                else
                {
                    if (!star->gname().isEmpty())
                        greekBoxNames << star->gname().simplified();
                }
            }
        }
    }

    boxNames.sort(Qt::CaseInsensitive);
    boxNames.removeDuplicates();
    greekBoxNames.removeDuplicates();
    std::sort(greekBoxNames.begin(), greekBoxNames.end(), [](const QString & a, const QString & b)
    {
        QStringList aParts = a.split(' ');
        QStringList bParts = b.split(' ');
        if (aParts.length() < 2 || bParts.length() < 2)
            return a < b; //This should not happen, they should all have 2 words in the string.
        if (aParts[1] == bParts[1])
        {
            return aParts[0] < bParts[0]; //This compares the greek letter when the constellation is the same
        }
        else
            return aParts[1] < bParts[1]; //This compares the constellation names
    });

    mountModel.starListBox->addItem("Select one:");
    mountModel.greekStarListBox->addItem("Select one:");
    for (int i = 0; i < boxNames.size(); i++)
        mountModel.starListBox->addItem(boxNames.at(i));
    for (int i = 0; i < greekBoxNames.size(); i++)
        mountModel.greekStarListBox->addItem(greekBoxNames.at(i));
}

bool Align::isVisible(const SkyObject *so)
{
    return (getAltitude(so) > 30);
}

double Align::getAltitude(const SkyObject *so)
{
    KStarsData *data  = KStarsData::Instance();
    SkyPoint sp       = so->recomputeCoords(data->ut(), data->geo());

    //check altitude of object at this time.
    sp.EquatorialToHorizontal(data->lst(), data->geo()->lat());

    return sp.alt().Degrees();
}

void Align::togglePreviewAlignPoints()
{
    previewShowing = !previewShowing;
    mountModel.previewB->setChecked(previewShowing);
    updatePreviewAlignPoints();
}

void Align::updatePreviewAlignPoints()
{
    FlagComponent *flags = KStarsData::Instance()->skyComposite()->flags();
    for (int i = 0; i < flags->size(); i++)
    {
        if (flags->label(i).startsWith(QLatin1String("Align")))
        {
            flags->remove(i);
            i--;
        }
    }
    if (previewShowing)
    {
        for (int i = 0; i < mountModel.alignTable->rowCount(); i++)
        {
            QTableWidgetItem *raCell      = mountModel.alignTable->item(i, 0);
            QTableWidgetItem *deCell      = mountModel.alignTable->item(i, 1);
            QTableWidgetItem *objNameCell = mountModel.alignTable->item(i, 2);

            if (raCell && deCell && objNameCell)
            {
                QString raString = raCell->text();
                QString deString = deCell->text();
                dms raDMS        = dms::fromString(raString, false);
                dms decDMS       = dms::fromString(deString, true);

                QString objString = objNameCell->text();

                SkyPoint flagPoint(raDMS, decDMS);
                flags->add(flagPoint, "J2000", "Default", "Align " + QString::number(i + 1) + ' ' + objString, "white");
            }
        }
    }
    KStars::Instance()->map()->forceUpdate(true);
}

void Align::slotLoadAlignmentPoints()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(&mountModelDialog, i18n("Open Ekos Alignment List"), alignURLPath,
                   "Ekos AlignmentList (*.eal)");
    if (fileURL.isEmpty())
        return;

    if (fileURL.isValid() == false)
    {
        QString message = i18n("Invalid URL: %1", fileURL.toLocalFile());
        KSNotification::sorry(message, i18n("Invalid URL"));
        return;
    }

    alignURLPath = QUrl(fileURL.url(QUrl::RemoveFilename));

    loadAlignmentPoints(fileURL.toLocalFile());
    if (previewShowing)
        updatePreviewAlignPoints();
}

bool Align::loadAlignmentPoints(const QString &fileURL)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    mountModel.alignTable->setRowCount(0);

    LilXML *xmlParser = newLilXML();

    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    char c;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            double sqVersion = atof(findXMLAttValu(root, "version"));
            if (sqVersion < AL_FORMAT_VERSION)
            {
                appendLogText(i18n("Deprecated sequence file format version %1. Please construct a new sequence file.",
                                   sqVersion));
                return false;
            }

            XMLEle *ep    = nullptr;
            XMLEle *subEP = nullptr;

            int currentRow = 0;

            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                if (!strcmp(tagXMLEle(ep), "AlignmentPoint"))
                {
                    mountModel.alignTable->insertRow(currentRow);

                    subEP = findXMLEle(ep, "RA");
                    if (subEP)
                    {
                        QTableWidgetItem *RAReport = new QTableWidgetItem();
                        RAReport->setText(pcdataXMLEle(subEP));
                        RAReport->setTextAlignment(Qt::AlignHCenter);
                        mountModel.alignTable->setItem(currentRow, 0, RAReport);
                    }
                    else
                        return false;
                    subEP = findXMLEle(ep, "DE");
                    if (subEP)
                    {
                        QTableWidgetItem *DEReport = new QTableWidgetItem();
                        DEReport->setText(pcdataXMLEle(subEP));
                        DEReport->setTextAlignment(Qt::AlignHCenter);
                        mountModel.alignTable->setItem(currentRow, 1, DEReport);
                    }
                    else
                        return false;
                    subEP = findXMLEle(ep, "NAME");
                    if (subEP)
                    {
                        QTableWidgetItem *ObjReport = new QTableWidgetItem();
                        ObjReport->setText(pcdataXMLEle(subEP));
                        ObjReport->setTextAlignment(Qt::AlignHCenter);
                        mountModel.alignTable->setItem(currentRow, 2, ObjReport);
                    }
                    else
                        return false;
                }
                currentRow++;
            }
            return true;
        }
    }
    return false;
}

void Align::slotSaveAsAlignmentPoints()
{
    alignURL.clear();
    slotSaveAlignmentPoints();
}
void Align::slotSaveAlignmentPoints()
{
    QUrl backupCurrent = alignURL;

    if (alignURL.toLocalFile().startsWith(QLatin1String("/tmp/")) || alignURL.toLocalFile().contains("/Temp"))
        alignURL.clear();

    if (alignURL.isEmpty())
    {
        alignURL = QFileDialog::getSaveFileUrl(&mountModelDialog, i18n("Save Ekos Alignment List"), alignURLPath,
                                               "Ekos Alignment List (*.eal)");
        // if user presses cancel
        if (alignURL.isEmpty())
        {
            alignURL = backupCurrent;
            return;
        }

        alignURLPath = QUrl(alignURL.url(QUrl::RemoveFilename));

        if (alignURL.toLocalFile().endsWith(QLatin1String(".eal")) == false)
            alignURL.setPath(alignURL.toLocalFile() + ".eal");

        if (QFile::exists(alignURL.toLocalFile()))
        {
            int r = KMessageBox::warningContinueCancel(nullptr,
                    i18n("A file named \"%1\" already exists. "
                         "Overwrite it?",
                         alignURL.fileName()),
                    i18n("Overwrite File?"), KStandardGuiItem::overwrite());
            if (r == KMessageBox::Cancel)
                return;
        }
    }

    if (alignURL.isValid())
    {
        if ((saveAlignmentPoints(alignURL.toLocalFile())) == false)
        {
            KSNotification::error(i18n("Failed to save alignment list"), i18n("Save"));
            return;
        }
    }
    else
    {
        QString message = i18n("Invalid URL: %1", alignURL.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
    }
}

bool Align::saveAlignmentPoints(const QString &path)
{
    QFile file;
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    QTextStream outstream(&file);

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    outstream << "<AlignmentList version='" << AL_FORMAT_VERSION << "'>" << endl;

    for (int i = 0; i < mountModel.alignTable->rowCount(); i++)
    {
        QTableWidgetItem *raCell      = mountModel.alignTable->item(i, 0);
        QTableWidgetItem *deCell      = mountModel.alignTable->item(i, 1);
        QTableWidgetItem *objNameCell = mountModel.alignTable->item(i, 2);

        if (!raCell || !deCell || !objNameCell)
            return false;
        QString raString  = raCell->text();
        QString deString  = deCell->text();
        QString objString = objNameCell->text();

        outstream << "<AlignmentPoint>" << endl;
        outstream << "<RA>" << raString << "</RA>" << endl;
        outstream << "<DE>" << deString << "</DE>" << endl;
        outstream << "<NAME>" << objString << "</NAME>" << endl;
        outstream << "</AlignmentPoint>" << endl;
    }
    outstream << "</AlignmentList>" << endl;
    appendLogText(i18n("Alignment List saved to %1", path));
    file.close();
    return true;
}

void Align::slotSortAlignmentPoints()
{
    int firstAlignmentPt = findClosestAlignmentPointToTelescope();
    if (firstAlignmentPt != -1)
    {
        swapAlignPoints(firstAlignmentPt, 0);
    }

    for (int i = 0; i < mountModel.alignTable->rowCount() - 1; i++)
    {
        int nextAlignmentPoint = findNextAlignmentPointAfter(i);
        if (nextAlignmentPoint != -1)
        {
            swapAlignPoints(nextAlignmentPoint, i + 1);
        }
    }
    if (previewShowing)
        updatePreviewAlignPoints();
}

int Align::findClosestAlignmentPointToTelescope()
{
    dms bestDiff = dms(360);
    double index = -1;

    for (int i = 0; i < mountModel.alignTable->rowCount(); i++)
    {
        QTableWidgetItem *raCell = mountModel.alignTable->item(i, 0);
        QTableWidgetItem *deCell = mountModel.alignTable->item(i, 1);

        if (raCell && deCell)
        {
            dms raDMS = dms::fromString(raCell->text(), false);
            dms deDMS = dms::fromString(deCell->text(), true);

            dms thisDiff = telescopeCoord.angularDistanceTo(new SkyPoint(raDMS, deDMS));
            if (thisDiff.Degrees() < bestDiff.Degrees())
            {
                index    = i;
                bestDiff = thisDiff;
            }
        }
    }
    return index;
}

int Align::findNextAlignmentPointAfter(int currentSpot)
{
    QTableWidgetItem *currentRACell = mountModel.alignTable->item(currentSpot, 0);
    QTableWidgetItem *currentDECell = mountModel.alignTable->item(currentSpot, 1);

    if (currentRACell && currentDECell)
    {
        dms thisRADMS = dms::fromString(currentRACell->text(), false);
        dms thisDEDMS = dms::fromString(currentDECell->text(), true);

        SkyPoint thisPt(thisRADMS, thisDEDMS);

        dms bestDiff = dms(360);
        double index = -1;

        for (int i = currentSpot + 1; i < mountModel.alignTable->rowCount(); i++)
        {
            QTableWidgetItem *raCell = mountModel.alignTable->item(i, 0);
            QTableWidgetItem *deCell = mountModel.alignTable->item(i, 1);

            if (raCell && deCell)
            {
                dms raDMS = dms::fromString(raCell->text(), false);
                dms deDMS = dms::fromString(deCell->text(), true);
                SkyPoint point(raDMS, deDMS);
                dms thisDiff = thisPt.angularDistanceTo(&point);

                if (thisDiff.Degrees() < bestDiff.Degrees())
                {
                    index    = i;
                    bestDiff = thisDiff;
                }
            }
        }
        return index;
    }
    else
        return -1;
}

void Align::exportSolutionPoints()
{
    if (solutionTable->rowCount() == 0)
        return;

    QUrl exportFile = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Export Solution Points"), alignURLPath,
                      "CSV File (*.csv)");
    if (exportFile.isEmpty()) // if user presses cancel
        return;
    if (exportFile.toLocalFile().endsWith(QLatin1String(".csv")) == false)
        exportFile.setPath(exportFile.toLocalFile() + ".csv");

    QString path = exportFile.toLocalFile();

    if (QFile::exists(path))
    {
        int r = KMessageBox::warningContinueCancel(nullptr,
                i18n("A file named \"%1\" already exists. "
                     "Overwrite it?",
                     exportFile.fileName()),
                i18n("Overwrite File?"), KStandardGuiItem::overwrite());
        if (r == KMessageBox::Cancel)
            return;
    }

    if (!exportFile.isValid())
    {
        QString message = i18n("Invalid URL: %1", exportFile.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
        return;
    }

    QFile file;
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return;
    }

    QTextStream outstream(&file);

    QString epoch = QString::number(KStarsDateTime::currentDateTime().epoch());

    outstream << "RA (J" << epoch << "),DE (J" << epoch
              << "),RA (degrees),DE (degrees),Name,RA Error (arcsec),DE Error (arcsec)" << endl;

    for (int i = 0; i < solutionTable->rowCount(); i++)
    {
        QTableWidgetItem *raCell      = solutionTable->item(i, 0);
        QTableWidgetItem *deCell      = solutionTable->item(i, 1);
        QTableWidgetItem *objNameCell = solutionTable->item(i, 2);
        QTableWidgetItem *raErrorCell = solutionTable->item(i, 4);
        QTableWidgetItem *deErrorCell = solutionTable->item(i, 5);

        if (!raCell || !deCell || !objNameCell || !raErrorCell || !deErrorCell)
        {
            KSNotification::sorry(i18n("Error in table structure."));
            return;
        }
        dms raDMS = dms::fromString(raCell->text(), false);
        dms deDMS = dms::fromString(deCell->text(), true);
        outstream << raDMS.toHMSString() << ',' << deDMS.toDMSString() << ',' << raDMS.Degrees() << ','
                  << deDMS.Degrees() << ',' << objNameCell->text() << ',' << raErrorCell->text().remove('\"') << ','
                  << deErrorCell->text().remove('\"') << endl;
    }
    appendLogText(i18n("Solution Points Saved as: %1", path));
    file.close();
}

void Align::slotClearAllSolutionPoints()
{
    if (solutionTable->rowCount() == 0)
        return;

    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
    {
        //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
        KSMessageBox::Instance()->disconnect(this);

        solutionTable->setRowCount(0);
        alignPlot->graph(0)->data()->clear();
        alignPlot->clearItems();
        buildTarget();

        slotAutoScaleGraph();

    });

    KSMessageBox::Instance()->questionYesNo(i18n("Are you sure you want to clear all of the solution points?"),
                                            i18n("Clear Solution Points"), 60);
}

void Align::slotClearAllAlignPoints()
{
    if (mountModel.alignTable->rowCount() == 0)
        return;

    if (KMessageBox::questionYesNo(&mountModelDialog, i18n("Are you sure you want to clear all the alignment points?"),
                                   i18n("Clear Align Points")) == KMessageBox::Yes)
        mountModel.alignTable->setRowCount(0);

    if (previewShowing)
        updatePreviewAlignPoints();
}

void Align::slotRemoveSolutionPoint()
{
    QCPAbstractItem *abstractItem = alignPlot->item(solutionTable->currentRow());
    if (abstractItem)
    {
        QCPItemText *item = qobject_cast<QCPItemText *>(abstractItem);
        if (item)
        {
            double point = item->position->key();
            alignPlot->graph(0)->data()->remove(point);
        }
    }
    alignPlot->removeItem(solutionTable->currentRow());
    for (int i = 0; i < alignPlot->itemCount(); i++)
    {
        QCPAbstractItem *abstractItem = alignPlot->item(i);
        if (abstractItem)
        {
            QCPItemText *item = qobject_cast<QCPItemText *>(abstractItem);
            if (item)
                item->setText(QString::number(i + 1));
        }
    }
    solutionTable->removeRow(solutionTable->currentRow());
    alignPlot->replot();
}

void Align::slotRemoveAlignPoint()
{
    mountModel.alignTable->removeRow(mountModel.alignTable->currentRow());
    if (previewShowing)
        updatePreviewAlignPoints();
}

void Align::moveAlignPoint(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    Q_UNUSED(logicalIndex)

    for (int i = 0; i < mountModel.alignTable->columnCount(); i++)
    {
        QTableWidgetItem *oldItem = mountModel.alignTable->takeItem(oldVisualIndex, i);
        QTableWidgetItem *newItem = mountModel.alignTable->takeItem(newVisualIndex, i);

        mountModel.alignTable->setItem(newVisualIndex, i, oldItem);
        mountModel.alignTable->setItem(oldVisualIndex, i, newItem);
    }
    mountModel.alignTable->verticalHeader()->blockSignals(true);
    mountModel.alignTable->verticalHeader()->moveSection(newVisualIndex, oldVisualIndex);
    mountModel.alignTable->verticalHeader()->blockSignals(false);

    if (previewShowing)
        updatePreviewAlignPoints();
}

void Align::swapAlignPoints(int firstPt, int secondPt)
{
    for (int i = 0; i < mountModel.alignTable->columnCount(); i++)
    {
        QTableWidgetItem *firstPtItem  = mountModel.alignTable->takeItem(firstPt, i);
        QTableWidgetItem *secondPtItem = mountModel.alignTable->takeItem(secondPt, i);

        mountModel.alignTable->setItem(firstPt, i, secondPtItem);
        mountModel.alignTable->setItem(secondPt, i, firstPtItem);
    }
}

void Align::slotMountModel()
{
    generateAlignStarList();

    SkyPoint spWest;
    spWest.setAlt(30);
    spWest.setAz(270);
    spWest.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    mountModel.alignDec->setValue(static_cast<int>(spWest.dec().Degrees()));

    mountModelDialog.show();
}

void Align::slotAddAlignPoint()
{
    int currentRow = mountModel.alignTable->rowCount();
    mountModel.alignTable->insertRow(currentRow);

    QTableWidgetItem *disabledBox = new QTableWidgetItem();
    disabledBox->setFlags(Qt::ItemIsSelectable);
    mountModel.alignTable->setItem(currentRow, 3, disabledBox);
}

void Align::slotFindAlignObject()
{
    KStarsData *data        = KStarsData::Instance();
    if (FindDialog::Instance()->exec() == QDialog::Accepted)
    {
        SkyObject *object = FindDialog::Instance()->targetObject();
        if (object != nullptr)
        {
            SkyObject *o = object->clone();
            o->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);
            int currentRow = mountModel.alignTable->rowCount();
            mountModel.alignTable->insertRow(currentRow);

            QString ra_report, dec_report;
            getFormattedCoords(o->ra0().Hours(), o->dec0().Degrees(), ra_report, dec_report);

            QTableWidgetItem *RAReport = new QTableWidgetItem();
            RAReport->setText(ra_report);
            RAReport->setTextAlignment(Qt::AlignHCenter);
            mountModel.alignTable->setItem(currentRow, 0, RAReport);

            QTableWidgetItem *DECReport = new QTableWidgetItem();
            DECReport->setText(dec_report);
            DECReport->setTextAlignment(Qt::AlignHCenter);
            mountModel.alignTable->setItem(currentRow, 1, DECReport);

            QTableWidgetItem *ObjNameReport = new QTableWidgetItem();
            ObjNameReport->setText(o->longname());
            ObjNameReport->setTextAlignment(Qt::AlignHCenter);
            mountModel.alignTable->setItem(currentRow, 2, ObjNameReport);

            QTableWidgetItem *disabledBox = new QTableWidgetItem();
            disabledBox->setFlags(Qt::ItemIsSelectable);
            mountModel.alignTable->setItem(currentRow, 3, disabledBox);
        }
    }
    if (previewShowing)
        updatePreviewAlignPoints();
}

void Align::resetAlignmentProcedure()
{
    mountModel.alignTable->setCellWidget(currentAlignmentPoint, 3, new QWidget());
    QTableWidgetItem *statusReport = new QTableWidgetItem();
    statusReport->setFlags(Qt::ItemIsSelectable);
    statusReport->setIcon(QIcon(":/icons/AlignWarning.svg"));
    mountModel.alignTable->setItem(currentAlignmentPoint, 3, statusReport);

    appendLogText(i18n("The Mount Model Tool is Reset."));
    mountModel.startAlignB->setIcon(
        QIcon::fromTheme("media-playback-start"));
    mountModelRunning     = false;
    currentAlignmentPoint = 0;
    abort();
}

bool Align::alignmentPointsAreBad()
{
    for (int i = 0; i < mountModel.alignTable->rowCount(); i++)
    {
        QTableWidgetItem *raCell = mountModel.alignTable->item(i, 0);
        if (!raCell)
            return true;
        QString raString = raCell->text();
        if (dms().setFromString(raString, false) == false)
            return true;

        QTableWidgetItem *decCell = mountModel.alignTable->item(i, 1);
        if (!decCell)
            return true;
        QString decString = decCell->text();
        if (dms().setFromString(decString, true) == false)
            return true;
    }
    return false;
}

void Align::startStopAlignmentProcedure()
{
    if (!mountModelRunning)
    {
        if (mountModel.alignTable->rowCount() > 0)
        {
            if (alignmentPointsAreBad())
            {
                KSNotification::error(i18n("Please Check the Alignment Points."));
                return;
            }
            if (currentGotoMode == GOTO_NOTHING)
            {
                int r = KMessageBox::warningContinueCancel(
                            nullptr,
                            i18n("In the Align Module, \"Nothing\" is Selected for the Solver Action.  This means that the "
                                 "mount model tool will not sync/align your mount but will only report the pointing model "
                                 "errors.  Do you wish to continue?"),
                            i18n("Pointing Model Report Only?"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                            "nothing_selected_warning");
                if (r == KMessageBox::Cancel)
                    return;
            }
            if (currentAlignmentPoint == 0)
            {
                for (int row = 0; row < mountModel.alignTable->rowCount(); row++)
                {
                    QTableWidgetItem *statusReport = new QTableWidgetItem();
                    statusReport->setIcon(QIcon());
                    mountModel.alignTable->setItem(row, 3, statusReport);
                }
            }
            mountModel.startAlignB->setIcon(
                QIcon::fromTheme("media-playback-pause"));
            mountModelRunning = true;
            appendLogText(i18n("The Mount Model Tool is Starting."));
            startAlignmentPoint();
        }
    }
    else
    {
        mountModel.startAlignB->setIcon(
            QIcon::fromTheme("media-playback-start"));
        mountModel.alignTable->setCellWidget(currentAlignmentPoint, 3, new QWidget());
        appendLogText(i18n("The Mount Model Tool is Paused."));
        abort();
        mountModelRunning = false;

        QTableWidgetItem *statusReport = new QTableWidgetItem();
        statusReport->setFlags(Qt::ItemIsSelectable);
        statusReport->setIcon(QIcon(":/icons/AlignWarning.svg"));
        mountModel.alignTable->setItem(currentAlignmentPoint, 3, statusReport);
    }
}

void Align::startAlignmentPoint()
{
    if (mountModelRunning && currentAlignmentPoint >= 0 && currentAlignmentPoint < mountModel.alignTable->rowCount())
    {
        QTableWidgetItem *raCell = mountModel.alignTable->item(currentAlignmentPoint, 0);
        QString raString         = raCell->text();
        dms raDMS                = dms::fromString(raString, false);
        double ra                = raDMS.Hours();

        QTableWidgetItem *decCell = mountModel.alignTable->item(currentAlignmentPoint, 1);
        QString decString         = decCell->text();
        dms decDMS                = dms::fromString(decString, true);
        double dec                = decDMS.Degrees();

        QProgressIndicator *alignIndicator = new QProgressIndicator(this);
        mountModel.alignTable->setCellWidget(currentAlignmentPoint, 3, alignIndicator);
        alignIndicator->startAnimation();

        targetCoord.setRA0(ra);
        targetCoord.setDec0(dec);
        targetCoord.updateCoordsNow(KStarsData::Instance()->updateNum());

        Slew();
    }
}

void Align::finishAlignmentPoint(bool solverSucceeded)
{
    if (mountModelRunning && currentAlignmentPoint >= 0 && currentAlignmentPoint < mountModel.alignTable->rowCount())
    {
        mountModel.alignTable->setCellWidget(currentAlignmentPoint, 3, new QWidget());
        QTableWidgetItem *statusReport = new QTableWidgetItem();
        statusReport->setFlags(Qt::ItemIsSelectable);
        if (solverSucceeded)
            statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
        else
            statusReport->setIcon(QIcon(":/icons/AlignFailure.svg"));
        mountModel.alignTable->setItem(currentAlignmentPoint, 3, statusReport);

        currentAlignmentPoint++;

        if (currentAlignmentPoint < mountModel.alignTable->rowCount())
        {
            startAlignmentPoint();
        }
        else
        {
            mountModelRunning = false;
            mountModel.startAlignB->setIcon(
                QIcon::fromTheme("media-playback-start"));
            appendLogText(i18n("The Mount Model Tool is Finished."));
            currentAlignmentPoint = 0;
        }
    }
}

bool Align::isParserOK()
{
    return true; //For now
    Q_ASSERT_X(parser, __FUNCTION__, "Astrometry parser is not valid.");

    bool rc = parser->init();

    if (rc)
    {
        connect(parser, &AstrometryParser::solverFinished, this, &Ekos::Align::solverFinished, Qt::UniqueConnection);
        connect(parser, &AstrometryParser::solverFailed, this, &Ekos::Align::solverFailed, Qt::UniqueConnection);
    }

    return rc;
}

void Align::checkAlignmentTimeout()
{
    if (loadSlewState != IPS_IDLE || ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
        abort();
    else if (loadSlewState == IPS_IDLE)
    {
        appendLogText(i18n("Solver timed out."));
        parser->stopSolver();

        int currentRow = solutionTable->rowCount() - 1;
        solutionTable->setCellWidget(currentRow, 3, new QWidget());
        QTableWidgetItem *statusReport = new QTableWidgetItem();
        statusReport->setIcon(QIcon(":/icons/timedout.svg"));
        statusReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 3, statusReport);

        captureAndSolve();
    }
    // TODO must also account for loadAndSlew. Retain file name
}

void Align::setSolverType(int type)
{
    if (sender() == nullptr && type >= 0 && type <= 1)
    {
        solverTypeButtonGroup->button(type)->setChecked(true);
    }

    Options::setSolverType(type);

    if (type == SOLVER_REMOTE)
    {
        if (remoteParser.get() != nullptr && remoteParserDevice != nullptr)
        {
            parser = remoteParser.get();
            (dynamic_cast<RemoteAstrometryParser *>(parser))->setAstrometryDevice(remoteParserDevice);
            return;
        }

        remoteParser.reset(new Ekos::RemoteAstrometryParser());
        parser = remoteParser.get();
        (dynamic_cast<RemoteAstrometryParser *>(parser))->setAstrometryDevice(remoteParserDevice);
        if (currentCCD)
            (dynamic_cast<RemoteAstrometryParser *>(parser))->setCCD(currentCCD->getDeviceName());

        parser->setAlign(this);
        if (parser->init())
        {
            connect(parser, &AstrometryParser::solverFinished, this, &Ekos::Align::solverFinished, Qt::UniqueConnection);
            connect(parser, &AstrometryParser::solverFailed, this, &Ekos::Align::solverFailed, Qt::UniqueConnection);
        }
        else
            parser->disconnect();
    }
}

/**
void Align::setSolverBackend(int type)
{
    if (sender() == nullptr && type >= 0 && type <= 1)
    {
        solverBackendGroup->button(type)->setChecked(true);
    }

    // Astrometry solver
    if (type == SOLVER_ASTROMETRYNET)
    {
        astrometryTypeCombo->setEnabled(true);
        setAstrometrySolverType(Options::astrometrySolverType());
    }
    // ASTAP solver
    else
    {
        if (!QFile::exists(Options::aSTAPExecutable()))
        {
            // Set to Astrometry for now.
            type = SOLVER_ASTROMETRYNET;
            astrometryTypeCombo->setEnabled(true);
            setAstrometrySolverType(Options::astrometrySolverType());

            KSMessageBox::Instance()->error(
                i18n("No valid ASTAP installation found. Install ASTAP and select the path to ASTAP executable in options."));
        }
        else
        {
            if (astapParser.get() != nullptr)
                parser = astapParser.get();
            else
            {
                astapParser.reset(new Ekos::ASTAPAstrometryParser());
                parser = astapParser.get();
            }

            parser->setAlign(this);
            if (parser->init())
            {
                connect(parser, &AstrometryParser::solverFinished, this, &Ekos::Align::solverFinished, Qt::UniqueConnection);
                connect(parser, &AstrometryParser::solverFailed, this, &Ekos::Align::solverFailed, Qt::UniqueConnection);
            }
            else
                parser->disconnect();

            astrometryTypeCombo->setEnabled(false);
        }
    }

    Options::setSolverBackend(type);

    generateArgs();

}
**/

/**
void Align::setAstrometrySolverType(int type)
{
    if (sender() == nullptr && type >= 0 && type <= 2)
    {
        astrometryTypeCombo->setCurrentIndex(type);
    }

    // For Windows, we only have two items in the combo box (Online & Remote)
    // When Remote is clicked, type = 1 is sent which is SOLVER_OFFLINE
    // We need to change that to SOLVER_REMOTE.
#ifdef Q_OS_WIN
    if (type == SOLVER_OFFLINE)
        type = SOLVER_REMOTE;
#endif

    syncSettings();

    Options::setAstrometrySolverType(type);

    switch (type)
    {
        case SOLVER_ONLINE:
            loadSlewB->setEnabled(true);
            if (onlineParser.get() != nullptr)
            {
                parser = onlineParser.get();
                return;
            }

            onlineParser.reset(new Ekos::OnlineAstrometryParser());
            parser = onlineParser.get();
            break;

        case SOLVER_OFFLINE:
            loadSlewB->setEnabled(true);
            if (offlineParser.get() != nullptr)
            {
                parser = offlineParser.get();
                return;
            }

            offlineParser.reset(new Ekos::OfflineAstrometryParser());
            parser = offlineParser.get();
            break;

        case SOLVER_REMOTE:
            loadSlewB->setEnabled(true);
            if (remoteParser.get() != nullptr && remoteParserDevice != nullptr)
            {
                parser = remoteParser.get();
                (dynamic_cast<RemoteAstrometryParser *>(parser))->setAstrometryDevice(remoteParserDevice);
                return;
            }

            remoteParser.reset(new Ekos::RemoteAstrometryParser());
            parser = remoteParser.get();
            (dynamic_cast<RemoteAstrometryParser *>(parser))->setAstrometryDevice(remoteParserDevice);
            if (currentCCD)
                (dynamic_cast<RemoteAstrometryParser *>(parser))->setCCD(currentCCD->getDeviceName());
            break;
    }

    parser->setAlign(this);
    if (parser->init())
    {
        connect(parser, &AstrometryParser::solverFinished, this, &Ekos::Align::solverFinished, Qt::UniqueConnection);
        connect(parser, &AstrometryParser::solverFailed, this, &Ekos::Align::solverFailed, Qt::UniqueConnection);
    }
    else
        parser->disconnect();
}
**/
bool Align::setCamera(const QString &device)
{
    for (int i = 0; i < CCDCaptureCombo->count(); i++)
        if (device == CCDCaptureCombo->itemText(i))
        {
            CCDCaptureCombo->setCurrentIndex(i);
            checkCCD(i);
            return true;
        }

    return false;
}

QString Align::camera()
{
    if (currentCCD)
        return currentCCD->getDeviceName();

    return QString();
}

void Align::setDefaultCCD(QString ccd)
{
    syncSettings();
    Options::setDefaultAlignCCD(ccd);
}

void Align::checkCCD(int ccdNum)
{
    if (ccdNum == -1 || ccdNum >= CCDs.count())
    {
        ccdNum = CCDCaptureCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    currentCCD = CCDs.at(ccdNum);

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    if (targetChip && targetChip->isCapturing())
        return;

    if (solverTypeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
        (dynamic_cast<RemoteAstrometryParser *>(remoteParser.get()))->setCCD(currentCCD->getDeviceName());

    syncCCDInfo();

    QStringList isoList = targetChip->getISOList();
    ISOCombo->clear();

    if (isoList.isEmpty())
    {
        ISOCombo->setEnabled(false);
    }
    else
    {
        ISOCombo->setEnabled(true);
        ISOCombo->addItems(isoList);
        ISOCombo->setCurrentIndex(targetChip->getISOIndex());
    }

    // Gain Check
    if (currentCCD->hasGain())
    {
        double min, max, step, value;
        currentCCD->getGainMinMaxStep(&min, &max, &step);

        // Allow the possibility of no gain value at all.
        GainSpinSpecialValue = min - step;
        GainSpin->setRange(GainSpinSpecialValue, max);
        GainSpin->setSpecialValueText(i18n("--"));
        GainSpin->setEnabled(true);
        GainSpin->setSingleStep(step);
        currentCCD->getGain(&value);

        // Set the custom gain if we have one
        // otherwise it will not have an effect.
        if (TargetCustomGainValue > 0)
            GainSpin->setValue(TargetCustomGainValue);
        else
            GainSpin->setValue(GainSpinSpecialValue);

        GainSpin->setReadOnly(currentCCD->getGainPermission() == IP_RO);

        connect(GainSpin, &QDoubleSpinBox::editingFinished, [this]()
        {
            if (GainSpin->value() > GainSpinSpecialValue)
                TargetCustomGainValue = GainSpin->value();
        });
    }
    else
        GainSpin->setEnabled(false);

    syncTelescopeInfo();
}

void Align::addCCD(ISD::GDInterface *newCCD)
{
    if (CCDs.contains(static_cast<ISD::CCD *>(newCCD)))
    {
        syncCCDInfo();
        return;
    }

    CCDs.append(static_cast<ISD::CCD *>(newCCD));

    CCDCaptureCombo->addItem(newCCD->getDeviceName());

    checkCCD();

    syncSettings();
}

void Align::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = static_cast<ISD::Telescope *>(newTelescope);
    currentTelescope->disconnect(this);

    connect(currentTelescope, &ISD::GDInterface::numberUpdated, this, &Ekos::Align::processNumber, Qt::UniqueConnection);
    connect(currentTelescope, &ISD::GDInterface::switchUpdated, this, &Ekos::Align::processSwitch, Qt::UniqueConnection);
    connect(currentTelescope, &ISD::GDInterface::Disconnected, this, [this]()
    {
        m_isRateSynced = false;
    });


    if (m_isRateSynced == false)
    {
        PAHSlewRateCombo->blockSignals(true);
        PAHSlewRateCombo->clear();
        PAHSlewRateCombo->addItems(currentTelescope->slewRates());
        if (Options::pAHMountSpeedIndex() >= 0)
            PAHSlewRateCombo->setCurrentIndex(Options::pAHMountSpeedIndex());
        else
            PAHSlewRateCombo->setCurrentIndex(currentTelescope->getSlewRate());
        PAHSlewRateCombo->blockSignals(false);

        m_isRateSynced = !currentTelescope->slewRates().empty();
    }

    syncTelescopeInfo();
}

void Align::setDome(ISD::GDInterface *newDome)
{
    currentDome = static_cast<ISD::Dome *>(newDome);
    connect(currentDome, &ISD::GDInterface::switchUpdated, this, &Ekos::Align::processSwitch, Qt::UniqueConnection);
}

void Align::removeDevice(ISD::GDInterface *device)
{
    device->disconnect(this);
    if (currentTelescope && currentTelescope->getDeviceName() == device->getDeviceName())
    {
        currentTelescope = nullptr;
        m_isRateSynced = false;
    }
    else if (currentDome && currentDome->getDeviceName() == device->getDeviceName())
    {
        currentDome = nullptr;
    }
    else if (currentRotator && currentRotator->getDeviceName() == device->getDeviceName())
    {
        currentRotator = nullptr;
    }

    if (CCDs.contains(static_cast<ISD::CCD *>(device)))
    {
        CCDs.removeAll(static_cast<ISD::CCD *>(device));
        CCDCaptureCombo->removeItem(CCDCaptureCombo->findText(device->getDeviceName()));
        CCDCaptureCombo->removeItem(CCDCaptureCombo->findText(device->getDeviceName() + QString(" Guider")));
        if (CCDs.empty())
        {
            currentCCD = nullptr;
            CCDCaptureCombo->setCurrentIndex(-1);
        }
        else
        {
            currentCCD = CCDs[0];
            CCDCaptureCombo->setCurrentIndex(0);
        }

        QTimer::singleShot(1000, this, [this]()
        {
            checkCCD();
        });
        //checkCCD();
    }

    if (Filters.contains(static_cast<ISD::Filter *>(device)))
    {
        Filters.removeAll(static_cast<ISD::Filter *>(device));
        filterManager->removeDevice(device);
        FilterDevicesCombo->removeItem(FilterDevicesCombo->findText(device->getDeviceName()));
        if (Filters.empty())
        {
            currentFilter = nullptr;
            FilterDevicesCombo->setCurrentIndex(-1);
        }
        else
            FilterDevicesCombo->setCurrentIndex(0);

        //checkFilter();
        QTimer::singleShot(1000, this, [this]()
        {
            checkFilter();
        });
    }

}

bool Align::syncTelescopeInfo()
{
    if (currentTelescope == nullptr || currentTelescope->isConnected() == false)
        return false;

    canSync = currentTelescope->canSync();

    if (canSync == false && syncR->isEnabled())
    {
        slewR->setChecked(true);
        appendLogText(i18n("Mount does not support syncing."));
    }

    syncR->setEnabled(canSync);

    INumberVectorProperty *nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "TELESCOPE_APERTURE");

        if (np && np->value > 0)
            primaryAperture = np->value;

        np = IUFindNumber(nvp, "GUIDER_APERTURE");
        if (np && np->value > 0)
            guideAperture = np->value;

        aperture = primaryAperture;

        //if (currentCCD && currentCCD->getTelescopeType() == ISD::CCD::TELESCOPE_GUIDE)
        if (FOVScopeCombo->currentIndex() == ISD::CCD::TELESCOPE_GUIDE)
            aperture = guideAperture;

        np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
        if (np && np->value > 0)
            primaryFL = np->value;

        np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        if (np && np->value > 0)
            guideFL = np->value;

        focal_length = primaryFL;

        //if (currentCCD && currentCCD->getTelescopeType() == ISD::CCD::TELESCOPE_GUIDE)
        if (FOVScopeCombo->currentIndex() == ISD::CCD::TELESCOPE_GUIDE)
            focal_length = guideFL;
    }

    if (focal_length == -1 || aperture == -1)
        return false;

    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
    {
        FOVScopeCombo->setItemData(
            ISD::CCD::TELESCOPE_PRIMARY,
            i18nc("F-Number, Focal Length, Aperture",
                  "<nobr>F<b>%1</b> Focal Length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                  QString::number(primaryFL / primaryAperture, 'f', 1), QString::number(primaryFL, 'f', 2),
                  QString::number(primaryAperture, 'f', 2)),
            Qt::ToolTipRole);
        FOVScopeCombo->setItemData(
            ISD::CCD::TELESCOPE_GUIDE,
            i18nc("F-Number, Focal Length, Aperture",
                  "<nobr>F<b>%1</b> Focal Length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                  QString::number(guideFL / guideAperture, 'f', 1), QString::number(guideFL, 'f', 2),
                  QString::number(guideAperture, 'f', 2)),
            Qt::ToolTipRole);

        calculateFOV();

        //generateArgs();

        return true;
    }

    return false;
}

void Align::setTelescopeInfo(double primaryFocalLength, double primaryAperture, double guideFocalLength,
                             double guideAperture)
{
    if (primaryFocalLength > 0)
        primaryFL = primaryFocalLength;
    if (guideFocalLength > 0)
        guideFL = guideFocalLength;

    if (primaryAperture > 0)
        this->primaryAperture = primaryAperture;
    if (guideAperture > 0)
        this->guideAperture = guideAperture;

    focal_length = primaryFL;

    if (currentCCD && currentCCD->getTelescopeType() == ISD::CCD::TELESCOPE_GUIDE)
        focal_length = guideFL;

    aperture = primaryAperture;

    if (currentCCD && currentCCD->getTelescopeType() == ISD::CCD::TELESCOPE_GUIDE)
        aperture = guideAperture;

    syncTelescopeInfo();
}

void Align::syncCCDInfo()
{
    INumberVectorProperty *nvp = nullptr;

    if (currentCCD == nullptr)
        return;

    if (useGuideHead)
        nvp = currentCCD->getBaseDevice()->getNumber("GUIDER_INFO");
    else
        nvp = currentCCD->getBaseDevice()->getNumber("CCD_INFO");

    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_X");
        if (np && np->value > 0)
            ccd_hor_pixel = ccd_ver_pixel = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np && np->value > 0)
            ccd_ver_pixel = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np && np->value > 0)
            ccd_ver_pixel = np->value;
    }

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    ISwitchVectorProperty *svp = currentCCD->getBaseDevice()->getSwitch("WCS_CONTROL");
    if (svp)
        setWCSEnabled(Options::astrometrySolverWCS());

    targetChip->setImageView(alignView, FITS_ALIGN);

    targetChip->getFrameMinMax(nullptr, nullptr, nullptr, nullptr, nullptr, &ccd_width, nullptr, &ccd_height);
    binningCombo->setEnabled(targetChip->canBin());
    if (targetChip->canBin())
    {
        binningCombo->blockSignals(true);

        int binx = 1, biny = 1;
        targetChip->getMaxBin(&binx, &biny);
        binningCombo->clear();

        for (int i = 0; i < binx; i++)
            binningCombo->addItem(QString("%1x%2").arg(i + 1).arg(i + 1));

        // By default, set to maximum binning since the solver behaves better this way
        // solverBinningIndex is set by default to 4, but as soon as the user changes the binning, it changes
        // to whatever value the user selected.
        if (Options::solverBinningIndex() == 4 && binningCombo->count() <= 4)
        {
            binningCombo->setCurrentIndex(binningCombo->count() - 1);
            Options::setSolverBinningIndex(binningCombo->count() - 1);
        }
        else
            binningCombo->setCurrentIndex(Options::solverBinningIndex());

        binningCombo->blockSignals(false);
    }

    if (ccd_hor_pixel == -1 || ccd_ver_pixel == -1)
        return;

    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
    {
        calculateFOV();
        //generateArgs();
    }
}

void Align::getFOVScale(double &fov_w, double &fov_h, double &fov_scale)
{
    fov_w     = fov_x;
    fov_h     = fov_y;
    fov_scale = fov_pixscale;
}

QList<double> Align::fov()
{
    QList<double> result;

    result << fov_x << fov_y << fov_pixscale;

    return result;
}

QList<double> Align::cameraInfo()
{
    QList<double> result;

    result << ccd_width << ccd_height << ccd_hor_pixel << ccd_ver_pixel;

    return result;
}

QList<double> Align::telescopeInfo()
{
    QList<double> result;

    result << focal_length << aperture;

    return result;
}

void Align::getCalculatedFOVScale(double &fov_w, double &fov_h, double &fov_scale)
{
    // FOV in arcsecs
    fov_w = 206264.8062470963552 * ccd_width * ccd_hor_pixel / 1000.0 / focal_length;
    fov_h = 206264.8062470963552 * ccd_height * ccd_ver_pixel / 1000.0 / focal_length;

    // Pix Scale
    fov_scale = (fov_w * (Options::solverBinningIndex() + 1)) / ccd_width;

    // FOV in arcmins
    fov_w /= 60.0;
    fov_h /= 60.0;
}

void Align::calculateEffectiveFocalLength(double newFOVW)
{
    if (newFOVW < 0 || newFOVW == fov_x)
        return;

    double new_focal_length = ((ccd_width * ccd_hor_pixel / 1000.0) * 206264.8062470963552) / (newFOVW * 60.0);
    double focal_diff = std::fabs(new_focal_length - focal_length);

    if (focal_diff > 1)
    {
        if (FOVScopeCombo->currentIndex() == ISD::CCD::TELESCOPE_PRIMARY)
            primaryEffectiveFL = new_focal_length;
        else
            guideEffectiveFL = new_focal_length;
        appendLogText(i18n("Effective telescope focal length is updated to %1 mm.", focal_length));
    }
}

void Align::calculateFOV()
{
    // Calculate FOV

    // FOV in arcsecs
    fov_x = 206264.8062470963552 * ccd_width * ccd_hor_pixel / 1000.0 / focal_length;
    fov_y = 206264.8062470963552 * ccd_height * ccd_ver_pixel / 1000.0 / focal_length;

    // Pix Scale
    fov_pixscale = (fov_x * (Options::solverBinningIndex() + 1)) / ccd_width;

    // FOV in arcmins
    fov_x /= 60.0;
    fov_y /= 60.0;

    // Put FOV upper limit as 180 degrees
    if (fov_x < 1 || fov_x > 60 * 180 || fov_y < 1 || fov_y > 60 * 180)
    {
        appendLogText(
            i18n("Warning! The calculated field of view is out of bounds. Ensure the telescope focal length and camera pixel size are correct."));
        return;
    }

    double calculated_fov_x = fov_x;
    double calculated_fov_y = fov_y;

    QString calculatedFOV = (QString("%1' x %2'").arg(fov_x, 0, 'f', 1).arg(fov_y, 0, 'f', 1));

    double effectiveFocalLength = FOVScopeCombo->currentIndex() == ISD::CCD::TELESCOPE_PRIMARY ? primaryEffectiveFL :
                                  guideEffectiveFL;

    FocalLengthOut->setText(QString("%1 (%2)").arg(focal_length, 0, 'f', 1).
                            arg(effectiveFocalLength > 0 ? effectiveFocalLength : focal_length, 0, 'f', 1));
    FocalRatioOut->setText(QString("%1 (%2)").arg(focal_length / aperture, 0, 'f', 1).
                           arg(effectiveFocalLength > 0 ? effectiveFocalLength / aperture : focal_length / aperture, 0, 'f', 1));

    if (effectiveFocalLength > 0)
    {
        double focal_diff = std::fabs(effectiveFocalLength  - focal_length);
        if (focal_diff < 5)
            FocalLengthOut->setStyleSheet("color:green");
        else if (focal_diff < 15)
            FocalLengthOut->setStyleSheet("color:yellow");
        else
            FocalLengthOut->setStyleSheet("color:red");
    }

    // JM 2018-04-20 Above calculations are for RAW FOV. Starting from 2.9.5, we are using EFFECTIVE FOV
    // Which is the real FOV as measured from the plate solution. The effective FOVs are stored in the database and are unique
    // per profile/pixel_size/focal_length combinations. It defaults to 0' x 0' and gets updated after the first successful solver is complete.
    getEffectiveFOV();

    if (fov_x == 0)
    {
        //FOVOut->setReadOnly(false);
        FOVOut->setToolTip(
            i18n("<p>Effective field of view size in arcminutes.</p><p>Please capture and solve once to measure the effective FOV or enter the values manually.</p><p>Calculated FOV: %1</p>",
                 calculatedFOV));
        fov_x = calculated_fov_x;
        fov_y = calculated_fov_y;
        m_EffectiveFOVPending = true;
    }
    else
    {
        m_EffectiveFOVPending = false;
        FOVOut->setToolTip(i18n("<p>Effective field of view size in arcminutes.</p>"));
    }

    solverFOV->setSize(fov_x, fov_y);
    sensorFOV->setSize(fov_x, fov_y);
    if (currentCCD)
        sensorFOV->setName(currentCCD->getDeviceName());

    FOVOut->setText(QString("%1' x %2'").arg(fov_x, 0, 'f', 1).arg(fov_y, 0, 'f', 1));

    if (((fov_x + fov_y) / 2.0) > PAH_CUTOFF_FOV)
    {
        if (isPAHReady == false)
        {
            PAHWidgets->setEnabled(true);
            isPAHReady = true;
            emit PAHEnabled(true);
            PAHWidgets->setToolTip(QString());
            FOVDisabledLabel->hide();
        }
    }
    else if (PAHWidgets->isEnabled())
    {
        PAHWidgets->setEnabled(false);
        isPAHReady = false;
        emit PAHEnabled(false);
        PAHWidgets->setToolTip(i18n(
                                   "<p>Polar Alignment Helper tool requires the following:</p><p>1. German Equatorial Mount</p><p>2. FOV &gt;"
                                   " 0.5 degrees</p><p>For small FOVs, use the Legacy Polar Alignment Tool.</p>"));
        FOVDisabledLabel->show();
    }

    if (opsAstrometry->kcfg_AstrometryUseImageScale->isChecked())
    {
        int unitType = opsAstrometry->kcfg_AstrometryImageScaleUnits->currentIndex();

        // Degrees
        if (unitType == 0)
        {
            double fov_low  = qMin(fov_x / 60, fov_y / 60);
            double fov_high = qMax(fov_x / 60, fov_y / 60);
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_low);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_high);

            Options::setAstrometryImageScaleLow(fov_low);
            Options::setAstrometryImageScaleHigh(fov_high);
        }
        // Arcmins
        else if (unitType == 1)
        {
            double fov_low  = qMin(fov_x, fov_y);
            double fov_high = qMax(fov_x, fov_y);
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_low);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_high);

            Options::setAstrometryImageScaleLow(fov_low);
            Options::setAstrometryImageScaleHigh(fov_high);
        }
        // Arcsec per pixel
        else
        {
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_pixscale * 0.9);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_pixscale * 1.1);

            // 10% boundary
            Options::setAstrometryImageScaleLow(fov_pixscale * 0.9);
            Options::setAstrometryImageScaleHigh(fov_pixscale * 1.1);
        }
    }
}

QStringList Align::generateRemoteOptions(const QVariantMap &optionsMap)
{
    QStringList solver_args;

    // -O overwrite
    // -3 Expected RA
    // -4 Expected DEC
    // -5 Radius (deg)
    // -L lower scale of image in arcminutes
    // -H upper scale of image in arcminutes
    // -u aw set scale to be in arcminutes
    // -W solution.wcs name of solution file
    // apog1.jpg name of target file to analyze
    //solve-field -O -3 06:40:51 -4 +09:49:53 -5 1 -L 40 -H 100 -u aw -W solution.wcs apod1.jpg

    // Start with always-used arguments
    solver_args << "-O"
                << "--no-plots";

    // Now go over boolean options

    // noverify
    if (optionsMap.contains("noverify"))
        solver_args << "--no-verify";

    // noresort
    if (optionsMap.contains("resort"))
        solver_args << "--resort";

    // fits2fits
    if (optionsMap.contains("nofits2fits"))
        solver_args << "--no-fits2fits";

    // downsample
    if (optionsMap.contains("downsample"))
        solver_args << "--downsample" << QString::number(optionsMap.value("downsample", 2).toInt());

    // image scale low
    if (optionsMap.contains("scaleL"))
        solver_args << "-L" << QString::number(optionsMap.value("scaleL").toDouble());

    // image scale high
    if (optionsMap.contains("scaleH"))
        solver_args << "-H" << QString::number(optionsMap.value("scaleH").toDouble());

    // image scale units
    if (optionsMap.contains("scaleUnits"))
        solver_args << "-u" << optionsMap.value("scaleUnits").toString();

    // RA
    if (optionsMap.contains("ra"))
        solver_args << "-3" << QString::number(optionsMap.value("ra").toDouble());

    // DE
    if (optionsMap.contains("de"))
        solver_args << "-4" << QString::number(optionsMap.value("de").toDouble());

    // Radius
    if (optionsMap.contains("radius"))
        solver_args << "-5" << QString::number(optionsMap.value("radius").toDouble());

    // Custom
    if (optionsMap.contains("custom"))
        solver_args << optionsMap.value("custom").toString();

    return solver_args;
}

//This will generate the high and low scale of the imager field size based on the stated units.
void Align::generateFOVBounds(double fov_w, QString &fov_low, QString &fov_high, double tolerance)
{
    // This sets the percentage we search outside the lower and upper boundary limits
    // by default, we stretch the limits by 5% (tolerance = 0.05)
    double lower_boundary = 1.0 - tolerance;
    double upper_boundary = 1.0 + tolerance;

    // let's stretch the boundaries by 5%
    //    fov_lower = ((fov_h < fov_v) ? (fov_h * lower_boundary) : (fov_v * lower_boundary));
    //    fov_upper = ((fov_h > fov_v) ? (fov_h * upper_boundary) : (fov_v * upper_boundary));

    // JM 2019-10-20: The bounds consider image width only, not height.
    double fov_lower = fov_w * lower_boundary;
    double fov_upper = fov_w * upper_boundary;

    //No need to do anything if they are aw, since that is the default
    fov_low  = QString::number(fov_lower);
    fov_high = QString::number(fov_upper);
}


QStringList Align::generateRemoteArgs(FITSData *data)
{
    QVariantMap optionsMap;

    // -O overwrite
    // -3 Expected RA
    // -4 Expected DEC
    // -5 Radius (deg)
    // -L lower scale of image in arcminutes
    // -H upper scale of image in arcminutes
    // -u aw set scale to be in arcminutes
    // -W solution.wcs name of solution file
    // apog1.jpg name of target file to analyze
    //solve-field -O -3 06:40:51 -4 +09:49:53 -5 1 -L 40 -H 100 -u aw -W solution.wcs apod1.jpg

    if (Options::astrometryUseNoVerify())
        optionsMap["noverify"] = true;

    if (Options::astrometryUseResort())
        optionsMap["resort"] = true;

    if (Options::astrometryUseNoFITS2FITS())
        optionsMap["nofits2fits"] = true;

    if (data == nullptr)
    {
        if (Options::astrometryUseDownsample())
        {
            if (Options::astrometryAutoDownsample() && ccd_width && ccd_height)
            {
                uint8_t bin = qMax(Options::solverBinningIndex() + 1, 1u);
                uint16_t w = ccd_width / bin;
                optionsMap["downsample"] = getSolverDownsample(w);
            }
            else
                optionsMap["downsample"] = Options::astrometryDownsample();
        }

        //Options needed for Sextractor
        int bin = Options::solverBinningIndex() + 1;
        optionsMap["image_width"] = ccd_width / bin;
        optionsMap["image_height"] = ccd_height / bin;

        if (Options::astrometryUseImageScale() && fov_x > 0 && fov_y > 0)
        {
            QString units = "dw";
            if (Options::astrometryImageScaleUnits() == 1)
                units = "aw";
            else if (Options::astrometryImageScaleUnits() == 2)
                units = "app";
            if (Options::astrometryAutoUpdateImageScale())
            {
                QString fov_low, fov_high;
                double fov_w = fov_x;
                double fov_h = fov_y;

                if (Options::astrometryImageScaleUnits() == SSolver::DEG_WIDTH)
                {
                    fov_w /= 60;
                    fov_h /= 60;
                }
                else if (Options::astrometryImageScaleUnits() == SSolver::ARCSEC_PER_PIX)
                {
                    fov_w = fov_pixscale;
                    fov_h = fov_pixscale;
                }

                // If effective FOV is pending, let's set a wider tolerance range
                generateFOVBounds(fov_w, fov_low, fov_high, m_EffectiveFOVPending ? 0.3 : 0.05);

                optionsMap["scaleL"]     = fov_low;
                optionsMap["scaleH"]     = fov_high;
                optionsMap["scaleUnits"] = units;
            }
            else
            {
                optionsMap["scaleL"]     = Options::astrometryImageScaleLow();
                optionsMap["scaleH"]     = Options::astrometryImageScaleHigh();
                optionsMap["scaleUnits"] = units;
            }
        }

        if (Options::astrometryUsePosition() && currentTelescope != nullptr)
        {
            double ra = 0, dec = 0;
            currentTelescope->getEqCoords(&ra, &dec);

            optionsMap["ra"]     = ra * 15.0;
            optionsMap["de"]     = dec;
            optionsMap["radius"] = Options::astrometryRadius();
        }
    }
    else
    {
        // Downsample
        QVariant width;
        data->getRecordValue("NAXIS1", width);
        if (width.isValid())
        {
            optionsMap["downsample"] = getSolverDownsample(width.toInt());
        }
        else
            optionsMap["downsample"] = Options::astrometryDownsample();

        // Pixel Scale
        QVariant pixscale;
        data->getRecordValue("SCALE", pixscale);
        if (pixscale.isValid())
        {
            optionsMap["scaleL"]     = 0.8 * pixscale.toDouble();
            optionsMap["scaleH"]     = 1.2 * pixscale.toDouble();
            optionsMap["scaleUnits"] = "app";
        }

        // Position
        QVariant ra, de;
        data->getRecordValue("RA", ra);
        data->getRecordValue("DEC", de);
        if (ra.isValid() && de.isValid())
        {
            optionsMap["ra"]     = ra.toDouble();
            optionsMap["de"]     = de.toDouble();
            optionsMap["radius"] = Options::astrometryRadius();
        }
    }

    if (Options::astrometryCustomOptions().isEmpty() == false)
        optionsMap["custom"] = Options::astrometryCustomOptions();

    return generateRemoteOptions(optionsMap);
}

bool Align::captureAndSolve()
{
    m_AlignTimer.stop();
    m_CaptureTimer.stop();

#ifdef Q_OS_OSX
    if(Options::solverType() == SSolver::SOLVER_LOCALASTROMETRY
            && Options::solveSextractorType() == SSolver::EXTRACTOR_BUILTIN)
    {
        if( !opsPrograms->astropyInstalled() || !opsPrograms->pythonInstalled() )
        {
            KSNotification::error(
                i18n("Astrometry.net uses python3 and the astropy package for plate solving images offline when using the built in Sextractor. These were not detected on your system.  Please install Python and the Astropy package or select a different Sextractor for solving."));
            return false;
        }
    }
#endif

    if (currentCCD == nullptr)
    {
        appendLogText(i18n("Error: No camera detected."));
        return false;
    }

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: lost connection to camera."));
        KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed"), KSNotification::EVENT_ALERT);
        return false;
    }

    if (currentCCD->isBLOBEnabled() == false)
    {
        currentCCD->setBLOBEnabled(true);
    }

    // If CCD Telescope Type does not match desired scope type, change it
    // but remember current value so that it can be reset once capture is complete or is aborted.
    if (currentCCD->getTelescopeType() != FOVScopeCombo->currentIndex())
    {
        rememberTelescopeType = currentCCD->getTelescopeType();
        currentCCD->setTelescopeType(static_cast<ISD::CCD::TelescopeType>(FOVScopeCombo->currentIndex()));
    }

    //if (parser->init() == false)
    //    return false;

    if (focal_length == -1 || aperture == -1)
    {
        KSNotification::error(
            i18n("Telescope aperture and focal length are missing. Please check your driver settings and try again."));
        return false;
    }

    if (ccd_hor_pixel == -1 || ccd_ver_pixel == -1)
    {
        KSNotification::error(i18n("CCD pixel size is missing. Please check your driver settings and try again."));
        return false;
    }

    if (currentFilter != nullptr)
    {
        if (currentFilter->isConnected() == false)
        {
            appendLogText(i18n("Error: lost connection to filter wheel."));
            return false;
        }

        int targetPosition = FilterPosCombo->currentIndex() + 1;

        if (targetPosition > 0 && targetPosition != currentFilterPosition)
        {
            filterPositionPending    = true;
            // Disabling the autofocus policy for align.
            filterManager->setFilterPosition(
                targetPosition, FilterManager::NO_AUTOFOCUS_POLICY);
            state = ALIGN_PROGRESS;
            return true;
        }
    }

    if (currentCCD->getDriverInfo()->getClientManager()->getBLOBMode(currentCCD->getDeviceName().toLatin1().constData(),
            "CCD1") == B_NEVER)
    {
        if (KMessageBox::questionYesNo(
                    nullptr, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) ==
                KMessageBox::Yes)
        {
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ONLY, currentCCD->getDeviceName().toLatin1().constData(),
                    "CCD1");
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ONLY, currentCCD->getDeviceName().toLatin1().constData(),
                    "CCD2");
        }
        else
        {
            return false;
        }
    }

    double seqExpose = exposureIN->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (m_FocusState >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy. Retrying in %1 seconds...", CAPTURE_RETRY_DELAY / 1000));
        m_CaptureTimer.start(CAPTURE_RETRY_DELAY);
        return false;
    }

    if (targetChip->isCapturing())
    {
        appendLogText(i18n("Cannot capture while CCD exposure is in progress. Retrying in %1 seconds...",
                           CAPTURE_RETRY_DELAY / 1000));
        m_CaptureTimer.start(CAPTURE_RETRY_DELAY);
        return false;
    }

    alignView->setBaseSize(alignWidget->size());

    connect(currentCCD, &ISD::CCD::newImage, this, &Ekos::Align::processData);
    connect(currentCCD, &ISD::CCD::newExposureValue, this, &Ekos::Align::checkCCDExposureProgress);

    // In case of remote solver, check if we need to update active CCD
    if (solverTypeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
    {
        if (remoteParserDevice == nullptr)
        {
            appendLogText(i18n("No remote astrometry driver detected, switching to StellarSolver."));
            setSolverType(SOLVER_LOCAL);
        }
        else
        {
            // Update ACTIVE_CCD of the remote astrometry driver so it listens to BLOB emitted by the CCD
            ITextVectorProperty *activeDevices = remoteParserDevice->getBaseDevice()->getText("ACTIVE_DEVICES");
            if (activeDevices)
            {
                IText *activeCCD = IUFindText(activeDevices, "ACTIVE_CCD");
                if (QString(activeCCD->text) != CCDCaptureCombo->currentText())
                {
                    IUSaveText(activeCCD, CCDCaptureCombo->currentText().toLatin1().data());

                    remoteParserDevice->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
                }
            }

            // Enable remote parse
            dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->setEnabled(true);
            dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->sendArgs(generateRemoteArgs());
            solverTimer.start();
        }
    }

    if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
    {
        rememberUploadMode = ISD::CCD::UPLOAD_LOCAL;
        currentCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
    }

    rememberCCDExposureLooping = currentCCD->isLooping();
    if (rememberCCDExposureLooping)
        currentCCD->setExposureLoopingEnabled(false);

    // Remove temporary FITS files left before by the solver
    QDir dir(QDir::tempPath());
    dir.setNameFilters(QStringList() << "fits*"  << "tmp.*");
    dir.setFilter(QDir::Files);
    for (auto &dirFile : dir.entryList())
        dir.remove(dirFile);

    currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);

    targetChip->resetFrame();
    targetChip->setBatchMode(false);
    targetChip->setCaptureMode(FITS_ALIGN);
    targetChip->setFrameType(FRAME_LIGHT);

    int bin = Options::solverBinningIndex() + 1;
    targetChip->setBinning(bin, bin);

    // Set gain if applicable
    if (currentCCD->hasGain() && GainSpin->value() > GainSpinSpecialValue)
        currentCCD->setGain(GainSpin->value());
    // Set ISO if applicable
    if (ISOCombo->currentIndex() >= 0)
        targetChip->setISOIndex(ISOCombo->currentIndex());

    // In case we're in refresh phase of the polar alignment helper then we use capture value from there
    if (pahStage == PAH_REFRESH)
        targetChip->capture(PAHExposure->value());
    else
        targetChip->capture(seqExpose);

    Options::setAlignExposure(seqExpose);

    solveB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    differentialSlewingActivated = false;

    state = ALIGN_PROGRESS;
    emit newStatus(state);
    solverFOV->setProperty("visible", true);

    // If we're just refreshing, then we're done
    if (pahStage == PAH_REFRESH)
        return true;

    appendLogText(i18n("Capturing image..."));

    //This block of code will create the row in the solution table and populate RA, DE, and object name.
    //It also starts the progress indicator.
    double ra, dec;
    currentTelescope->getEqCoords(&ra, &dec);
    if (loadSlewState == IPS_IDLE)
    {
        int currentRow = solutionTable->rowCount();
        solutionTable->insertRow(currentRow);
        for (int i = 4; i < 6; i++)
        {
            QTableWidgetItem *disabledBox = new QTableWidgetItem();
            disabledBox->setFlags(Qt::ItemIsSelectable);
            solutionTable->setItem(currentRow, i, disabledBox);
        }

        QTableWidgetItem *RAReport = new QTableWidgetItem();
        RAReport->setText(ScopeRAOut->text());
        RAReport->setTextAlignment(Qt::AlignHCenter);
        RAReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 0, RAReport);

        QTableWidgetItem *DECReport = new QTableWidgetItem();
        DECReport->setText(ScopeDecOut->text());
        DECReport->setTextAlignment(Qt::AlignHCenter);
        DECReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 1, DECReport);

        double maxrad = 1.0;
        SkyObject *so =
            KStarsData::Instance()->skyComposite()->objectNearest(new SkyPoint(dms(ra * 15), dms(dec)), maxrad);
        QString name;
        if (so)
        {
            name = so->longname();
        }
        else
        {
            name = "None";
        }
        QTableWidgetItem *ObjNameReport = new QTableWidgetItem();
        ObjNameReport->setText(name);
        ObjNameReport->setTextAlignment(Qt::AlignHCenter);
        ObjNameReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 2, ObjNameReport);
#ifdef Q_OS_OSX
        repaint(); //This is a band-aid for a bug in QT 5.10.0
#endif

        QProgressIndicator *alignIndicator = new QProgressIndicator(this);
        solutionTable->setCellWidget(currentRow, 3, alignIndicator);
        alignIndicator->startAnimation();
#ifdef Q_OS_OSX
        repaint(); //This is a band-aid for a bug in QT 5.10.0
#endif
    }

    return true;
}

void Align::processData(const QSharedPointer<FITSData> &data)
{
    if (data->property("chip").toInt() == ISD::CCDChip::GUIDE_CCD)
        return;

    disconnect(currentCCD, &ISD::CCD::newImage, this, &Ekos::Align::processData);
    disconnect(currentCCD, &ISD::CCD::newExposureValue, this, &Ekos::Align::checkCCDExposureProgress);

    if (data)
        m_ImageData = data;
    else
        m_ImageData.reset();
    //    blobType     = *(static_cast<ISD::CCD::BlobType *>(bp->aux1));
    //    blobFileName = QString(static_cast<char *>(bp->aux2));

    // If it's Refresh, we're done
    if (pahStage == PAH_REFRESH)
    {
        setCaptureComplete();
        return;
    }

    appendLogText(i18n("Image received."));

    // If Local solver, then set capture complete or perform calibration first.
    if (solverTypeButtonGroup->checkedId() == SOLVER_LOCAL)
    {
        // Only perform dark image subtraction on local images.
        if (alignDarkFrameCheck->isChecked())
        {
            int x, y, w, h, binx = 1, biny = 1;
            ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
            targetChip->getFrame(&x, &y, &w, &h);
            targetChip->getBinning(&binx, &biny);

            uint16_t offsetX = x / binx;
            uint16_t offsetY = y / biny;

            connect(DarkLibrary::Instance(), &DarkLibrary::darkFrameCompleted, this, [&](bool completed)
            {
                DarkLibrary::Instance()->disconnect(this);
                alignDarkFrameCheck->setChecked(completed);
                if (completed)
                {
                    alignView->rescale(ZOOM_KEEP_LEVEL);
                    alignView->updateFrame();
                    setCaptureComplete();
                }
                else
                    abort();
            });
            connect(DarkLibrary::Instance(), &DarkLibrary::newLog, this, &Ekos::Align::appendLogText);
            DarkLibrary::Instance()->captureAndSubtract(targetChip, m_ImageData, exposureIN->value(), targetChip->getCaptureFilter(),
                    offsetX, offsetY);
            return;
        }

        setCaptureComplete();
    }
}

void Align::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    if (pahStage == PAH_REFRESH)
    {
        newFrame(alignView);
        captureAndSolve();
        return;
    }

    emit newImage(alignView);

#if 0
    if (Options::solverType() == SSolver::SOLVER_ONLINEASTROMETRY &&
            Options::astrometryUseJPEG())
    {
        ISD::CCDChip *targetChip =
            currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip)
        {
            QString jpegFile = blobFileName + ".jpg";
            bool rc          = alignView->getDisplayImage().save(jpegFile, "JPG");
            if (rc)
                blobFileName = jpegFile;
        }
    }
#endif

    solverFOV->setImage(alignView->getDisplayImage());

    //m_FileToSolve = blobFileName;
    startSolving();
}

void Align::setSolverAction(int mode)
{
    gotoModeButtonGroup->button(mode)->setChecked(true);
    currentGotoMode = static_cast<GotoMode>(mode);
}

void Align::startSolving()
{
    // This is needed because they might have directories stored in the config file.
    // So we can't just use the options folder list.
    QStringList astrometryDataDirs = KSUtils::getAstrometryDataDirs();
    FITSData *data = alignView->getImageData();
    disconnect(alignView, &FITSView::loaded, this, &Align::startSolving);

    if (solverTypeButtonGroup->checkedId() == SOLVER_LOCAL)
    {
        if (m_StellarSolver)
        {
            auto *solver = m_StellarSolver.release();
            solver->disconnect(this);
            if (solver->isRunning())
            {
                connect(solver, &StellarSolver::finished, solver, &StellarSolver::deleteLater);
                solver->abort();
            }
            else
                solver->deleteLater();
        }
        m_StellarSolver.reset(new StellarSolver(SSolver::SOLVE, data->getStatistics(), data->getImageBuffer()));
        m_StellarSolver->setProperty("ExtractorType", Options::solveSextractorType());
        m_StellarSolver->setProperty("SolverType", Options::solverType());
        connect(m_StellarSolver.get(), &StellarSolver::ready, this, &Align::solverComplete);
        connect(m_StellarSolver.get(), &StellarSolver::logOutput, this, &Align::appendLogText);
        m_StellarSolver->setIndexFolderPaths(Options::astrometryIndexFolderList());
        m_StellarSolver->setParameters(m_StellarSolverProfiles.at(Options::solveOptionsProfile()));

        const SSolver::SolverType type = static_cast<SSolver::SolverType>(m_StellarSolver->property("SolverType").toInt());
        if(type == SSolver::SOLVER_LOCALASTROMETRY || type == SSolver::SOLVER_ASTAP)
        {
            QString filename = QDir::tempPath() + QString("/solver%1.fits").arg(QUuid::createUuid().toString().remove(
                                   QRegularExpression("[-{}]")));
            alignView->saveImage(filename);
            m_StellarSolver->setProperty("FileToProcess", filename);

            if(Options::sextractorIsInternal())
                m_StellarSolver->setProperty("SextractorBinaryPath", QString("%1/%2").arg(QCoreApplication::applicationDirPath())
                                             .arg("astrometry/bin/sex"));
            else
                m_StellarSolver->setProperty("SextractorBinaryPath", Options::sextractorBinary());

            if (Options::astrometrySolverIsInternal())
                m_StellarSolver->setProperty("SolverPath", QString("%1/%2").arg(QCoreApplication::applicationDirPath())
                                             .arg("astrometry/bin/solve-field"));
            else
                m_StellarSolver->setProperty("SolverPath", Options::astrometrySolverBinary());

            m_StellarSolver->setProperty("ASTAPBinaryPath", Options::aSTAPExecutable());
            if (Options::astrometryWCSIsInternal())
                m_StellarSolver->setProperty("WCSPath", QString("%1/%2").arg(QCoreApplication::applicationDirPath())
                                             .arg("astrometry/bin/wcsinfo"));
            else
                m_StellarSolver->setProperty("WCSPath", Options::astrometryWCSInfo());

            //No need for a conf file this way.
            m_StellarSolver->setProperty("AutoGenerateAstroConfig", true);
        }

        if(type == SSolver::SOLVER_ONLINEASTROMETRY )
        {
            QString filename = QDir::tempPath() + QString("/solver%1.jpg").arg(QUuid::createUuid().toString().remove(
                                   QRegularExpression("[-{}]")));
            alignView->saveImage(filename);

            m_StellarSolver->setProperty("FileToProcess", filename);
            m_StellarSolver->setProperty("AstrometryAPIKey", Options::astrometryAPIKey());
            m_StellarSolver->setProperty("AstrometryAPIURL", Options::astrometryAPIURL());
        }

        if (loadSlewState == IPS_BUSY)
        {
            FITSImage::Solution solution;
            data->parseSolution(solution);

            if (solution.pixscale > 0)
                m_StellarSolver->setSearchScale(solution.pixscale * 0.8,
                                                solution.pixscale * 1.2,
                                                SSolver::ARCSEC_PER_PIX);
            else
                m_StellarSolver->setProperty("UseScale", false);

            if (solution.ra > 0)
                m_StellarSolver->setSearchPositionInDegrees(solution.ra, solution.dec);
            else
                m_StellarSolver->setProperty("UsePostion", false);
        }
        else
        {
            //Setting the initial search scale settings
            if(Options::astrometryUseImageScale())
            {
                SSolver::ScaleUnits units = static_cast<SSolver::ScaleUnits>(Options::astrometryImageScaleUnits());
                // Extend search scale from 80% to 120%
                m_StellarSolver->setSearchScale(Options::astrometryImageScaleLow() * 0.8,
                                                Options::astrometryImageScaleHigh() * 1.2,
                                                units);
            }
            else
                m_StellarSolver->setProperty("UseScale", false);
            //Setting the initial search location settings
            if(Options::astrometryUsePosition())
                m_StellarSolver->setSearchPositionInDegrees(telescopeCoord.ra().Degrees(), telescopeCoord.dec().Degrees());
            else
                m_StellarSolver->setProperty("UsePostion", false);
        }

        if(Options::alignmentLogging())
        {
            m_StellarSolver->setLogLevel(static_cast<SSolver::logging_level>(Options::loggerLevel()));
            m_StellarSolver->setSSLogLevel(SSolver::LOG_NORMAL);
            if(Options::astrometryLogToFile())
            {
                m_StellarSolver->setProperty("LogToFile", true);
                m_StellarSolver->setProperty("LogFileName", Options::astrometryLogFilepath());
            }
        }
        else
        {
            m_StellarSolver->setLogLevel(SSolver::LOG_NONE);
            m_StellarSolver->setSSLogLevel(SSolver::LOG_OFF);
        }

        //Unless we decide to load the WCS Coord, let's turn it off.
        //Be sure to set this to true instead if we want WCS from the solve.
        m_StellarSolver->setLoadWCS(false);

        // Start solving process
        m_StellarSolver->start();
    }
    else
    {
        // This should run only for load&slew. For regular solve, we don't get here
        // as the image is read and solved server-side.
        remoteParser->startSovler(data->filename(), generateRemoteArgs(data), false);
    }

    // Kick off timer
    solverTimer.start();

    state = ALIGN_PROGRESS;
    emit newStatus(state);

    /**
    QStringList solverArgs;

    QString options = solverOptions->text().simplified();

    if (isGenerated)
    {
        solverArgs = options.split(' ');
        // Replace RA and DE with LST & 90/-90 pole
        if (pahStage == PAH_FIRST_CAPTURE)
        {
            for (int i = 0; i < solverArgs.count(); i++)
            {
                // RA
                if (solverArgs[i] == "-3")
                    solverArgs[i + 1] = QString::number(KStarsData::Instance()->lst()->Degrees());
                // DE. +90 for Northern hemisphere. -90 for southern hemisphere
                else if (solverArgs[i] == "-4")
                    solverArgs[i + 1] = QString::number(hemisphere == NORTH_HEMISPHERE ? 90 : -90);
            }
        }
    }
    else if (filename.endsWith(QLatin1String("fits")) || filename.endsWith(QLatin1String("fit")))
    {
        solverArgs = getSolverOptionsFromFITS(filename);
        appendLogText(i18n("Using solver options: %1", solverArgs.join(' ')));
    }
    else
    {
        KGuiItem blindItem(i18n("Blind solver"), QString(),
                           i18n("Blind solver takes a very long time to solve but can reliably solve any image any "
                                "where in the sky given enough time."));
        KGuiItem existingItem(i18n("Use existing settings"), QString(),
                              i18n("Mount must be pointing close to the target location and current field of view must "
                                   "match the image's field of view."));

        int rc = KMessageBox::questionYesNoCancel(nullptr,
                 i18n("No metadata is available in this image. Do you want to use the "
                      "blind solver or the existing solver settings?"),
                 i18n("Astrometry solver"), blindItem, existingItem,
                 KStandardGuiItem::cancel(), "blind_solver_or_existing_solver_option");

        if (rc == KMessageBox::Yes)
        {
            QVariantMap optionsMap;

            if (Options::astrometryUseNoVerify())
                optionsMap["noverify"] = true;

            if (Options::astrometryUseResort())
                optionsMap["resort"] = true;

            if (Options::astrometryUseNoFITS2FITS())
                optionsMap["nofits2fits"] = true;

            if (Options::astrometryUseDownsample())
                optionsMap["downsample"] = Options::astrometryDownsample();

            //Options needed for Sextractor
            int bin = Options::solverBinningIndex() + 1;
            optionsMap["image_width"] = ccd_width / bin;
            optionsMap["image_height"] = ccd_height / bin;

            solverArgs = generateOptions(optionsMap, solverBackendGroup->checkedId());
        }
        else if (rc == KMessageBox::No)
            solverArgs = options.split(' ');
        else
        {
            abort();
            return;
        }
    }

    //    if (solverIterations == 0 && mountModelReset == false)
    //    {
    //        double ra, dec;
    //        currentTelescope->getEqCoords(&ra, &dec);
    //        targetCoord.setRA(ra);
    //        targetCoord.setDec(dec);
    //    }

    //    mountModelReset = false;

    Options::setSolverAccuracyThreshold(accuracySpin->value());
    Options::setAlignDarkFrame(alignDarkFrameCheck->isChecked());
    Options::setSolverGotoOption(currentGotoMode);

    if (fov_x > 0)
        parser->verifyIndexFiles(fov_x, fov_y);

    solverTimer.start();

    m_AlignTimer.start();

    if (currentGotoMode == GOTO_SLEW)
        appendLogText(i18n("Solver iteration #%1", solverIterations + 1));

    state = ALIGN_PROGRESS;
    emit newStatus(state);

    parser->startSovler(filename, solverArgs, isGenerated);
    **/
}

//Note this is temporary, just trying to see if it works.
void Align::solverComplete()
{
    disconnect(m_StellarSolver.get(), &StellarSolver::ready, this, &Align::solverComplete);
    if(!m_StellarSolver->solvingDone() || m_StellarSolver->failed())
        solverFailed();
    else
    {
        FITSImage::Solution solution = m_StellarSolver->getSolution();
        solverFinished(solution.orientation, solution.ra, solution.dec, solution.pixscale);
    }
}

void Align::solverFinished(double orientation, double ra, double dec, double pixscale)
{
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    sOrientation = orientation;
    sRA          = ra;
    sDEC         = dec;

    double elapsed = solverTimer.elapsed() / 1000.0;
    appendLogText(i18n("Solver completed after %1 seconds.", QString::number(elapsed, 'f', 2)));

    // Reset Telescope Type to remembered value
    if (rememberTelescopeType != ISD::CCD::TELESCOPE_UNKNOWN)
    {
        currentCCD->setTelescopeType(rememberTelescopeType);
        rememberTelescopeType = ISD::CCD::TELESCOPE_UNKNOWN;
    }

    m_AlignTimer.stop();
    if (solverTypeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParserDevice && remoteParser.get())
    {
        // Disable remote parse
        dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->setEnabled(false);
    }

    int binx, biny;
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    targetChip->getBinning(&binx, &biny);

    if (Options::alignmentLogging())
        appendLogText(i18n("Solver RA (%1) DEC (%2) Orientation (%3) Pixel Scale (%4)", QString::number(ra, 'f', 5),
                           QString::number(dec, 'f', 5), QString::number(orientation, 'f', 5),
                           QString::number(pixscale, 'f', 5)));

    // When solving (without Load&Slew), update effective FOV and focal length accordingly.
    if (loadSlewState == IPS_IDLE &&
            (fov_x == 0 || m_EffectiveFOVPending || std::fabs(pixscale - fov_pixscale) > 0.05) &&
            pixscale > 0)
    {
        double newFOVW = ccd_width * pixscale / binx / 60.0;
        double newFOVH = ccd_height * pixscale / biny / 60.0;

        calculateEffectiveFocalLength(newFOVW);
        saveNewEffectiveFOV(newFOVW, newFOVH);

        m_EffectiveFOVPending = false;
    }

    alignCoord.setRA0(ra / 15.0);
    alignCoord.setDec0(dec);
    RotOut->setText(QString::number(orientation, 'f', 5));

    // Convert to JNow
    alignCoord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
    // Get horizontal coords
    alignCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    // Do not update diff if we are performing load & slew.
    if (loadSlewState == IPS_IDLE)
    {
        pixScaleOut->setText(QString::number(pixscale, 'f', 2));
        calculateAlignTargetDiff();
    }

    double solverPA = orientation;
    // TODO 2019-11-06 JM: KStars needs to support "upside-down" displays since this is a hack.
    // Because astrometry reads image upside-down (bottom to top), the orientation is rotated 180 degrees when compared to PA
    // PA = Orientation + 180
    double solverFlippedPA = orientation + 180;
    // Limit PA to -180 to +180
    if (solverFlippedPA > 180)
        solverFlippedPA -= 360;
    if (solverFlippedPA < -180)
        solverFlippedPA += 360;
    solverFOV->setCenter(alignCoord);
    solverFOV->setPA(solverFlippedPA);
    solverFOV->setImageDisplay(Options::astrometrySolverOverlay());
    // Sensor FOV as well
    sensorFOV->setPA(solverFlippedPA);

    QString ra_dms, dec_dms;
    getFormattedCoords(alignCoord.ra().Hours(), alignCoord.dec().Degrees(), ra_dms, dec_dms);

    SolverRAOut->setText(ra_dms);
    SolverDecOut->setText(dec_dms);


    if (Options::astrometrySolverWCS())
    {
        INumberVectorProperty *ccdRotation = currentCCD->getBaseDevice()->getNumber("CCD_ROTATION");
        if (ccdRotation)
        {
            INumber *rotation = IUFindNumber(ccdRotation, "CCD_ROTATION_VALUE");
            if (rotation)
            {
                ClientManager *clientManager = currentCCD->getDriverInfo()->getClientManager();
                rotation->value              = orientation;
                clientManager->sendNewNumber(ccdRotation);

                if (m_wcsSynced == false)
                {
                    appendLogText(
                        i18n("WCS information updated. Images captured from this point forward shall have valid WCS."));

                    // Just send telescope info in case the CCD driver did not pick up before.
                    INumberVectorProperty *telescopeInfo =
                        currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");
                    if (telescopeInfo)
                        clientManager->sendNewNumber(telescopeInfo);

                    m_wcsSynced = true;
                }
            }
        }
    }

    m_CaptureErrorCounter = 0;
    m_SlewErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;

    appendLogText(i18n("Solution coordinates: RA (%1) DEC (%2) Telescope Coordinates: RA (%3) DEC (%4)",
                       alignCoord.ra().toHMSString(), alignCoord.dec().toDMSString(), telescopeCoord.ra().toHMSString(),
                       telescopeCoord.dec().toDMSString()));
    if (loadSlewState == IPS_IDLE && currentGotoMode == GOTO_SLEW)
    {
        dms diffDeg(m_TargetDiffTotal / 3600.0);
        appendLogText(i18n("Target is within %1 degrees of solution coordinates.", diffDeg.toDMSString()));
    }

    if (rememberUploadMode != currentCCD->getUploadMode())
        currentCCD->setUploadMode(rememberUploadMode);

    if (rememberCCDExposureLooping)
        currentCCD->setExposureLoopingEnabled(true);

    //This block of code along with some sections in the switch below will set the status report in the solution table for this item.
    std::unique_ptr<QTableWidgetItem> statusReport(new QTableWidgetItem());
    int currentRow = solutionTable->rowCount() - 1;
    if (loadSlewState == IPS_IDLE)
    {
        solutionTable->setCellWidget(currentRow, 3, new QWidget());
        statusReport->setFlags(Qt::ItemIsSelectable);
    }

    // Update Rotator offsets
    if (currentRotator != nullptr)
    {
        // When Load&Slew image is solved, we check if we need to rotate the rotator to match the position angle of the image
        if (loadSlewState == IPS_BUSY && Options::astrometryUseRotator())
        {
            loadSlewTargetPA = solverPA;
            qCDebug(KSTARS_EKOS_ALIGN) << "loaSlewTargetPA:" << loadSlewTargetPA;

        }
        else
        {
            INumberVectorProperty *absAngle = currentRotator->getBaseDevice()->getNumber("ABS_ROTATOR_ANGLE");
            if (absAngle)
            {
                // PA = RawAngle * Multiplier + Offset
                currentRotatorPA = solverPA;
                double rawAngle = absAngle->np[0].value;
                double offset = range360(solverPA - (rawAngle * Options::pAMultiplier()));

                qCDebug(KSTARS_EKOS_ALIGN) << "Raw Rotator Angle:" << rawAngle << "Rotator PA:" << currentRotatorPA << "Rotator Offset:" <<
                                           offset;
                Options::setPAOffset(offset);
            }

            if (absAngle && std::isnan(loadSlewTargetPA) == false
                    && fabs(currentRotatorPA - loadSlewTargetPA) * 60 > Options::astrometryRotatorThreshold())
            {
                double rawAngle = range360((loadSlewTargetPA - Options::pAOffset()) / Options::pAMultiplier());
                //                if (rawAngle < 0)
                //                    rawAngle += 360;
                //                else if (rawAngle > 360)
                //                    rawAngle -= 360;
                absAngle->np[0].value = rawAngle;
                ClientManager *clientManager = currentRotator->getDriverInfo()->getClientManager();
                clientManager->sendNewNumber(absAngle);
                appendLogText(i18n("Setting position angle to %1 degrees E of N...", loadSlewTargetPA));
                return;
            }
        }
    }

    emit newSolverResults(orientation, ra, dec, pixscale);
    QJsonObject solution =
    {
        {"ra", SolverRAOut->text()},
        {"de", SolverDecOut->text()},
        {"dRA", m_TargetDiffRA},
        {"dDE", m_TargetDiffDE},
        {"targetDiff", m_TargetDiffTotal},
        {"pix", pixscale},
        {"rot", orientation},
        {"fov", FOVOut->text()},
    };
    emit newSolution(solution.toVariantMap());

    switch (currentGotoMode)
    {
        case GOTO_SYNC:
            executeGOTO();

            if (loadSlewState == IPS_IDLE)
            {
                statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
                solutionTable->setItem(currentRow, 3, statusReport.release());
            }

            return;

        case GOTO_SLEW:
            if (loadSlewState == IPS_BUSY || m_TargetDiffTotal > static_cast<double>(accuracySpin->value()))
            {
                if (loadSlewState == IPS_IDLE && ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
                {
                    appendLogText(i18n("Maximum number of iterations reached. Solver failed."));

                    if (loadSlewState == IPS_IDLE)
                    {
                        statusReport->setIcon(QIcon(":/icons/AlignFailure.svg"));
                        solutionTable->setItem(currentRow, 3, statusReport.release());
                    }

                    solverFailed();
                    if (mountModelRunning)
                        finishAlignmentPoint(false);
                    return;
                }

                targetAccuracyNotMet = true;

                if (loadSlewState == IPS_IDLE)
                {
                    statusReport->setIcon(QIcon(":/icons/AlignWarning.svg"));
                    solutionTable->setItem(currentRow, 3, statusReport.release());
                }

                executeGOTO();
                return;
            }

            if (loadSlewState == IPS_IDLE)
            {
                statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
                solutionTable->setItem(currentRow, 3, statusReport.release());
            }

            appendLogText(i18n("Target is within acceptable range. Astrometric solver is successful."));

            if (mountModelRunning)
            {
                finishAlignmentPoint(true);
                if (mountModelRunning)
                    return;
            }
            break;

        case GOTO_NOTHING:
            if (loadSlewState == IPS_IDLE)
            {
                statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
                solutionTable->setItem(currentRow, 3, statusReport.release());
            }
            if (mountModelRunning)
            {
                finishAlignmentPoint(true);
                if (mountModelRunning)
                    return;
            }
            break;
    }

    KSNotification::event(QLatin1String("AlignSuccessful"), i18n("Astrometry alignment completed successfully"));
    state = ALIGN_COMPLETE;
    emit newStatus(state);
    solverIterations = 0;

    solverFOV->setProperty("visible", true);

    if (pahStage != PAH_IDLE)
        processPAHStage(orientation, ra, dec, pixscale);
    else if (azStage > AZ_INIT || altStage > ALT_INIT)
        executePolarAlign();
    else
    {
        solveB->setEnabled(true);
        loadSlewB->setEnabled(true);
    }
}

void Align::solverFailed()
{
    appendLogText(i18n("Solver Failed"));

    KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed"),
                          KSNotification::EVENT_ALERT);

    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);
    loadSlewB->setEnabled(true);

    m_AlignTimer.stop();

    azStage  = AZ_INIT;
    altStage = ALT_INIT;

    loadSlewState = IPS_IDLE;
    solverIterations = 0;
    m_CaptureErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;
    m_SlewErrorCounter = 0;

    state = ALIGN_FAILED;
    emit newStatus(state);

    solverFOV->setProperty("visible", false);

    int currentRow = solutionTable->rowCount() - 1;
    solutionTable->setCellWidget(currentRow, 3, new QWidget());
    QTableWidgetItem *statusReport = new QTableWidgetItem();
    statusReport->setIcon(QIcon(":/icons/AlignFailure.svg"));
    statusReport->setFlags(Qt::ItemIsSelectable);
    solutionTable->setItem(currentRow, 3, statusReport);
}

void Align::abort()
{
    m_CaptureTimer.stop();
    if (solverTypeButtonGroup->checkedId() == SOLVER_LOCAL && m_StellarSolver)
        m_StellarSolver->abort();
    else if (solverTypeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser)
        remoteParser->stopSolver();
    //parser->stopSolver();
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);
    loadSlewB->setEnabled(true);

    // Reset Telescope Type to remembered value
    if (rememberTelescopeType != ISD::CCD::TELESCOPE_UNKNOWN)
    {
        currentCCD->setTelescopeType(rememberTelescopeType);
        rememberTelescopeType = ISD::CCD::TELESCOPE_UNKNOWN;
    }

    azStage  = AZ_INIT;
    altStage = ALT_INIT;

    loadSlewState = IPS_IDLE;
    solverIterations = 0;
    m_CaptureErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;
    m_SlewErrorCounter = 0;
    m_AlignTimer.stop();

    disconnect(currentCCD, &ISD::CCD::newImage, this, &Ekos::Align::processData);
    disconnect(currentCCD, &ISD::CCD::newExposureValue, this, &Ekos::Align::checkCCDExposureProgress);

    if (rememberUploadMode != currentCCD->getUploadMode())
        currentCCD->setUploadMode(rememberUploadMode);

    if (rememberCCDExposureLooping)
        currentCCD->setExposureLoopingEnabled(true);

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    // If capture is still in progress, let's stop that.
    if (pahStage == PAH_REFRESH)
    {
        if (targetChip->isCapturing())
            targetChip->abortExposure();

        appendLogText(i18n("Refresh is complete."));
    }
    else
    {
        if (targetChip->isCapturing())
        {
            targetChip->abortExposure();
            appendLogText(i18n("Capture aborted."));
        }
        else
        {
            double elapsed = solverTimer.elapsed() / 1000.0;
            appendLogText(i18n("Solver aborted after %1 seconds.", QString::number(elapsed, 'f', 2)));
        }
    }

    state = ALIGN_ABORTED;
    emit newStatus(state);

    int currentRow = solutionTable->rowCount() - 1;

    solutionTable->setCellWidget(currentRow, 3, new QWidget());
    QTableWidgetItem *statusReport = new QTableWidgetItem();
    statusReport->setIcon(QIcon(":/icons/AlignFailure.svg"));
    statusReport->setFlags(Qt::ItemIsSelectable);
    solutionTable->setItem(currentRow, 3, statusReport);
}

QList<double> Align::getSolutionResult()
{
    QList<double> result;

    result << sOrientation << sRA << sDEC;

    return result;
}

void Align::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_ALIGN) << text;

    emit newLog(text);
}

void Align::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

void Align::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "DOME_MOTION"))
    {
        // If dome is not ready and state is now
        if (domeReady == false && svp->s == IPS_OK)
        {
            domeReady = true;
            // trigger process number for mount so that it proceeds with normal workflow since
            // it was stopped by dome not being ready
            handleMountStatus();
        }
    }
    else if ((!strcmp(svp->name, "TELESCOPE_MOTION_NS") || !strcmp(svp->name, "TELESCOPE_MOTION_WE")))
        switch (svp->s)
        {
            case IPS_BUSY:
                // react upon mount motion
                handleMountMotion();
                m_wasSlewStarted = true;
                break;
            default:
                qCDebug(KSTARS_EKOS_ALIGN) << "Mount motion finished.";
                handleMountStatus();
                break;
        }

}

void Align::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "EQUATORIAL_EOD_COORD") || !strcmp(nvp->name, "EQUATORIAL_COORD"))
    {
        QString ra_dms, dec_dms;

        if (!strcmp(nvp->name, "EQUATORIAL_COORD"))
        {
            telescopeCoord.setRA0(nvp->np[0].value);
            telescopeCoord.setDec0(nvp->np[1].value);
            // Get JNow as well
            telescopeCoord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
        }
        else
        {
            telescopeCoord.setRA(nvp->np[0].value);
            telescopeCoord.setDec(nvp->np[1].value);
        }

        getFormattedCoords(telescopeCoord.ra().Hours(), telescopeCoord.dec().Degrees(), ra_dms, dec_dms);

        telescopeCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        ScopeRAOut->setText(ra_dms);
        ScopeDecOut->setText(dec_dms);

        //        qCDebug(KSTARS_EKOS_ALIGN) << "## RA" << ra_dms << "DE" << dec_dms
        //                                   << "state:" << pstateStr(nvp->s) << "slewStarted?" << m_wasSlewStarted;

        switch (nvp->s)
        {
            // Idle --> Mount not tracking or slewing
            case IPS_IDLE:
                m_wasSlewStarted = false;
                //qCDebug(KSTARS_EKOS_ALIGN) << "## IPS_IDLE --> setting slewStarted to FALSE";
                break;

            // Ok --> Mount Tracking. If m_wasSlewStarted is true
            // then it just finished slewing
            case IPS_OK:
            {
                // Update the boxes as the mount just finished slewing
                if (m_wasSlewStarted && Options::astrometryAutoUpdatePosition())
                {
                    //qCDebug(KSTARS_EKOS_ALIGN) << "## IPS_OK --> Auto Update Position...";
                    opsAstrometry->estRA->setText(ra_dms);
                    opsAstrometry->estDec->setText(dec_dms);

                    Options::setAstrometryPositionRA(nvp->np[0].value * 15);
                    Options::setAstrometryPositionDE(nvp->np[1].value);

                    //generateArgs();
                }

                // If dome is syncing, wait until it stops
                if (currentDome && currentDome->isMoving())
                {
                    domeReady = false;
                    return;
                }

                // If we are looking for celestial pole
                if (m_wasSlewStarted && pahStage == PAH_FIND_CP)
                {
                    //qCDebug(KSTARS_EKOS_ALIGN) << "## PAH_FIND_CP--> setting slewStarted to FALSE";
                    m_wasSlewStarted = false;
                    appendLogText(i18n("Mount completed slewing near celestial pole. Capture again to verify."));
                    setSolverAction(GOTO_NOTHING);
                    pahStage = PAH_FIRST_CAPTURE;
                    emit newPAHStage(pahStage);
                    return;
                }

                switch (state)
                {
                    case ALIGN_PROGRESS:
                        break;

                    case ALIGN_SYNCING:
                    {
                        m_wasSlewStarted = false;
                        //qCDebug(KSTARS_EKOS_ALIGN) << "## ALIGN_SYNCING --> setting slewStarted to FALSE";
                        if (currentGotoMode == GOTO_SLEW)
                        {
                            Slew();
                            return;
                        }
                        else
                        {
                            appendLogText(i18n("Mount is synced to solution coordinates. Astrometric solver is successful."));
                            KSNotification::event(QLatin1String("AlignSuccessful"),
                                                  i18n("Astrometry alignment completed successfully"));
                            state = ALIGN_COMPLETE;
                            emit newStatus(state);
                            solverIterations = 0;

                            if (mountModelRunning)
                                finishAlignmentPoint(true);
                        }
                    }
                    break;

                    case ALIGN_SLEWING:

                        if (!didSlewStart())
                        {
                            // If mount has not started slewing yet, then skip
                            //qCDebug(KSTARS_EKOS_ALIGN) << "Mount slew planned, but not started slewing yet...";
                            break;
                        }

                        //qCDebug(KSTARS_EKOS_ALIGN) << "Mount slew completed.";
                        m_wasSlewStarted = false;
                        if (loadSlewState == IPS_BUSY)
                        {
                            loadSlewState = IPS_IDLE;

                            //qCDebug(KSTARS_EKOS_ALIGN) << "loadSlewState is IDLE.";

                            state = ALIGN_PROGRESS;
                            emit newStatus(state);

                            if (delaySpin->value() >= DELAY_THRESHOLD_NOTIFY)
                                appendLogText(i18n("Settling..."));
                            m_CaptureTimer.start(delaySpin->value());
                            return;
                        }
                        else if (differentialSlewingActivated)
                        {
                            appendLogText(i18n("Differential slewing complete. Astrometric solver is successful."));
                            KSNotification::event(QLatin1String("AlignSuccessful"), i18n("Astrometry alignment completed successfully"));
                            state = ALIGN_COMPLETE;
                            emit newStatus(state);
                            solverIterations = 0;

                            if (mountModelRunning)
                                finishAlignmentPoint(true);
                        }
                        else if (currentGotoMode == GOTO_SLEW || mountModelRunning)
                        {
                            if (targetAccuracyNotMet)
                                appendLogText(i18n("Slew complete. Target accuracy is not met, running solver again..."));
                            else
                                appendLogText(i18n("Slew complete. Solving Alignment Point. . ."));

                            targetAccuracyNotMet = false;

                            state = ALIGN_PROGRESS;
                            emit newStatus(state);

                            if (delaySpin->value() >= DELAY_THRESHOLD_NOTIFY)
                                appendLogText(i18n("Settling..."));
                            m_CaptureTimer.start(delaySpin->value());
                            return;
                        }
                        break;

                    default:
                    {
                        //qCDebug(KSTARS_EKOS_ALIGN) << "## Align State " << state << "--> setting slewStarted to FALSE";
                        m_wasSlewStarted = false;
                    }
                    break;
                }
            }
            break;

            // Busy --> Mount Slewing or Moving (NSWE buttons)
            case IPS_BUSY:
            {
                //qCDebug(KSTARS_EKOS_ALIGN) << "Mount slew running.";
                m_wasSlewStarted = true;

                handleMountMotion();
            }
            break;

            // Alert --> Mount has problem moving or communicating.
            case IPS_ALERT:
            {
                //qCDebug(KSTARS_EKOS_ALIGN) << "IPS_ALERT --> setting slewStarted to FALSE";
                m_wasSlewStarted = false;

                if (state == ALIGN_SYNCING || state == ALIGN_SLEWING)
                {
                    if (state == ALIGN_SYNCING)
                        appendLogText(i18n("Syncing failed."));
                    else
                        appendLogText(i18n("Slewing failed."));

                    if (++m_SlewErrorCounter == 3)
                    {
                        abort();
                        return;
                    }
                    else
                    {
                        if (currentGotoMode == GOTO_SLEW)
                            Slew();
                        else
                            Sync();
                    }
                }

                return;
            }
        }

        if (pahStage == PAH_FIRST_ROTATE)
        {
            // only wait for telescope to slew to new position if manual slewing is switched off
            if(!PAHManual->isChecked())
            {
                double deltaAngle = fabs(telescopeCoord.ra().deltaAngle(targetPAH.ra()).Degrees());
                qCDebug(KSTARS_EKOS_ALIGN) << "First mount rotation remaining degrees:" << deltaAngle;
                if (deltaAngle <= PAH_ROTATION_THRESHOLD)
                {
                    currentTelescope->StopWE();
                    appendLogText(i18n("Mount first rotation is complete."));

                    pahStage = PAH_SECOND_CAPTURE;
                    emit newPAHStage(pahStage);

                    PAHWidgets->setCurrentWidget(PAHSecondCapturePage);
                    emit newPAHMessage(secondCaptureText->text());

                    if (delaySpin->value() >= DELAY_THRESHOLD_NOTIFY)
                        appendLogText(i18n("Settling..."));
                    m_CaptureTimer.start(delaySpin->value());
                }
                // If for some reason we didn't stop, let's stop if we get too far
                else if (deltaAngle > PAHRotationSpin->value() * 1.25)
                {
                    currentTelescope->Abort();
                    appendLogText(i18n("Mount aborted. Please restart the process and reduce the speed."));
                    stopPAHProcess();
                }
                return;
            } // endif not manual slew
        }
        else if (pahStage == PAH_SECOND_ROTATE)
        {
            // only wait for telescope to slew to new position if manual slewing is switched off
            if(!PAHManual->isChecked())
            {
                double deltaAngle = fabs(telescopeCoord.ra().deltaAngle(targetPAH.ra()).Degrees());
                qCDebug(KSTARS_EKOS_ALIGN) << "Second mount rotation remaining degrees:" << deltaAngle;
                if (deltaAngle <= PAH_ROTATION_THRESHOLD)
                {
                    currentTelescope->StopWE();
                    appendLogText(i18n("Mount second rotation is complete."));

                    pahStage = PAH_THIRD_CAPTURE;
                    emit newPAHStage(pahStage);


                    PAHWidgets->setCurrentWidget(PAHThirdCapturePage);
                    emit newPAHMessage(thirdCaptureText->text());

                    if (delaySpin->value() >= DELAY_THRESHOLD_NOTIFY)
                        appendLogText(i18n("Settling..."));
                    m_CaptureTimer.start(delaySpin->value());
                }
                // If for some reason we didn't stop, let's stop if we get too far
                else if (deltaAngle > PAHRotationSpin->value() * 1.25)
                {
                    currentTelescope->Abort();
                    appendLogText(i18n("Mount aborted. Please restart the process and reduce the speed."));
                    stopPAHProcess();
                }
                return;
            } // endif not manual slew
        }

        switch (azStage)
        {
            case AZ_SYNCING:
                if (currentTelescope->isSlewing())
                    azStage = AZ_SLEWING;
                break;

            case AZ_SLEWING:
                if (currentTelescope->isSlewing() == false)
                {
                    azStage = AZ_SECOND_TARGET;
                    measureAzError();
                }
                break;

            case AZ_CORRECTING:
                if (currentTelescope->isSlewing() == false)
                {
                    appendLogText(i18n(
                                      "Slew complete. Please adjust azimuth knob until the target is in the center of the view."));
                    azStage = AZ_INIT;
                }
                break;

            default:
                break;
        }

        switch (altStage)
        {
            case ALT_SYNCING:
                if (currentTelescope->isSlewing())
                    altStage = ALT_SLEWING;
                break;

            case ALT_SLEWING:
                if (currentTelescope->isSlewing() == false)
                {
                    altStage = ALT_SECOND_TARGET;
                    measureAltError();
                }
                break;

            case ALT_CORRECTING:
                if (currentTelescope->isSlewing() == false)
                {
                    appendLogText(i18n(
                                      "Slew complete. Please adjust altitude knob until the target is in the center of the view."));
                    altStage = ALT_INIT;
                }
                break;
            default:
                break;
        }
    }
    else if (!strcmp(nvp->name, "ABS_ROTATOR_ANGLE"))
    {
        // PA = RawAngle * Multiplier + Offset
        currentRotatorPA = (nvp->np[0].value * Options::pAMultiplier()) + Options::pAOffset();
        if (currentRotatorPA > 180)
            currentRotatorPA -= 360;
        if (currentRotatorPA < -180)
            currentRotatorPA += 360;
        if (std::isnan(loadSlewTargetPA) == false
                && fabs(currentRotatorPA - loadSlewTargetPA) * 60 <= Options::astrometryRotatorThreshold())
        {
            appendLogText(i18n("Rotator reached target position angle."));
            targetAccuracyNotMet = true;
            loadSlewTargetPA = std::numeric_limits<double>::quiet_NaN();
            QTimer::singleShot(Options::settlingTime(), this, &Ekos::Align::executeGOTO);
        }
    }

    // N.B. Ekos::Manager already manages TELESCOPE_INFO, why here again?
    //if (!strcmp(coord->name, "TELESCOPE_INFO"))
    //syncTelescopeInfo();
}

void Align::handleMountMotion()
{
    if (state == ALIGN_PROGRESS)
    {
        if (pahStage == PAH_IDLE)
        {
            // whoops, mount slews during alignment
            appendLogText(i18n("Slew detected, aborting solving..."));
            abort();
            // reset the state to busy so that solving restarts after slewing finishes
            loadSlewState = IPS_BUSY;
            // if mount model is running, retry the current alignment point
            if (mountModelRunning)
            {
                appendLogText(i18n("Restarting alignment point %1", currentAlignmentPoint + 1));
                if (currentAlignmentPoint > 0)
                    currentAlignmentPoint--;
            }
        }

        state = ALIGN_SLEWING;
    }
}

void Align::handleMountStatus()
{
    INumberVectorProperty *nvp = nullptr;

    if (currentTelescope->isJ2000())
        nvp = currentTelescope->getBaseDevice()->getNumber("EQUATORIAL_COORD");
    else
        nvp = currentTelescope->getBaseDevice()->getNumber("EQUATORIAL_EOD_COORD");

    if (nvp)
        processNumber(nvp);
}


void Align::executeGOTO()
{
    if (loadSlewState == IPS_BUSY)
    {
        targetCoord = alignCoord;
        SlewToTarget();
    }
    else if (currentGotoMode == GOTO_SYNC)
        Sync();
    else if (currentGotoMode == GOTO_SLEW)
        SlewToTarget();
}

void Align::Sync()
{
    state = ALIGN_SYNCING;

    if (currentTelescope->Sync(&alignCoord))
    {
        emit newStatus(state);
        appendLogText(
            i18n("Syncing to RA (%1) DEC (%2)", alignCoord.ra().toHMSString(), alignCoord.dec().toDMSString()));
    }
    else
    {
        state = ALIGN_IDLE;
        emit newStatus(state);
        appendLogText(i18n("Syncing failed."));
    }
}

void Align::Slew()
{
    state = ALIGN_SLEWING;
    emit newStatus(state);

    //qCDebug(KSTARS_EKOS_ALIGN) << "## Before SLEW command: wasSlewStarted -->" << m_wasSlewStarted;
    //m_wasSlewStarted = currentTelescope->Slew(&targetCoord);
    //qCDebug(KSTARS_EKOS_ALIGN) << "## After SLEW command: wasSlewStarted -->" << m_wasSlewStarted;

    // JM 2019-08-23: Do not assume that slew was started immediately. Wait until IPS_BUSY state is triggered
    // from Goto
    currentTelescope->Slew(&targetCoord);
    slewStartTimer.start();
    appendLogText(i18n("Slewing to target coordinates: RA (%1) DEC (%2).", targetCoord.ra().toHMSString(),
                       targetCoord.dec().toDMSString()));
}

void Align::SlewToTarget()
{
    if (canSync && loadSlewState == IPS_IDLE)
    {
        // 2018-01-24 JM: This is ugly. Maybe use DBus? Signal/Slots? Ekos Manager usage like this should be avoided
#if 0
        if (Ekos::Manager::Instance()->getCurrentJobName().isEmpty())
        {
            KSNotification::event(QLatin1String("EkosSchedulerTelescopeSynced"),
                                  i18n("Ekos job (%1) - Telescope synced",
                                       Ekos::Manager::Instance()->getCurrentJobName()));
        }
#endif

        // Do we perform a regular sync or use differential slewing?
        if (Options::astrometryDifferentialSlewing())
        {
            dms m_TargetDiffRA = alignCoord.ra().deltaAngle(targetCoord.ra());
            dms m_TargetDiffDE = alignCoord.dec().deltaAngle(targetCoord.dec());

            targetCoord.setRA(targetCoord.ra() - m_TargetDiffRA);
            targetCoord.setDec(targetCoord.dec() - m_TargetDiffDE);

            differentialSlewingActivated = true;

            qCDebug(KSTARS_EKOS_ALIGN) << "Using differential slewing...";

            Slew();
        }
        else
            Sync();

        return;
    }

    Slew();
}

void Align::executePolarAlign()
{
    appendLogText(i18n("Processing solution for polar alignment..."));

    switch (azStage)
    {
        case AZ_FIRST_TARGET:
        case AZ_FINISHED:
            measureAzError();
            break;

        default:
            break;
    }

    switch (altStage)
    {
        case ALT_FIRST_TARGET:
        case ALT_FINISHED:
            measureAltError();
            break;

        default:
            break;
    }
}

void Align::measureAzError()
{
    static double initRA = 0, initDEC = 0, finalRA = 0, finalDEC = 0, initAz = 0;

    if (pahStage != PAH_IDLE &&
            (KMessageBox::warningContinueCancel(KStars::Instance(),
                    i18n("Polar Alignment Helper is still active. Do you want to continue "
                         "using legacy polar alignment tool?")) != KMessageBox::Continue))
        return;

    pahStage = PAH_IDLE;
    emit newPAHStage(pahStage);

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Measuring Azimuth Error...";

    switch (azStage)
    {
        case AZ_INIT:

            // Display message box confirming user point scope near meridian and south

            // N.B. This action cannot be automated.
            if (KMessageBox::warningContinueCancel(
                        nullptr,
                        hemisphere == NORTH_HEMISPHERE ?
                        i18n("Point the telescope at the southern meridian. Press Continue when ready.") :
                        i18n("Point the telescope at the northern meridian. Press Continue when ready."),
                        i18n("Polar Alignment Measurement"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                        "ekos_measure_az_error") != KMessageBox::Continue)
                return;

            appendLogText(i18n("Solving first frame near the meridian."));
            azStage = AZ_FIRST_TARGET;
            captureAndSolve();
            break;

        case AZ_FIRST_TARGET:
            // start solving there, find RA/DEC
            initRA  = alignCoord.ra().Degrees();
            initDEC = alignCoord.dec().Degrees();
            initAz  = alignCoord.az().Degrees();

            qCDebug(KSTARS_EKOS_ALIGN) << "Polar initRA " << alignCoord.ra().toHMSString() << " initDEC "
                                       << alignCoord.dec().toDMSString() << " initlAz " << alignCoord.az().toDMSString()
                                       << " initAlt " << alignCoord.alt().toDMSString();

            // Now move 30 arcminutes in RA
            if (canSync)
            {
                azStage = AZ_SYNCING;
                currentTelescope->Sync(initRA / 15.0, initDEC);
                currentTelescope->Slew((initRA - RAMotion) / 15.0, initDEC);
            }
            // If telescope doesn't sync, we slew relative to its current coordinates
            else
            {
                azStage = AZ_SLEWING;
                currentTelescope->Slew(telescopeCoord.ra().Hours() - RAMotion / 15.0, telescopeCoord.dec().Degrees());
            }

            appendLogText(i18n("Slewing 30 arcminutes in RA..."));
            break;

        case AZ_SECOND_TARGET:
            // We reached second target now
            // Let now solver for RA/DEC
            appendLogText(i18n("Solving second frame near the meridian."));
            azStage = AZ_FINISHED;
            captureAndSolve();
            break;

        case AZ_FINISHED:
            // Measure deviation in DEC
            // Call function to report error
            // set stage to AZ_FIRST_TARGET again
            appendLogText(i18n("Calculating azimuth alignment error..."));
            finalRA  = alignCoord.ra().Degrees();
            finalDEC = alignCoord.dec().Degrees();

            qCDebug(KSTARS_EKOS_ALIGN) << "Polar finalRA " << alignCoord.ra().toHMSString() << " finalDEC "
                                       << alignCoord.dec().toDMSString() << " finalAz " << alignCoord.az().toDMSString()
                                       << " finalAlt " << alignCoord.alt().toDMSString();

            // Slew back to original position
            if (canSync)
                currentTelescope->Slew(initRA / 15.0, initDEC);
            else
            {
                currentTelescope->Slew(telescopeCoord.ra().Hours() + RAMotion / 15.0, telescopeCoord.dec().Degrees());
            }

            appendLogText(i18n("Slewing back to original position..."));

            calculatePolarError(initRA, initDEC, finalRA, finalDEC, initAz);

            azStage = AZ_INIT;
            break;

        default:
            break;
    }
}

void Align::measureAltError()
{
    static double initRA = 0, initDEC = 0, finalRA = 0, finalDEC = 0, initAz = 0;

    if (pahStage != PAH_IDLE && (KMessageBox::warningContinueCancel(KStars::Instance(),
                                 i18n("Polar Alignment Helper is still active. Do you want to continue "
                                      "using legacy polar alignment tool?")) != KMessageBox::Continue))
        return;

    pahStage = PAH_IDLE;
    emit newPAHStage(pahStage);

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Measuring Altitude Error...";

    switch (altStage)
    {
        case ALT_INIT:

            // Display message box confirming user point scope near meridian and south

            // N.B. This action cannot be automated.
            if (KMessageBox::warningContinueCancel(nullptr,
                                                   i18n("Point the telescope to the eastern or western horizon with a "
                                                           "minimum altitude of 20 degrees. Press continue when ready."),
                                                   i18n("Polar Alignment Measurement"), KStandardGuiItem::cont(),
                                                   KStandardGuiItem::cancel(),
                                                   "ekos_measure_alt_error") != KMessageBox::Continue)
                return;

            appendLogText(i18n("Solving first frame."));
            altStage = ALT_FIRST_TARGET;
            if (delaySpin->value() >= DELAY_THRESHOLD_NOTIFY)
                appendLogText(i18n("Settling..."));
            m_CaptureTimer.start(delaySpin->value());
            break;

        case ALT_FIRST_TARGET:
            // start solving there, find RA/DEC
            initRA  = alignCoord.ra().Degrees();
            initDEC = alignCoord.dec().Degrees();
            initAz  = alignCoord.az().Degrees();

            qCDebug(KSTARS_EKOS_ALIGN) << "Polar initRA " << alignCoord.ra().toHMSString() << " initDEC "
                                       << alignCoord.dec().toDMSString() << " initlAz " << alignCoord.az().toDMSString()
                                       << " initAlt " << alignCoord.alt().toDMSString();

            // Now move 30 arcminutes in RA
            if (canSync)
            {
                altStage = ALT_SYNCING;
                currentTelescope->Sync(initRA / 15.0, initDEC);
                currentTelescope->Slew((initRA - RAMotion) / 15.0, initDEC);
            }
            // If telescope doesn't sync, we slew relative to its current coordinates
            else
            {
                altStage = ALT_SLEWING;
                currentTelescope->Slew(telescopeCoord.ra().Hours() - RAMotion / 15.0, telescopeCoord.dec().Degrees());
            }

            appendLogText(i18n("Slewing 30 arcminutes in RA..."));
            break;

        case ALT_SECOND_TARGET:
            // We reached second target now
            // Let now solver for RA/DEC
            appendLogText(i18n("Solving second frame."));
            altStage = ALT_FINISHED;

            if (delaySpin->value() >= DELAY_THRESHOLD_NOTIFY)
                appendLogText(i18n("Settling..."));
            m_CaptureTimer.start(delaySpin->value());
            break;

        case ALT_FINISHED:
            // Measure deviation in DEC
            // Call function to report error
            appendLogText(i18n("Calculating altitude alignment error..."));
            finalRA  = alignCoord.ra().Degrees();
            finalDEC = alignCoord.dec().Degrees();

            qCDebug(KSTARS_EKOS_ALIGN) << "Polar finalRA " << alignCoord.ra().toHMSString() << " finalDEC "
                                       << alignCoord.dec().toDMSString() << " finalAz " << alignCoord.az().toDMSString()
                                       << " finalAlt " << alignCoord.alt().toDMSString();

            // Slew back to original position
            if (canSync)
                currentTelescope->Slew(initRA / 15.0, initDEC);
            // If telescope doesn't sync, we slew relative to its current coordinates
            else
            {
                currentTelescope->Slew(telescopeCoord.ra().Hours() + RAMotion / 15.0, telescopeCoord.dec().Degrees());
            }

            appendLogText(i18n("Slewing back to original position..."));

            calculatePolarError(initRA, initDEC, finalRA, finalDEC, initAz);

            altStage = ALT_INIT;
            break;

        default:
            break;
    }
}

void Align::calculatePolarError(double initRA, double initDEC, double finalRA, double finalDEC, double initAz)
{
    double raMotion = finalRA - initRA;
    decDeviation    = finalDEC - initDEC;

    // East/West of meridian
    int horizon = (initAz > 0 && initAz <= 180) ? 0 : 1;

    // How much time passed siderrally form initRA to finalRA?
    //double RATime = fabs(raMotion / SIDRATE) / 60.0;

    // 2016-03-30: Diff in RA is sufficient for time difference
    // raMotion in degrees. RATime in minutes.
    double RATime = fabs(raMotion) * 60.0;

    // Equation by Frank Berret (Measuring Polar Axis Alignment Error, page 4)
    // In degrees
    double deviation = (3.81 * (decDeviation * 3600)) / (RATime * cos(initDEC * dms::DegToRad)) / 60.0;
    dms devDMS(fabs(deviation));

    KLocalizedString deviationDirection;

    switch (hemisphere)
    {
        // Northern hemisphere
        case NORTH_HEMISPHERE:
            if (azStage == AZ_FINISHED)
            {
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far east");
                else
                    deviationDirection = ki18n("%1 too far west");
            }
            else if (altStage == ALT_FINISHED)
            {
                switch (horizon)
                {
                    // East
                    case 0:
                        if (decDeviation > 0)
                            deviationDirection = ki18n("%1 too far high");
                        else
                            deviationDirection = ki18n("%1 too far low");

                        break;

                    // West
                    case 1:
                        if (decDeviation > 0)
                            deviationDirection = ki18n("%1 too far low");
                        else
                            deviationDirection = ki18n("%1 too far high");
                        break;

                    default:
                        break;
                }
            }
            break;

        // Southern hemisphere
        case SOUTH_HEMISPHERE:
            if (azStage == AZ_FINISHED)
            {
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far west");
                else
                    deviationDirection = ki18n("%1 too far east");
            }
            else if (altStage == ALT_FINISHED)
            {
                switch (horizon)
                {
                    // East
                    case 0:
                        if (decDeviation > 0)
                            deviationDirection = ki18n("%1 too far low");
                        else
                            deviationDirection = ki18n("%1 too far high");
                        break;

                    // West
                    case 1:
                        if (decDeviation > 0)
                            deviationDirection = ki18n("%1 too far high");
                        else
                            deviationDirection = ki18n("%1 too far low");
                        break;

                    default:
                        break;
                }
            }
            break;
    }

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Hemisphere is " << ((hemisphere == NORTH_HEMISPHERE) ? "North" : "South")
                               << " --- initAz " << initAz;
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar initRA " << initRA << " initDEC " << initDEC << " finalRA " << finalRA
                               << " finalDEC " << finalDEC;
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar decDeviation " << decDeviation * 3600 << " arcsec "
                               << " RATime " << RATime << " minutes";
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Raw Deviation " << deviation << " degrees.";

    if (azStage == AZ_FINISHED)
    {
        azError->setText(deviationDirection.subs(QString("%1").arg(devDMS.toDMSString())).toString());
        azDeviation = deviation * (decDeviation > 0 ? 1 : -1);

        qCDebug(KSTARS_EKOS_ALIGN) << "Polar Azimuth Deviation " << azDeviation << " degrees.";

        correctAzB->setEnabled(true);
    }
    if (altStage == ALT_FINISHED)
    {
        altError->setText(deviationDirection.subs(QString("%1").arg(devDMS.toDMSString())).toString());
        altDeviation = deviation * (decDeviation > 0 ? 1 : -1);

        qCDebug(KSTARS_EKOS_ALIGN) << "Polar Altitude Deviation " << altDeviation << " degrees.";

        correctAltB->setEnabled(true);
    }
}

void Align::correctAltError()
{
    double newRA, newDEC;

    SkyPoint currentCoord(telescopeCoord);
    dms targetLat;

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Correcting Altitude Error...";
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Current Mount RA " << currentCoord.ra().toHMSString() << " DEC "
                               << currentCoord.dec().toDMSString() << "Az " << currentCoord.az().toDMSString() << " Alt "
                               << currentCoord.alt().toDMSString();

    // An error in polar alignment altitude reflects a deviation in the latitude of the mount from actual latitude of the site
    // Calculating the latitude accounting for the altitude deviation. This is the latitude at which the altitude deviation should be zero.
    targetLat.setD(KStars::Instance()->data()->geo()->lat()->Degrees() + altDeviation);

    // Calculate the Az/Alt of the mount if it were located at the corrected latitude
    currentCoord.EquatorialToHorizontal(KStars::Instance()->data()->lst(), &targetLat);

    // Convert corrected Az/Alt to RA/DEC given the local sideral time and current (not corrected) latitude
    currentCoord.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    // New RA/DEC should reflect the position in the sky at which the polar alignment altitude error is minimal.
    newRA  = currentCoord.ra().Hours();
    newDEC = currentCoord.dec().Degrees();

    altStage = ALT_CORRECTING;

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Target Latitude = Latitude "
                               << KStars::Instance()->data()->geo()->lat()->Degrees() << " + Altitude Deviation " << altDeviation
                               << " = " << targetLat.Degrees();
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Slewing to calibration position...";

    currentTelescope->Slew(newRA, newDEC);

    appendLogText(i18n("Slewing to calibration position, please wait until telescope completes slewing."));
}

void Align::correctAzError()
{
    double newRA, newDEC, currentAlt, currentAz;

    SkyPoint currentCoord(telescopeCoord);

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Correcting Azimuth Error...";
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Current Mount RA " << currentCoord.ra().toHMSString() << " DEC "
                               << currentCoord.dec().toDMSString() << "Az " << currentCoord.az().toDMSString() << " Alt "
                               << currentCoord.alt().toDMSString();
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Target Azimuth = Current Azimuth " << currentCoord.az().Degrees()
                               << " + Azimuth Deviation " << azDeviation << " = " << currentCoord.az().Degrees() + azDeviation;

    // Get current horizontal coordinates of the mount
    currentCoord.EquatorialToHorizontal(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    // Keep Altitude as it is and change Azimuth to account for the azimuth deviation
    // The new sky position should be where the polar alignment azimuth error is minimal
    currentAlt = currentCoord.alt().Degrees();
    currentAz  = currentCoord.az().Degrees() + azDeviation;

    // Update current Alt and Azimuth to new values
    currentCoord.setAlt(currentAlt);
    currentCoord.setAz(currentAz);

    // Convert Alt/Az back to equatorial coordinates
    currentCoord.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    // Get new RA and DEC
    newRA  = currentCoord.ra().Hours();
    newDEC = currentCoord.dec().Degrees();

    azStage = AZ_CORRECTING;

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Slewing to calibration position...";

    currentTelescope->Slew(newRA, newDEC);

    appendLogText(i18n("Slewing to calibration position, please wait until telescope completes slewing."));
}

void Align::getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str)
{
    dms ra_s, dec_s;
    ra_s.setH(ra);
    dec_s.setD(dec);

    ra_str = QString("%1:%2:%3")
             .arg(ra_s.hour(), 2, 10, QChar('0'))
             .arg(ra_s.minute(), 2, 10, QChar('0'))
             .arg(ra_s.second(), 2, 10, QChar('0'));
    if (dec_s.Degrees() < 0)
        dec_str = QString("-%1:%2:%3")
                  .arg(abs(dec_s.degree()), 2, 10, QChar('0'))
                  .arg(abs(dec_s.arcmin()), 2, 10, QChar('0'))
                  .arg(dec_s.arcsec(), 2, 10, QChar('0'));
    else
        dec_str = QString("%1:%2:%3")
                  .arg(dec_s.degree(), 2, 10, QChar('0'))
                  .arg(dec_s.arcmin(), 2, 10, QChar('0'))
                  .arg(dec_s.arcsec(), 2, 10, QChar('0'));
}

bool Align::loadAndSlew(QString fileURL)
{
#ifdef Q_OS_OSX
    if(Options::solverType() == SSolver::SOLVER_LOCALASTROMETRY
            && Options::solveSextractorType() == SSolver::EXTRACTOR_BUILTIN)
    {
        if( !opsPrograms->astropyInstalled() || !opsPrograms->pythonInstalled() )
        {
            KSNotification::error(
                i18n("Astrometry.net uses python3 and the astropy package for plate solving images offline when using the built in Sextractor. These were not detected on your system.  Please install Python and the Astropy package or select a different Sextractor for solving."));
            return false;
        }
    }
#endif

    if (fileURL.isEmpty())
        fileURL = QFileDialog::getOpenFileName(KStars::Instance(), i18n("Load Image"), dirPath,
                                               "Images (*.fits *.fits.fz *.fit *.fts "
                                               "*.jpg *.jpeg *.png *.gif *.bmp "
                                               "*.cr2 *.cr3 *.crw *.nef *.raf *.dng *.arw)");

    if (fileURL.isEmpty())
        return false;

    QFileInfo fileInfo(fileURL);

    dirPath = fileInfo.absolutePath();

    differentialSlewingActivated = false;

    loadSlewState = IPS_BUSY;

    stopPAHProcess();

    slewR->setChecked(true);
    currentGotoMode = GOTO_SLEW;

    solveB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    if (solverTypeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParserDevice == nullptr)
    {
        appendLogText(i18n("No remote astrometry driver detected, switching to StellarSolver."));
        setSolverType(SOLVER_LOCAL);
    }

    alignView->loadFile(fileURL, false);
    //m_FileToSolve = fileURL;
    connect(alignView, &FITSView::loaded, this, &Align::startSolving);

    return true;
}

bool Align::loadAndSlew(const QByteArray &image, const QString &extension)
{
#ifdef Q_OS_OSX
    if(Options::solverType() == SSolver::SOLVER_LOCALASTROMETRY
            && Options::solveSextractorType() == SSolver::EXTRACTOR_BUILTIN)
    {
        if( !opsPrograms->astropyInstalled() || !opsPrograms->pythonInstalled() )
        {
            KSNotification::error(
                i18n("Astrometry.net uses python3 and the astropy package for plate solving images offline when using the built in Sextractor. These were not detected on your system.  Please install Python and the Astropy package or select a different Sextractor for solving."));
            return false;
        }
    }
#endif

    differentialSlewingActivated = false;
    loadSlewState = IPS_BUSY;
    stopPAHProcess();
    slewR->setChecked(true);
    currentGotoMode = GOTO_SLEW;
    solveB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    QSharedPointer<FITSData> data;
    data.reset(new FITSData(), &QObject::deleteLater);
    data->loadFromBuffer(image, extension);
    alignView->loadData(data);
    startSolving();
    return true;
}

void Align::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Align::setBinningIndex(int binIndex)
{
    syncSettings();
    Options::setSolverBinningIndex(binIndex);

    // If sender is not our combo box, then we need to update the combobox itself
    if (dynamic_cast<QComboBox *>(sender()) != binningCombo)
    {
        binningCombo->blockSignals(true);
        binningCombo->setCurrentIndex(binIndex);
        binningCombo->blockSignals(false);
    }

    // Need to calculate FOV and args for APP
    if (Options::astrometryImageScaleUnits() == SSolver::ARCSEC_PER_PIX)
    {
        calculateFOV();
        //generateArgs();
    }
}
/**
void Align::setSolverArguments(const QString &value)
{
    //solverOptions->setText(value);
}

QString Align::solverArguments()
{
   // return solverOptions->text();
}
**/
void Align::setFOVTelescopeType(int index)
{
    FOVScopeCombo->setCurrentIndex(index);
}

void Align::addFilter(ISD::GDInterface *newFilter)
{
    for (auto filter : Filters)
    {
        if (filter->getDeviceName() == newFilter->getDeviceName())
            return;
    }

    FilterCaptureLabel->setEnabled(true);
    FilterDevicesCombo->setEnabled(true);
    FilterPosLabel->setEnabled(true);
    FilterPosCombo->setEnabled(true);

    FilterDevicesCombo->addItem(newFilter->getDeviceName());

    Filters.append(static_cast<ISD::Filter *>(newFilter));

    int filterWheelIndex = 1;
    if (Options::defaultAlignFilterWheel().isEmpty() == false)
        filterWheelIndex = FilterDevicesCombo->findText(Options::defaultAlignFilterWheel());

    if (filterWheelIndex < 1)
        filterWheelIndex = 1;

    checkFilter(filterWheelIndex);
    FilterDevicesCombo->setCurrentIndex(filterWheelIndex);

    emit settingsUpdated(getSettings());
}

bool Align::setFilterWheel(const QString &device)
{
    bool deviceFound = false;

    for (int i = 1; i < FilterDevicesCombo->count(); i++)
        if (device == FilterDevicesCombo->itemText(i))
        {
            checkFilter(i);
            deviceFound = true;
            break;
        }

    if (deviceFound == false)
        return false;

    return true;
}

QString Align::filterWheel()
{
    if (FilterDevicesCombo->currentIndex() >= 1)
        return FilterDevicesCombo->currentText();

    return QString();
}

bool Align::setFilter(const QString &filter)
{
    if (FilterDevicesCombo->currentIndex() >= 1)
    {
        FilterPosCombo->setCurrentText(filter);
        return true;
    }

    return false;
}

QString Align::filter()
{
    return FilterPosCombo->currentText();
}


void Align::checkFilter(int filterNum)
{
    if (filterNum == -1)
    {
        filterNum = FilterDevicesCombo->currentIndex();
        if (filterNum == -1)
            return;
    }

    // "--" is no filter
    if (filterNum == 0)
    {
        currentFilter = nullptr;
        currentFilterPosition = -1;
        FilterPosCombo->clear();
        return;
    }

    if (filterNum <= Filters.count())
        currentFilter = Filters.at(filterNum - 1);

    FilterPosCombo->clear();

    FilterPosCombo->addItems(filterManager->getFilterLabels());

    currentFilterPosition = filterManager->getFilterPosition();

    FilterPosCombo->setCurrentIndex(Options::lockAlignFilterIndex());

    syncSettings();
}

void Align::setWCSEnabled(bool enable)
{
    if (currentCCD == nullptr)
        return;

    ISwitchVectorProperty *wcsControl = currentCCD->getBaseDevice()->getSwitch("WCS_CONTROL");

    ISwitch *wcs_enable  = IUFindSwitch(wcsControl, "WCS_ENABLE");
    ISwitch *wcs_disable = IUFindSwitch(wcsControl, "WCS_DISABLE");

    if (!wcs_enable || !wcs_disable)
        return;

    if ((wcs_enable->s == ISS_ON && enable) || (wcs_disable->s == ISS_ON && !enable))
        return;

    IUResetSwitch(wcsControl);
    if (enable)
    {
        appendLogText(i18n("World Coordinate System (WCS) is enabled. CCD rotation must be set either manually in the "
                           "CCD driver or by solving an image before proceeding to capture any further images, "
                           "otherwise the WCS information may be invalid."));
        wcs_enable->s = ISS_ON;
    }
    else
    {
        wcs_disable->s = ISS_ON;
        m_wcsSynced    = false;
        appendLogText(i18n("World Coordinate System (WCS) is disabled."));
    }

    ClientManager *clientManager = currentCCD->getDriverInfo()->getClientManager();
    if (clientManager)
        clientManager->sendNewSwitch(wcsControl);
}

void Align::checkCCDExposureProgress(ISD::CCDChip *targetChip, double remaining, IPState state)
{
    INDI_UNUSED(targetChip);
    INDI_UNUSED(remaining);

    if (state == IPS_ALERT)
    {
        if (++m_CaptureErrorCounter == 3 && pahStage != PAH_REFRESH)
        {
            appendLogText(i18n("Capture error. Aborting..."));

            abort();
            return;
        }

        appendLogText(i18n("Restarting capture attempt #%1", m_CaptureErrorCounter));

        int currentRow = solutionTable->rowCount() - 1;

        solutionTable->setCellWidget(currentRow, 3, new QWidget());
        QTableWidgetItem *statusReport = new QTableWidgetItem();
        statusReport->setIcon(QIcon(":/icons/AlignFailure.svg"));
        statusReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 3, statusReport);

        captureAndSolve();
    }
}

void Align::setFocusStatus(Ekos::FocusState state)
{
    m_FocusState = state;
}

/**
QStringList Align::getSolverOptionsFromFITS(const QString &filename)
{
    QVariantMap optionsMap;

    // For ASTAP, we just default settings
    if (solverBackendGroup->checkedId() == SOLVER_ASTAP)
    {
        if (Options::aSTAPSearchRadius())
            optionsMap["radius"] = Options::aSTAPSearchRadiusValue();

        if (Options::aSTAPDownSample() && Options::aSTAPDownSampleValue() > 0)
            optionsMap["downsample"] = Options::aSTAPDownSampleValue();

        optionsMap["speed"] = Options::aSTAPLargeSearchWindow() ? "slow" : "auto";

        if (Options::aSTAPUpdateFITS())
            optionsMap["update"] = true;

        return generateOptions(optionsMap, solverBackendGroup->checkedId());

    }

    int status = 0, fits_ccd_width, fits_ccd_height, fits_binx = 1, fits_biny = 1;
    char comment[128], error_status[512];
    fitsfile *fptr = nullptr;
    double ra = 0, dec = 0, fits_fov_x, fits_fov_y, fov_lower, fov_upper, fits_ccd_hor_pixel = -1,
           fits_ccd_ver_pixel = -1, fits_focal_length = -1;
    QString fov_low, fov_high;
    QStringList solver_args;

    if (Options::astrometryUseNoVerify())
        optionsMap["noverify"] = true;

    if (Options::astrometryUseResort())
        optionsMap["resort"] = true;

    if (Options::astrometryUseNoFITS2FITS())
        optionsMap["nofits2fits"] = true;

    if (Options::astrometryUseDownsample())
        optionsMap["downsample"] = Options::astrometryDownsample();

    if (Options::astrometryCustomOptions().isEmpty() == false)
        optionsMap["custom"] = Options::astrometryCustomOptions();

    solver_args = generateOptions(optionsMap, solverBackendGroup->checkedId());

    status = 0;

    // Use open diskfile as it does not use extended file names which has problems opening
    // files with [ ] or ( ) in their names.
    if (fits_open_diskfile(&fptr, filename.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        qCCritical(KSTARS_EKOS_ALIGN) << QString::fromUtf8(error_status);
        return solver_args;
    }

    status = 0;
    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        qCCritical(KSTARS_EKOS_ALIGN) << QString::fromUtf8(error_status);
        return solver_args;
    }

    status = 0;
    if (fits_read_key(fptr, TINT, "NAXIS1", &fits_ccd_width, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: cannot find NAXIS1."));
        return solver_args;
    }

    status = 0;
    if (fits_read_key(fptr, TINT, "NAXIS2", &fits_ccd_height, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: cannot find NAXIS2."));
        return solver_args;
    }

    // If we need to auto downsample, let us figure out the scale and regenerate options
    if (Options::astrometryAutoDownsample())
    {
        optionsMap["downsample"] = getSolverDownsample(fits_ccd_width);
        solver_args = generateOptions(optionsMap, SOLVER_ASTROMETRYNET);
    }

    //Needed for Sextractor, let us figure out the image size and regenerate options
    if(Options::useSextractor())
    {
        optionsMap["image_width"] = fits_ccd_width;
        optionsMap["image_height"] = fits_ccd_height;
        solver_args = generateOptions(optionsMap, SOLVER_ASTROMETRYNET);
    }

    bool coord_ok = true;

    status = 0;
    char objectra_str[32];
    if (fits_read_key(fptr, TSTRING, "OBJCTRA", objectra_str, comment, &status))
    {
        if (fits_read_key(fptr, TDOUBLE, "RA", &ra, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            coord_ok = false;
            appendLogText(i18n("FITS header: cannot find OBJCTRA (%1).", QString(error_status)));
        }
        else
            // Degrees to hours
            ra /= 15;
    }
    else
    {
        dms raDMS = dms::fromString(objectra_str, false);
        ra        = raDMS.Hours();
    }

    status = 0;
    char objectde_str[32];
    if (coord_ok && fits_read_key(fptr, TSTRING, "OBJCTDEC", objectde_str, comment, &status))
    {
        if (fits_read_key(fptr, TDOUBLE, "DEC", &dec, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            coord_ok = false;
            appendLogText(i18n("FITS header: cannot find OBJCTDEC (%1).", QString(error_status)));
        }
    }
    else
    {
        dms deDMS = dms::fromString(objectde_str, true);
        dec       = deDMS.Degrees();
    }

    if (coord_ok && Options::astrometryUsePosition())
        solver_args << "-3" << QString::number(ra * 15.0) << "-4" << QString::number(dec) << "-5" << "15";

    status = 0;
    double pixelScale = 0;
    // If we have pixel scale in arcsecs per pixel then lets use that directly
    // instead of calculating it from FOCAL length and other information
    if (fits_read_key(fptr, TDOUBLE, "SCALE", &pixelScale, comment, &status) == 0)
    {
        fov_low  = QString::number(0.9 * pixelScale);
        fov_high = QString::number(1.1 * pixelScale);

        if (Options::astrometryUseImageScale())
            solver_args << "-L" << fov_low << "-H" << fov_high << "-u"
                        << "app";

        return solver_args;
    }

    if (fits_read_key(fptr, TDOUBLE, "FOCALLEN", &fits_focal_length, comment, &status))
    {
        int integer_focal_length = -1;
        if (fits_read_key(fptr, TINT, "FOCALLEN", &integer_focal_length, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            appendLogText(i18n("FITS header: cannot find FOCALLEN (%1).", QString(error_status)));
            return solver_args;
        }
        else
            fits_focal_length = integer_focal_length;
    }

    status = 0;
    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE1", &fits_ccd_hor_pixel, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: cannot find PIXSIZE1 (%1).", QString(error_status)));
        return solver_args;
    }

    status = 0;
    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE2", &fits_ccd_ver_pixel, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: cannot find PIXSIZE2 (%1).", QString(error_status)));
        return solver_args;
    }

    status = 0;
    fits_read_key(fptr, TINT, "XBINNING", &fits_binx, comment, &status);
    status = 0;
    fits_read_key(fptr, TINT, "YBINNING", &fits_biny, comment, &status);

    // Calculate FOV
    fits_fov_x = 206264.8062470963552 * fits_ccd_width * fits_ccd_hor_pixel / 1000.0 / fits_focal_length * fits_binx;
    fits_fov_y = 206264.8062470963552 * fits_ccd_height * fits_ccd_ver_pixel / 1000.0 / fits_focal_length * fits_biny;

    fits_fov_x /= 60.0;
    fits_fov_y /= 60.0;

    // let's stretch the boundaries by 10%
    fov_lower = qMin(fits_fov_x, fits_fov_y);
    fov_upper = qMax(fits_fov_x, fits_fov_y);

    fov_lower *= 0.90;
    fov_upper *= 1.10;

    fov_low  = QString::number(fov_lower);
    fov_high = QString::number(fov_upper);

    if (Options::astrometryUseImageScale())
        solver_args << "-L" << fov_low << "-H" << fov_high << "-u"
                    << "aw";

    return solver_args;
}
**/

uint8_t Align::getSolverDownsample(uint16_t binnedW)
{
    uint8_t downsample = Options::astrometryDownsample();

    if (!Options::astrometryAutoDownsample())
        return downsample;

    while (downsample < 8)
    {
        if (binnedW / downsample <= 1024)
            break;

        downsample += 2;
    }

    return downsample;
}

void Align::saveSettleTime()
{
    Options::setSettlingTime(delaySpin->value());
}

void Align::setCaptureStatus(CaptureState newState)
{
    switch (newState)
    {
        case CAPTURE_PROGRESS:
        {
            // Only reset targetCoord if capture wasn't suspended then resumed due to error duing ongoing
            // capture
            if (currentTelescope && m_CaptureState != CAPTURE_SUSPENDED)
            {
                double ra, dec;
                currentTelescope->getEqCoords(&ra, &dec);
                targetCoord.setRA(ra);
                targetCoord.setDec(dec);
                // While we can set targetCoord = J2000Coord now, it's better to use setTargetCoord
                // Function to do that in case of any changes in the future in that function.
                SkyPoint J2000Coord = targetCoord.catalogueCoord(KStarsData::Instance()->lt().djd());
                setTargetCoords(J2000Coord.ra0().Hours(), J2000Coord.dec0().Degrees());
            }

        }
        break;
        case CAPTURE_ALIGNING:
            if (currentTelescope && currentTelescope->hasAlignmentModel() && Options::resetMountModelAfterMeridian())
            {
                mountModelReset = currentTelescope->clearAlignmentModel();
                qCDebug(KSTARS_EKOS_ALIGN) << "Post meridian flip mount model reset" << (mountModelReset ? "successful." : "failed.");
            }

            m_CaptureTimer.start(Options::settlingTime());
            break;

        default:
            break;
    }

    m_CaptureState = newState;
}

void Align::showFITSViewer()
{
    static int lastFVTabID = -1;
    if (m_ImageData)
    {
        QUrl url = QUrl::fromLocalFile("align.fits");
        if (fv.isNull())
        {
            fv = KStars::Instance()->createFITSViewer();
            fv->loadData(m_ImageData, url, &lastFVTabID);
        }
        else if (fv->updateData(m_ImageData, url, lastFVTabID, &lastFVTabID) == false)
            fv->loadData(m_ImageData, url, &lastFVTabID);

        fv->show();
    }
}

void Align::toggleAlignWidgetFullScreen()
{
    if (alignWidget->parent() == nullptr)
    {
        alignWidget->setParent(this);
        rightLayout->insertWidget(0, alignWidget);
        alignWidget->showNormal();
    }
    else
    {
        alignWidget->setParent(nullptr);
        alignWidget->setWindowTitle(i18n("Align Frame"));
        alignWidget->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
        alignWidget->showMaximized();
        alignWidget->show();
    }
}

void Align::startPAHProcess()
{
    qCInfo(KSTARS_EKOS_ALIGN) << "Starting Polar Alignment Assistant process...";

    pahStage = PAH_FIRST_CAPTURE;
    emit newPAHStage(pahStage);

    nothingR->setChecked(true);
    currentGotoMode = GOTO_NOTHING;
    loadSlewB->setEnabled(false);

    rememberSolverWCS = Options::astrometrySolverWCS();
    rememberAutoWCS   = Options::autoWCS();
    //rememberMeridianFlip = Options::executeMeridianFlip();

    Options::setAutoWCS(false);
    Options::setAstrometrySolverWCS(true);
    //Options::setExecuteMeridianFlip(false);

    if (Options::limitedResourcesMode())
        appendLogText(i18n("Warning: Equatorial Grid Lines will not be drawn due to limited resources mode."));

    if (currentTelescope->hasAlignmentModel())
    {
        appendLogText(i18n("Clearing mount Alignment Model..."));
        mountModelReset = currentTelescope->clearAlignmentModel();
    }

    // Unpark
    currentTelescope->UnPark();

    // Set tracking ON if not already
    if (currentTelescope->canControlTrack() && currentTelescope->isTracking() == false)
        currentTelescope->setTrackEnabled(true);

    PAHStartB->setEnabled(false);
    PAHStopB->setEnabled(true);


    PAHWidgets->setCurrentWidget(PAHFirstCapturePage);
    emit newPAHMessage(firstCaptureText->text());

    captureAndSolve();
}

void Align::stopPAHProcess()
{
    if (pahStage == PAH_IDLE)
        return;

    qCInfo(KSTARS_EKOS_ALIGN) << "Stopping Polar Alignment Assistant process...";

    // Only display dialog if user explicitly restarts
    if ((static_cast<QPushButton *>(sender()) == PAHStopB) && KMessageBox::questionYesNo(KStars::Instance(),
            i18n("Are you sure you want to stop the polar alignment process?"),
            i18n("Polar Alignment Assistant"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
            "restart_PAA_process_dialog") == KMessageBox::No)
        return;

    stopB->click();
    if (currentTelescope && currentTelescope->isInMotion())
        currentTelescope->Abort();

    pahStage = PAH_IDLE;
    emit newPAHStage(pahStage);

    PAHStartB->setEnabled(true);
    PAHStopB->setEnabled(false);
    PAHRefreshB->setEnabled(true);
    PAHWidgets->setCurrentWidget(PAHIntroPage);
    emit newPAHMessage(introText->text());

    qDeleteAll(pahImageInfos);
    pahImageInfos.clear();

    correctionVector = QLineF();
    correctionOffset = QPointF();

    alignView->setCorrectionParams(correctionVector);
    alignView->setCorrectionOffset(correctionOffset);
    alignView->setRACircle(QVector3D());
    alignView->setRefreshEnabled(false);

    emit newFrame(alignView);
    disconnect(alignView, &AlignView::trackingStarSelected, this, &Ekos::Align::setPAHCorrectionOffset);
    disconnect(alignView, &AlignView::newCorrectionVector, this, &Ekos::Align::newCorrectionVector);

    if (Options::pAHAutoPark())
    {
        currentTelescope->Park();
        appendLogText(i18n("Parking the mount..."));
    }

    state = ALIGN_IDLE;
    emit newStatus(state);
}

void Align::rotatePAH()
{
    double TargetDiffRA = PAHRotationSpin->value();
    bool westMeridian = PAHDirectionCombo->currentIndex() == 0;

    // West
    if (westMeridian)
        TargetDiffRA *= -1;
    // East
    else
        TargetDiffRA *= 1;

    // JM 2018-05-03: Hemispheres shouldn't affect rotation direction in RA

    // if Manual slewing is selected, don't move the mount
    if (PAHManual->isChecked())
    {
        appendLogText(i18n("Please rotate your mount about %1 deg in RA", m_TargetDiffRA ));
        return;
    }

    // TargetDiffRA is in degrees
    dms newTelescopeRA = (telescopeCoord.ra() + dms(TargetDiffRA)).reduce();

    targetPAH.setRA(newTelescopeRA);
    targetPAH.setDec(telescopeCoord.dec());

    //currentTelescope->Slew(&targetPAH);
    // Set Selected Speed
    if (PAHSlewRateCombo->currentIndex() >= 0)
        currentTelescope->setSlewRate(PAHSlewRateCombo->currentIndex());
    // Go to direction
    currentTelescope->MoveWE(westMeridian ? ISD::Telescope::MOTION_WEST : ISD::Telescope::MOTION_EAST,
                             ISD::Telescope::MOTION_START);

    appendLogText(i18n("Please wait until mount completes rotating to RA (%1) DE (%2)", targetPAH.ra().toHMSString(),
                       targetPAH.dec().toDMSString()));
}

void Align::calculatePAHError()
{
    QVector3D RACircle;

    bool rc = findRACircle(RACircle);

    if (rc == false)
    {
        appendLogText(i18n("Failed to find a solution. Try again."));
        stopPAHProcess();
        return;
    }

    if (alignView->isEQGridShown() == false)
        alignView->toggleEQGrid();
    alignView->setRACircle(RACircle);

    FITSData *imageData = alignView->getImageData();

    RACenterPoint.setX(RACircle.x());
    RACenterPoint.setY(RACircle.y());

    SkyPoint RACenter;
    rc = imageData->pixelToWCS(RACenterPoint, RACenter);

    if (rc == false)
    {
        appendLogText(i18n("Failed to find RA Axis center: %1.", imageData->getLastError()));
        return;
    }

    SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);

    RACenter.setRA(RACenter.ra0());
    RACenter.setDec(RACenter.dec0());
    double PA = 0;
    dms polarError = RACenter.angularDistanceTo(&CP, &PA);

    if (Options::alignmentLogging())
    {
        qCDebug(KSTARS_EKOS_ALIGN) << "RA Axis Circle X: " << RACircle.x() << " Y: " << RACircle.y()
                                   << " Radius: " << RACircle.z();
        qCDebug(KSTARS_EKOS_ALIGN) << "RA Axis Location RA: " << RACenter.ra0().toHMSString()
                                   << "DE: " << RACenter.dec0().toDMSString();
        qCDebug(KSTARS_EKOS_ALIGN) << "RA Axis Offset: " << polarError.toDMSString() << "PA:" << PA;
        qCDebug(KSTARS_EKOS_ALIGN) << "CP Axis Location X:" << celestialPolePoint.x() << "Y:" << celestialPolePoint.y();
    }

    RACenter.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    QString azDirection = RACenter.az().Degrees() < 30 ? "Right" : "Left";
    QString atDirection = RACenter.alt().Degrees() < KStarsData::Instance()->geo()->lat()->Degrees() ? "Bottom" : "Top";
    // FIXME should this be reversed for southern hemisphere?
    appendLogText(i18n("Mount axis is to the %1 %2 of the celestial pole", atDirection, azDirection));

    PAHErrorLabel->setText(polarError.toDMSString());

    correctionVector.setP1(Options::pAHFlipCorrectionVector() ? RACenterPoint : celestialPolePoint);
    correctionVector.setP2(Options::pAHFlipCorrectionVector() ? celestialPolePoint : RACenterPoint);

    connect(alignView, &AlignView::trackingStarSelected, this, &Ekos::Align::setPAHCorrectionOffset);
    emit polarResultUpdated(correctionVector, polarError.toDMSString());

    connect(alignView, &AlignView::newCorrectionVector, this, &Ekos::Align::newCorrectionVector, Qt::UniqueConnection);
    emit newCorrectionVector(correctionVector);
    alignView->setCorrectionParams(correctionVector);

    emit newFrame(alignView);
}

void Align::syncCorrectionVector()
{
    correctionVector.setP1(Options::pAHFlipCorrectionVector() ? RACenterPoint : celestialPolePoint);
    correctionVector.setP2(Options::pAHFlipCorrectionVector() ? celestialPolePoint : RACenterPoint);
    emit newCorrectionVector(correctionVector);
    alignView->setCorrectionParams(correctionVector);
}

void Align::setPAHCorrectionOffsetPercentage(double dx, double dy)
{
    double x = dx * alignView->zoomedWidth() * (alignView->getCurrentZoom() / 100);
    double y = dy * alignView->zoomedHeight() * (alignView->getCurrentZoom() / 100);

    setPAHCorrectionOffset(static_cast<int>(round(x)), static_cast<int>(round(y)));

}

void Align::setPAHCorrectionOffset(int x, int y)
{
    correctionOffset.setX(x);
    correctionOffset.setY(y);

    alignView->setCorrectionOffset(correctionOffset);

    emit newFrame(alignView);
}

void Align::setPAHCorrectionSelectionComplete()
{
    pahStage = PAH_PRE_REFRESH;
    emit newPAHStage(pahStage);

    // If user stops here, we restore the settings, if not we
    // disable again in the refresh process
    // and restore when refresh is complete
    Options::setAstrometrySolverWCS(rememberSolverWCS);
    Options::setAutoWCS(rememberAutoWCS);
    //Options::setExecuteMeridianFlip(rememberMeridianFlip);

    PAHWidgets->setCurrentWidget(PAHRefreshPage);
    emit newPAHMessage(refreshText->text());
}

void Align::setPAHSlewDone()
{
    emit newPAHMessage("Manual slew done.");
    switch(pahStage)
    {
        case PAH_FIRST_ROTATE :
            pahStage = PAH_SECOND_CAPTURE;
            emit newPAHStage(pahStage);
            PAHWidgets->setCurrentWidget(PAHSecondCapturePage);
            appendLogText(i18n("First manual rotation done."));
            break;
        case PAH_SECOND_ROTATE :
            pahStage = PAH_THIRD_CAPTURE;
            emit newPAHStage(pahStage);
            PAHWidgets->setCurrentWidget(PAHThirdCapturePage);
            appendLogText(i18n("Second manual rotation done."));
            break;
        default :
            return; // no other stage should be able to trigger this event
    }
    if (delaySpin->value() >= DELAY_THRESHOLD_NOTIFY)
        appendLogText(i18n("Settling..."));
    m_CaptureTimer.start(delaySpin->value());
}



void Align::startPAHRefreshProcess()
{
    qCInfo(KSTARS_EKOS_ALIGN) << "Starting Polar Alignment Assistant refreshing...";

    pahStage = PAH_REFRESH;
    emit newPAHStage(pahStage);

    PAHRefreshB->setEnabled(false);

    // Hide EQ Grids if shown
    if (alignView->isEQGridShown())
        alignView->toggleEQGrid();

    alignView->setRefreshEnabled(true);

    Options::setAstrometrySolverWCS(false);
    Options::setAutoWCS(false);

    // We for refresh, just capture really
    captureAndSolve();
}

void Align::setPAHRefreshComplete()
{
    abort();

    Options::setAstrometrySolverWCS(rememberSolverWCS);
    Options::setAutoWCS(rememberAutoWCS);
    //Options::setExecuteMeridianFlip(rememberMeridianFlip);

    stopPAHProcess();
}

void Align::processPAHStage(double orientation, double ra, double dec, double pixscale)
{
    //QString newWCSFile = QDir::tempPath() + QString("/fitswcs%1").arg(QUuid::createUuid().toString().remove(QRegularExpression("[-{}]")));
    FITSData *imageData = alignView->getImageData();

    if (pahStage == PAH_FIND_CP)
    {
        setSolverAction(GOTO_NOTHING);
        appendLogText(
            i18n("Mount is synced to celestial pole. You can now continue Polar Alignment Assistant procedure."));
        pahStage = PAH_FIRST_CAPTURE;
        emit newPAHStage(pahStage);
        return;
    }

    if (pahStage == PAH_FIRST_CAPTURE)
    {
        // Set First PAH Center
        PAHImageInfo *solution = new PAHImageInfo();
        solution->skyCenter.setRA0(alignCoord.ra0());
        solution->skyCenter.setDec0(alignCoord.dec0());
        // J2000 to JNow
        solution->skyCenter.apparentCoord(J2000, KStarsData::Instance()->ut().djd());
        //solution->ts = KStarsData::Instance()->ut();
        solution->ts = imageData->getDateTime();
        solution->orientation = orientation;
        solution->pixelScale  = pixscale;

        pahImageInfos.append(solution);

        // Only invoke this if limited resource mode is false since we want to use CPU heavy WCS
        if (Options::limitedResourcesMode() == false)
        {
            appendLogText(i18n("Please wait while WCS data is processed..."));
            connect(alignView, &AlignView::wcsToggled, this, &Ekos::Align::setWCSToggled, Qt::UniqueConnection);
            alignView->injectWCS(orientation, ra, dec, pixscale);
            return;
        }

        pahStage = PAH_FIRST_ROTATE;
        emit newPAHStage(pahStage);

        PAHWidgets->setCurrentWidget(PAHFirstRotatePage);
        emit newPAHMessage(firstRotateText->text());

        rotatePAH();
    }
    else if (pahStage == PAH_SECOND_CAPTURE)
    {
        // Set 2nd PAH Center
        PAHImageInfo *solution = new PAHImageInfo();
        solution->skyCenter.setRA0(alignCoord.ra0());
        solution->skyCenter.setDec0(alignCoord.dec0());
        // J2000 to JNow
        solution->skyCenter.apparentCoord(J2000, KStarsData::Instance()->ut().djd());
        //solution->ts = KStarsData::Instance()->ut();
        solution->ts = imageData->getDateTime();
        solution->orientation = orientation;
        solution->pixelScale  = pixscale;

        pahImageInfos.append(solution);

        // Only invoke this if limited resource mode is false since we want to use CPU heavy WCS
        if (Options::limitedResourcesMode() == false)
        {
            appendLogText(i18n("Please wait while WCS data is processed..."));
            connect(alignView, &AlignView::wcsToggled, this, &Ekos::Align::setWCSToggled, Qt::UniqueConnection);
            alignView->injectWCS(orientation, ra, dec, pixscale);
            return;
        }

        pahStage = PAH_SECOND_ROTATE;
        emit newPAHStage(pahStage);

        PAHWidgets->setCurrentWidget(PAHSecondRotatePage);
        emit newPAHMessage(secondRotateText->text());

        rotatePAH();
    }
    else if (pahStage == PAH_THIRD_CAPTURE)
    {
        // Set Third PAH Center
        PAHImageInfo *solution = new PAHImageInfo();
        solution->skyCenter.setRA0(alignCoord.ra0());
        solution->skyCenter.setDec0(alignCoord.dec0());
        // J2000 to JNow
        solution->skyCenter.apparentCoord(J2000, KStarsData::Instance()->ut().djd());
        //solution->ts = KStarsData::Instance()->ut();
        solution->ts = imageData->getDateTime();
        solution->orientation = orientation;
        solution->pixelScale  = pixscale;

        pahImageInfos.append(solution);

        appendLogText(i18n("Please wait while WCS data is processed..."));
        connect(alignView, &AlignView::wcsToggled, this, &Ekos::Align::setWCSToggled, Qt::UniqueConnection);
        alignView->injectWCS(orientation, ra, dec, pixscale);
        return;
    }
}

void Align::setWCSToggled(bool result)
{
    appendLogText(i18n("WCS data processing is complete."));

    //alignView->disconnect(this);
    disconnect(alignView, &AlignView::wcsToggled, this, &Ekos::Align::setWCSToggled);

    if (pahStage == PAH_FIRST_CAPTURE)
    {
        // We need WCS to be synced first
        if (result == false && m_wcsSynced == true)
        {
            appendLogText(i18n("WCS info is now valid. Capturing next frame..."));
            pahImageInfos.clear();
            captureAndSolve();
            return;
        }

        // Find Celestial pole location
        SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);

        FITSData *imageData = alignView->getImageData();
        QPointF pixelPoint, imagePoint;

        bool rc = imageData->wcsToPixel(CP, pixelPoint, imagePoint);

        pahImageInfos[0]->celestialPole = pixelPoint;

        // TODO check if pixelPoint is located TOO far from the current position as well
        // i.e. if X > Width * 2..etc
        if (rc == false)
        {
            appendLogText(i18n("Failed to process World Coordinate System: %1. Try again.", imageData->getLastError()));
            return;
        }

        // If celestial pole out of range, ask the user if they want to move to it
        if (pixelPoint.x() < (-1 * imageData->width()) || pixelPoint.x() > (imageData->width() * 2) ||
                pixelPoint.y() < (-1 * imageData->height()) || pixelPoint.y() > (imageData->height() * 2))
        {
            // JM 2019-11-15: This creates more problems at times, better leave it off
#if 0
            if (currentTelescope->canSync() &&
                    KMessageBox::questionYesNo(
                        nullptr, i18n("Celestial pole is located outside of the field of view. Would you like to sync and slew "
                                      "the telescope to the celestial pole? WARNING: Slewing near poles may cause your mount to "
                                      "end up in unsafe position. Proceed with caution.")) == KMessageBox::Yes)
            {
                pahStage = PAH_FIND_CP;
                emit newPAHStage(pahStage);
                targetCoord.setRA(KStarsData::Instance()->lst()->Hours());
                targetCoord.setDec(CP.dec().Degrees() > 0 ? 89.5 : -89.5);

                qDeleteAll(pahImageInfos);
                pahImageInfos.clear();

                setSolverAction(GOTO_SLEW);
                Sync();
                return;
            }
            else
#endif
                appendLogText(
                    i18n("Warning: Celestial pole is located outside the field of view. Move the mount closer to the celestial pole."));
        }

        pahStage = PAH_FIRST_ROTATE;
        emit newPAHStage(pahStage);

        PAHWidgets->setCurrentWidget(PAHFirstRotatePage);
        emit newPAHMessage(firstRotateText->text());

        rotatePAH();
    }
    else if (pahStage == PAH_SECOND_CAPTURE)
    {
        // Find Celestial pole location
        SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);

        FITSData *imageData = alignView->getImageData();
        QPointF pixelPoint, imagePoint;
        imageData->wcsToPixel(CP, pixelPoint, imagePoint);
        pahImageInfos[1]->celestialPole = pixelPoint;

        pahStage = PAH_SECOND_ROTATE;
        emit newPAHStage(pahStage);

        PAHWidgets->setCurrentWidget(PAHSecondRotatePage);
        emit newPAHMessage(secondRotateText->text());

        rotatePAH();
    }
    else if (pahStage == PAH_THIRD_CAPTURE)
    {
        FITSData *imageData = alignView->getImageData();

        // Critical error
        if (result == false)
        {
            appendLogText(i18n("Failed to process World Coordinate System: %1. Try again.", imageData->getLastError()));
            return;
        }

        // Find Celestial pole location
        SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);

        QPointF imagePoint;

        imageData->wcsToPixel(CP, celestialPolePoint, imagePoint);

        pahImageInfos[2]->celestialPole = celestialPolePoint;

        // Because we are measuing the coordinates in the THIRD frame
        // The JNow RA/DE coordinates of the first two frames _already_ rotated by some amount since time passed.
        // So if we try to measure their cartesian coordinates now in the 3rd frame, they would be the coordinates
        // of the ROTATED coordinates, and not the original coordinates. Therefore, we subtract the time from RA
        // to compensate for this which is equivelent to de-rotating the points.
        double hoursSinceFirstFrame = (pahImageInfos[2]->ts.secsTo(pahImageInfos[0]->ts)) / 3600.0;
        pahImageInfos[0]->skyCenter.setRA(pahImageInfos[0]->skyCenter.ra().Hours() + hoursSinceFirstFrame);
        double hoursSinceSecondFrame = (pahImageInfos[2]->ts.secsTo(pahImageInfos[1]->ts)) / 3600.0;
        pahImageInfos[1]->skyCenter.setRA(pahImageInfos[1]->skyCenter.ra().Hours() + hoursSinceSecondFrame);

        // Now reset ra0,de0 to JNow before calling wcsToPixel since that function checks ra0,dec0
        for (int i = 0; i < 3; i++)
        {
            pahImageInfos[i]->skyCenter.setRA0(pahImageInfos[i]->skyCenter.ra());
            pahImageInfos[i]->skyCenter.setDec0(pahImageInfos[i]->skyCenter.dec());
        }

        // Now find pixel locations for all recorded center coordinates in the 3rd frame reference
        if (!imageData->wcsToPixel(pahImageInfos[0]->skyCenter, pahImageInfos[0]->pixelCenter, imagePoint) ||
                !imageData->wcsToPixel(pahImageInfos[1]->skyCenter, pahImageInfos[1]->pixelCenter, imagePoint) ||
                !imageData->wcsToPixel(pahImageInfos[2]->skyCenter, pahImageInfos[2]->pixelCenter, imagePoint))
        {
            appendLogText(i18n("WCS transformation failed: %1", imageData->getLastError()));
            stopPAHProcess();
            return;
        }

        qCDebug(KSTARS_EKOS_ALIGN) << "P1 RA: " << pahImageInfos[0]->skyCenter.ra0().toHMSString()
                                   << "DE: " << pahImageInfos[0]->skyCenter.dec0().toDMSString();
        qCDebug(KSTARS_EKOS_ALIGN) << "P2 RA: " << pahImageInfos[1]->skyCenter.ra0().toHMSString()
                                   << "DE: " << pahImageInfos[1]->skyCenter.dec0().toDMSString();
        qCDebug(KSTARS_EKOS_ALIGN) << "P3 RA: " << pahImageInfos[2]->skyCenter.ra0().toHMSString()
                                   << "DE: " << pahImageInfos[2]->skyCenter.dec0().toDMSString();

        qCDebug(KSTARS_EKOS_ALIGN) << "P1 X: " << pahImageInfos[0]->pixelCenter.x()
                                   << "Y: " << pahImageInfos[0]->pixelCenter.y();
        qCDebug(KSTARS_EKOS_ALIGN) << "P2 X: " << pahImageInfos[1]->pixelCenter.x()
                                   << "Y: " << pahImageInfos[1]->pixelCenter.y();
        qCDebug(KSTARS_EKOS_ALIGN) << "P3 X: " << pahImageInfos[2]->pixelCenter.x()
                                   << "Y: " << pahImageInfos[2]->pixelCenter.y();

        qCDebug(KSTARS_EKOS_ALIGN) << "P1 CP X: " << pahImageInfos[0]->celestialPole.x()
                                   << "CP Y: " << pahImageInfos[0]->celestialPole.y();
        qCDebug(KSTARS_EKOS_ALIGN) << "P2 CP X: " << pahImageInfos[1]->celestialPole.x()
                                   << "CP Y: " << pahImageInfos[1]->celestialPole.y();
        qCDebug(KSTARS_EKOS_ALIGN) << "P3 CP X: " << pahImageInfos[2]->celestialPole.x()
                                   << "CP Y: " << pahImageInfos[2]->celestialPole.y();

        // We have 3 points which uniquely defines a circle with its center representing the RA Axis
        // We have celestial pole location. So correction vector is just the vector between these two points
        calculatePAHError();

        pahStage = PAH_STAR_SELECT;
        emit newPAHStage(pahStage);

        PAHWidgets->setCurrentWidget(PAHCorrectionPage);
        emit newPAHMessage(correctionText->text());
    }
}

void Align::updateTelescopeType(int index)
{
    if (currentCCD == nullptr)
        return;

    // Reset style sheet.
    FocalLengthOut->setStyleSheet(QString());

    syncSettings();

    focal_length = (index == ISD::CCD::TELESCOPE_PRIMARY) ? primaryFL : guideFL;
    aperture = (index == ISD::CCD::TELESCOPE_PRIMARY) ? primaryAperture : guideAperture;

    Options::setSolverScopeType(index);

    syncTelescopeInfo();
}

// Function adapted from https://rosettacode.org/wiki/Circles_of_given_radius_through_two_points
Align::CircleSolution Align::findCircleSolutions(const QPointF &p1, const QPointF p2, double angle,
        QPair<QPointF, QPointF> &circleSolutions)
{
    QPointF solutionOne(1, 1), solutionTwo(1, 1);

    double radius = distance(p1, p2) / (dms::DegToRad * angle);

    if (p1 == p2)
    {
        if (angle == 0)
        {
            circleSolutions = qMakePair(p1, p2);
            appendLogText(i18n("Only one solution is found."));
            return ONE_CIRCLE_SOLUTION;
        }
        else
        {
            circleSolutions = qMakePair(solutionOne, solutionTwo);
            appendLogText(i18n("Infinite number of solutions found."));
            return INFINITE_CIRCLE_SOLUTION;
        }
    }

    QPointF center(p1.x() / 2 + p2.x() / 2, p1.y() / 2 + p2.y() / 2);

    double halfDistance = distance(center, p1);

    if (halfDistance > radius)
    {
        circleSolutions = qMakePair(solutionOne, solutionTwo);
        appendLogText(i18n("No solution is found. Points are too far away"));
        return NO_CIRCLE_SOLUTION;
    }

    if (halfDistance - radius == 0)
    {
        circleSolutions = qMakePair(center, solutionTwo);
        appendLogText(i18n("Only one solution is found."));
        return ONE_CIRCLE_SOLUTION;
    }

    double root = std::hypotf(radius, halfDistance) / distance(p1, p2);

    solutionOne.setX(center.x() + root * (p1.y() - p2.y()));
    solutionOne.setY(center.y() + root * (p2.x() - p1.x()));

    solutionTwo.setX(center.x() - root * (p1.y() - p2.y()));
    solutionTwo.setY(center.y() - root * (p2.x() - p1.x()));

    circleSolutions = qMakePair(solutionOne, solutionTwo);

    return TWO_CIRCLE_SOLUTION;
}

double Align::distance(const QPointF &p1, const QPointF &p2)
{
    return std::hypotf(p2.x() - p1.x(), p2.y() - p1.y());
}

bool Align::findRACircle(QVector3D &RACircle)
{
    bool rc = false;

    QPointF p1 = pahImageInfos[0]->pixelCenter;
    QPointF p2 = pahImageInfos[1]->pixelCenter;
    QPointF p3 = pahImageInfos[2]->pixelCenter;

    if (!isPerpendicular(p1, p2, p3))
        rc = calcCircle(p1, p2, p3, RACircle);
    else if (!isPerpendicular(p1, p3, p2))
        rc = calcCircle(p1, p3, p2, RACircle);
    else if (!isPerpendicular(p2, p1, p3))
        rc = calcCircle(p2, p1, p3, RACircle);
    else if (!isPerpendicular(p2, p3, p1))
        rc = calcCircle(p2, p3, p1, RACircle);
    else if (!isPerpendicular(p3, p2, p1))
        rc = calcCircle(p3, p2, p1, RACircle);
    else if (!isPerpendicular(p3, p1, p2))
        rc = calcCircle(p3, p1, p2, RACircle);
    else
    {
        //TRACE("\nThe three pts are perpendicular to axis\n");
        return false;
    }

    return rc;
}

bool Align::isPerpendicular(const QPointF &p1, const QPointF &p2, const QPointF &p3)
// Check the given point are perpendicular to x or y axis
{
    double yDelta_a = p2.y() - p1.y();
    double xDelta_a = p2.x() - p1.x();
    double yDelta_b = p3.y() - p2.y();
    double xDelta_b = p3.x() - p2.x();

    // checking whether the line of the two pts are vertical
    if (fabs(xDelta_a) <= 0.000000001 && fabs(yDelta_b) <= 0.000000001)
    {
        //TRACE("The points are perpendicular and parallel to x-y axis\n");
        return false;
    }

    if (fabs(yDelta_a) <= 0.0000001)
    {
        //TRACE(" A line of two point are perpendicular to x-axis 1\n");
        return true;
    }
    else if (fabs(yDelta_b) <= 0.0000001)
    {
        //TRACE(" A line of two point are perpendicular to x-axis 2\n");
        return true;
    }
    else if (fabs(xDelta_a) <= 0.000000001)
    {
        //TRACE(" A line of two point are perpendicular to y-axis 1\n");
        return true;
    }
    else if (fabs(xDelta_b) <= 0.000000001)
    {
        //TRACE(" A line of two point are perpendicular to y-axis 2\n");
        return true;
    }
    else
        return false;
}

bool Align::calcCircle(const QPointF &p1, const QPointF &p2, const QPointF &p3, QVector3D &RACircle)
{
    double yDelta_a = p2.y() - p1.y();
    double xDelta_a = p2.x() - p1.x();
    double yDelta_b = p3.y() - p2.y();
    double xDelta_b = p3.x() - p2.x();

    if (fabs(xDelta_a) <= 0.000000001 && fabs(yDelta_b) <= 0.000000001)
    {
        RACircle.setX(0.5 * (p2.x() + p3.x()));
        RACircle.setY(0.5 * (p1.y() + p2.y()));
        QPointF center(RACircle.x(), RACircle.y());
        RACircle.setZ(distance(center, p1));
        return true;
    }

    // IsPerpendicular() assure that xDelta(s) are not zero
    double aSlope = yDelta_a / xDelta_a; //
    double bSlope = yDelta_b / xDelta_b;
    if (fabs(aSlope - bSlope) <= 0.000000001)
    {
        // checking whether the given points are colinear.
        //TRACE("The three ps are colinear\n");
        return false;
    }

    // calc center
    RACircle.setX((aSlope * bSlope * (p1.y() - p3.y()) + bSlope * (p1.x() + p2.x()) - aSlope * (p2.x() + p3.x())) /
                  (2 * (bSlope - aSlope)));
    RACircle.setY(-1 * (RACircle.x() - (p1.x() + p2.x()) / 2) / aSlope + (p1.y() + p2.y()) / 2);
    QPointF center(RACircle.x(), RACircle.y());

    RACircle.setZ(distance(center, p1));
    return true;
}

void Align::setMountStatus(ISD::Telescope::Status newState)
{
    switch (newState)
    {
        case ISD::Telescope::MOUNT_PARKING:
        case ISD::Telescope::MOUNT_SLEWING:
        case ISD::Telescope::MOUNT_MOVING:
            solveB->setEnabled(false);
            loadSlewB->setEnabled(false);
            PAHStartB->setEnabled(false);
            break;

        default:
            if (state != ALIGN_PROGRESS)
            {
                solveB->setEnabled(true);
                if (pahStage == PAH_IDLE)
                {
                    PAHStartB->setEnabled(true);
                    loadSlewB->setEnabled(true);
                }
            }
            break;
    }
}

void Align::setAstrometryDevice(ISD::GDInterface *newAstrometry)
{
    remoteParserDevice = newAstrometry;

    remoteSolverR->setEnabled(true);
    if (remoteParser.get() != nullptr)
    {
        remoteParser->setAstrometryDevice(remoteParserDevice);
        connect(remoteParser.get(), &AstrometryParser::solverFinished, this, &Ekos::Align::solverFinished, Qt::UniqueConnection);
        connect(remoteParser.get(), &AstrometryParser::solverFailed, this, &Ekos::Align::solverFailed, Qt::UniqueConnection);
    }
}

void Align::setRotator(ISD::GDInterface *newRotator)
{
    currentRotator = newRotator;
    connect(currentRotator, &ISD::GDInterface::numberUpdated, this, &Ekos::Align::processNumber, Qt::UniqueConnection);
}

void Align::refreshAlignOptions()
{
    solverFOV->setImageDisplay(Options::astrometrySolverWCS());
    m_AlignTimer.setInterval(Options::astrometryTimeout() * 1000);
}

void Align::setFilterManager(const QSharedPointer<FilterManager> &manager)
{
    filterManager = manager;

    connect(filterManager.data(), &FilterManager::ready, [this]()
    {
        if (filterPositionPending)
        {
            m_FocusState = FOCUS_IDLE;
            filterPositionPending = false;
            captureAndSolve();
        }
    }
           );

    connect(filterManager.data(), &FilterManager::failed, [this]()
    {
        appendLogText(i18n("Filter operation failed."));
        abort();
    }
           );

    connect(filterManager.data(), &FilterManager::newStatus, [this](Ekos::FilterState filterState)
    {
        if (filterPositionPending)
        {
            switch (filterState)
            {
                case FILTER_OFFSET:
                    appendLogText(i18n("Changing focus offset by %1 steps...", filterManager->getTargetFilterOffset()));
                    break;

                case FILTER_CHANGE:
                {
                    const int filterComboIndex = filterManager->getTargetFilterPosition() - 1;
                    if (filterComboIndex >= 0 && filterComboIndex < FilterPosCombo->count())
                        appendLogText(i18n("Changing filter to %1...", FilterPosCombo->itemText(filterComboIndex)));
                }
                break;

                case FILTER_AUTOFOCUS:
                    appendLogText(i18n("Auto focus on filter change..."));
                    break;

                default:
                    break;
            }
        }
    });

    connect(filterManager.data(), &FilterManager::labelsChanged, this, [this]()
    {
        checkFilter();
    });
    connect(filterManager.data(), &FilterManager::positionChanged, this, [this]()
    {
        checkFilter();
    });
}

QVariantMap Align::getEffectiveFOV()
{
    KStarsData::Instance()->userdb()->GetAllEffectiveFOVs(effectiveFOVs);

    fov_x = fov_y = 0;

    for (auto &map : effectiveFOVs)
    {
        if (map["Profile"].toString() == m_ActiveProfile->name)
        {
            if (map["Width"].toInt() == ccd_width &&
                    map["Height"].toInt() == ccd_height &&
                    map["PixelW"].toDouble() == ccd_hor_pixel &&
                    map["PixelH"].toDouble() == ccd_ver_pixel &&
                    map["FocalLength"].toDouble() == focal_length)
            {
                fov_x = map["FovW"].toDouble();
                fov_y = map["FovH"].toDouble();
                return map;
            }
        }
    }

    return QVariantMap();
}

void Align::saveNewEffectiveFOV(double newFOVW, double newFOVH)
{
    if (newFOVW < 0 || newFOVH < 0 || (newFOVW == fov_x && newFOVH == fov_y))
        return;

    QVariantMap effectiveMap = getEffectiveFOV();

    // If ID exists, delete it first.
    if (effectiveMap.isEmpty() == false)
        KStarsData::Instance()->userdb()->DeleteEffectiveFOV(effectiveMap["id"].toString());

    // If FOV is 0x0, then we just remove existing effective FOV
    if (newFOVW == 0.0 && newFOVH == 0.0)
    {
        calculateFOV();
        return;
    }

    effectiveMap["Profile"] = m_ActiveProfile->name;
    effectiveMap["Width"] = ccd_width;
    effectiveMap["Height"] = ccd_height;
    effectiveMap["PixelW"] = ccd_hor_pixel;
    effectiveMap["PixelH"] = ccd_ver_pixel;
    effectiveMap["FocalLength"] = focal_length;
    effectiveMap["FovW"] = newFOVW;
    effectiveMap["FovH"] = newFOVH;

    KStarsData::Instance()->userdb()->AddEffectiveFOV(effectiveMap);

    calculateFOV();

}

int Align::getActiveSolver() const
{
    return Options::solverType();
}

QString Align::getPAHMessage() const
{
    switch (pahStage)
    {
        case PAH_IDLE:
        case PAH_FIND_CP:
            return introText->text();
        case PAH_FIRST_CAPTURE:
            return firstCaptureText->text();
        case PAH_FIRST_ROTATE:
            return firstRotateText->text();
        case PAH_SECOND_CAPTURE:
            return secondCaptureText->text();
        case PAH_SECOND_ROTATE:
            return secondRotateText->text();
        case PAH_THIRD_CAPTURE:
            return thirdCaptureText->text();
        case PAH_STAR_SELECT:
            return correctionText->text();
        case PAH_PRE_REFRESH:
        case PAH_REFRESH:
            return refreshText->text();
        case PAH_ERROR:
            return PAHErrorDescriptionLabel->text();
    }

    return QString();
}

void Align::zoomAlignView()
{
    alignView->ZoomDefault();

    emit newFrame(alignView);
}

QJsonObject Align::getSettings() const
{
    QJsonObject settings;

    double gain = -1;
    if (GainSpinSpecialValue > INVALID_VALUE && GainSpin->value() > GainSpinSpecialValue)
        gain = GainSpin->value();
    else if (currentCCD && currentCCD->hasGain())
        currentCCD->getGain(&gain);

    settings.insert("camera", CCDCaptureCombo->currentText());
    settings.insert("fw", FilterDevicesCombo->currentText());
    settings.insert("filter", FilterPosCombo->currentText());
    settings.insert("exp", exposureIN->value());
    settings.insert("bin", qMax(1, binningCombo->currentIndex() + 1));
    settings.insert("solverAction", gotoModeButtonGroup->checkedId());
    settings.insert("scopeType", FOVScopeCombo->currentIndex());
    settings.insert("gain", gain);
    settings.insert("iso", ISOCombo->currentIndex());
    settings.insert("accuracy", accuracySpin->value());
    settings.insert("settle", delaySpin->value());
    settings.insert("dark", alignDarkFrameCheck->isChecked());

    return settings;
}

void Align::setSettings(const QJsonObject &settings)
{
    auto syncControl = [settings](const QString & key, QWidget * widget)
    {
        QSpinBox *pSB = nullptr;
        QDoubleSpinBox *pDSB = nullptr;
        QCheckBox *pCB = nullptr;
        QComboBox *pComboBox = nullptr;

        if ((pSB = qobject_cast<QSpinBox *>(widget)))
        {
            const int value = settings[key].toInt(pSB->value());
            if (value != pSB->value())
            {
                pSB->setValue(value);
                return true;
            }
        }
        else if ((pDSB = qobject_cast<QDoubleSpinBox *>(widget)))
        {
            const double value = settings[key].toDouble(pDSB->value());
            if (value != pDSB->value())
            {
                pDSB->setValue(value);
                return true;
            }
        }
        else if ((pCB = qobject_cast<QCheckBox *>(widget)))
        {
            const bool value = settings[key].toBool(pCB->isChecked());
            if (value != pCB->isChecked())
            {
                pCB->setChecked(value);
                return true;
            }
        }
        // ONLY FOR STRINGS, not INDEX
        else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
        {
            const QString value = settings[key].toString(pComboBox->currentText());
            if (value != pComboBox->currentText())
            {
                pComboBox->setCurrentText(value);
                return true;
            }
        }

        return false;
    };

    // Camera. If camera changed, check CCD.
    if (syncControl("camera", CCDCaptureCombo))
        checkCCD();
    // Filter Wheel
    if (syncControl("fw", FilterDevicesCombo))
        checkFilter();
    // Filter
    syncControl("filter", FilterPosCombo);
    Options::setLockAlignFilterIndex(FilterPosCombo->currentIndex());
    // Exposure
    syncControl("exp", exposureIN);
    // Binning
    const int bin = settings["bin"].toInt(binningCombo->currentIndex() + 1) - 1;
    if (bin != binningCombo->currentIndex())
        binningCombo->setCurrentIndex(bin);
    // Profiles
    const QString profileName = settings["sep"].toString();
    QStringList profiles = getStellarSolverProfiles();
    int profileIndex = getStellarSolverProfiles().indexOf(profileName);
    if (profileIndex >= 0 && profileIndex != static_cast<int>(Options::solveOptionsProfile()))
        Options::setSolveOptionsProfile(profileIndex);

    int solverAction = settings["solverAction"].toInt(gotoModeButtonGroup->checkedId());
    if (solverAction != gotoModeButtonGroup->checkedId())
        gotoModeButtonGroup->button(solverAction)->setChecked(true);

    FOVScopeCombo->setCurrentIndex(settings["scopeType"].toInt(0));

    // Gain
    if (GainSpin->isEnabled())
    {
        syncControl("gain", GainSpin);
    }
    // ISO
    if (ISOCombo->isEnabled())
    {
        const int iso = settings["iso"].toInt(ISOCombo->currentIndex());
        if (iso != ISOCombo->currentIndex())
            ISOCombo->setCurrentIndex(iso);
    }
    // Dark
    syncControl("dark", alignDarkFrameCheck);
    // Accuracy
    syncControl("accuracy", accuracySpin);
    // Settle
    syncControl("settle", delaySpin);
}

void Align::syncSettings()
{
    emit settingsUpdated(getSettings());
}

QJsonObject Align::getPAHSettings() const
{
    QJsonObject settings = getSettings();

    settings.insert("mountDirection", PAHDirectionCombo->currentIndex());
    settings.insert("mountSpeed", PAHSlewRateCombo->currentIndex());
    settings.insert("mountRotation", PAHRotationSpin->value());
    settings.insert("refresh", PAHExposure->value());
    settings.insert("manualslew", PAHManual->isChecked());

    return settings;
}

void Align::setPAHSettings(const QJsonObject &settings)
{
    setSettings(settings);

    PAHDirectionCombo->setCurrentIndex(settings["mountDirection"].toInt(0));
    PAHRotationSpin->setValue(settings["mountRotation"].toInt(30));
    PAHExposure->setValue(settings["refresh"].toDouble(1));
    if (settings.contains("mountSpeed"))
        PAHSlewRateCombo->setCurrentIndex(settings["mountSpeed"].toInt(0));
    PAHManual->setChecked(settings["manualslew"].toBool(false));
}

void Align::syncFOV()
{
    QString newFOV = FOVOut->text();
    QRegularExpression re("(\\d+\\.*\\d*)\\D*x\\D*(\\d+\\.*\\d*)");
    QRegularExpressionMatch match = re.match(newFOV);
    if (match.hasMatch())
    {
        double newFOVW = match.captured(1).toDouble();
        double newFOVH = match.captured(2).toDouble();

        //if (newFOVW > 0 && newFOVH > 0)
        saveNewEffectiveFOV(newFOVW, newFOVH);

        FOVOut->setStyleSheet(QString());
    }
    else
    {
        KSNotification::error(i18n("Invalid FOV."));
        FOVOut->setStyleSheet("background-color:red");
    }
}

// m_wasSlewStarted can't be false for more than 10s after a slew starts.
bool Align::didSlewStart()
{
    if (m_wasSlewStarted)
        return true;
    if (slewStartTimer.isValid() && slewStartTimer.elapsed() > MAX_WAIT_FOR_SLEW_START_MSEC)
    {
        qCDebug(KSTARS_EKOS_ALIGN) << "Slew timed out...waited > 10s, it must have started already.";
        return true;
    }
    return false;
}

void Align::setTargetCoords(double ra, double de)
{
    targetCoord.setRA0(ra);
    targetCoord.setDec0(de);
    targetCoord.updateCoordsNow(KStarsData::Instance()->updateNum());

    qCDebug(KSTARS_EKOS_ALIGN) << "Target Coordinates updated to RA:" << targetCoord.ra().toHMSString()
                               << "DE:" << targetCoord.dec().toDMSString();
}

void Align::calculateAlignTargetDiff()
{
    m_TargetDiffRA = (alignCoord.ra().deltaAngle(targetCoord.ra())).Degrees() * 3600;
    m_TargetDiffDE = (alignCoord.dec().deltaAngle(targetCoord.dec())).Degrees() * 3600;

    dms RADiff(fabs(m_TargetDiffRA) / 3600.0), DEDiff(m_TargetDiffDE / 3600.0);
    QString dRAText = QString("%1%2").arg((m_TargetDiffRA > 0 ? "+" : "-"), RADiff.toHMSString());
    QString dDEText = DEDiff.toDMSString(true);

    m_TargetDiffTotal = sqrt(m_TargetDiffRA * m_TargetDiffRA + m_TargetDiffDE * m_TargetDiffDE);

    errOut->setText(QString("%1 arcsec. RA:%2 DE:%3").arg(
                        QString::number(m_TargetDiffTotal, 'f', 0),
                        QString::number(m_TargetDiffRA, 'f', 0),
                        QString::number(m_TargetDiffDE, 'f', 0)));
    if (m_TargetDiffTotal <= static_cast<double>(accuracySpin->value()))
        errOut->setStyleSheet("color:green");
    else if (m_TargetDiffTotal < 1.5 * accuracySpin->value())
        errOut->setStyleSheet("color:yellow");
    else
        errOut->setStyleSheet("color:red");

    //This block of code will write the result into the solution table and plot it on the graph.
    int currentRow = solutionTable->rowCount() - 1;
    QTableWidgetItem *dRAReport = new QTableWidgetItem();
    if (dRAReport)
    {
        dRAReport->setText(QString::number(m_TargetDiffRA, 'f', 3) + "\"");
        dRAReport->setTextAlignment(Qt::AlignHCenter);
        dRAReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 4, dRAReport);
    }

    QTableWidgetItem *dDECReport = new QTableWidgetItem();
    if (dDECReport)
    {
        dDECReport->setText(QString::number(m_TargetDiffDE, 'f', 3) + "\"");
        dDECReport->setTextAlignment(Qt::AlignHCenter);
        dDECReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 5, dDECReport);
    }

    double raPlot  = m_TargetDiffRA;
    double decPlot = m_TargetDiffDE;
    alignPlot->graph(0)->addData(raPlot, decPlot);

    QCPItemText *textLabel = new QCPItemText(alignPlot);
    textLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    textLabel->position->setType(QCPItemPosition::ptPlotCoords);
    textLabel->position->setCoords(raPlot, decPlot);
    textLabel->setColor(Qt::red);
    textLabel->setPadding(QMargins(0, 0, 0, 0));
    textLabel->setBrush(Qt::white);
    textLabel->setPen(Qt::NoPen);
    textLabel->setText(' ' + QString::number(solutionTable->rowCount()) + ' ');
    textLabel->setFont(QFont(font().family(), 8));

    if (!alignPlot->xAxis->range().contains(m_TargetDiffRA))
    {
        alignPlot->graph(0)->rescaleKeyAxis(true);
        alignPlot->yAxis->setScaleRatio(alignPlot->xAxis, 1.0);
    }
    if (!alignPlot->yAxis->range().contains(m_TargetDiffDE))
    {
        alignPlot->graph(0)->rescaleValueAxis(true);
        alignPlot->xAxis->setScaleRatio(alignPlot->yAxis, 1.0);
    }
    alignPlot->replot();
}

void Align::syncTargetToMount()
{
    double ra, dec;
    currentTelescope->getEqCoords(&ra, &dec);
    targetCoord.setRA(ra);
    targetCoord.setDec(dec);
    // While we can set targetCoord = J2000Coord now, it's better to use setTargetCoord
    // Function to do that in case of any changes in the future in that function.
    SkyPoint J2000Coord = targetCoord.catalogueCoord(KStarsData::Instance()->lt().djd());
    setTargetCoords(J2000Coord.ra0().Hours(), J2000Coord.dec0().Degrees());
}

QStringList Align::getStellarSolverProfiles()
{
    QStringList profiles;
    for (auto param : m_StellarSolverProfiles)
        profiles << param.listName;

    return profiles;
}


}

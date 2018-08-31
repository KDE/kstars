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
#include "ksuserdb.h"
#include "offlineastrometryparser.h"
#include "onlineastrometryparser.h"
#include "opsalign.h"
#include "opsastrometry.h"
#include "opsastrometrycfg.h"
#include "opsastrometryindexfiles.h"
#include "Options.h"
#include "remoteastrometryparser.h"
#include "skymap.h"
#include "skymapcomposite.h"
#include "starobject.h"
#include "auxiliary/QProgressIndicator.h"
#include "dialogs/finddialog.h"
#include "ekos/ekosmanager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitstab.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/indifilter.h"
#include "profileinfo.h"
#include "ksnotification.h"

#include <KConfigDialog>
#include <KActionCollection>

#include <basedevice.h>

#include <memory>

#include <ekos_align_debug.h>

#define PAH_CUTOFF_FOV            30 // Minimum FOV width in arcminutes for PAH to work
#define MAXIMUM_SOLVER_ITERATIONS 10

#define AL_FORMAT_VERSION 1.0

namespace Ekos
{
// 30 arcmiutes RA movement
const double Align::RAMotion = 0.5;
// Sidereal rate, degrees/s
const double Align::SIDRATE = 0.004178;

const QMap<Align::PAHStage, QString> Align::PAHStages = {
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
    new AlignAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Align", this);

    dirPath = QDir::homePath();

    //loadSlewMode = false;
    solverFOV.reset(new FOV());
    solverFOV->setName(i18n("Solver FOV"));
    solverFOV->setLockCelestialPole(true);
    solverFOV->setColor(KStars::Instance()->data()->colorScheme()->colorNamed("SolverFOVColor").name());

    sensorFOV.reset(new FOV());
    sensorFOV->setLockCelestialPole(true);

    QAction *a = KStars::Instance()->actionCollection()->action("show_sensor_fov");
    if (a)
        a->setEnabled(true);

    showFITSViewerB->setIcon(
                QIcon::fromTheme("kstars_fitsviewer"));
    showFITSViewerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(showFITSViewerB, SIGNAL(clicked()), this, SLOT(showFITSViewer()));

    toggleFullScreenB->setIcon(
                QIcon::fromTheme("view-fullscreen"));
    toggleFullScreenB->setShortcut(Qt::Key_F4);
    toggleFullScreenB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(toggleFullScreenB, SIGNAL(clicked()), this, SLOT(toggleAlignWidgetFullScreen()));

    alignView = new AlignView(alignWidget, FITS_ALIGN);
    alignView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    alignView->setBaseSize(alignWidget->size());
    alignView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();

    vlayout->addWidget(alignView);
    alignWidget->setLayout(vlayout);

    connect(solveB, SIGNAL(clicked()), this, SLOT(captureAndSolve()));
    connect(stopB, SIGNAL(clicked()), this, SLOT(abort()));
    connect(measureAltB, SIGNAL(clicked()), this, SLOT(measureAltError()));
    connect(measureAzB, SIGNAL(clicked()), this, SLOT(measureAzError()));

    alignTimer.setInterval(10000);
    alignTimer.setSingleShot(true);
    connect(&alignTimer, &QTimer::timeout, this, &Align::captureAndSolve);

    // Effective FOV Edit
    connect(FOVOut, &QLineEdit::editingFinished, this, &Align::syncFOV);

    connect(CCDCaptureCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultCCD(QString)));
    connect(CCDCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkCCD(int)));

    connect(correctAltB, SIGNAL(clicked()), this, SLOT(correctAltError()));
    connect(correctAzB, SIGNAL(clicked()), this, SLOT(correctAzError()));
    connect(loadSlewB, SIGNAL(clicked()), this, SLOT(loadAndSlew()));

    FilterDevicesCombo->addItem("--");
    connect(FilterDevicesCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated),
            [=](const QString &text)
    {
        syncSettings();
        Options::setDefaultAlignFW(text);
    });

    connect(FilterDevicesCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));

    connect(FilterPosCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            [=](int index)
    {
        syncSettings();
        Options::setLockAlignFilterIndex(index);
    }
    );

    gotoModeButtonGroup->setId(syncR, GOTO_SYNC);
    gotoModeButtonGroup->setId(slewR, GOTO_SLEW);
    gotoModeButtonGroup->setId(nothingR, GOTO_NOTHING);

    connect(gotoModeButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this,
            [=](int id) { this->currentGotoMode = static_cast<GotoMode>(id); });

    alignTimer.setInterval(Options::astrometryTimeout() * 1000);
    connect(&alignTimer, SIGNAL(timeout()), this, SLOT(checkAlignmentTimeout()));

    currentGotoMode = static_cast<GotoMode>(Options::solverGotoOption());
    gotoModeButtonGroup->button(currentGotoMode)->setChecked(true);

    editOptionsB->setIcon(QIcon::fromTheme("document-edit"));
    editOptionsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    KConfigDialog *dialog = new KConfigDialog(this, "alignsettings", Options::self());

#ifdef Q_OS_OSX
    dialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    opsAlign = new OpsAlign(this);
    connect(opsAlign, SIGNAL(settingsUpdated()), this, SLOT(refreshAlignOptions()));
    KPageWidgetItem *page = dialog->addPage(opsAlign, i18n("Astrometry.net"));
    page->setIcon(QIcon(":/icons/astrometry.svg"));

    opsAstrometry = new OpsAstrometry(this);
    page = dialog->addPage(opsAstrometry, i18n("Solver Options"));
    page->setIcon(QIcon::fromTheme("configure"));

#ifdef Q_OS_OSX
    opsAstrometryCfg = new OpsAstrometryCfg(this);
    page = dialog->addPage(opsAstrometryCfg, i18n("Astrometry.cfg"));
    page->setIcon(QIcon::fromTheme("document-edit"));
#endif

#ifndef Q_OS_WIN
    opsAstrometryIndexFiles = new OpsAstrometryIndexFiles(this);
    page = dialog->addPage(opsAstrometryIndexFiles, i18n("Index Files"));
    page->setIcon(QIcon::fromTheme("map-flat"));
#endif

    connect(editOptionsB, SIGNAL(clicked()), dialog, SLOT(show()));

    appendLogText(i18n("Idle."));

    pi.reset(new QProgressIndicator(this));

    stopLayout->addWidget(pi.get());

    exposureIN->setValue(Options::alignExposure());
    connect(exposureIN, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [&]() { syncSettings();});

    altStage = ALT_INIT;
    azStage  = AZ_INIT;

    rememberSolverWCS = Options::astrometrySolverWCS();
    rememberAutoWCS   = Options::autoWCS();

    // Online/Offline/Remote solver check
    solverTypeGroup->setId(onlineSolverR, SOLVER_ONLINE);
    solverTypeGroup->setId(offlineSolverR, SOLVER_OFFLINE);
    solverTypeGroup->setId(remoteSolverR, SOLVER_REMOTE);
#ifdef Q_OS_WIN
    offlineSolverR->setEnabled(false);
    offlineSolverR->setToolTip(
                i18n("Offline solver is not supported under Windows. Please use either the Online or Remote solvers."));
#endif
    solverTypeGroup->button(Options::solverType())->setChecked(true);
    connect(solverTypeGroup, SIGNAL(buttonClicked(int)), SLOT(setSolverType(int)));

    switch (solverTypeGroup->checkedId())
    {
    case SOLVER_ONLINE:
        onlineParser.reset(new Ekos::OnlineAstrometryParser());
        parser = onlineParser.get();
        break;

    case SOLVER_OFFLINE:
        offlineParser.reset(new OfflineAstrometryParser());
        parser = offlineParser.get();
        break;

    case SOLVER_REMOTE:
        remoteParser.reset(new RemoteAstrometryParser());
        parser = remoteParser.get();
        break;
    }

    parser->setAlign(this);
    if (parser->init() == false)
        setEnabled(false);
    else
    {
        connect(parser, SIGNAL(solverFinished(double,double,double,double)), this,
                SLOT(solverFinished(double,double,double,double)), Qt::UniqueConnection);
        connect(parser, SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }

    //solverOptions->setText(Options::solverOptions());

    // Which telescope info to use for FOV calculations
    //kcfg_solverOTA->setChecked(Options::solverOTA());
    //guideScopeCCDs = Options::guideScopeCCDs();
    FOVScopeCombo->setCurrentIndex(Options::solverScopeType());
    connect(FOVScopeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateTelescopeType(int)));
    //connect(FOVScopeCombo, SIGNAL(currentIndexChanged(int)), this, SIGNAL(newFOVTelescopeType(int)));

    accuracySpin->setValue(Options::solverAccuracyThreshold());
    alignDarkFrameCheck->setChecked(Options::alignDarkFrame());

    delaySpin->setValue(Options::settlingTime());
    connect(delaySpin, SIGNAL(editingFinished()), this, SLOT(saveSettleTime()));

    connect(binningCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setBinningIndex(int)));

    // PAH Connections
    connect(this, &Align::PAHEnabled, [&](bool enabled) {
        PAHStartB->setEnabled(enabled);
        directionLabel->setEnabled(enabled);
        PAHDirectionCombo->setEnabled(enabled);
        PAHRotationSpin->setEnabled(enabled);
    });
    connect(PAHStartB, SIGNAL(clicked()), this, SLOT(startPAHProcess()));
    // PAH StopB is just a shortcut for the regular stop
    connect(PAHStopB, &QPushButton::clicked, this, &Align::stopPAHProcess);
    connect(PAHCorrectionsNextB, SIGNAL(clicked()), this, SLOT(setPAHCorrectionSelectionComplete()));
    connect(PAHRefreshB, SIGNAL(clicked()), this, SLOT(startPAHRefreshProcess()));
    connect(PAHDoneB, SIGNAL(clicked()), this, SLOT(setPAHRefreshComplete()));

    if (solverOptions->text().contains("no-fits2fits"))
        appendLogText(i18n(
                          "Warning: If using astrometry.net v0.68 or above, remove the --no-fits2fits from the astrometry options."));

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

    connect(alignPlot, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(handlePointTooltip(QMouseEvent*)));
    connect(rightLayout, SIGNAL(splitterMoved(int,int)), this, SLOT(handleVerticalPlotSizeChange()));
    connect(alignSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(handleHorizontalPlotSizeChange()));
    connect(accuracySpin, SIGNAL(valueChanged(int)), this, SLOT(buildTarget()));

    alignPlot->resize(190, 190);
    alignPlot->replot();

    solutionTable->setColumnWidth(0, 70);
    solutionTable->setColumnWidth(1, 75);
    solutionTable->setColumnWidth(2, 80);
    solutionTable->setColumnWidth(3, 30);
    solutionTable->setColumnWidth(4, 100);
    solutionTable->setColumnWidth(5, 100);

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
    connect(mountModel.alignTable->verticalHeader(), SIGNAL(sectionMoved(int,int,int)), this,
            SLOT(moveAlignPoint(int,int,int)));

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

    connect(clearAllSolutionsB, SIGNAL(clicked()), this, SLOT(slotClearAllSolutionPoints()));
    connect(removeSolutionB, SIGNAL(clicked()), this, SLOT(slotRemoveSolutionPoint()));
    connect(exportSolutionsCSV, SIGNAL(clicked()), this, SLOT(exportSolutionPoints()));
    connect(autoScaleGraphB, SIGNAL(clicked()), this, SLOT(slotAutoScaleGraph()));
    connect(mountModelB, SIGNAL(clicked()), this, SLOT(slotMountModel()));
    connect(solutionTable, SIGNAL(cellClicked(int,int)), this, SLOT(selectSolutionTableRow(int,int)));

    connect(mountModel.wizardAlignB, SIGNAL(clicked()), this, SLOT(slotWizardAlignmentPoints()));
    connect(mountModel.alignTypeBox, SIGNAL(currentIndexChanged(QString)), this,
            SLOT(alignTypeChanged(QString)));

    connect(mountModel.starListBox, SIGNAL(currentIndexChanged(QString)), this,
            SLOT(slotStarSelected(QString)));
    connect(mountModel.greekStarListBox, SIGNAL(currentIndexChanged(QString)), this,
            SLOT(slotStarSelected(QString)));

    connect(mountModel.loadAlignB, SIGNAL(clicked()), this, SLOT(slotLoadAlignmentPoints()));
    connect(mountModel.saveAsAlignB, SIGNAL(clicked()), this, SLOT(slotSaveAsAlignmentPoints()));
    connect(mountModel.saveAlignB, SIGNAL(clicked()), this, SLOT(slotSaveAlignmentPoints()));
    connect(mountModel.clearAllAlignB, SIGNAL(clicked()), this, SLOT(slotClearAllAlignPoints()));
    connect(mountModel.removeAlignB, SIGNAL(clicked()), this, SLOT(slotRemoveAlignPoint()));
    connect(mountModel.addAlignB, SIGNAL(clicked()), this, SLOT(slotAddAlignPoint()));
    connect(mountModel.findAlignB, SIGNAL(clicked()), this, SLOT(slotFindAlignObject()));
    connect(mountModel.sortAlignB, SIGNAL(clicked()), this, SLOT(slotSortAlignmentPoints()));

    connect(mountModel.previewB, SIGNAL(clicked()), this, SLOT(togglePreviewAlignPoints()));
    connect(mountModel.stopAlignB, SIGNAL(clicked()), this, SLOT(resetAlignmentProcedure()));
    connect(mountModel.startAlignB, SIGNAL(clicked()), this, SLOT(startStopAlignmentProcedure()));

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);
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
    Q_UNUSED(column);

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
        QTimer::singleShot(10, this, SLOT(handleHorizontalPlotSizeChange()));
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
                               tr("<table>"
                                  "<tr>"
                                  "<th colspan=\"2\">Object %L1: %L2</th>"
                                  "</tr>"
                                  "<tr>"
                                  "<td>RA:</td><td>%L3</td>"
                                  "</tr>"
                                  "<tr>"
                                  "<td>DE:</td><td>%L4</td>"
                                  "</tr>"
                                  "<tr>"
                                  "<td>dRA:</td><td>%L5</td>"
                                  "</tr>"
                                  "<tr>"
                                  "<td>dDE:</td><td>%L6</td>"
                                  "</tr>"
                                  "</table>")
                               .arg(point + 1)
                               .arg(solutionTable->item(point, 2)->text(),
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
        double theta = i / (double)(pointCount)*2 * M_PI;

        for (double ring = 1; ring < 8; ring++)
        {
            if (ring != 4 && ring != 6)
            {
                if (i % (9 - (int)ring) == 0) //This causes fewer points to draw on the inner circles.
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

    if (mountModel.alignTypeBox->currentText() == "Fixed DEC")
    {
        double decAngle = mountModel.alignDec->value();
        //Dec that never rises.
        if (lat > 0)
        {
            if (decAngle < lat - 90 + minAlt) //Min altitude possible at minAlt deg above horizon
            {
                KMessageBox::sorry(0, i18n("DEC is below the altitude limit"));
                return;
            }
        }
        else
        {
            if (decAngle > lat + 90 - minAlt) //Max altitude possible at minAlt deg above horizon
            {
                KMessageBox::sorry(0, i18n("DEC is below the altitude limit"));
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

    if (mountModel.alignTypeBox->currentText() == "Fixed DEC")
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

        if (mountModel.alignTypeBox->currentText() == "Fixed DEC")
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
            KMessageBox::sorry(0, i18n("Point calculation error."));
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
                name = "None";
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

        dms angleSep;
        if (spEast.ra().Degrees() > spWest.ra().Degrees())
            angleSep = spEast.ra() - spWest.ra();
        else
            angleSep = spEast.ra() + dms(360) - spWest.ra();

        initRA = spWest.ra().Degrees();
        if (raPoints > 1)
            raIncrement = angleSep.Degrees() / (raPoints - 1);
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
    if (mountModel.alignTypeBox->currentText() == "Any Object")
        return KStarsData::Instance()->skyComposite()->objectNearest(new SkyPoint(dms(ra), dms(dec)), maxSearch);
    else if (mountModel.alignTypeBox->currentText() == "Fixed DEC" ||
             mountModel.alignTypeBox->currentText() == "Fixed Grid")
        return nullptr;
    else if (mountModel.alignTypeBox->currentText() == "Any Stars")
        return KStarsData::Instance()->skyComposite()->starNearest(new SkyPoint(dms(ra), dms(dec)), maxSearch);

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

void Align::alignTypeChanged(const QString alignType)
{
    if (alignType == "Fixed DEC")
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
    qSort(greekBoxNames.begin(), greekBoxNames.end(), [](const QString &a, const QString &b) {
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
    GeoLocation *geo  = data->geo();
    CachingDms *lst   = data->lst();
    KStarsDateTime ut = geo->LTtoUT(KStarsDateTime(QDateTime::currentDateTime().toLocalTime()));
    SkyPoint sp       = so->recomputeCoords(ut, geo);

    //check altitude of object at this time.
    sp.EquatorialToHorizontal(lst, geo->lat());

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
        KMessageBox::sorry(0, message, i18n("Invalid URL"));
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
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
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
            int r = KMessageBox::warningContinueCancel(0,
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
            KMessageBox::error(KStars::Instance(), i18n("Failed to save alignment list"), i18n("Save"));
            return;
        }
    }
    else
    {
        QString message = i18n("Invalid URL: %1", alignURL.url());
        KMessageBox::sorry(KStars::Instance(), message, i18n("Invalid URL"));
    }
}

bool Align::saveAlignmentPoints(const QString &path)
{
    QFile file;
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
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
        int r = KMessageBox::warningContinueCancel(0,
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
        KMessageBox::sorry(KStars::Instance(), message, i18n("Invalid URL"));
        return;
    }

    QFile file;
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
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
            KMessageBox::sorry(0, i18n("Error in table structure."));
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

    if (KMessageBox::questionYesNo(
                KStars::Instance(), i18n("Are you sure you want to clear all of the solution points?"),
                i18n("Clear Solution Points"), KStandardGuiItem::yes(), KStandardGuiItem::no()) == KMessageBox::Yes)
    {
        solutionTable->setRowCount(0);
        alignPlot->graph(0)->data()->clear();
        alignPlot->clearItems();
        buildTarget();

        slotAutoScaleGraph();
    }
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
    Q_UNUSED(logicalIndex);

    for (int i = 0; i < mountModel.alignTable->columnCount(); i++)
    {
        QTableWidgetItem *oldItem = mountModel.alignTable->takeItem(oldVisualIndex, i);
        QTableWidgetItem *newItem = mountModel.alignTable->takeItem(newVisualIndex, i);

        mountModel.alignTable->setItem(newVisualIndex, i, oldItem);
        mountModel.alignTable->setItem(oldVisualIndex, i, newItem);
    }
    disconnect(mountModel.alignTable->verticalHeader(), SIGNAL(sectionMoved(int,int,int)), this,
               SLOT(moveAlignPoint(int,int,int)));
    mountModel.alignTable->verticalHeader()->moveSection(newVisualIndex, oldVisualIndex);
    connect(mountModel.alignTable->verticalHeader(), SIGNAL(sectionMoved(int,int,int)), this,
            SLOT(moveAlignPoint(int,int,int)));

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

    mountModel.alignDec->setValue((int)spWest.dec().Degrees());

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
    QPointer<FindDialog> fd = new FindDialog(KStars::Instance());
    if (fd->exec() == QDialog::Accepted)
    {
        SkyObject *object = fd->targetObject();
        if (object != 0)
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
    delete fd;
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
                KMessageBox::error(0, i18n("Please Check the Alignment Points."));
                return;
            }
            if (currentGotoMode == GOTO_NOTHING)
            {
                int r = KMessageBox::warningContinueCancel(
                            0,
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
    bool rc = parser->init();

    if (rc)
    {
        connect(parser, SIGNAL(solverFinished(double,double,double,double)), this,
                SLOT(solverFinished(double,double,double,double)), Qt::UniqueConnection);
        connect(parser, SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }

    return rc;
}

void Align::checkAlignmentTimeout()
{
    if (loadSlewState != IPS_IDLE || ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
        abort();
    else if (loadSlewState == IPS_IDLE)
    {
        appendLogText(i18n("Solver timed out"));
        parser->stopSolver();
        captureAndSolve();
    }
    // TODO must also account for loadAndSlew. Retain file name
}

void Align::setSolverType(int type)
{
    if (sender() == nullptr && type >= 0 && type <= 2)
        solverTypeGroup->button(type)->setChecked(true);

    syncSettings();

    Options::setSolverType(type);

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
        connect(parser, SIGNAL(solverFinished(double,double,double,double)), this,
                SLOT(solverFinished(double,double,double,double)), Qt::UniqueConnection);
        connect(parser, SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }
    else
        parser->disconnect();
}

bool Align::setCCD(const QString & device)
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

    if (solverTypeGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
        (dynamic_cast<RemoteAstrometryParser *>(remoteParser.get()))->setCCD(currentCCD->getDeviceName());

    syncCCDInfo();

    /*
    FOVScopeCombo->blockSignals(true);
    ISD::CCD::TelescopeType type = currentCCD->getTelescopeType();
    FOVScopeCombo->setCurrentIndex(type == ISD::CCD::TELESCOPE_UNKNOWN ? 0 : type);
    FOVScopeCombo->blockSignals(false);
    */

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

    connect(currentTelescope, SIGNAL(numberUpdated(INumberVectorProperty*)), this,
            SLOT(processNumber(INumberVectorProperty*)), Qt::UniqueConnection);

    syncTelescopeInfo();
}

void Align::setDome(ISD::GDInterface *newDome)
{
    currentDome = static_cast<ISD::Dome *>(newDome);
    connect(currentDome, SIGNAL(switchUpdated(ISwitchVectorProperty*)), this,
            SLOT(processSwitch(ISwitchVectorProperty*)), Qt::UniqueConnection);
}

void Align::syncTelescopeInfo()
{
    if (currentTelescope == nullptr || currentTelescope->isConnected() == false)
        return;

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
        return;

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

        generateArgs();
    }
}

void Align::setTelescopeInfo(double primaryFocalLength, double primaryAperture, double guideFocalLength, double guideAperture)
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
    //targetChip->getFrame(&x,&y,&ccd_width,&ccd_height);
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
            binningCombo->setCurrentIndex(binningCombo->count()-1);
            Options::setSolverBinningIndex(binningCombo->count()-1);
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
        generateArgs();
    }
}

void Align::getFOVScale(double &fov_w, double &fov_h, double &fov_scale)
{
    fov_w     = fov_x;
    fov_h     = fov_y;
    fov_scale = fov_pixscale;
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

    QString calculatedFOV = (QString("%1' x %2'").arg(fov_x, 0, 'g', 3).arg(fov_y, 0, 'g', 3));
    // JM 2018-04-20 Above calculations are for RAW FOV. Starting from 2.9.5, we are using EFFECTIVE FOV
    // Which is the real FOV as measured from the plate solution. The effective FOVs are stored in the database and are unique
    // per profile/pixel_size/focal_length combinations. It defaults to 0' x 0' and gets updated after the first successful solver is complete.
    getEffectiveFOV();

    if (fov_x == 0)
    {
        //FOVOut->setReadOnly(false);
        FOVOut->setToolTip(i18n("<p>Effective field of view size in arcminutes.</p><p>Please capture and solve once to measure the effective FOV or enter the values manually.</p><p>Calculated FOV: %1</p>", calculatedFOV));
    }
    else
    {
        FOVOut->setToolTip(i18n("<p>Effective field of view size in arcminutes.</p>"));
        //FOVOut->setReadOnly(true);
    }

    solverFOV->setSize(fov_x, fov_y);
    sensorFOV->setSize(fov_x, fov_y);
    if (currentCCD)
        sensorFOV->setName(currentCCD->getDeviceName());

    FOVOut->setText(QString("%1' x %2'").arg(fov_x, 0, 'g', 3).arg(fov_y, 0, 'g', 3));

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

QStringList Align::generateOptions(const QVariantMap &optionsMap)
{
    // -O overwrite
    // -3 Expected RA
    // -4 Expected DEC
    // -5 Radius (deg)
    // -L lower scale of image in arcminutes
    // -H upper scale of image in arcmiutes
    // -u aw set scale to be in arcminutes
    // -W solution.wcs name of solution file
    // apog1.jpg name of target file to analyze
    //solve-field -O -3 06:40:51 -4 +09:49:53 -5 1 -L 40 -H 100 -u aw -W solution.wcs apod1.jpg

    QStringList solver_args;

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
void Align::generateFOVBounds(double fov_h, double fov_v, QString &fov_low, QString &fov_high)
{
    double fov_lower, fov_upper;
    // let's stretch the boundaries by 5%
    fov_lower = ((fov_h < fov_v) ? (fov_h * 0.95) : (fov_v * 0.95));
    fov_upper = ((fov_h > fov_v) ? (fov_h * 1.05) : (fov_v * 1.05));

    //No need to do anything if they are aw, since that is the default
    fov_low  = QString::number(fov_lower);
    fov_high = QString::number(fov_upper);
}

void Align::generateArgs()
{
    // -O overwrite
    // -3 Expected RA
    // -4 Expected DEC
    // -5 Radius (deg)
    // -L lower scale of image in arcminutes
    // -H upper scale of image in arcmiutes
    // -u aw set scale to be in arcminutes
    // -W solution.wcs name of solution file
    // apog1.jpg name of target file to analyze
    //solve-field -O -3 06:40:51 -4 +09:49:53 -5 1 -L 40 -H 100 -u aw -W solution.wcs apod1.jpg

    QVariantMap optionsMap;

    if (Options::astrometryUseNoVerify())
        optionsMap["noverify"] = true;

    if (Options::astrometryUseResort())
        optionsMap["resort"] = true;

    if (Options::astrometryUseNoFITS2FITS())
        optionsMap["nofits2fits"] = true;

    if (Options::astrometryUseDownsample())
        optionsMap["downsample"] = Options::astrometryDownsample();

    if (Options::astrometryUseImageScale() && fov_x > 0 && fov_y > 0)
    {
        QString units = ImageScales[Options::astrometryImageScaleUnits()];
        if (Options::astrometryAutoUpdateImageScale())
        {
            QString fov_low, fov_high;
            double fov_w = fov_x;
            double fov_h = fov_y;

            if (units == "dw")
            {
                fov_w /= 60;
                fov_h /= 60;
            }
            else if (units == "app")
            {
                fov_w = fov_pixscale;
                fov_h = fov_pixscale;
            }

            generateFOVBounds(fov_w, fov_h, fov_low, fov_high);

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

    if (Options::astrometryCustomOptions().isEmpty() == false)
        optionsMap["custom"] = Options::astrometryCustomOptions();

    QStringList solverArgs = generateOptions(optionsMap);

    QString options = solverArgs.join(" ");
    solverOptions->setText(options);
    solverOptions->setToolTip(options);
}

bool Align::captureAndSolve()
{
    alignTimer.stop();

    if (currentCCD == nullptr)
        return false;

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: lost connection to CCD."));
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

    if (parser->init() == false)
        return false;

    if (focal_length == -1 || aperture == -1)
    {
        KMessageBox::error(
                    0,
                    i18n("Telescope aperture and focal length are missing. Please check your driver settings and try again."));
        return false;
    }

    if (ccd_hor_pixel == -1 || ccd_ver_pixel == -1)
    {
        KMessageBox::error(0, i18n("CCD pixel size is missing. Please check your driver settings and try again."));
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
            filterManager->setFilterPosition(targetPosition);
            state = ALIGN_PROGRESS;
            return true;
        }
    }

    if (currentCCD->getDriverInfo()->getClientManager()->getBLOBMode(currentCCD->getDeviceName(), "CCD1") == B_NEVER)
    {
        if (KMessageBox::questionYesNo(
                    0, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) ==
                KMessageBox::Yes)
        {
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD1");
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD2");
        }
        else
        {
            return false;
        }
    }

    double seqExpose = exposureIN->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (focusState >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy. Retrying in 10 seconds..."));
        alignTimer.start();
        return false;
    }

    if (targetChip->isCapturing())
    {
        appendLogText(i18n("Cannot capture while CCD exposure is in progress. Retrying in 10 seconds..."));
        alignTimer.start();
        return false;
    }

    alignView->setBaseSize(alignWidget->size());

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    connect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this,
            SLOT(checkCCDExposureProgress(ISD::CCDChip*,double,IPState)));

    // In case of remote solver, check if we need to update active CCD
    if (solverTypeGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
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

        QString options        = solverOptions->text().simplified();
        QStringList solverArgs = options.split(' ');

        dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->sendArgs(solverArgs);

        // If mount model was reset, we do not update targetCoord
        // since the RA/DE is now different immediately after the reset
        // so we still try to lock for the coordinates before the reset.
        if (solverIterations == 0 && mountModelReset == false)
        {
            double ra, dec;
            currentTelescope->getEqCoords(&ra, &dec);
            targetCoord.setRA(ra);
            targetCoord.setDec(dec);
        }
        mountModelReset = false;

        solverTimer.start();
    }
    //else
    //{
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
    dir.setNameFilters(QStringList() << "fits*"
                       << "tmp.*");
    dir.setFilter(QDir::Files);
    for (auto &dirFile : dir.entryList())
        dir.remove(dirFile);
    //}

    currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);

    targetChip->resetFrame();
    targetChip->setBatchMode(false);
    targetChip->setCaptureMode(FITS_ALIGN);
    targetChip->setFrameType(FRAME_LIGHT);

    int bin = Options::solverBinningIndex() + 1;
    targetChip->setBinning(bin, bin);

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

void Align::newFITS(IBLOB *bp)
{
    // Ignore guide head if there is any.
    if (!strcmp(bp->name, "CCD2"))
        return;

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*, double, IPState)), this,
               SLOT(checkCCDExposureProgress(ISD::CCDChip*, double, IPState)));

    blobType     = *(static_cast<ISD::CCD::BlobType *>(bp->aux1));
    blobFileName = QString(static_cast<char *>(bp->aux2));

    // If it's Refresh, we're done
    if (pahStage == PAH_REFRESH)
    {
        setCaptureComplete();
        return;
    }

    appendLogText(i18n("Image received."));

    if (solverTypeGroup->checkedId() != SOLVER_REMOTE)
    {
        if (blobType == ISD::CCD::BLOB_FITS)
        {
            ISD::CCDChip *targetChip =
                    currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

            if (alignDarkFrameCheck->isChecked())
            {
                int x, y, w, h, binx = 1, biny = 1;
                targetChip->getFrame(&x, &y, &w, &h);
                targetChip->getBinning(&binx, &biny);

                uint16_t offsetX = x / binx;
                uint16_t offsetY = y / biny;
                FITSData *darkData = DarkLibrary::Instance()->getDarkFrame(targetChip, exposureIN->value());

                connect(DarkLibrary::Instance(), SIGNAL(darkFrameCompleted(bool)), this, SLOT(setCaptureComplete()));
                connect(DarkLibrary::Instance(), SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));

                if (darkData)
                    DarkLibrary::Instance()->subtract(darkData, alignView, FITS_NONE, offsetX, offsetY);
                else
                {
                    bool rc = DarkLibrary::Instance()->captureAndSubtract(targetChip, alignView, exposureIN->value(),
                                                                          offsetX, offsetY);
                    alignDarkFrameCheck->setChecked(rc);
                }

                return;
            }
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

    if (solverTypeGroup->checkedId() == SOLVER_ONLINE && Options::astrometryUseJPEG())
    {
        ISD::CCDChip *targetChip =
                currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip)
        {
            QString jpegFile = blobFileName + ".jpg";
            bool rc          = alignView->getDisplayImage()->save(jpegFile, "JPG");
            if (rc)
                blobFileName = jpegFile;
        }
    }

    if (fov())
        fov()->setImageDisplay(alignView->getDisplayImage());

    startSolving(blobFileName);
}

void Align::setSolverAction(int mode)
{
    gotoModeButtonGroup->button(mode)->setChecked(true);
    currentGotoMode = static_cast<GotoMode>(mode);
}

void Align::startSolving(const QString &filename, bool isGenerated)
{
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

            solverArgs = generateOptions(optionsMap);
        }
        else if (rc == KMessageBox::No)
            solverArgs = options.split(' ');
        else
        {
            abort();
            return;
        }
    }

    if (solverIterations == 0 && mountModelReset == false)
    {
        double ra, dec;
        currentTelescope->getEqCoords(&ra, &dec);
        targetCoord.setRA(ra);
        targetCoord.setDec(dec);
    }

    mountModelReset = false;

    //Options::setSolverOptions(solverOptions->text());
    //Options::setGuideScopeCCDs(guideScopeCCDs);
    Options::setSolverAccuracyThreshold(accuracySpin->value());
    Options::setAlignDarkFrame(alignDarkFrameCheck->isChecked());
    Options::setSolverGotoOption(currentGotoMode);

    //m_isSolverComplete = false;
    //m_isSolverSuccessful = false;

    if (fov_x > 0)
        parser->verifyIndexFiles(fov_x, fov_y);

    solverTimer.start();

    alignTimer.start();

    if (currentGotoMode == GOTO_SLEW)
        appendLogText(i18n("Solver iteration #%1", solverIterations + 1));

    state = ALIGN_PROGRESS;
    emit newStatus(state);

    parser->startSovler(filename, solverArgs, isGenerated);
}

void Align::solverFinished(double orientation, double ra, double dec, double pixscale)
{
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    sOrientation = orientation;
    sRA          = ra;
    sDEC         = dec;

    // Reset Telescope Type to remembered value
    if (rememberTelescopeType != ISD::CCD::TELESCOPE_UNKNOWN)
    {
        currentCCD->setTelescopeType(rememberTelescopeType);
        rememberTelescopeType = ISD::CCD::TELESCOPE_UNKNOWN;
    }

    alignTimer.stop();

    if (solverTypeGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
    {
        // Disable remote parse
        dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->setEnabled(false);
    }

    int binx, biny;
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    targetChip->getBinning(&binx, &biny);

    if (Options::astrometrySolverVerbose())
        appendLogText(i18n("Solver RA (%1) DEC (%2) Orientation (%3) Pixel Scale (%4)", QString::number(ra, 'g', 5),
                           QString::number(dec, 'g', 5), QString::number(orientation, 'g', 5),
                           QString::number(pixscale, 'g', 5)));

#if 0
    if (pixscale > 0 && loadSlewState == IPS_IDLE)
    {
        double solver_focal_length = (206.264 * ccd_hor_pixel) / pixscale * binx;
        if (fabs(focal_length - solver_focal_length) > 1)
            appendLogText(i18n("Current focal length is %1 mm while computed focal length from the solver is %2 mm. "
                               "Please update the mount focal length to obtain accurate results.",
                               QString::number(focal_length, 'g', 5), QString::number(solver_focal_length, 'g', 5)));
    }
#endif

    if (fov_x == 0 && pixscale > 0)
    {
        double newFOVW = ccd_width * pixscale / binx / 60.0;
        double newFOVH = ccd_height * pixscale / biny / 60.0;

        saveNewEffectiveFOV(newFOVW, newFOVH);
    }

    alignCoord.setRA0(ra / 15.0);
    alignCoord.setDec0(dec);
    RotOut->setText(QString::number(orientation, 'g', 5));

    // Convert to JNow
    alignCoord.apparentCoord((long double)J2000, KStars::Instance()->data()->ut().djd());
    // Get horizontal coords
    alignCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    double raDiff = (alignCoord.ra().Degrees() - targetCoord.ra().Degrees()) * 3600;
    double deDiff = (alignCoord.dec().Degrees() - targetCoord.dec().Degrees()) * 3600;

    dms RADiff(fabs(raDiff) / 3600.0), DEDiff(deDiff / 3600.0);

    dRAOut->setText(QString("%1%2").arg((raDiff > 0 ? "+" : "-"), RADiff.toHMSString()));
    dDEOut->setText(DEDiff.toDMSString(true));

    pixScaleOut->setText(QString::number(pixscale, 'f', 2));

    //emit newSolutionDeviation(raDiff, deDiff);

    targetDiff = sqrt(raDiff * raDiff + deDiff * deDiff);

    // Because astrometry reads image upside-down (bottom to top), the orientation is rotated 180 degrees when compared to PA
    // PA = Orientation + 180
    double solverPA = orientation + 180;
    // Limit PA to -180 to +180
    if (solverPA > 180)
        solverPA -= 360;
    if (solverPA < -180)
        solverPA += 360;

    solverFOV->setCenter(alignCoord);
    solverFOV->setPA(solverPA);
    solverFOV->setImageDisplay(Options::astrometrySolverOverlay());

    sensorFOV->setPA(solverPA);

    QString ra_dms, dec_dms;
    getFormattedCoords(alignCoord.ra().Hours(), alignCoord.dec().Degrees(), ra_dms, dec_dms);

    SolverRAOut->setText(ra_dms);
    SolverDecOut->setText(dec_dms);

    //This block of code will write the result into the solution table and plot it on the graph.
    int currentRow = solutionTable->rowCount() - 1;
    if (loadSlewState == IPS_IDLE)
    {
        QTableWidgetItem *dRAReport = new QTableWidgetItem();
        if (dRAReport)
        {
            dRAReport->setText(QString::number(raDiff, 'f', 3) + "\"");
            dRAReport->setTextAlignment(Qt::AlignHCenter);
            dRAReport->setFlags(Qt::ItemIsSelectable);
            solutionTable->setItem(currentRow, 4, dRAReport);
        }

        QTableWidgetItem *dDECReport = new QTableWidgetItem();
        if (dDECReport)
        {
            dDECReport->setText(QString::number(deDiff, 'f', 3) + "\"");
            dDECReport->setTextAlignment(Qt::AlignHCenter);
            dDECReport->setFlags(Qt::ItemIsSelectable);
            solutionTable->setItem(currentRow, 5, dDECReport);
        }

        double raPlot  = raDiff;
        double decPlot = deDiff;
        alignPlot->graph(0)->addData(raPlot, decPlot);

        QCPItemText *textLabel = new QCPItemText(alignPlot);
        textLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

        textLabel->position->setType(QCPItemPosition::ptPlotCoords);
        textLabel->position->setCoords(raPlot, decPlot);
        textLabel->setColor(Qt::red);
        textLabel->setPadding(QMargins(0, 0, 0, 0));
        textLabel->setBrush(Qt::white);
        //textLabel->setBrush(Qt::NoBrush);
        textLabel->setPen(Qt::NoPen);
        textLabel->setText(' ' + QString::number(solutionTable->rowCount()) + ' ');
        textLabel->setFont(QFont(font().family(), 8));

        if (!alignPlot->xAxis->range().contains(raDiff))
        {
            alignPlot->graph(0)->rescaleKeyAxis(true);
            alignPlot->yAxis->setScaleRatio(alignPlot->xAxis, 1.0);
        }
        if (!alignPlot->yAxis->range().contains(deDiff))
        {
            alignPlot->graph(0)->rescaleValueAxis(true);
            alignPlot->xAxis->setScaleRatio(alignPlot->yAxis, 1.0);
        }
        alignPlot->replot();
    }

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

    retries = 0;

    appendLogText(i18n("Solution coordinates: RA (%1) DEC (%2) Telescope Coordinates: RA (%3) DEC (%4)",
                       alignCoord.ra().toHMSString(), alignCoord.dec().toDMSString(), telescopeCoord.ra().toHMSString(),
                       telescopeCoord.dec().toDMSString()));
    if (loadSlewState == IPS_IDLE && currentGotoMode == GOTO_SLEW)
    {
        dms diffDeg(targetDiff / 3600.0);
        appendLogText(i18n("Target is within %1 degrees of solution coordinates.", diffDeg.toDMSString()));
    }

    if (rememberUploadMode != currentCCD->getUploadMode())
        currentCCD->setUploadMode(rememberUploadMode);

    if (rememberCCDExposureLooping)
        currentCCD->setExposureLoopingEnabled(true);

    //if (syncR->isChecked() || nothingR->isChecked() || targetDiff <= accuracySpin->value())
    // CONTINUE HERE

    //This block of code along with some sections in the switch below will set the status report in the solution table for this item.
    std::unique_ptr<QTableWidgetItem> statusReport(new QTableWidgetItem());

    if (loadSlewState == IPS_IDLE)
    {
        solutionTable->setCellWidget(currentRow, 3, new QWidget());

        statusReport->setFlags(Qt::ItemIsSelectable);
    }

    // Update Rotater offsets
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
                double offset = solverPA - (rawAngle * Options::pAMultiplier());

                qCDebug(KSTARS_EKOS_ALIGN) << "Raw Rotator Angle:" << rawAngle << "Rotator PA:" << currentRotatorPA << "Rotator Offset:" << offset;
                Options::setPAOffset(offset);
            }

            if (absAngle && std::isnan(loadSlewTargetPA) == false && fabs(currentRotatorPA - loadSlewTargetPA) * 60 > Options::astrometryRotatorThreshold())
            {
                double rawAngle = (loadSlewTargetPA - Options::pAOffset()) / Options::pAMultiplier();
                if (rawAngle < 0)
                    rawAngle += 360;
                else if (rawAngle > 360)
                    rawAngle -= 360;
                absAngle->np[0].value = rawAngle;
                ClientManager *clientManager = currentRotator->getDriverInfo()->getClientManager();
                clientManager->sendNewNumber(absAngle);
                appendLogText(i18n("Setting position angle to %1 degrees E of N...", loadSlewTargetPA));
                return;
            }
        }
    }

    emit newSolverResults(orientation, ra, dec, pixscale);
    QJsonObject solution = {
        {"ra", SolverRAOut->text()},
        {"de", SolverDecOut->text()},
        {"dRA", dRAOut->text()},
        {"dDE", dDEOut->text()},
        {"pix", pixscale},
        {"rot", orientation},
        {"fov", FOVOut->text()},
    };
    emit newSolution(solution);

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
        break;

    case GOTO_SLEW:
        if (loadSlewState == IPS_BUSY || targetDiff > (double)accuracySpin->value())
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
    KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed with errors"),KSNotification::EVENT_ALERT);

    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    alignTimer.stop();

    azStage  = AZ_INIT;
    altStage = ALT_INIT;

    //loadSlewMode = false;
    loadSlewState = IPS_IDLE;
    solverIterations = 0;
    retries          = 0;

    //emit solverComplete(false);

    state = ALIGN_FAILED;
    emit newStatus(state);

    int currentRow = solutionTable->rowCount() - 1;

    solutionTable->setCellWidget(currentRow, 3, new QWidget());
    QTableWidgetItem *statusReport = new QTableWidgetItem();
    statusReport->setIcon(QIcon(":/icons/AlignFailure.svg"));
    statusReport->setFlags(Qt::ItemIsSelectable);
    solutionTable->setItem(currentRow, 3, statusReport);
}

void Align::abort()
{
    parser->stopSolver();
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

    //loadSlewMode = false;
    loadSlewState = IPS_IDLE;
    solverIterations = 0;
    retries          = 0;
    alignTimer.stop();

    //currentCCD->disconnect(this);
    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*, double, IPState)), this,
               SLOT(checkCCDExposureProgress(ISD::CCDChip*, double, IPState)));

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
            int elapsed = (int)round(solverTimer.elapsed() / 1000.0);
            appendLogText(i18np("Solver aborted after %1 second.", "Solver aborted after %1 seconds", elapsed));
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
    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                            QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_ALIGN) << text;

    emit newLog();
}

void Align::clearLog()
{
    logText.clear();
    emit newLog();
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
            INumberVectorProperty *nvp = nullptr;

            if (currentTelescope->isJ2000())
                nvp = currentTelescope->getBaseDevice()->getNumber("EQUATORIAL_COORD");
            else
                nvp = currentTelescope->getBaseDevice()->getNumber("EQUATORIAL_EOD_COORD");

            if (nvp)
                processNumber(nvp);
        }
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

        switch (nvp->s)
        {
        case IPS_OK:
        {
            // Update the boxes as the mount just finished slewing
            if (isSlewDirty && Options::astrometryAutoUpdatePosition())
            {
                opsAstrometry->estRA->setText(ra_dms);
                opsAstrometry->estDec->setText(dec_dms);

                Options::setAstrometryPositionRA(nvp->np[0].value * 15);
                Options::setAstrometryPositionDE(nvp->np[1].value);

                generateArgs();
            }

            // If dome is syncing, wait until it stops
            if (currentDome && currentDome->isMoving())
            {
                domeReady = false;
                return;
            }

            if (isSlewDirty && pahStage == PAH_FIND_CP)
            {
                isSlewDirty = false;
                appendLogText(i18n("Mount completed slewing near celestial pole. Capture again to verify."));
                setSolverAction(GOTO_NOTHING);
                pahStage = PAH_FIRST_CAPTURE;
                emit newPAHStage(pahStage);
                return;
            }

            if (isSlewDirty && pahStage == PAH_FIRST_ROTATE)
            {
                isSlewDirty = false;

                appendLogText(i18n("Mount first rotation is complete."));

                pahStage = PAH_SECOND_CAPTURE;
                emit newPAHStage(pahStage);


                PAHWidgets->setCurrentWidget(PAHSecondCapturePage);
                emit newPAHMessage(secondCaptureText->text());

                captureAndSolve();
            }
            else if (isSlewDirty && pahStage == PAH_SECOND_ROTATE)
            {
                isSlewDirty = false;

                appendLogText(i18n("Mount second rotation is complete."));

                pahStage = PAH_THIRD_CAPTURE;
                emit newPAHStage(pahStage);


                PAHWidgets->setCurrentWidget(PAHThirdCapturePage);
                emit newPAHMessage(thirdCaptureText->text());

                captureAndSolve();
            }

            switch (state)
            {
            case ALIGN_PROGRESS:
                break;

            case ALIGN_SYNCING:
            {
                isSlewDirty = false;
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
                if (isSlewDirty == false)
                    break;

                isSlewDirty = false;
                if (loadSlewState == IPS_BUSY)
                {
                    loadSlewState = IPS_IDLE;

                    qCDebug(KSTARS_EKOS_ALIGN) << "loadSlewState is IDLE.";

                    state = ALIGN_PROGRESS;
                    emit newStatus(state);

                    QTimer::singleShot(delaySpin->value(), this, SLOT(captureAndSolve()));
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

                    QTimer::singleShot(delaySpin->value(), this, SLOT(captureAndSolve()));
                    return;
                }
                break;

            default:
            {
                isSlewDirty = false;
            }
                break;
            }
        }
            break;

        case IPS_BUSY:
        {
            isSlewDirty = true;
        }
            break;

        case IPS_ALERT:
        {
            if (state == ALIGN_SYNCING || state == ALIGN_SLEWING)
            {
                if (state == ALIGN_SYNCING)
                    appendLogText(i18n("Syncing failed."));
                else
                    appendLogText(i18n("Slewing failed."));

                if (++retries == 3)
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
            break;

        default:
            break;
        }

        /*if (Options::alignmentLogging())
            qDebug() << "State is " << Ekos::getAlignStatusString(state) << " isSlewing? " << currentTelescope->isSlewing() << " slew Dirty? " << slew_dirty
                     << " Current GOTO Mode? " << currentGotoMode << " LoadSlewState? " << pstateStr(loadSlewState);*/

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
        if (std::isnan(loadSlewTargetPA) == false && fabs(currentRotatorPA - loadSlewTargetPA)*60 <= Options::astrometryRotatorThreshold())
        {
            appendLogText(i18n("Rotator reached target position angle."));
            targetAccuracyNotMet = true;
            loadSlewTargetPA = std::numeric_limits<double>::quiet_NaN();
            QTimer::singleShot(Options::settlingTime(), this, SLOT(executeGOTO()));
        }
    }

    // N.B. EkosManager already mananges TELESCOPE_INFO, why here again?
    //if (!strcmp(coord->name, "TELESCOPE_INFO"))
    //syncTelescopeInfo();
}

void Align::executeGOTO()
{
    if (loadSlewState == IPS_BUSY)
    {
        //if (loadSlewIterations == loadSlewIterationsSpin->value())
        //loadSlewCoord = alignCoord;

        //targetCoord = loadSlewCoord;
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

    isSlewDirty = currentTelescope->Slew(&targetCoord);

    appendLogText(i18n("Slewing to target coordinates: RA (%1) DEC (%2).", targetCoord.ra().toHMSString(),
                       targetCoord.dec().toDMSString()));
}

void Align::SlewToTarget()
{
    if (canSync && loadSlewState == IPS_IDLE)
    {
        // 2018-01-24 JM: This is ugly. Maybe use DBus? Signal/Slots? Ekos Manager usage like this should be avoided
        if (KStars::Instance()->ekosManager() &&
                !KStars::Instance()->ekosManager()->getCurrentJobName().isEmpty())
        {
            KSNotification::event(QLatin1String("EkosSchedulerTelescopeSynced"),
                                  i18n("Ekos job (%1) - Telescope synced",
                                       KStars::Instance()->ekosManager()->getCurrentJobName()));
        }

        // Do we perform a regular sync or use differential slewing?
        if (Options::astrometryDifferentialSlewing())
        {
            double raDiff = alignCoord.ra().Degrees() - targetCoord.ra().Degrees();
            double deDiff = alignCoord.dec().Degrees() - targetCoord.dec().Degrees();

            targetCoord.setRA((targetCoord.ra().Degrees()-raDiff)/15.0);
            targetCoord.setDec(targetCoord.dec().Degrees()-deDiff);

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

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Measureing Azimuth Error...";

    switch (azStage)
    {
    case AZ_INIT:

        // Display message box confirming user point scope near meridian and south

        if (KMessageBox::warningContinueCancel(
                    0,
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

    if (pahStage != PAH_IDLE &&
            (KMessageBox::warningContinueCancel(KStars::Instance(),
                                                i18n("Polar Alignment Helper is still active. Do you want to continue "
                                                     "using legacy polar alignment tool?")) != KMessageBox::Continue))
        return;

    pahStage = PAH_IDLE;
    emit newPAHStage(pahStage);

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Measureing Altitude Error...";

    switch (altStage)
    {
    case ALT_INIT:

        // Display message box confirming user point scope near meridian and south

        if (KMessageBox::warningContinueCancel(0,
                                               i18n("Point the telescope to the eastern or western horizon with a "
                                                    "minimum altitude of 20 degrees. Press continue when ready."),
                                               i18n("Polar Alignment Measurement"), KStandardGuiItem::cont(),
                                               KStandardGuiItem::cancel(),
                                               "ekos_measure_alt_error") != KMessageBox::Continue)
            return;

        appendLogText(i18n("Solving first frame."));
        altStage = ALT_FIRST_TARGET;
        captureAndSolve();
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
        captureAndSolve();
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

    default:
        break;
    }

    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Hemisphere is " << ((hemisphere == NORTH_HEMISPHERE) ? "North" : "South")
                               << " --- initAz " << initAz;
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar initRA " << initRA << " initDEC " << initDEC << " finalRA " << finalRA
                               << " finalDEC " << finalDEC;
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar decDeviation " << decDeviation * 3600 << " arcsec "
                               << " RATime " << RATime << " minutes";
    qCDebug(KSTARS_EKOS_ALIGN) << "Polar Raw Deviaiton " << deviation << " degrees.";

    if (azStage == AZ_FINISHED)
    {
        azError->setText(deviationDirection.subs(QString("%1").arg(devDMS.toDMSString())).toString());
        //azError->setText(deviationDirection.subs(QString("%1")azDMS.toDMSString());
        azDeviation = deviation * (decDeviation > 0 ? 1 : -1);

        qCDebug(KSTARS_EKOS_ALIGN) << "Polar Azimuth Deviation " << azDeviation << " degrees.";

        correctAzB->setEnabled(true);
    }
    if (altStage == ALT_FINISHED)
    {
        //altError->setText(deviationDirection.subs(QString("%1").arg(fabs(deviation), 0, 'g', 3)).toString());
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

void Align::loadAndSlew(QString fileURL)
{
    /*if (solverTypeGroup->checkedId() == SOLVER_REMOTE)
    {
        appendLogText(i18n("Load and Slew is not supported in remote solver mode."));
        loadSlewB->setEnabled(false);
        return;
    }*/

    if (fileURL.isEmpty())
        fileURL = QFileDialog::getOpenFileName(KStars::Instance(), i18n("Load Image"), dirPath,
                                               "Images (*.fits *.fit *.jpg *.jpeg)");

    if (fileURL.isEmpty())
        return;

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

    startSolving(fileURL, false);
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
    if (Options::astrometryImageScaleUnits() == OpsAstrometry::SCALE_ARCSECPERPIX)
    {
        calculateFOV();
        generateArgs();
    }
}

void Align::setSolverArguments(const QString &value)
{
    solverOptions->setText(value);
}

QString Align::getSolverArguments()
{
    return solverOptions->text();
}

void Align::setFOVTelescopeType(int index)
{
    FOVScopeCombo->setCurrentIndex(index);
}

FOV *Align::fov()
{
    if (sOrientation == -1)
        return nullptr;
    else
        return solverFOV.get();
}

void Align::addFilter(ISD::GDInterface *newFilter)
{
    foreach (ISD::GDInterface *filter, Filters)
    {
        if (!strcmp(filter->getDeviceName(), newFilter->getDeviceName()))
            return;
    }

    FilterCaptureLabel->setEnabled(true);
    FilterDevicesCombo->setEnabled(true);
    FilterPosLabel->setEnabled(true);
    FilterPosCombo->setEnabled(true);

    FilterDevicesCombo->addItem(newFilter->getDeviceName());

    Filters.append(static_cast<ISD::Filter *>(newFilter));

    checkFilter(1);

    FilterDevicesCombo->setCurrentIndex(1);
}

bool Align::setFilter(QString device, int filterSlot)
{
    bool deviceFound = false;

    for (int i = 0; i < FilterDevicesCombo->count(); i++)
        if (device == FilterDevicesCombo->itemText(i))
        {
            FilterDevicesCombo->setCurrentIndex(i);
            deviceFound = true;
            break;
        }

    if (deviceFound == false)
        return false;

    if (filterSlot >=0 && filterSlot < FilterDevicesCombo->count())
        FilterDevicesCombo->setCurrentIndex(filterSlot);

    return true;
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
        currentFilterPosition=-1;
        FilterPosCombo->clear();
        return;
    }

    if (filterNum <= Filters.count())
        currentFilter = Filters.at(filterNum-1);

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

    clientManager->sendNewSwitch(wcsControl);
}

void Align::checkCCDExposureProgress(ISD::CCDChip *targetChip, double remaining, IPState state)
{
    INDI_UNUSED(targetChip);
    INDI_UNUSED(remaining);

    if (state == IPS_ALERT)
    {
        if (++retries == 3 && pahStage != PAH_REFRESH)
        {
            appendLogText(i18n("Capture error. Aborting..."));

            abort();
            return;
        }

        appendLogText(i18n("Restarting capture attempt #%1", retries));

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
    focusState = state;
}

QStringList Align::getSolverOptionsFromFITS(const QString &filename)
{
    int status = 0, fits_ccd_width, fits_ccd_height, fits_binx = 1, fits_biny = 1;
    char comment[128], error_status[512];
    fitsfile *fptr = nullptr;
    double ra = 0, dec = 0, fits_fov_x, fits_fov_y, fov_lower, fov_upper, fits_ccd_hor_pixel = -1,
            fits_ccd_ver_pixel = -1, fits_focal_length = -1;
    QString fov_low, fov_high;
    QStringList solver_args;

    QVariantMap optionsMap;

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

    solver_args = generateOptions(optionsMap);

    status = 0;
#if 0
    if (fits_open_image(&fptr, filename.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        qCritical(KSTARS_EKOS_ALIGN) << "Could not open file " << filename << "  Error: " << QString::fromUtf8(error_status);
        return solver_args;
    }
#endif

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

    /*if (coord_ok == false)
    {
        ra  = telescopeCoord.ra0().Hours();
        dec = telescopeCoord.dec0().Degrees();
    }*/

    if (coord_ok && Options::astrometryUsePosition())
        solver_args << "-3" << QString::number(ra * 15.0) << "-4" << QString::number(dec) << "-5 15";

    status = 0;
    double pixelScale=0;
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

void Align::saveSettleTime()
{
    Options::setSettlingTime(delaySpin->value());
}

void Align::setCaptureStatus(CaptureState newState)
{
    switch (newState)
    {
    case CAPTURE_ALIGNING:
        if (currentTelescope && currentTelescope->hasAlignmentModel() && Options::resetMountModelAfterMeridian())
        {
            mountModelReset = currentTelescope->clearAlignmentModel();
            qCDebug(KSTARS_EKOS_ALIGN) << "Post meridian flip mount model reset" << (mountModelReset ? "successful." : "failed.");
        }

        QTimer::singleShot(Options::settlingTime(), this, SLOT(captureAndSolve()));
        break;

    default:
        break;
    }
}

void Align::showFITSViewer()
{
    FITSData *data = alignView->getImageData();

    if (data)
    {
        QUrl url = QUrl::fromLocalFile(data->getFilename());

        if (fv.isNull())
        {
            if (Options::singleWindowCapturedFITS())
                fv = KStars::Instance()->genericFITSViewer();
            else
            {
                fv = new FITSViewer(Options::independentWindowFITS() ? nullptr : KStars::Instance());
                KStars::Instance()->getFITSViewersList().append(fv);
            }

            fv->addFITS(&url);
            FITSView *currentView = fv->getCurrentView();
            if (currentView)
                currentView->getImageData()->setAutoRemoveTemporaryFITS(false);
        }
        else
            fv->updateFITS(&url, 0);

        fv->show();
    }
}

void Align::toggleAlignWidgetFullScreen()
{
    if (alignWidget->parent() == nullptr)
    {
        alignWidget->setParent(this);
        rightLayout->insertWidget(0, alignWidget);
        //rightLayout->setStretch(0, 2);
        // rightLayout->setStretch(1, 1);
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

    Options::setAutoWCS(false);
    Options::setAstrometrySolverWCS(true);

    if (Options::limitedResourcesMode())
        appendLogText(i18n("Warning: Equatorial Grid Lines will not be drawn due to limited resources mode."));

    if (currentTelescope->hasAlignmentModel())
    {
        appendLogText(i18n("Clearing mount Alignment Model..."));
        mountModelReset = currentTelescope->clearAlignmentModel();
    }

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
    if ((static_cast<QPushButton *>(sender()) == PAHStopB) &&
            KMessageBox::questionYesNo(KStars::Instance(),
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
    disconnect(alignView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(setPAHCorrectionOffset(int,int)));
    disconnect(alignView, SIGNAL(newCorrectionVector(QLineF)), this, SIGNAL(newCorrectionVector(QLineF)));

    state = ALIGN_IDLE;
    emit newStatus(state);
}

void Align::rotatePAH()
{
    double raDiff = PAHRotationSpin->value();
    bool westMeridian = PAHDirectionCombo->currentIndex() == 0;

    // West
    if (westMeridian)
        raDiff *= -1;
    // East
    else
        raDiff *= 1;

    // JM 2018-05-03: Hemispheres shouldn't affect rotation direction in RA
#if 0
    // North
    if (hemisphere == NORTH_HEMISPHERE)
    {
        // West
        if (westMeridian)
            raDiff *= -1;
        // East
        else
            raDiff *= 1;
    }
    // South
    else
    {
        // West
        if (westMeridian)
            raDiff *= 1;
        // East
        else
            raDiff *= -1;
    }
#endif

    SkyPoint targetPAH;

    double newTelescopeRA = (telescopeCoord.ra().Degrees() + raDiff) / 15.0;

    targetPAH.setRA(newTelescopeRA);
    targetPAH.setDec(telescopeCoord.dec());

    // Convert to JNow
    //targetPAH.apparentCoord((long double) J2000, KStars::Instance()->data()->ut().djd());
    // Get horizontal coords
    //targetPAH.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    currentTelescope->Slew(&targetPAH);

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

    QPointF RACenterPoint(RACircle.x(), RACircle.y());
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
    double PA=0;
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

    correctionVector.setP1(celestialPolePoint);
    correctionVector.setP2(RACenterPoint);

    /*
    bool RAAxisInside = imageData->contains(RACenterPoint);
    bool CPPointInside= imageData->contains(celestialPolePoint);

    if (RAAxisInside == false && CPPointInside == false)
        appendLogText(i18n("Warning: Mount axis and celestial pole are outside the field of view. Correction vector may be inaccurate."));
    */

    connect(alignView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(setPAHCorrectionOffset(int,int)));
    emit polarResultUpdated(correctionVector, polarError.toDMSString());

    connect(alignView, SIGNAL(newCorrectionVector(QLineF)), this, SIGNAL(newCorrectionVector(QLineF)), Qt::UniqueConnection);
    emit newCorrectionVector(correctionVector);
    alignView->setCorrectionParams(correctionVector);

    emit newFrame(alignView);
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

    PAHWidgets->setCurrentWidget(PAHRefreshPage);
    emit newPAHMessage(refreshText->text());
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

    stopPAHProcess();
}

void Align::processPAHStage(double orientation, double ra, double dec, double pixscale)
{
    // Create temporary file to hold all WCS data
//    QTemporaryFile tmpFile(QDir::tempPath() + "/fitswcsXXXXXX");
//    tmpFile.setAutoRemove(false);
//    tmpFile.open();
//    QString newWCSFile = tmpFile.fileName();
//    tmpFile.close();
      QString newWCSFile = QDir::tempPath() + QString("/fitswcs%1").arg(QUuid::createUuid().toString().remove(QRegularExpression("[-{}]")));

    //alignView->setLoadWCSEnabled(true);

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
        solution->orientation = orientation;
        solution->pixelScale  = pixscale;

        pahImageInfos.append(solution);

        // Only invoke this if limited resource mode is false since we want to use CPU heavy WCS
        if (Options::limitedResourcesMode() == false)
        {
            appendLogText(i18n("Please wait while WCS data is processed..."));
            connect(alignView, SIGNAL(wcsToggled(bool)), this, SLOT(setWCSToggled(bool)), Qt::UniqueConnection);
            alignView->createWCSFile(newWCSFile, orientation, ra, dec, pixscale);
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
        // Second capture WCS is not important. Since it consumes quite a bit of resources, skip it
#if 0
        if (Options::limitedResourcesMode() == false)
        {
            rc = alignView->createWCSFile(newWCSFile, orientation, ra, dec, pixscale);
            if (rc == false)
            {
                appendLogText(i18n("Error creating WCS file: %1", alignView->getImageData()->getLastError()));
                // Not critical error
                //return;
            }
        }
#endif

        // Set 2nd PAH Center
        PAHImageInfo *solution = new PAHImageInfo();
        solution->skyCenter.setRA0(alignCoord.ra0());
        solution->skyCenter.setDec0(alignCoord.dec0());
        solution->orientation = orientation;
        solution->pixelScale  = pixscale;

        pahImageInfos.append(solution);

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
        solution->orientation = orientation;
        solution->pixelScale  = pixscale;

        pahImageInfos.append(solution);

        appendLogText(i18n("Please wait while WCS data is processed..."));
        connect(alignView, SIGNAL(wcsToggled(bool)), this, SLOT(setWCSToggled(bool)), Qt::UniqueConnection);
        alignView->createWCSFile(newWCSFile, orientation, ra, dec, pixscale);
        return;
    }
}

void Align::setWCSToggled(bool result)
{
    appendLogText(i18n("WCS data processing is complete."));

    //alignView->disconnect(this);
    disconnect(alignView, SIGNAL(wcsToggled(bool)), this, SLOT(setWCSToggled(bool)));

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

        // Not critical error
        /*
        if (result == false)
        {
            appendLogText(
                i18n("Warning: failed to load WCS data in file: %1", alignView->getImageData()->getLastError()));
            pahStage = PAH_FIRST_ROTATE;
            PAHWidgets->setCurrentWidget(PAHFirstRotatePage);
            return;
        }*/

        // Find Celestial pole location
        SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);

        FITSData *imageData = alignView->getImageData();
        QPointF pixelPoint, imagePoint;

        bool rc = imageData->wcsToPixel(CP, pixelPoint, imagePoint);

        // TODO check if pixelPoint is located TOO far from the current position as well
        // i.e. if X > Width * 2..etc
        if (rc == false)
        {
            appendLogText(i18n("Failed to process World Coordinate System: %1. Try again.", imageData->getLastError()));
            return;
        }

        // If celestial pole out of range, ask the user if they want to move to it
        if (pixelPoint.x() < (-1 * imageData->getWidth()) || pixelPoint.x() > (imageData->getWidth() * 2) ||
                pixelPoint.y() < (-1 * imageData->getHeight()) || pixelPoint.y() > (imageData->getHeight() * 2))
        {
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
                appendLogText(i18n("Warning: Celestial pole is located outside the field of view. Move the mount closer to the celestial pole."));
        }

        pahStage = PAH_FIRST_ROTATE;
        emit newPAHStage(pahStage);

        PAHWidgets->setCurrentWidget(PAHFirstRotatePage);
        emit newPAHMessage(firstRotateText->text());

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

        // Find Celstial pole location
        SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);

        QPointF imagePoint;

        imageData->wcsToPixel(CP, celestialPolePoint, imagePoint);

        // Now find pixel locations for all recorded center coordinates in the 3rd frame reference
        imageData->wcsToPixel(pahImageInfos[0]->skyCenter, pahImageInfos[0]->pixelCenter, imagePoint);
        imageData->wcsToPixel(pahImageInfos[1]->skyCenter, pahImageInfos[1]->pixelCenter, imagePoint);
        imageData->wcsToPixel(pahImageInfos[2]->skyCenter, pahImageInfos[2]->pixelCenter, imagePoint);

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

    syncSettings();

    /*
    bool rc = currentCCD->setTelescopeType(static_cast<ISD::CCD::TelescopeType>(index));

    // If false, try to set it to existing known telescope
    if (rc == false)
    {
       focal_length = (index == ISD::CCD::TELESCOPE_PRIMARY) ? primaryFL : guideFL;
       aperture = (index == ISD::CCD::TELESCOPE_PRIMARY) ? primaryAperture : guideAperture;

       syncTelescopeInfo();
    }*/

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
        //TRACE("The points are pependicular and parallel to x-y axis\n");
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

void Align::setMountStatus(ISD::Telescope::TelescopeStatus newState)
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
        connect(remoteParser.get(), SIGNAL(solverFinished(double,double,double,double)), this,
                SLOT(solverFinished(double,double,double,double)), Qt::UniqueConnection);
        connect(remoteParser.get(), SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }
}

void Align::setRotator(ISD::GDInterface *newRotator)
{
    currentRotator = newRotator;
    connect(currentRotator, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processNumber(INumberVectorProperty*)), Qt::UniqueConnection);
}

void Align::refreshAlignOptions()
{
    if (fov())
        fov()->setImageDisplay(Options::astrometrySolverWCS());

    alignTimer.setInterval(Options::astrometryTimeout() * 1000);
}

void Align::setFilterManager(const QSharedPointer<FilterManager> &manager)
{
    filterManager = manager;

    connect(filterManager.data(), &FilterManager::ready, [this]()
    {
        if (filterPositionPending)
        {
            focusState = FOCUS_IDLE;
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
                appendLogText(i18n("Changing filter to %1...", FilterPosCombo->itemText(filterManager->getTargetFilterPosition()-1)));
                break;

            case FILTER_AUTOFOCUS:
                appendLogText(i18n("Auto focus on filter change..."));
                break;

            default:
                break;
            }
        }
    });

    connect(filterManager.data(), &FilterManager::labelsChanged, this, [this]() { checkFilter(); });
    connect(filterManager.data(), &FilterManager::positionChanged, this, [this]() { checkFilter();});
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

QStringList Align::getActiveSolvers() const
{
    QStringList solvers;

    solvers << "Online";
#ifndef Q_OS_WIN
    solvers << "Offline";
#endif
    if (remoteParserDevice != nullptr)
        solvers << "Remote";

    return solvers;
}

int Align::getActiveSolverIndex() const
{
    return solverTypeGroup->checkedId();
}

QString Align::getPAHMessage() const
{
    switch (pahStage)
    {
    case PAH_IDLE:
    case PAH_FIND_CP:
    default:
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
}

void Align::zoomAlignView()
{
    alignView->ZoomDefault();

    emit newFrame(alignView);
}

QJsonObject Align::getSettings() const
{
    QJsonObject settings;

    settings.insert("camera", CCDCaptureCombo->currentText());
    settings.insert("fw", FilterDevicesCombo->currentText());
    settings.insert("filter", FilterPosCombo->currentText());
    settings.insert("exp", exposureIN->value());
    settings.insert("bin", binningCombo->currentIndex()+1);
    settings.insert("solverAction", gotoModeButtonGroup->checkedId());
    settings.insert("solverType", solverTypeGroup->checkedId());
    settings.insert("scopeType", FOVScopeCombo->currentIndex());

    return settings;
}

void Align::setSettings(const QJsonObject &settings)
{
    CCDCaptureCombo->setCurrentText(settings["camera"].toString());
    FilterDevicesCombo->setCurrentText(settings["fw"].toString());
    FilterPosCombo->setCurrentText(settings["filter"].toString());
    Options::setLockAlignFilterIndex(FilterPosCombo->currentIndex());
    exposureIN->setValue(settings["exp"].toDouble(1));
    binningCombo->setCurrentIndex(settings["bin"].toInt()-1);

    gotoModeButtonGroup->button(settings["solverAction"].toInt(1))->setChecked(true);
    solverTypeGroup->button(settings["solverType"].toInt(1))->setChecked(true);
    FOVScopeCombo->setCurrentIndex(settings["scopeType"].toInt(0));
}

void Align::syncSettings()
{
    emit settingsUpdated(getSettings());
}

QJsonObject Align::getPAHSettings() const
{
    QJsonObject settings = getSettings();

    settings.insert("mountDirection", PAHDirectionCombo->currentIndex());
    settings.insert("mountRotation", PAHRotationSpin->value());
    settings.insert("refresh", PAHExposure->value());

    return settings;
}

void Align::setPAHSettings(const QJsonObject &settings)
{
    setSettings(settings);

    PAHDirectionCombo->setCurrentIndex(settings["mountDirection"].toInt(0));
    PAHRotationSpin->setValue(settings["mountRotation"].toInt(30));
    PAHExposure->setValue(settings["refresh"].toDouble(1));
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
        KMessageBox::error(nullptr, i18n("Invalid FOV."));
        FOVOut->setStyleSheet("background-color:red");
    }
}

}

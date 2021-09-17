/*
    SPDX-FileCopyrightText: 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2018-2020 Robert Lancaster <rlancaste@gmail.com>
    SPDX-FileCopyrightText: 2019-2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "align.h"
#include "alignadaptor.h"
#include "alignview.h"
#include <ekos_align_debug.h>

// Options
#include "Options.h"
#include "opsalign.h"
#include "opsprograms.h"
#include "opsastrometry.h"
#include "opsastrometryindexfiles.h"

// Components
#include "mountmodel.h"
#include "polaralignmentassistant.h"
#include "remoteastrometryparser.h"
#include "polaralign.h"
#include "manualrotator.h"

// FITS
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitstab.h"

// Auxiliary
#include "auxiliary/QProgressIndicator.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include "dialogs/finddialog.h"
#include "ksnotification.h"
#include "kspaths.h"
#include "fov.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"

// INDI
#include "ekos/manager.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/indifilter.h"
#include "profileinfo.h"

// System Includes
#include <KActionCollection>
#include <basedevice.h>
#include <indicom.h>
#include <memory>

#define MAXIMUM_SOLVER_ITERATIONS 10
#define CAPTURE_RETRY_DELAY       10000
#define PAH_CUTOFF_FOV            10 // Minimum FOV width in arcminutes for PAH to work
#define CHECK_PAH(x) \
    m_PolarAlignmentAssistant && m_PolarAlignmentAssistant->x
#define RUN_PAH(x) \
    if (m_PolarAlignmentAssistant) m_PolarAlignmentAssistant->x

namespace Ekos
{

using PAA = PolarAlignmentAssistant;

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

    // Effective FOV Edit
    connect(FOVOut, &QLineEdit::editingFinished, this, &Align::syncFOV);

    connect(CCDCaptureCombo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Align::setDefaultCCD);
    connect(CCDCaptureCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Align::checkCCD);

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

    gotoModeButtonGroup->setId(syncR, GOTO_SYNC);
    gotoModeButtonGroup->setId(slewR, GOTO_SLEW);
    gotoModeButtonGroup->setId(nothingR, GOTO_NOTHING);

    connect(gotoModeButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this,
            [ = ](int id)
    {
        this->m_CurrentGotoMode = static_cast<GotoMode>(id);
    });

    m_CaptureTimer.setSingleShot(true);
    m_CaptureTimer.setInterval(CAPTURE_RETRY_DELAY);
    connect(&m_CaptureTimer, &QTimer::timeout, [&]()
    {
        if (m_CaptureTimeoutCounter++ > 3)
        {
            appendLogText(i18n("Capture timed out."));
            m_CaptureTimer.stop();
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
            {
                setAlignTableResult(ALIGN_RESULT_FAILED);
                captureAndSolve();
            }
        }
    });

    m_AlignTimer.setSingleShot(true);
    m_AlignTimer.setInterval(Options::astrometryTimeout() * 1000);
    connect(&m_AlignTimer, &QTimer::timeout, this, &Ekos::Align::checkAlignmentTimeout);

    m_CurrentGotoMode = static_cast<GotoMode>(Options::solverGotoOption());
    gotoModeButtonGroup->button(m_CurrentGotoMode)->setChecked(true);

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
    m_IndexFilesPage = dialog->addPage(opsAstrometryIndexFiles, i18n("Index Files"));
    m_IndexFilesPage->setIcon(QIcon::fromTheme("map-flat"));

    appendLogText(i18n("Idle."));

    pi.reset(new QProgressIndicator(this));

    stopLayout->addWidget(pi.get());

    exposureIN->setValue(Options::alignExposure());
    connect(exposureIN, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [&]()
    {
        syncSettings();
    });

    rememberSolverWCS = Options::astrometrySolverWCS();
    rememberAutoWCS   = Options::autoWCS();

    solverModeButtonGroup->setId(localSolverR, SOLVER_LOCAL);
    solverModeButtonGroup->setId(remoteSolverR, SOLVER_REMOTE);

    localSolverR->setChecked(Options::solverMode() == SOLVER_LOCAL);
    remoteSolverR->setChecked(Options::solverMode() == SOLVER_REMOTE);
    connect(solverModeButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this,
            &Align::setSolverMode);
    setSolverMode(solverModeButtonGroup->checkedId());

    // Which telescope info to use for FOV calculations
    FOVScopeCombo->setCurrentIndex(Options::solverScopeType());
    connect(FOVScopeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Align::updateTelescopeType);

    accuracySpin->setValue(Options::solverAccuracyThreshold());
    connect(accuracySpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this]()
    {
        Options::setSolverAccuracyThreshold(accuracySpin->value());
        buildTarget();
    });

    connect(alignDarkFrameCheck, &QCheckBox::toggled, [this]()
    {
        Options::setAlignDarkFrame(alignDarkFrameCheck->isChecked());
    });
    alignDarkFrameCheck->setChecked(Options::alignDarkFrame());

    delaySpin->setValue(Options::settlingTime());
    connect(delaySpin, &QSpinBox::editingFinished, this, &Ekos::Align::saveSettleTime);

    connect(binningCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Align::setBinningIndex);

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

    connect(clearAllSolutionsB, &QPushButton::clicked, this, &Ekos::Align::slotClearAllSolutionPoints);
    connect(removeSolutionB, &QPushButton::clicked, this, &Ekos::Align::slotRemoveSolutionPoint);
    connect(exportSolutionsCSV, &QPushButton::clicked, this, &Ekos::Align::exportSolutionPoints);
    connect(autoScaleGraphB, &QPushButton::clicked, this, &Ekos::Align::slotAutoScaleGraph);
    connect(mountModelB, &QPushButton::clicked, this, &Ekos::Align::slotMountModel);
    connect(solutionTable, &QTableWidget::cellClicked, this, &Ekos::Align::selectSolutionTableRow);

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    savedOptionsProfiles = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("SavedAlignProfiles.ini");
    if(QFile(savedOptionsProfiles).exists())
        m_StellarSolverProfiles = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        m_StellarSolverProfiles = getDefaultAlignOptionsProfiles();

    initPolarAlignmentAssistant();
    initManualRotator();
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

void Align::slotMountModel()
{
    if (!m_MountModel)
    {
        m_MountModel = new MountModel(this);
        connect(m_MountModel, &Ekos::MountModel::newLog, this, &Ekos::Align::appendLogText, Qt::UniqueConnection);
        connect(this, &Ekos::Align::newStatus, m_MountModel, &Ekos::MountModel::setAlignStatus, Qt::UniqueConnection);
    }

    m_MountModel->show();

    //    SkyPoint spWest;
    //    spWest.setAlt(30);
    //    spWest.setAz(270);
    //    spWest.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
    //    mountModel.alignDec->setValue(static_cast<int>(spWest.dec().Degrees()));

    //    mountModelDialog.show();
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
    if (m_SolveFromFile || ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
        abort();
    else if (!m_SolveFromFile)
    {
        appendLogText(i18n("Solver timed out."));
        parser->stopSolver();

        setAlignTableResult(ALIGN_RESULT_FAILED);
        captureAndSolve();
    }
    // TODO must also account for loadAndSlew. Retain file name
}

void Align::setSolverMode(int mode)
{
    if (sender() == nullptr && mode >= 0 && mode <= 1)
    {
        solverModeButtonGroup->button(mode)->setChecked(true);
    }

    Options::setSolverMode(mode);

    if (mode == SOLVER_REMOTE)
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

    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
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
        TargetCustomGainValue = Options::solverCameraGain();
        if (TargetCustomGainValue > 0)
            GainSpin->setValue(TargetCustomGainValue);
        else
            GainSpin->setValue(GainSpinSpecialValue);

        GainSpin->setReadOnly(currentCCD->getGainPermission() == IP_RO);

        connect(GainSpin, &QDoubleSpinBox::editingFinished, [this]()
        {
            if (GainSpin->value() > GainSpinSpecialValue)
            {
                TargetCustomGainValue = GainSpin->value();
                // Save custom gain
                Options::setSolverCameraGain(TargetCustomGainValue);
            }
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

    RUN_PAH(setCurrentTelescope(currentTelescope));

    connect(currentTelescope, &ISD::GDInterface::numberUpdated, this, &Ekos::Align::processNumber, Qt::UniqueConnection);
    connect(currentTelescope, &ISD::GDInterface::switchUpdated, this, &Ekos::Align::processSwitch, Qt::UniqueConnection);
    connect(currentTelescope, &ISD::GDInterface::Disconnected, this, [this]()
    {
        m_isRateSynced = false;
    });


    if (m_isRateSynced == false)
    {
        RUN_PAH(syncMountSpeed());
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

    auto nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        auto np = nvp->findWidgetByName("TELESCOPE_APERTURE");

        if (np && np->getValue() > 0)
            primaryAperture = np->getValue();

        np = nvp->findWidgetByName("GUIDER_APERTURE");
        if (np && np->getValue() > 0)
            guideAperture = np->getValue();

        aperture = primaryAperture;

        //if (currentCCD && currentCCD->getTelescopeType() == ISD::CCD::TELESCOPE_GUIDE)
        if (FOVScopeCombo->currentIndex() == ISD::CCD::TELESCOPE_GUIDE)
            aperture = guideAperture;

        np = nvp->findWidgetByName("TELESCOPE_FOCAL_LENGTH");
        if (np && np->getValue() > 0)
            primaryFL = np->getValue();

        np = nvp->findWidgetByName("GUIDER_FOCAL_LENGTH");
        if (np && np->getValue() > 0)
            guideFL = np->getValue();

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
            i18nc("F-Number, Focal length, Aperture",
                  "<nobr>F<b>%1</b> Focal length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                  QString::number(primaryFL / primaryAperture, 'f', 1), QString::number(primaryFL, 'f', 2),
                  QString::number(primaryAperture, 'f', 2)),
            Qt::ToolTipRole);
        FOVScopeCombo->setItemData(
            ISD::CCD::TELESCOPE_GUIDE,
            i18nc("F-Number, Focal length, Aperture",
                  "<nobr>F<b>%1</b> Focal length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
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
    if (!currentCCD)
        return;

    auto nvp = currentCCD->getBaseDevice()->getNumber(useGuideHead ? "GUIDER_INFO" : "CCD_INFO");

    if (nvp)
    {
        auto np = nvp->findWidgetByName("CCD_PIXEL_SIZE_X");
        if (np && np->getValue() > 0)
            ccd_hor_pixel = ccd_ver_pixel = np->getValue();

        np = nvp->findWidgetByName("CCD_PIXEL_SIZE_Y");
        if (np && np->getValue() > 0)
            ccd_ver_pixel = np->getValue();

        np = nvp->findWidgetByName("CCD_PIXEL_SIZE_Y");
        if (np && np->getValue() > 0)
            ccd_ver_pixel = np->getValue();
    }

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    auto svp = currentCCD->getBaseDevice()->getSwitch("WCS_CONTROL");
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

        binningCombo->setCurrentIndex(Options::solverBinningIndex());
        binningCombo->blockSignals(false);
    }

    if (ccd_hor_pixel <= 0 || ccd_ver_pixel <= 0)
        return;

    if (ccd_hor_pixel > 0 && ccd_ver_pixel > 0 && focal_length > 0 && aperture > 0)
    {
        calculateFOV();
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
        appendLogText(i18n("Effective telescope focal length is updated to %1 mm.", new_focal_length));
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

    // Enable or Disable PAA depending on current FOV
    const bool fovOK = ((fov_x + fov_y) / 2.0) > PAH_CUTOFF_FOV;
    if (fovOK != (CHECK_PAH(isEnabled())))
        m_PolarAlignmentAssistant->setEnabled(fovOK);

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


QStringList Align::generateRemoteArgs(const QSharedPointer<FITSData> &data)
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
    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
    {
        if (remoteParserDevice == nullptr)
        {
            appendLogText(i18n("No remote astrometry driver detected, switching to StellarSolver."));
            setSolverMode(SOLVER_LOCAL);
        }
        else
        {
            // Update ACTIVE_CCD of the remote astrometry driver so it listens to BLOB emitted by the CCD
            auto activeDevices = remoteParserDevice->getBaseDevice()->getText("ACTIVE_DEVICES");
            if (activeDevices)
            {
                auto activeCCD = activeDevices->findWidgetByName("ACTIVE_CCD");
                if (QString(activeCCD->text) != CCDCaptureCombo->currentText())
                {
                    activeCCD->setText(CCDCaptureCombo->currentText().toLatin1().data());

                    remoteParserDevice->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
                }
            }

            // Enable remote parse
            dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->setEnabled(true);
            dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->sendArgs(generateRemoteArgs(QSharedPointer<FITSData>()));
            solverTimer.start();
        }
    }

    // Remove temporary FITS files left before by the solver
    QDir dir(QDir::tempPath());
    dir.setNameFilters(QStringList() << "fits*"  << "tmp.*");
    dir.setFilter(QDir::Files);
    for (auto &dirFile : dir.entryList())
        dir.remove(dirFile);

    prepareCapture(targetChip);

    // In case we're in refresh phase of the polar alignment helper then we use capture value from there
    if (matchPAHStage(PAA::PAH_REFRESH))
        targetChip->capture(m_PolarAlignmentAssistant->getPAHExposureDuration());
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
    if (matchPAHStage(PAA::PAH_REFRESH))
        return true;

    appendLogText(i18n("Capturing image..."));

    //This block of code will create the row in the solution table and populate RA, DE, and object name.
    //It also starts the progress indicator.
    double ra, dec;
    currentTelescope->getEqCoords(&ra, &dec);
    if (!m_SolveFromFile)
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

    RUN_PAH(setImageData(m_ImageData));

    // If it's Refresh, we're done
    if (matchPAHStage(PAA::PAH_REFRESH))
    {
        setCaptureComplete();
        return;
    }
    else
        appendLogText(i18n("Image received."));

    // If Local solver, then set capture complete or perform calibration first.
    if (solverModeButtonGroup->checkedId() == SOLVER_LOCAL)
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
                }

                setCaptureComplete();
            });
            connect(DarkLibrary::Instance(), &DarkLibrary::newLog, this, &Ekos::Align::appendLogText);
            DarkLibrary::Instance()->denoise(targetChip, m_ImageData, exposureIN->value(), targetChip->getCaptureFilter(),
                                             offsetX, offsetY);
            return;
        }

        setCaptureComplete();
    }
}

void Align::prepareCapture(ISD::CCDChip *targetChip)
{
    if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
    {
        rememberUploadMode = ISD::CCD::UPLOAD_LOCAL;
        currentCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
    }

    rememberCCDExposureLooping = currentCCD->isLooping();
    if (rememberCCDExposureLooping)
        currentCCD->setExposureLoopingEnabled(false);

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
}


void Align::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    if (matchPAHStage(PAA::PAH_REFRESH))
    {
        emit newFrame(alignView);
        m_PolarAlignmentAssistant->processPAHRefresh();
        return;
    }

    emit newImage(alignView);

    solverFOV->setImage(alignView->getDisplayImage());

    startSolving();
}

void Align::setSolverAction(int mode)
{
    gotoModeButtonGroup->button(mode)->setChecked(true);
    m_CurrentGotoMode = static_cast<GotoMode>(mode);
}

void Align::startSolving()
{
    //RUN_PAH(syncStage());

    // This is needed because they might have directories stored in the config file.
    // So we can't just use the options folder list.
    QStringList astrometryDataDirs = KSUtils::getAstrometryDataDirs();
    disconnect(alignView, &FITSView::loaded, this, &Align::startSolving);

    if (solverModeButtonGroup->checkedId() == SOLVER_LOCAL)
    {
        if(Options::solverType() != SSolver::SOLVER_ASTAP) //You don't need astrometry index files to use ASTAP
        {
            bool foundAnIndex = false;
            for(QString dataDir : astrometryDataDirs)
            {
                QDir dir = QDir(dataDir);
                if(dir.exists())
                {
                    dir.setNameFilters(QStringList() << "*.fits");
                    QStringList indexList = dir.entryList();
                    if(indexList.count() > 0)
                        foundAnIndex = true;
                }
            }
            if(!foundAnIndex)
            {
                appendLogText(
                    i18n("No index files were found on your system in the specified index file directories.  Please download some index files or add the correct directory to the list."));
                KConfigDialog * alignSettings = KConfigDialog::exists("alignsettings");
                if(alignSettings && m_IndexFilesPage)
                {
                    alignSettings->setCurrentPage(m_IndexFilesPage);
                    alignSettings->show();
                }
            }
        }
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
        if (!m_ImageData)
            m_ImageData = alignView->imageData();
        m_StellarSolver.reset(new StellarSolver(SSolver::SOLVE, m_ImageData->getStatistics(), m_ImageData->getImageBuffer()));
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

        bool useImageScale = Options::astrometryUseImageScale();
        if (useBlindScale == BLIND_ENGAGNED)
        {
            useImageScale = false;
            useBlindScale = BLIND_USED;
            appendLogText(i18n("Solving with blind image scale..."));
        }

        bool useImagePostion = Options::astrometryUsePosition();
        if (useBlindPosition == BLIND_ENGAGNED)
        {
            useImagePostion = false;
            useBlindPosition = BLIND_USED;
            appendLogText(i18n("Solving with blind image position..."));
        }

        if (m_SolveFromFile)
        {
            FITSImage::Solution solution;
            m_ImageData->parseSolution(solution);

            if (useImageScale && solution.pixscale > 0)
                m_StellarSolver->setSearchScale(solution.pixscale * 0.8,
                                                solution.pixscale * 1.2,
                                                SSolver::ARCSEC_PER_PIX);
            else
                m_StellarSolver->setProperty("UseScale", false);

            if (useImagePostion && solution.ra > 0)
                m_StellarSolver->setSearchPositionInDegrees(solution.ra, solution.dec);
            else
                m_StellarSolver->setProperty("UsePostion", false);
        }
        else
        {
            //Setting the initial search scale settings
            if (useImageScale)
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
            if(useImagePostion)
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
        remoteParser->startSolver(m_ImageData->filename(), generateRemoteArgs(m_ImageData), false);
    }

    // Kick off timer
    solverTimer.start();

    state = ALIGN_PROGRESS;
    emit newStatus(state);
}

void Align::solverComplete()
{
    disconnect(m_StellarSolver.get(), &StellarSolver::ready, this, &Align::solverComplete);
    if(!m_StellarSolver->solvingDone() || m_StellarSolver->failed())
    {
        // If processed, we retruned. Otherwise, it is a fail
        if (CHECK_PAH(processSolverFailure()))
            return;
        solverFailed();
        return;
    }
    else
    {
        FITSImage::Solution solution = m_StellarSolver->getSolution();
        // Would be better if parity was a bool field instead of a QString with "pos" and "neg" as possible values.
        const bool eastToTheRight = solution.parity == "pos" ? false : true;
        solverFinished(solution.orientation, solution.ra, solution.dec, solution.pixscale, eastToTheRight);
    }
}

void Align::solverFinished(double orientation, double ra, double dec, double pixscale, bool eastToTheRight)
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
    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParserDevice && remoteParser.get())
    {
        // Disable remote parse
        dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->setEnabled(false);
    }

    int binx, biny;
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    targetChip->getBinning(&binx, &biny);

    if (Options::alignmentLogging())
    {
        QString parityString = eastToTheRight ? "neg" : "pos";
        appendLogText(i18n("Solver RA (%1) DEC (%2) Orientation (%3) Pixel Scale (%4) Parity (%5)", QString::number(ra, 'f', 5),
                           QString::number(dec, 'f', 5), QString::number(orientation, 'f', 5),
                           QString::number(pixscale, 'f', 5), parityString));
    }

    // When solving (without Load&Slew), update effective FOV and focal length accordingly.
    if (!m_SolveFromFile &&
            (fov_x == 0 || m_EffectiveFOVPending || std::fabs(pixscale - fov_pixscale) > 0.005) &&
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
    if (!m_SolveFromFile)
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
        auto ccdRotation = currentCCD->getBaseDevice()->getNumber("CCD_ROTATION");
        if (ccdRotation)
        {
            auto rotation = ccdRotation->findWidgetByName("CCD_ROTATION_VALUE");
            if (rotation)
            {
                ClientManager *clientManager = currentCCD->getDriverInfo()->getClientManager();
                rotation->setValue(orientation);
                clientManager->sendNewNumber(ccdRotation);

                if (m_wcsSynced == false)
                {
                    appendLogText(
                        i18n("WCS information updated. Images captured from this point forward shall have valid WCS."));

                    // Just send telescope info in case the CCD driver did not pick up before.
                    auto telescopeInfo = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");
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
    if (!m_SolveFromFile && m_CurrentGotoMode == GOTO_SLEW)
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
    if (!m_SolveFromFile)
    {
        stopProgressAnimation();
        solutionTable->setCellWidget(currentRow, 3, new QWidget());
        statusReport->setFlags(Qt::ItemIsSelectable);
    }

    if (m_SolveFromFile && Options::astrometryUseRotator())
    {
        loadSlewTargetPA = solverPA;
        qCDebug(KSTARS_EKOS_ALIGN) << "loaSlewTargetPA:" << loadSlewTargetPA;
    }
    else if (Options::astrometryUseRotator())
    {
        currentRotatorPA = solverPA;

        // When Load&Slew image is solved, we check if we need to rotate the rotator to match the position angle of the image
        if (currentRotator != nullptr)
        {
            // Update Rotator offsets
            auto absAngle = currentRotator->getBaseDevice()->getNumber("ABS_ROTATOR_ANGLE");
            if (absAngle)
            {
                // PA = RawAngle * Multiplier + Offset
                double rawAngle = absAngle->at(0)->getValue();
                double offset   = range360(solverPA - (rawAngle * Options::pAMultiplier()));

                qCDebug(KSTARS_EKOS_ALIGN) << "Raw Rotator Angle:" << rawAngle << "Rotator PA:" << currentRotatorPA
                                           << "Rotator Offset:" << offset;
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
                absAngle->at(0)->setValue(rawAngle);
                ClientManager *clientManager = currentRotator->getDriverInfo()->getClientManager();
                clientManager->sendNewNumber(absAngle);
                appendLogText(i18n("Setting position angle to %1 degrees E of N...", loadSlewTargetPA));
                return;
            }
        }
        else if (std::isnan(loadSlewTargetPA) == false)
        {
            double current = range360(currentRotatorPA);
            double target = range360(loadSlewTargetPA);
            double targetFlipped = range360(loadSlewTargetPA + 180);

            double diff = current - target;
            if (fabs(current + 360.0 - target) < fabs(diff))
            {
                diff = current + 360.0 - target;
            }

            if (fabs(current - targetFlipped) < fabs(diff))
            {
                diff = current - targetFlipped;
                target = targetFlipped;
            }

            if (fabs(current + 360.0 - targetFlipped) < fabs(diff))
            {
                diff = current + 360.0 - targetFlipped;
                target = targetFlipped;
            }

            double threshold = Options::astrometryRotatorThreshold() / 60.0;

            appendLogText(i18n("Current Rotation is %1; Target Rotation is %2; diff: %3", current, target, diff));

            m_ManualRotator->setRotatorDiff(current, target, diff);
            if (fabs(diff) > threshold)
            {
                targetAccuracyNotMet = true;
                m_ManualRotator->show();
                m_ManualRotator->raise();
                return;
            }
            else
            {
                loadSlewTargetPA = std::numeric_limits<double>::quiet_NaN();
                targetAccuracyNotMet = false;
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

    switch (m_CurrentGotoMode)
    {
        case GOTO_SYNC:
            executeGOTO();

            if (!m_SolveFromFile)
            {
                stopProgressAnimation();
                statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
                solutionTable->setItem(currentRow, 3, statusReport.release());
            }

            return;

        case GOTO_SLEW:
            if (m_SolveFromFile || m_TargetDiffTotal > static_cast<double>(accuracySpin->value()))
            {
                if (!m_SolveFromFile && ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
                {
                    appendLogText(i18n("Maximum number of iterations reached. Solver failed."));

                    if (!m_SolveFromFile)
                    {
                        statusReport->setIcon(QIcon(":/icons/AlignFailure.svg"));
                        solutionTable->setItem(currentRow, 3, statusReport.release());
                    }

                    solverFailed();
                    return;
                }

                targetAccuracyNotMet = true;

                if (!m_SolveFromFile)
                {
                    stopProgressAnimation();
                    statusReport->setIcon(QIcon(":/icons/AlignWarning.svg"));
                    solutionTable->setItem(currentRow, 3, statusReport.release());
                }

                executeGOTO();
                return;
            }

            if (!m_SolveFromFile)
            {
                stopProgressAnimation();
                statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
                solutionTable->setItem(currentRow, 3, statusReport.release());
            }

            appendLogText(i18n("Target is within acceptable range. Astrometric solver is successful."));

            //            if (mountModelRunning)
            //            {
            //                finishAlignmentPoint(true);
            //                if (mountModelRunning)
            //                    return;
            //            }
            break;

        case GOTO_NOTHING:
            if (!m_SolveFromFile)
            {
                stopProgressAnimation();
                statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
                solutionTable->setItem(currentRow, 3, statusReport.release());
            }
            break;
    }

    KSNotification::event(QLatin1String("AlignSuccessful"), i18n("Astrometry alignment completed successfully"));
    state = ALIGN_COMPLETE;
    emit newStatus(state);
    solverIterations = 0;

    solverFOV->setProperty("visible", true);

    if (!matchPAHStage(PAA::PAH_IDLE))
        m_PolarAlignmentAssistant->processPAHStage(orientation, ra, dec, pixscale, eastToTheRight);
    else
    {
        solveB->setEnabled(true);
        loadSlewB->setEnabled(true);
    }
}

void Align::solverFailed()
{
    if (state != ALIGN_ABORTED)
    {
        // Try to solve with scale turned off, if not turned off already
        if (Options::astrometryUseImageScale() && useBlindScale == BLIND_IDLE)
        {
            useBlindScale = BLIND_ENGAGNED;
            setAlignTableResult(ALIGN_RESULT_FAILED);
            captureAndSolve();
            return;
        }

        // Try to solve with the position turned off, if not turned off already
        if (Options::astrometryUsePosition() && useBlindPosition == BLIND_IDLE)
        {
            useBlindPosition = BLIND_ENGAGNED;
            setAlignTableResult(ALIGN_RESULT_FAILED);
            captureAndSolve();
            return;
        }


        appendLogText(i18n("Solver Failed."));
        if(!Options::alignmentLogging())
            appendLogText(
                i18n("Please check you have sufficient stars in the image, the indicated FOV is correct, and the necessary index files are installed. Enable Alignment Logging in Setup Tab -> Logs to get detailed information on the failure."));

        KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed"),
                              KSNotification::EVENT_ALERT);
    }

    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);
    loadSlewB->setEnabled(true);

    m_AlignTimer.stop();

    m_SolveFromFile = false;
    solverIterations = 0;
    m_CaptureErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;
    m_SlewErrorCounter = 0;

    state = ALIGN_FAILED;
    emit newStatus(state);

    solverFOV->setProperty("visible", false);

    setAlignTableResult(ALIGN_RESULT_FAILED);
}

void Align::stop(Ekos::AlignState mode)
{
    m_CaptureTimer.stop();
    if (solverModeButtonGroup->checkedId() == SOLVER_LOCAL && m_StellarSolver)
        m_StellarSolver->abort();
    else if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser)
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

    m_SolveFromFile = false;
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
    if (matchPAHStage(PAA::PAH_POST_REFRESH))
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

    state = mode;
    emit newStatus(state);

    setAlignTableResult(ALIGN_RESULT_FAILED);
}

QProgressIndicator * Align::getProgressStatus()
{
    int currentRow = solutionTable->rowCount() - 1;

    // check if the current row indicates a progress state
    // 1. no row present
    if (currentRow < 0)
        return nullptr;
    // 2. indicator is not present or not a progress indicator
    QWidget *indicator = solutionTable->cellWidget(currentRow, 3);
    if (indicator == nullptr)
        return nullptr;
    return dynamic_cast<QProgressIndicator *>(indicator);
}

void Align::stopProgressAnimation()
{
    QProgressIndicator *progress_indicator = getProgressStatus();
    if (progress_indicator != nullptr)
        progress_indicator->stopAnimation();
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
                if (m_wasSlewStarted && matchPAHStage(PAA::PAH_FIND_CP))
                {
                    //qCDebug(KSTARS_EKOS_ALIGN) << "## PAH_FIND_CP--> setting slewStarted to FALSE";
                    m_wasSlewStarted = false;
                    appendLogText(i18n("Mount completed slewing near celestial pole. Capture again to verify."));
                    setSolverAction(GOTO_NOTHING);

                    m_PolarAlignmentAssistant->setPAHStage(PAA::PAH_FIRST_CAPTURE);
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
                        if (m_CurrentGotoMode == GOTO_SLEW)
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
                        if (m_SolveFromFile)
                        {
                            m_SolveFromFile = false;

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
                        }
                        else if (m_CurrentGotoMode == GOTO_SLEW || (m_MountModel && m_MountModel->isRunning()))
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
                        if (m_CurrentGotoMode == GOTO_SLEW)
                            Slew();
                        else
                            Sync();
                    }
                }

                return;
            }
        }

        RUN_PAH(processMountRotation(telescopeCoord.ra(), delaySpin->value()));
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
        if (matchPAHStage(PAA::PAH_IDLE))
        {
            // whoops, mount slews during alignment
            appendLogText(i18n("Slew detected, suspend solving..."));
            suspend();
            // reset the state to busy so that solving restarts after slewing finishes
            m_SolveFromFile = true;
            // if mount model is running, retry the current alignment point
            //            if (mountModelRunning)
            //                appendLogText(i18n("Restarting alignment point %1", currentAlignmentPoint + 1));
        }

        state = ALIGN_SLEWING;
    }
}

void Align::handleMountStatus()
{
    auto nvp = currentTelescope->getBaseDevice()->getNumber(currentTelescope->isJ2000() ? "EQUATORIAL_COORD" :
               "EQUATORIAL_EOD_COORD");

    if (nvp)
        processNumber(nvp);
}


void Align::executeGOTO()
{
    if (m_SolveFromFile)
    {
        targetCoord = alignCoord;
        SlewToTarget();
    }
    else if (m_CurrentGotoMode == GOTO_SYNC)
        Sync();
    else if (m_CurrentGotoMode == GOTO_SLEW)
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
    if (canSync && !m_SolveFromFile)
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
    if (fileURL.isEmpty())
        fileURL = QFileDialog::getOpenFileName(Ekos::Manager::Instance(), i18nc("@title:window", "Load Image"), dirPath,
                                               "Images (*.fits *.fits.fz *.fit *.fts "
                                               "*.jpg *.jpeg *.png *.gif *.bmp "
                                               "*.cr2 *.cr3 *.crw *.nef *.raf *.dng *.arw *.orf)");

    if (fileURL.isEmpty())
        return false;

    QFileInfo fileInfo(fileURL);

    dirPath = fileInfo.absolutePath();

    differentialSlewingActivated = false;

    m_SolveFromFile = true;

    if (m_PolarAlignmentAssistant)
        m_PolarAlignmentAssistant->stopPAHProcess();

    slewR->setChecked(true);
    m_CurrentGotoMode = GOTO_SLEW;

    solveB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParserDevice == nullptr)
    {
        appendLogText(i18n("No remote astrometry driver detected, switching to StellarSolver."));
        setSolverMode(SOLVER_LOCAL);
    }

    m_ImageData.clear();

    alignView->loadFile(fileURL, false);
    //m_FileToSolve = fileURL;
    connect(alignView, &FITSView::loaded, this, &Align::startSolving);

    return true;
}

bool Align::loadAndSlew(const QByteArray &image, const QString &extension)
{
    differentialSlewingActivated = false;
    m_SolveFromFile = true;
    RUN_PAH(stopPAHProcess());
    slewR->setChecked(true);
    m_CurrentGotoMode = GOTO_SLEW;
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
    if (!currentCCD)
        return;

    auto wcsControl = currentCCD->getBaseDevice()->getSwitch("WCS_CONTROL");

    auto wcs_enable  = wcsControl->findWidgetByName("WCS_ENABLE");
    auto wcs_disable = wcsControl->findWidgetByName("WCS_DISABLE");

    if (!wcs_enable || !wcs_disable)
        return;

    if ((wcs_enable->getState() == ISS_ON && enable) || (wcs_disable->getState() == ISS_ON && !enable))
        return;

    wcsControl->reset();
    if (enable)
    {
        appendLogText(i18n("World Coordinate System (WCS) is enabled. CCD rotation must be set either manually in the "
                           "CCD driver or by solving an image before proceeding to capture any further images, "
                           "otherwise the WCS information may be invalid."));
        wcs_enable->setState(ISS_ON);
    }
    else
    {
        wcs_disable->setState(ISS_ON);
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
        if (++m_CaptureErrorCounter == 3 && !matchPAHStage(PolarAlignmentAssistant::PAH_REFRESH))
        {
            appendLogText(i18n("Capture error. Aborting..."));
            abort();
            return;
        }

        appendLogText(i18n("Restarting capture attempt #%1", m_CaptureErrorCounter));
        setAlignTableResult(ALIGN_RESULT_FAILED);
        captureAndSolve();
    }
}

void Align::setAlignTableResult(AlignResult result)
{
    // Do nothing if the progress indicator is not running.
    // This is necessary since it could happen that a problem occurs
    // before #captureAndSolve() has been started and there does not
    // exist a table entry for the current run.
    QProgressIndicator *progress_indicator = getProgressStatus();
    if (progress_indicator == nullptr || ! progress_indicator->isAnimated())
        return;
    stopProgressAnimation();

    QIcon icon;
    switch (result)
    {
        case ALIGN_RESULT_SUCCESS:
            icon = QIcon(":/icons/AlignSuccess.svg");
            break;

        case ALIGN_RESULT_WARNING:
            icon = QIcon(":/icons/AlignWarning.svg");
            break;

        case ALIGN_RESULT_FAILED:
        default:
            icon = QIcon(":/icons/AlignFailure.svg");
            break;
    }
    int currentRow = solutionTable->rowCount() - 1;
    solutionTable->setCellWidget(currentRow, 3, new QWidget());
    QTableWidgetItem *statusReport = new QTableWidgetItem();
    statusReport->setIcon(icon);
    statusReport->setFlags(Qt::ItemIsSelectable);
    solutionTable->setItem(currentRow, 3, statusReport);
}

void Align::setFocusStatus(Ekos::FocusState state)
{
    m_FocusState = state;
}

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
                syncTargetToMount();
            }

        }
        break;
        case CAPTURE_ALIGNING:
            if (currentTelescope && currentTelescope->hasAlignmentModel() && Options::resetMountModelAfterMeridian())
            {
                qCDebug(KSTARS_EKOS_ALIGN) << "Post meridian flip mount model reset" << (currentTelescope->clearAlignmentModel() ?
                                           "successful." : "failed.");
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
        alignWidget->setWindowTitle(i18nc("@title:window", "Align Frame"));
        alignWidget->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
        alignWidget->showMaximized();
        alignWidget->show();
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

//void Align::setMountCoords(const QString &raStr, const QString &decStr, const QString &azStr,
//                           const QString &altStr, int pierSide, const QString &haStr)
//{
//    mountRa = dms(raStr, false);
//    mountDec = dms(decStr, true);
//    mountHa = dms(haStr, false);
//    mountAz = dms(azStr, true);
//    mountAlt = dms(altStr, true);
//    mountPierSide = static_cast<ISD::Telescope::PierSide>(pierSide);
//}

void Align::setMountStatus(ISD::Telescope::Status newState)
{
    switch (newState)
    {
        case ISD::Telescope::MOUNT_PARKING:
        case ISD::Telescope::MOUNT_SLEWING:
        case ISD::Telescope::MOUNT_MOVING:
            solveB->setEnabled(false);
            loadSlewB->setEnabled(false);
            break;

        default:
            if (state != ALIGN_PROGRESS)
            {
                solveB->setEnabled(true);
                if (matchPAHStage(PAA::PAH_IDLE))
                {
                    loadSlewB->setEnabled(true);
                }
            }
            break;
    }

    RUN_PAH(setMountStatus(newState));
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

void Align::zoomAlignView()
{
    alignView->ZoomDefault();

    emit newFrame(alignView);
}

void Align::setAlignZoom(double scale)
{
    if (scale > 1)
        alignView->ZoomIn();
    else if (scale < 1)
        alignView->ZoomOut();

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
    static bool init = false;

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
    if (syncControl("camera", CCDCaptureCombo) || init == false)
        checkCCD();
    // Filter Wheel
    if (syncControl("fw", FilterDevicesCombo) || init == false)
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
    if (currentCCD->hasGain())
    {
        syncControl("gain", GainSpin);
    }
    // ISO
    if (ISOCombo->count() > 1)
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

    init = true;
}

void Align::syncSettings()
{
    emit settingsUpdated(getSettings());
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

    qCDebug(KSTARS_EKOS_ALIGN) << "Target Coordinates updated to JNow RA:" << targetCoord.ra().toHMSString()
                               << "DE:" << targetCoord.dec().toDMSString();
}

void Align::setTargetRotation(double rotation)
{
    loadSlewTargetPA = rotation;

    qCDebug(KSTARS_EKOS_ALIGN) << "Target Rotation updated to: " << loadSlewTargetPA;
}

void Align::calculateAlignTargetDiff()
{
    if (matchPAHStage(PAA::PAH_FIRST_CAPTURE) || matchPAHStage(PAA::PAH_SECOND_CAPTURE)
            || matchPAHStage(PAA::PAH_THIRD_CAPTURE))
        return;
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
    SkyPoint currentMountCoords(ra, dec);

    qCDebug(KSTARS_EKOS_ALIGN) << "Mount Coordinates JNow RA:" << currentMountCoords.ra().toHMSString()
                               << "DE:" << currentMountCoords.dec().toDMSString();

    // While we can set targetCoord = J2000Coord now, it's better to use setTargetCoord
    // Function to do that in case of any changes in the future in that function.
    SkyPoint J2000Coord = currentMountCoords.catalogueCoord(KStarsData::Instance()->lt().djd());

    qCDebug(KSTARS_EKOS_ALIGN) << "Mount Coordinates J2000 RA:" << J2000Coord.ra().toHMSString()
                               << "DE:" << J2000Coord.dec().toDMSString();

    setTargetCoords(J2000Coord.ra0().Hours(), J2000Coord.dec0().Degrees());
}

QStringList Align::getStellarSolverProfiles()
{
    QStringList profiles;
    for (auto param : m_StellarSolverProfiles)
        profiles << param.listName;

    return profiles;
}

void Align::exportSolutionPoints()
{
    if (solutionTable->rowCount() == 0)
        return;

    QUrl exportFile = QFileDialog::getSaveFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Export Solution Points"),
                      alignURLPath,
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
    emit newLog(i18n("Solution Points Saved as: %1", path));
    file.close();
}

void Align::initPolarAlignmentAssistant()
{
    // Create PAA instance
    m_PolarAlignmentAssistant = new PolarAlignmentAssistant(this, alignView);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::captureAndSolve, this, &Ekos::Align::captureAndSolve);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::settleStarted, [this](double duration)
    {
        m_CaptureTimer.start(duration);
    });
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newAlignTableResult, this, &Ekos::Align::setAlignTableResult);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newFrame, this, &Ekos::Align::newFrame);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newPAHStage, this, &Ekos::Align::processPAHStage);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newLog, this, &Ekos::Align::appendLogText);

    tabWidget->addTab(m_PolarAlignmentAssistant, i18n("Polar Alignment"));
}

void Align::initManualRotator()
{
    if (m_ManualRotator)
        return;

    m_ManualRotator = new ManualRotator(this);
    connect(m_ManualRotator, &Ekos::ManualRotator::captureAndSolve, this, &Ekos::Align::captureAndSolve);
}

void Align::processPAHStage(int stage)
{
    switch (stage)
    {
        case PAA::PAH_POST_REFRESH:
        {
            Options::setAstrometrySolverWCS(rememberSolverWCS);
            Options::setAutoWCS(rememberAutoWCS);
            stop(ALIGN_IDLE);
        }
        break;
        case PAA::PAH_PRE_REFRESH:
            // If user stops here, we restore the settings, if not we
            // disable again in the refresh process
            // and restore when refresh is complete
            Options::setAstrometrySolverWCS(rememberSolverWCS);
            Options::setAutoWCS(rememberAutoWCS);
            break;
        case PAA::PAH_FIRST_CAPTURE:
            nothingR->setChecked(true);
            m_CurrentGotoMode = GOTO_NOTHING;
            loadSlewB->setEnabled(false);

            rememberSolverWCS = Options::astrometrySolverWCS();
            rememberAutoWCS   = Options::autoWCS();

            Options::setAutoWCS(false);
            Options::setAstrometrySolverWCS(true);
            break;
        case PAA::PAH_SECOND_CAPTURE:
        case PAA::PAH_THIRD_CAPTURE:
            if (delaySpin->value() >= DELAY_THRESHOLD_NOTIFY)
                emit newLog(i18n("Settling..."));
            m_CaptureTimer.start(delaySpin->value());
            break;

        default:
            break;
    }
}

bool Align::matchPAHStage(uint32_t stage)
{
    return m_PolarAlignmentAssistant && m_PolarAlignmentAssistant->getPAHStage() == stage;
}
}

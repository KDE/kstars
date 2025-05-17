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
#include "manualrotator.h"

// FITS
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsviewer.h"

// Auxiliary
#include "auxiliary/QProgressIndicator.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/auxiliary/darkprocessor.h"
#include "ekos/auxiliary/filtermanager.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ksnotification.h"
#include "kspaths.h"
#include "ksuserdb.h"
#include "fov.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/rotatorutils.h"

// INDI
#include "ekos/manager.h"
#include "indi/indidome.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/indifilterwheel.h"
#include "indi/indirotator.h"
#include "profileinfo.h"

// System Includes
#include <KActionCollection>
#include <basedevice.h>
#include <indicom.h>
#include <memory>

//Qt Includes
#include <QToolTip>
#include <QFileDialog>

// Qt version calming
#include <qtendl.h>

#define MAXIMUM_SOLVER_ITERATIONS 10
#define CAPTURE_RETRY_DELAY       10000
#define CAPTURE_ROTATOR_DELAY     5000  // After 5 seconds estimated value should be not bad
#define PAH_CUTOFF_FOV            10    // Minimum FOV width in arcminutes for PAH to work
#define CHECK_PAH(x) \
    m_PolarAlignmentAssistant && m_PolarAlignmentAssistant->x
#define RUN_PAH(x) \
    if (m_PolarAlignmentAssistant) m_PolarAlignmentAssistant->x

namespace Ekos
{

using PAA = PolarAlignmentAssistant;

bool isEqual(double a, double b)
{
    return std::abs(a - b) < 0.01;
}

Align::Align(const QSharedPointer<ProfileInfo> &activeProfile) : m_ActiveProfile(activeProfile)
{
    setupUi(this);

    qRegisterMetaType<Ekos::AlignState>("Ekos::AlignState");
    qDBusRegisterMetaType<Ekos::AlignState>();

    new AlignAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Align", this);

    dirPath = QDir::homePath();

    KStarsData::Instance()->clearTransientFOVs();

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

    // rotatorB->setIcon(QIcon::fromTheme("kstars_solarsystem"));
    rotatorB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    m_AlignView.reset(new AlignView(alignWidget, FITS_ALIGN));
    m_AlignView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_AlignView->setBaseSize(alignWidget->size());
    m_AlignView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();

    vlayout->addWidget(m_AlignView.get());
    alignWidget->setLayout(vlayout);

    connect(solveB, &QPushButton::clicked, this, [this]()
    {
        captureAndSolve(true);
    });

    connect(stopB, &QPushButton::clicked, this, &Ekos::Align::abort);

    // Effective FOV Edit
    connect(FOVOut, &QLineEdit::editingFinished, this, &Align::syncFOV);

    connect(loadSlewB, &QPushButton::clicked, this, [this]()
    {
        loadAndSlew();
    });

    gotoModeButtonGroup->setId(syncR, GOTO_SYNC);
    gotoModeButtonGroup->setId(slewR, GOTO_SLEW);
    gotoModeButtonGroup->setId(nothingR, GOTO_NOTHING);

    // Setup Debounce timer to limit over-activation of settings changes
    m_DebounceTimer.setInterval(500);
    m_DebounceTimer.setSingleShot(true);
    connect(&m_DebounceTimer, &QTimer::timeout, this, &Align::settleSettings);

    m_CurrentGotoMode = GOTO_SLEW;
    gotoModeButtonGroup->button(m_CurrentGotoMode)->setChecked(true);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(gotoModeButtonGroup, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), this,
            [ = ](int id, bool toggled)
#else
    connect(gotoModeButtonGroup, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::idToggled), this,
            [ = ](int id, bool toggled)
#endif
    {
        if (toggled)
            this->m_CurrentGotoMode = static_cast<GotoMode>(id);
    });

    m_CaptureTimer.setSingleShot(true);
    m_CaptureTimer.setInterval(CAPTURE_RETRY_DELAY);
    connect(&m_CaptureTimer, &QTimer::timeout, this, &Align::processCaptureTimeout);
    m_AlignTimer.setSingleShot(true);
    m_AlignTimer.setInterval(Options::astrometryTimeout() * 1000);
    connect(&m_AlignTimer, &QTimer::timeout, this, &Ekos::Align::checkAlignmentTimeout);

    pi.reset(new QProgressIndicator(this));
    stopLayout->addWidget(pi.get());

    rememberSolverWCS = Options::astrometrySolverWCS();
    rememberAutoWCS   = Options::autoWCS();

    solverModeButtonGroup->setId(localSolverR, SOLVER_LOCAL);
    solverModeButtonGroup->setId(remoteSolverR, SOLVER_REMOTE);

    setSolverMode(Options::solverMode());

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(solverModeButtonGroup, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), this,
            [ = ](int id, bool toggled)
#else
    connect(solverModeButtonGroup, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::idToggled), this,
            [ = ](int id, bool toggled)
#endif
    {
        if (toggled)
            setSolverMode(id);
    });

    connect(alignAccuracyThreshold, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this]()
    {
        buildTarget();
    });

    connect(alignBinning, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Align::setBinningIndex);

    connect(this, &Align::newStatus, this, [this](AlignState state)
    {
        opticalTrainCombo->setEnabled(state < ALIGN_PROGRESS);
        trainB->setEnabled(state < ALIGN_PROGRESS);
    });

    connect(alignUseCurrentFilter, &QCheckBox::toggled, this, &Align::checkFilter);

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    savedOptionsProfiles = QDir(KSPaths::writableLocation(
                                    QStandardPaths::AppLocalDataLocation)).filePath("SavedAlignProfiles.ini");
    if(QFile(savedOptionsProfiles).exists())
        m_StellarSolverProfiles = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        m_StellarSolverProfiles = getDefaultAlignOptionsProfiles();

    m_StellarSolver.reset(new StellarSolver());
    connect(m_StellarSolver.get(), &StellarSolver::logOutput, this, &Align::appendLogText);

    setupPolarAlignmentAssistant();
    setupManualRotator();
    setuptDarkProcessor();
    setupPlot();
    setupSolutionTable();
    setupOptions();

    // Load all settings
    loadGlobalSettings();
    connectSettings();

    setupOpticalTrainManager();
}

Align::~Align()
{
    if (m_StellarSolver.get() != nullptr)
        disconnect(m_StellarSolver.get(), &StellarSolver::logOutput, this, &Align::appendLogText);

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
    double accuracyRadius = alignAccuracyThreshold->value();
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
    double accuracyRadius = alignAccuracyThreshold->value();
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
    auto abstractItem = alignPlot->item(solutionTable->currentRow());
    if (abstractItem)
    {
        auto item = qobject_cast<QCPItemText *>(abstractItem);
        if (item)
        {
            double point = item->position->key();
            alignPlot->graph(0)->data()->remove(point);
        }
    }

    alignPlot->removeItem(solutionTable->currentRow());

    for (int i = 0; i < alignPlot->itemCount(); i++)
    {
        auto oneItem = alignPlot->item(i);
        if (oneItem)
        {
            auto item = qobject_cast<QCPItemText *>(oneItem);
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
        connect(m_MountModel, &Ekos::MountModel::aborted, this, [this]()
        {
            if (m_Mount && m_Mount->isSlewing())
                m_Mount->abort();
            abort();
        });
        connect(this, &Ekos::Align::newStatus, m_MountModel, &Ekos::MountModel::setAlignStatus, Qt::UniqueConnection);
    }

    m_MountModel->show();
}


bool Align::isParserOK()
{
    return true; //For now
    //    Q_ASSERT_X(parser, __FUNCTION__, "Astrometry parser is not valid.");

    //    bool rc = parser->init();

    //    if (rc)
    //    {
    //        connect(parser, &AstrometryParser::solverFinished, this, &Ekos::Align::solverFinished, Qt::UniqueConnection);
    //        connect(parser, &AstrometryParser::solverFailed, this, &Ekos::Align::solverFailed, Qt::UniqueConnection);
    //    }

    //    return rc;
}

void Align::checkAlignmentTimeout()
{
    if (m_SolveFromFile || ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
        abort();
    else
    {
        appendLogText(i18n("Solver timed out."));
        parser->stopSolver();

        setAlignTableResult(ALIGN_RESULT_FAILED);
        captureAndSolve(false);
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
        if (remoteParser.get() != nullptr && m_RemoteParserDevice != nullptr)
        {
            parser = remoteParser.get();
            (dynamic_cast<RemoteAstrometryParser *>(parser))->setAstrometryDevice(m_RemoteParserDevice);
            return;
        }

        remoteParser.reset(new Ekos::RemoteAstrometryParser());
        parser = remoteParser.get();
        (dynamic_cast<RemoteAstrometryParser *>(parser))->setAstrometryDevice(m_RemoteParserDevice);
        if (m_Camera)
            (dynamic_cast<RemoteAstrometryParser *>(parser))->setCCD(m_Camera->getDeviceName());

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

QString Align::camera()
{
    if (m_Camera)
        return m_Camera->getDeviceName();

    return QString();
}

void Align::checkCamera()
{
    if (!m_Camera)
        return;

    // Do NOT perform checks if align is in progress as this may result
    // in signals/slots getting disconnected.
    switch (state)
    {
        // Idle, camera change is OK.
        case ALIGN_IDLE:
        case ALIGN_COMPLETE:
        case ALIGN_FAILED:
        case ALIGN_ABORTED:
        case ALIGN_SUCCESSFUL:
            break;

        // Busy, camera change is not OK.
        case ALIGN_PROGRESS:
        case ALIGN_SYNCING:
        case ALIGN_SLEWING:
        case ALIGN_SUSPENDED:
        case ALIGN_ROTATING:
            return;
    }

    auto targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    if (targetChip == nullptr || (targetChip && targetChip->isCapturing()))
        return;

    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
        (dynamic_cast<RemoteAstrometryParser *>(remoteParser.get()))->setCCD(m_Camera->getDeviceName());

    syncCameraInfo();
    syncCameraControls();
    syncTelescopeInfo();
}

bool Align::setCamera(ISD::Camera *device)
{
    if (m_Camera && m_Camera == device)
    {
        checkCamera();
        return false;
    }

    if (m_Camera)
        m_Camera->disconnect(this);

    m_Camera = device;

    if (m_Camera)
    {
        connect(m_Camera, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
            controlBox->setEnabled(isConnected);
            gotoBox->setEnabled(isConnected);
            plateSolverOptionsGroup->setEnabled(isConnected);
            tabWidget->setEnabled(isConnected);
        });
        connect(m_Camera, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
            controlBox->setEnabled(isConnected);
            gotoBox->setEnabled(isConnected);
            plateSolverOptionsGroup->setEnabled(isConnected);
            tabWidget->setEnabled(isConnected);

            opticalTrainCombo->setEnabled(true);
            trainLabel->setEnabled(true);
        });
    }

    auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
    controlBox->setEnabled(isConnected);
    gotoBox->setEnabled(isConnected);
    plateSolverOptionsGroup->setEnabled(isConnected);
    tabWidget->setEnabled(isConnected);

    if (!m_Camera)
        return false;

    checkCamera();

    return true;
}

bool Align::setMount(ISD::Mount *device)
{
    if (m_Mount && m_Mount == device)
    {
        syncTelescopeInfo();
        return false;
    }

    if (m_Mount)
        m_Mount->disconnect(this);

    m_Mount = device;

    if (m_Mount)
    {
        connect(m_Mount, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
            controlBox->setEnabled(isConnected);
            gotoBox->setEnabled(isConnected);
            plateSolverOptionsGroup->setEnabled(isConnected);
            tabWidget->setEnabled(isConnected);
        });
        connect(m_Mount, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
            controlBox->setEnabled(isConnected);
            gotoBox->setEnabled(isConnected);
            plateSolverOptionsGroup->setEnabled(isConnected);
            tabWidget->setEnabled(isConnected);

            opticalTrainCombo->setEnabled(true);
            trainLabel->setEnabled(true);
        });
    }

    auto isConnected = m_Camera && m_Camera->isConnected() && m_Mount && m_Mount->isConnected();
    controlBox->setEnabled(isConnected);
    gotoBox->setEnabled(isConnected);
    plateSolverOptionsGroup->setEnabled(isConnected);
    tabWidget->setEnabled(isConnected);

    if (!m_Mount)
        return false;

    RUN_PAH(setCurrentTelescope(m_Mount));

    connect(m_Mount, &ISD::Mount::propertyUpdated, this, &Ekos::Align::updateProperty, Qt::UniqueConnection);
    connect(m_Mount, &ISD::Mount::Disconnected, this, [this]()
    {
        m_isRateSynced = false;
    });

    syncTelescopeInfo();
    return true;
}

bool Align::setDome(ISD::Dome *device)
{
    if (m_Dome && m_Dome == device)
        return false;

    if (m_Dome)
        m_Dome->disconnect(this);

    m_Dome = device;

    if (!m_Dome)
        return false;

    connect(m_Dome, &ISD::Dome::propertyUpdated, this, &Ekos::Align::updateProperty, Qt::UniqueConnection);
    return true;
}

void Align::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    auto name = device->getDeviceName();
    device->disconnect(this);

    // Check mounts
    if (m_Mount && m_Mount->getDeviceName() == name)
    {
        m_Mount->disconnect(this);
        m_Mount = nullptr;
    }

    // Check domes
    if (m_Dome && m_Dome->getDeviceName() == name)
    {
        m_Dome->disconnect(this);
        m_Dome = nullptr;
    }

    // Check rotators
    if (m_Rotator && m_Rotator->getDeviceName() == name)
    {
        m_Rotator->disconnect(this);
        m_Rotator = nullptr;
    }

    // Check cameras
    if (m_Camera && m_Camera->getDeviceName() == name)
    {
        m_Camera->disconnect(this);
        m_Camera = nullptr;

        QTimer::singleShot(1000, this, &Align::checkCamera);
    }

    // Check Remote Astrometry
    if (m_RemoteParserDevice && m_RemoteParserDevice->getDeviceName() == name)
    {
        m_RemoteParserDevice.clear();
    }

    // Check Filter Wheels
    if (m_FilterWheel && m_FilterWheel->getDeviceName() == name)
    {
        m_FilterWheel->disconnect(this);
        m_FilterWheel = nullptr;

        QTimer::singleShot(1000, this, &Align::checkFilter);
    }

}

bool Align::syncTelescopeInfo()
{
    if (m_Mount == nullptr || m_Mount->isConnected() == false)
        return false;

    if (m_isRateSynced == false)
    {
        auto speed = m_Settings["pAHMountSpeed"];
        auto slewRates = m_Mount->slewRates();
        if (speed.isValid())
        {
            RUN_PAH(syncMountSpeed(speed.toString()));
        }
        else if (!slewRates.isEmpty())
        {
            RUN_PAH(syncMountSpeed(slewRates.last()));
        }

        m_isRateSynced = !slewRates.empty();
    }

    canSync = m_Mount->canSync();

    if (canSync == false && syncR->isEnabled())
    {
        slewR->setChecked(true);
        appendLogText(i18n("Mount does not support syncing."));
    }

    syncR->setEnabled(canSync);

    if (m_FocalLength == -1 || m_Aperture == -1)
        return false;

    if (m_CameraPixelWidth != -1 && m_CameraPixelHeight != -1)
    {
        calculateFOV();
        return true;
    }

    return false;
}

void Align::syncCameraInfo()
{
    if (!m_Camera)
        return;

    auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
    Q_ASSERT(targetChip);

    // Get Maximum resolution and pixel size
    uint8_t bit_depth = 8;
    targetChip->getImageInfo(m_CameraWidth, m_CameraHeight, m_CameraPixelWidth, m_CameraPixelHeight, bit_depth);

    setWCSEnabled(Options::astrometrySolverWCS());

    int binx = 1, biny = 1;
    alignBinning->setEnabled(targetChip->canBin());
    if (targetChip->canBin())
    {
        alignBinning->blockSignals(true);

        targetChip->getMaxBin(&binx, &biny);
        alignBinning->clear();

        for (int i = 0; i < binx; i++)
            alignBinning->addItem(QString("%1x%2").arg(i + 1).arg(i + 1));

        auto binning = m_Settings["alignBinning"];
        if (binning.isValid())
            alignBinning->setCurrentText(binning.toString());

        alignBinning->blockSignals(false);
    }

    // In case ROI is different (smaller) than maximum resolution, let's use that.
    // N.B. 2022.08.14 JM: We must account for binning since this value is used for FOV calculations.
    int roiW = 0, roiH = 0;
    targetChip->getFrameMinMax(nullptr, nullptr, nullptr, nullptr, nullptr, &roiW, nullptr, &roiH);
    roiW *= binx;
    roiH *= biny;
    if ( (roiW > 0 && roiW < m_CameraWidth) || (roiH > 0 && roiH < m_CameraHeight))
    {
        m_CameraWidth = roiW;
        m_CameraHeight = roiH;
    }

    if (m_CameraPixelWidth > 0 && m_CameraPixelHeight > 0 && m_FocalLength > 0 && m_Aperture > 0)
    {
        calculateFOV();
    }
}

void Align::syncCameraControls()
{
    if (m_Camera == nullptr)
        return;

    auto targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    if (targetChip == nullptr || (targetChip && targetChip->isCapturing()))
        return;

    auto isoList = targetChip->getISOList();
    alignISO->clear();

    if (isoList.isEmpty())
    {
        alignISO->setEnabled(false);
    }
    else
    {
        alignISO->setEnabled(true);
        alignISO->addItems(isoList);
        alignISO->setCurrentIndex(targetChip->getISOIndex());
    }

    // Gain Check
    if (m_Camera->hasGain())
    {
        double min, max, step, value;
        m_Camera->getGainMinMaxStep(&min, &max, &step);

        // Allow the possibility of no gain value at all.
        alignGainSpecialValue = min - step;
        alignGain->setRange(alignGainSpecialValue, max);
        alignGain->setSpecialValueText(i18n("--"));
        alignGain->setEnabled(true);
        alignGain->setSingleStep(step);
        m_Camera->getGain(&value);

        auto gain = m_Settings["alignGain"];
        // Set the custom gain if we have one
        // otherwise it will not have an effect.
        if (gain.isValid())
            TargetCustomGainValue = gain.toDouble();
        if (TargetCustomGainValue > 0)
            alignGain->setValue(TargetCustomGainValue);
        else
            alignGain->setValue(alignGainSpecialValue);

        alignGain->setReadOnly(m_Camera->getGainPermission() == IP_RO);

        connect(alignGain, &QDoubleSpinBox::editingFinished, this, [this]()
        {
            if (alignGain->value() > alignGainSpecialValue)
                TargetCustomGainValue = alignGain->value();
        });
    }
    else
        alignGain->setEnabled(false);
}

void Align::getFOVScale(double &fov_w, double &fov_h, double &fov_scale)
{
    fov_w     = m_FOVWidth;
    fov_h     = m_FOVHeight;
    fov_scale = m_FOVPixelScale;
}

QList<double> Align::fov()
{
    QList<double> result;

    result << m_FOVWidth << m_FOVHeight << m_FOVPixelScale;

    return result;
}

QList<double> Align::cameraInfo()
{
    QList<double> result;

    result << m_CameraWidth << m_CameraHeight << m_CameraPixelWidth << m_CameraPixelHeight;

    return result;
}

QList<double> Align::telescopeInfo()
{
    QList<double> result;

    result << m_FocalLength << m_Aperture << m_Reducer;

    return result;
}

void Align::getCalculatedFOVScale(double &fov_w, double &fov_h, double &fov_scale)
{
    // FOV in arcsecs
    // DSLR
    auto reducedFocalLength = m_Reducer * m_FocalLength;
    if (m_FocalRatio > 0)
    {
        // The forumla is in radians, must convert to degrees.
        // Then to arcsecs
        fov_w = 3600 * 2 * atan(m_CameraWidth * (m_CameraPixelWidth / 1000.0) / (2 * reducedFocalLength)) / dms::DegToRad;
        fov_h = 3600 * 2 * atan(m_CameraHeight * (m_CameraPixelHeight / 1000.0) / (2 * reducedFocalLength)) / dms::DegToRad;
    }
    // Telescope
    else
    {
        fov_w = 206264.8062470963552 * m_CameraWidth * m_CameraPixelWidth / 1000.0 / reducedFocalLength;
        fov_h = 206264.8062470963552 * m_CameraHeight * m_CameraPixelHeight / 1000.0 / reducedFocalLength;
    }

    // Pix Scale
    fov_scale = (fov_w * (alignBinning->currentIndex() + 1)) / m_CameraWidth;

    // FOV in arcmins
    fov_w /= 60.0;
    fov_h /= 60.0;
}

void Align::calculateEffectiveFocalLength(double newFOVW)
{
    if (newFOVW < 0 || isEqual(newFOVW, m_FOVWidth))
        return;

    auto reducedFocalLength = m_Reducer * m_FocalLength;
    double new_focal_length = 0;

    if (m_FocalRatio > 0)
        new_focal_length = ((m_CameraWidth * m_CameraPixelWidth / 1000.0) / tan(newFOVW / 2)) / 2;
    else
        new_focal_length = ((m_CameraWidth * m_CameraPixelWidth / 1000.0) * 206264.8062470963552) / (newFOVW * 60.0);
    double focal_diff = std::fabs(new_focal_length - reducedFocalLength);

    if (focal_diff > 1)
    {
        m_EffectiveFocalLength = new_focal_length / m_Reducer;
        appendLogText(i18n("Effective telescope focal length is updated to %1 mm (FL: %2 Reducer: %3 W: %4 Pitch: %5 FOVW: %6).",
                           m_EffectiveFocalLength,
                           m_FocalLength,
                           m_Reducer,
                           m_CameraWidth,
                           m_CameraPixelWidth,
                           newFOVW));
    }
}

void Align::calculateFOV()
{
    auto reducedFocalLength = m_Reducer * m_FocalLength;
    auto reducecdEffectiveFocalLength = m_Reducer * m_EffectiveFocalLength;
    auto reducedFocalRatio = m_Reducer * m_FocalRatio;

    if (m_FocalRatio > 0)
    {
        // The forumla is in radians, must convert to degrees.
        // Then to arcsecs
        m_FOVWidth = 3600 * 2 * atan(m_CameraWidth * (m_CameraPixelWidth / 1000.0) / (2 * reducedFocalLength)) / dms::DegToRad;
        m_FOVHeight = 3600 * 2 * atan(m_CameraHeight * (m_CameraPixelHeight / 1000.0) / (2 * reducedFocalLength)) / dms::DegToRad;
    }
    // Telescope
    else
    {
        m_FOVWidth = 206264.8062470963552 * m_CameraWidth * m_CameraPixelWidth / 1000.0 / reducedFocalLength;
        m_FOVHeight = 206264.8062470963552 * m_CameraHeight * m_CameraPixelHeight / 1000.0 / reducedFocalLength;
    }

    // Calculate FOV

    // Pix Scale
    m_FOVPixelScale = (m_FOVWidth * (alignBinning->currentIndex() + 1)) / m_CameraWidth;

    // FOV in arcmins
    m_FOVWidth /= 60.0;
    m_FOVHeight /= 60.0;

    double calculated_fov_x = m_FOVWidth;
    double calculated_fov_y = m_FOVHeight;

    QString calculatedFOV = (QString("%1' x %2'").arg(m_FOVWidth, 0, 'f', 1).arg(m_FOVHeight, 0, 'f', 1));

    // Put FOV upper limit as 180 degrees
    if (m_FOVWidth < 1 || m_FOVWidth > 60 * 180 || m_FOVHeight < 1 || m_FOVHeight > 60 * 180)
    {
        appendLogText(
            i18n("Warning! The calculated field of view (%1) is out of bounds. Ensure the telescope focal length and camera pixel size are correct.",
                 calculatedFOV));
        return;
    }

    FocalLengthOut->setText(QString("%1 (%2)").arg(reducedFocalLength, 0, 'f', 1).
                            arg(m_EffectiveFocalLength > 0 ? reducecdEffectiveFocalLength : reducedFocalLength, 0, 'f', 1));
    // DSLR
    if (m_FocalRatio > 0)
        FocalRatioOut->setText(QString("%1 (%2)").arg(reducedFocalRatio, 0, 'f', 1).
                               arg(m_EffectiveFocalLength > 0 ? reducecdEffectiveFocalLength / m_Aperture : reducedFocalRatio, 0,
                                   'f', 1));
    // Telescope
    else if (m_Aperture > 0)
        FocalRatioOut->setText(QString("%1 (%2)").arg(reducedFocalLength / m_Aperture, 0, 'f', 1).
                               arg(m_EffectiveFocalLength > 0 ? reducecdEffectiveFocalLength / m_Aperture : reducedFocalLength / m_Aperture, 0,
                                   'f', 1));
    ReducerOut->setText(QString("%1x").arg(m_Reducer, 0, 'f', 2));

    if (m_EffectiveFocalLength > 0)
    {
        double focal_diff = std::fabs(m_EffectiveFocalLength  - m_FocalLength);
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

    if (m_FOVWidth == 0)
    {
        //FOVOut->setReadOnly(false);
        FOVOut->setToolTip(
            i18n("<p>Effective field of view size in arcminutes.</p><p>Please capture and solve once to measure the effective FOV or enter the values manually.</p><p>Calculated FOV: %1</p>",
                 calculatedFOV));
        m_FOVWidth = calculated_fov_x;
        m_FOVHeight = calculated_fov_y;
        m_EffectiveFOVPending = true;
    }
    else
    {
        m_EffectiveFOVPending = false;
        FOVOut->setToolTip(i18n("<p>Effective field of view size in arcminutes.</p>"));
    }

    solverFOV->setSize(m_FOVWidth, m_FOVHeight);
    sensorFOV->setSize(m_FOVWidth, m_FOVHeight);
    if (m_Camera)
        sensorFOV->setName(m_Camera->getDeviceName());

    FOVOut->setText(QString("%1' x %2'").arg(m_FOVWidth, 0, 'f', 1).arg(m_FOVHeight, 0, 'f', 1));

    // Enable or Disable PAA depending on current FOV
    const bool fovOK = ((m_FOVWidth + m_FOVHeight) / 2.0) > PAH_CUTOFF_FOV;
    if (m_PolarAlignmentAssistant != nullptr)
        m_PolarAlignmentAssistant->setEnabled(fovOK);

    if (Options::astrometryUseImageScale())
    {
        auto unitType = Options::astrometryImageScaleUnits();

        // Degrees
        if (unitType == 0)
        {
            double fov_low  = qMin(m_FOVWidth / 60, m_FOVHeight / 60);
            double fov_high = qMax(m_FOVWidth / 60, m_FOVHeight / 60);
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_low);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_high);

            Options::setAstrometryImageScaleLow(fov_low);
            Options::setAstrometryImageScaleHigh(fov_high);
        }
        // Arcmins
        else if (unitType == 1)
        {
            double fov_low  = qMin(m_FOVWidth, m_FOVHeight);
            double fov_high = qMax(m_FOVWidth, m_FOVHeight);
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(fov_low);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(fov_high);

            Options::setAstrometryImageScaleLow(fov_low);
            Options::setAstrometryImageScaleHigh(fov_high);
        }
        // Arcsec per pixel
        else
        {
            opsAstrometry->kcfg_AstrometryImageScaleLow->setValue(m_FOVPixelScale * 0.9);
            opsAstrometry->kcfg_AstrometryImageScaleHigh->setValue(m_FOVPixelScale * 1.1);

            // 10% boundary
            Options::setAstrometryImageScaleLow(m_FOVPixelScale * 0.9);
            Options::setAstrometryImageScaleHigh(m_FOVPixelScale * 1.1);
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
            if (Options::astrometryAutoDownsample() && m_CameraWidth && m_CameraHeight)
            {
                uint16_t w = m_CameraWidth / (alignBinning->currentIndex() + 1);
                optionsMap["downsample"] = getSolverDownsample(w);
            }
            else
                optionsMap["downsample"] = Options::astrometryDownsample();
        }

        //Options needed for Sextractor
        optionsMap["image_width"] = m_CameraWidth / (alignBinning->currentIndex() + 1);
        optionsMap["image_height"] = m_CameraHeight / (alignBinning->currentIndex() + 1);

        if (Options::astrometryUseImageScale() && m_FOVWidth > 0 && m_FOVHeight > 0)
        {
            QString units = "dw";
            if (Options::astrometryImageScaleUnits() == 1)
                units = "aw";
            else if (Options::astrometryImageScaleUnits() == 2)
                units = "app";
            if (Options::astrometryAutoUpdateImageScale())
            {
                QString fov_low, fov_high;
                double fov_w = m_FOVWidth;
                //double fov_h = m_FOVHeight;

                if (Options::astrometryImageScaleUnits() == SSolver::DEG_WIDTH)
                {
                    fov_w /= 60;
                    //fov_h /= 60;
                }
                else if (Options::astrometryImageScaleUnits() == SSolver::ARCSEC_PER_PIX)
                {
                    fov_w = m_FOVPixelScale;
                    //fov_h = m_FOVPixelScale;
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

        if (Options::astrometryUsePosition() && m_Mount != nullptr)
        {
            double ra = 0, dec = 0;
            m_Mount->getEqCoords(&ra, &dec);

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


// For option "differential slewing" destination coords have to be initialized, if call comes from "outside".
// Initialization is activated through the predefined argument "initialCall = true".
bool Align::captureAndSolve(bool initialCall)
{
    // Set target to current telescope position,if no object is selected yet.
    if (m_TargetCoord.ra().degree() < 0) // see default constructor skypoint()
    {
        if (m_TelescopeCoord.isValid() == false)
        {
            appendLogText(i18n("Mount coordinates are invalid. Check mount connection and try again."));
            KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed"), KSNotification::Align,
                                  KSNotification::Alert);
            return false;
        }

        m_TargetCoord = m_TelescopeCoord;
        appendLogText(i18n("Setting target to RA:%1 DEC:%2",
                           m_TargetCoord.ra().toHMSString(true), m_TargetCoord.dec().toDMSString(true)));
    }

    // Target coords will move the scope and in case of differential align the destination get lost.
    // Thus we have to save these coords for later use (-> SlewToTarget()).
    // This does not affect normal align, where destination is not used.
    if (initialCall)
        m_DestinationCoord = m_TargetCoord;

    qCDebug(KSTARS_EKOS_ALIGN) << "Capture&Solve - Target RA:" <<  m_TargetCoord.ra().toHMSString(true)
                               << " DE:" << m_TargetCoord.dec().toDMSString(true);
    qCDebug(KSTARS_EKOS_ALIGN) << "Capture&Solve - Destination RA:" <<  m_DestinationCoord.ra().toHMSString(true)
                               << " DE:" << m_DestinationCoord.dec().toDMSString(true);
    m_AlignTimer.stop();
    m_CaptureTimer.stop();

    if (m_Camera == nullptr)
    {
        appendLogText(i18n("Error: No camera detected."));
        return false;
    }

    if (m_Camera->isConnected() == false)
    {
        appendLogText(i18n("Error: lost connection to camera."));
        KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed"), KSNotification::Align,
                              KSNotification::Alert);
        return false;
    }

    if (m_Camera->isBLOBEnabled() == false)
    {
        m_Camera->setBLOBEnabled(true);
    }

    //if (parser->init() == false)
    //    return false;

    if (m_FocalLength == -1 || m_Aperture == -1)
    {
        KSNotification::error(
            i18n("Telescope aperture and focal length are missing. Please check your optical train settings and try again."));
        return false;
    }

    if (m_CameraPixelWidth == -1 || m_CameraPixelHeight == -1)
    {
        KSNotification::error(i18n("CCD pixel size is missing. Please check your driver settings and try again."));
        return false;
    }

    if (m_FilterWheel != nullptr)
    {
        if (m_FilterWheel->isConnected() == false)
        {
            appendLogText(i18n("Error: lost connection to filter wheel."));
            return false;
        }

        int targetPosition = alignFilter->currentIndex() + 1;

        if (targetPosition > 0 && targetPosition != currentFilterPosition)
        {
            filterPositionPending    = true;
            // Disabling the autofocus policy for align.
            m_FilterManager->setFilterPosition(targetPosition, FilterManager::NO_AUTOFOCUS_POLICY);
            setState(ALIGN_PROGRESS);
            return true;
        }
    }

    auto clientManager = m_Camera->getDriverInfo()->getClientManager();
    if (clientManager && clientManager->getBLOBMode(m_Camera->getDeviceName().toLatin1().constData(), "CCD1") == B_NEVER)
    {
        if (KMessageBox::warningContinueCancel(
                    nullptr, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) ==
                KMessageBox::Continue)
        {
            clientManager->setBLOBMode(B_ONLY, m_Camera->getDeviceName().toLatin1().constData(), "CCD1");
            clientManager->setBLOBMode(B_ONLY, m_Camera->getDeviceName().toLatin1().constData(), "CCD2");
        }
        else
        {
            return false;
        }
    }

    double seqExpose = alignExposure->value();

    auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

    if (m_FocusState >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy. Retrying in %1 seconds...",
                           CAPTURE_RETRY_DELAY / 1000));
        m_CaptureTimer.start(CAPTURE_RETRY_DELAY);
        return true;
    }

    if (targetChip->isCapturing())
    {
        appendLogText(i18n("Cannot capture while CCD exposure is in progress. Retrying in %1 seconds...",
                           CAPTURE_RETRY_DELAY / 1000));
        m_CaptureTimer.start(CAPTURE_RETRY_DELAY);
        return true;
    }

    if (m_Dome && m_Dome->isMoving())
    {
        qCWarning(KSTARS_EKOS_ALIGN) << "Cannot capture while dome is in motion. Retrying in" <<  CAPTURE_RETRY_DELAY / 1000 <<
                                        "seconds...";
        m_CaptureTimer.start(CAPTURE_RETRY_DELAY);
        return true;
    }

    // Let rotate the camera BEFORE taking a capture in [Capture & Solve]
    if (!m_SolveFromFile && m_Rotator && m_Rotator->absoluteAngleState() == IPS_BUSY)
    {
        int TimeOut = CAPTURE_ROTATOR_DELAY;
        switch (m_CaptureTimeoutCounter)
        {
            case 0:// Set start time & start angle and estimate rotator time frame during first timeout
            {
                auto absAngle = 0;
                if ((absAngle = m_Rotator->getNumber("ABS_ROTATOR_ANGLE")->at(0)->getValue()))
                {
                    RotatorUtils::Instance()->startTimeFrame(absAngle);
                    m_estimateRotatorTimeFrame = true;
                    appendLogText(i18n("Cannot capture while rotator is busy: Time delay estimate started..."));
                }
                m_CaptureTimer.start(TimeOut);
                break;
            }
            case 1:// Use estimated time frame (in updateProperty()) for second timeout
            {
                TimeOut = m_RotatorTimeFrame * 1000;
                [[fallthrough]];
            }
            default:
            {
                TimeOut *= m_CaptureTimeoutCounter; // Extend Timeout in case estimated value is too short
                m_estimateRotatorTimeFrame = false;
                appendLogText(i18n("Cannot capture while rotator is busy: Retrying in %1 seconds...", TimeOut / 1000));
                m_CaptureTimer.start(TimeOut);
            }
        }
        return true; // Return value is used in 'Scheduler::startAstrometry()'
    }

    m_AlignView->setBaseSize(alignWidget->size());
    m_AlignView->setProperty("suspended", (solverModeButtonGroup->checkedId() == SOLVER_LOCAL
                                           && alignDarkFrame->isChecked()));

    connect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Align::processData);
    connect(m_Camera, &ISD::Camera::newExposureValue, this, &Ekos::Align::checkCameraExposureProgress);

    // In case of remote solver, check if we need to update active CCD
    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
    {
        if (m_RemoteParserDevice == nullptr)
        {
            appendLogText(i18n("No remote astrometry driver detected, switching to StellarSolver."));
            setSolverMode(SOLVER_LOCAL);
        }
        else
        {
            // Update ACTIVE_CCD of the remote astrometry driver so it listens to BLOB emitted by the CCD
            auto activeDevices = m_RemoteParserDevice->getBaseDevice().getText("ACTIVE_DEVICES");
            if (activeDevices)
            {
                auto activeCCD = activeDevices.findWidgetByName("ACTIVE_CCD");
                if (QString(activeCCD->text) != m_Camera->getDeviceName())
                {
                    activeCCD->setText(m_Camera->getDeviceName().toLatin1().constData());
                    m_RemoteParserDevice->getClientManager()->sendNewProperty(activeDevices);
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

    solveB->setEnabled(false);
    loadSlewB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    RotatorGOTO = false;

    setState(ALIGN_PROGRESS);
    emit newStatus(state);
    solverFOV->setProperty("visible", true);

    // If we're just refreshing, then we're done
    if (matchPAHStage(PAA::PAH_REFRESH))
        return true;

    appendLogText(i18n("Capturing image..."));

    if (!m_Mount)
        return true;

    //This block of code will create the row in the solution table and populate RA, DE, and object name.
    //It also starts the progress indicator.
    double ra, dec;
    m_Mount->getEqCoords(&ra, &dec);
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
#ifdef Q_OS_MACOS
        repaint(); //This is a band-aid for a bug in QT 5.10.0
#endif

        QProgressIndicator *alignIndicator = new QProgressIndicator(this);
        solutionTable->setCellWidget(currentRow, 3, alignIndicator);
        alignIndicator->startAnimation();
#ifdef Q_OS_MACOS
        repaint(); //This is a band-aid for a bug in QT 5.10.0
#endif
    }

    return true;
}

void Align::processData(const QSharedPointer<FITSData> &data)
{
    auto chip = data->property("chip");
    if (chip.isValid() && chip.toInt() == ISD::CameraChip::GUIDE_CCD)
        return;

    disconnect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Align::processData);
    disconnect(m_Camera, &ISD::Camera::newExposureValue, this, &Ekos::Align::checkCameraExposureProgress);

    if (data)
    {
        m_AlignView->loadData(data);
        m_ImageData = data;
    }
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
        if (alignDarkFrame->isChecked())
        {
            int x, y, w, h, binx = 1, biny = 1;
            ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
            targetChip->getFrame(&x, &y, &w, &h);
            targetChip->getBinning(&binx, &biny);

            uint16_t offsetX = x / binx;
            uint16_t offsetY = y / biny;

            m_DarkProcessor->denoise(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()),
                                     targetChip, m_ImageData, alignExposure->value(), offsetX, offsetY);
            return;
        }

        setCaptureComplete();
    }
}

void Align::prepareCapture(ISD::CameraChip *targetChip)
{
    if (m_Camera->getUploadMode() == ISD::Camera::UPLOAD_REMOTE)
    {
        rememberUploadMode = ISD::Camera::UPLOAD_REMOTE;
        m_Camera->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
    }

    if (m_Camera->isFastExposureEnabled())
    {
        m_RememberCameraFastExposure = true;
        m_Camera->setFastExposureEnabled(false);
    }

    m_Camera->setEncodingFormat("FITS");
    targetChip->resetFrame();
    targetChip->setBatchMode(false);
    targetChip->setCaptureMode(FITS_ALIGN);
    targetChip->setFrameType(FRAME_LIGHT);

    int bin = alignBinning->currentIndex() + 1;
    targetChip->setBinning(bin, bin);

    // Set gain if applicable
    if (m_Camera->hasGain() && alignGain->isEnabled() && alignGain->value() > alignGainSpecialValue)
        m_Camera->setGain(alignGain->value());
    // Set ISO if applicable
    if (alignISO->currentIndex() >= 0)
        targetChip->setISOIndex(alignISO->currentIndex());
}


void Align::setCaptureComplete()
{
    if (matchPAHStage(PAA::PAH_REFRESH))
    {
        emit newFrame(m_AlignView);
        m_PolarAlignmentAssistant->processPAHRefresh();
        return;
    }

    emit newImage(m_AlignView);

    solverFOV->setImage(m_AlignView->getDisplayImage());

    // If align logging is enabled, let's save the frame.
    if (Options::saveAlignImages())
    {
        QDir dir;
        QDateTime now = KStarsData::Instance()->lt();
        QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("align/" +
                       now.toString("yyyy-MM-dd"));
        dir.mkpath(path);
        // IS8601 contains colons but they are illegal under Windows OS, so replacing them with '-'
        // The timestamp is no longer ISO8601 but it should solve interoperality issues between different OS hosts
        QString name     = "align_frame_" + now.toString("HH-mm-ss") + ".fits";
        QString filename = path + QStringLiteral("/") + name;
        if (m_ImageData)
            m_ImageData->saveImage(filename);
    }

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
    disconnect(m_AlignView.get(), &FITSView::loaded, this, &Align::startSolving);

    m_UsedScale = false;
    m_UsedPosition = false;
    m_ScaleUsed = 0;
    m_RAUsed = 0;
    m_DECUsed = 0;

    if (solverModeButtonGroup->checkedId() == SOLVER_LOCAL)
    {
        if(Options::solverType() != SSolver::SOLVER_ASTAP
                && Options::solverType() != SSolver::SOLVER_WATNEYASTROMETRY) //You don't need astrometry index files to use ASTAP or Watney
        {
            bool foundAnIndex = false;
            for(auto &dataDir : astrometryDataDirs)
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
        if (m_StellarSolver->isRunning())
            m_StellarSolver->abort();
        if (!m_ImageData)
            m_ImageData = m_AlignView->imageData();
        m_StellarSolver->loadNewImageBuffer(m_ImageData->getStatistics(), m_ImageData->getImageBuffer());
        m_StellarSolver->setProperty("ProcessType", SSolver::SOLVE);
        m_StellarSolver->setProperty("ExtractorType", Options::solveSextractorType());
        m_StellarSolver->setProperty("SolverType", Options::solverType());
        connect(m_StellarSolver.get(), &StellarSolver::ready, this, &Align::solverComplete);
        m_StellarSolver->setIndexFolderPaths(Options::astrometryIndexFolderList());

        SSolver::Parameters params;
        // Get solver parameters
        // In case of exception, use first profile
        try
        {
            params = m_StellarSolverProfiles.at(Options::solveOptionsProfile());
        }
        catch (std::out_of_range const &)
        {
            params = m_StellarSolverProfiles[0];
        }

        params.partition = Options::stellarSolverPartition();
        m_StellarSolver->setParameters(params);

        const SSolver::SolverType type = static_cast<SSolver::SolverType>(m_StellarSolver->property("SolverType").toInt());
        if(type == SSolver::SOLVER_LOCALASTROMETRY || type == SSolver::SOLVER_ASTAP || type == SSolver::SOLVER_WATNEYASTROMETRY)
        {
            QString filename = QDir::tempPath() + QString("/solver%1.fits").arg(QUuid::createUuid().toString().remove(
                                   QRegularExpression("[-{}]")));
            m_AlignView->saveImage(filename);
            m_StellarSolver->setProperty("FileToProcess", filename);
            ExternalProgramPaths externalPaths;
            externalPaths.sextractorBinaryPath = Options::sextractorBinary();
            externalPaths.solverPath = Options::astrometrySolverBinary();
            externalPaths.astapBinaryPath = Options::aSTAPExecutable();
            externalPaths.watneyBinaryPath = Options::watneyBinary();
            externalPaths.wcsPath = Options::astrometryWCSInfo();
            m_StellarSolver->setExternalFilePaths(externalPaths);

            //No need for a conf file this way.
            m_StellarSolver->setProperty("AutoGenerateAstroConfig", true);
        }

        if(type == SSolver::SOLVER_ONLINEASTROMETRY )
        {
            QString filename = QDir::tempPath() + QString("/solver%1.fits").arg(QUuid::createUuid().toString().remove(
                                   QRegularExpression("[-{}]")));
            m_AlignView->saveImage(filename);

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

        bool useImagePosition = Options::astrometryUsePosition();
        if (useBlindPosition == BLIND_ENGAGNED)
        {
            useImagePosition = false;
            useBlindPosition = BLIND_USED;
            appendLogText(i18n("Solving with blind image position..."));
        }

        if (m_SolveFromFile)
        {
            FITSImage::Solution solution;
            m_ImageData->parseSolution(solution);

            if (useImageScale && solution.pixscale > 0)
            {
                m_UsedScale = true;
                m_ScaleUsed = solution.pixscale;
                m_StellarSolver->setSearchScale(solution.pixscale * 0.8,
                                                solution.pixscale * 1.2,
                                                SSolver::ARCSEC_PER_PIX);
            }
            else
                m_StellarSolver->setProperty("UseScale", false);

            if (useImagePosition && solution.ra > 0)
            {
                m_UsedPosition = true;
                m_RAUsed = solution.ra;
                m_DECUsed = solution.dec;
                m_StellarSolver->setSearchPositionInDegrees(solution.ra, solution.dec);
            }
            else
                m_StellarSolver->setProperty("UsePosition", false);

            QVariant value = "";
            if (!m_ImageData->getRecordValue("PIERSIDE", value))
            {
                appendLogText(i18n("Loaded image does not have pierside information"));
                m_TargetPierside = ISD::Mount::PIER_UNKNOWN;
            }
            else
            {
                appendLogText(i18n("Loaded image was taken on pierside %1", value.toString()));
                (value == "WEST") ? m_TargetPierside = ISD::Mount::PIER_WEST : m_TargetPierside = ISD::Mount::PIER_EAST;
            }
            RotatorUtils::Instance()->Instance()->setImagePierside(m_TargetPierside);
        }
        else
        {
            //Setting the initial search scale settings
            if (useImageScale)
            {
                m_UsedScale = true;
                m_ScaleUsed = Options::astrometryImageScaleLow();

                SSolver::ScaleUnits units = static_cast<SSolver::ScaleUnits>(Options::astrometryImageScaleUnits());
                // Extend search scale from 80% to 120%
                m_StellarSolver->setSearchScale(Options::astrometryImageScaleLow() * 0.8,
                                                Options::astrometryImageScaleHigh() * 1.2,
                                                units);
            }
            else
                m_StellarSolver->setProperty("UseScale", false);
            //Setting the initial search location settings
            if(useImagePosition)
            {
                m_StellarSolver->setSearchPositionInDegrees(m_TelescopeCoord.ra().Degrees(), m_TelescopeCoord.dec().Degrees());
                m_UsedPosition = true;
                m_RAUsed = m_TelescopeCoord.ra().Degrees();
                m_DECUsed = m_TelescopeCoord.dec().Degrees();
            }
            else
                m_StellarSolver->setProperty("UsePosition", false);
        }

        if(Options::alignmentLogging())
        {
            // Not trusting SSolver logging right now (Hy Aug 1, 2022)
            // m_StellarSolver->setLogLevel(static_cast<SSolver::logging_level>(Options::loggerLevel()));
            // m_StellarSolver->setSSLogLevel(SSolver::LOG_NORMAL);
            m_StellarSolver->setLogLevel(SSolver::LOG_NONE);
            m_StellarSolver->setSSLogLevel(SSolver::LOG_OFF);
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

        SolverUtils::patchMultiAlgorithm(m_StellarSolver.get());

        // Start solving process
        m_StellarSolver->start();
    }
    else
    {
        if (m_ImageData.isNull())
            m_ImageData = m_AlignView->imageData();
        // This should run only for load&slew. For regular solve, we don't get here
        // as the image is read and solved server-side.
        remoteParser->startSolver(m_ImageData->filename(), generateRemoteArgs(m_ImageData), false);
    }

    // In these cases, the error box is not used, and we don't want it polluted
    // from some previous operation.
    if (matchPAHStage(PAA::PAH_FIRST_CAPTURE) ||
            matchPAHStage(PAA::PAH_SECOND_CAPTURE) ||
            matchPAHStage(PAA::PAH_THIRD_CAPTURE) ||
            matchPAHStage(PAA::PAH_FIRST_SOLVE) ||
            matchPAHStage(PAA::PAH_SECOND_SOLVE) ||
            matchPAHStage(PAA::PAH_THIRD_SOLVE) ||
            nothingR->isChecked() ||
            syncR->isChecked())
        errOut->clear();

    // Kick off timer
    solverTimer.start();

    setState(ALIGN_PROGRESS);
    emit newStatus(state);
}

void Align::solverComplete()
{
    disconnect(m_StellarSolver.get(), &StellarSolver::ready, this, &Align::solverComplete);
    if(!m_StellarSolver->solvingDone() || m_StellarSolver->failed())
    {
        if (matchPAHStage(PAA::PAH_FIRST_CAPTURE) ||
                matchPAHStage(PAA::PAH_SECOND_CAPTURE) ||
                matchPAHStage(PAA::PAH_THIRD_CAPTURE) ||
                matchPAHStage(PAA::PAH_FIRST_SOLVE) ||
                matchPAHStage(PAA::PAH_SECOND_SOLVE) ||
                matchPAHStage(PAA::PAH_THIRD_SOLVE))
        {
            if (CHECK_PAH(processSolverFailure()))
                return;
            else
                setState(ALIGN_ABORTED);
        }
        solverFailed();
        return;
    }
    else
    {
        FITSImage::Solution solution = m_StellarSolver->getSolution();
        const bool eastToTheRight = solution.parity == FITSImage::POSITIVE ? false : true;
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
    if (elapsed > 0)
        appendLogText(i18n("Solver completed after %1 seconds.", QString::number(elapsed, 'f', 2)));

    m_AlignTimer.stop();
    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && m_RemoteParserDevice && remoteParser.get())
    {
        // Disable remote parse
        dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->setEnabled(false);
    }

    int binx, biny;
    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
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
            (isEqual(m_FOVWidth, 0) || m_EffectiveFOVPending || std::fabs(pixscale - m_FOVPixelScale) > 0.005) &&
            pixscale > 0)
    {
        double newFOVW = m_CameraWidth * pixscale / binx / 60.0;
        double newFOVH = m_CameraHeight * pixscale / biny / 60.0;

        calculateEffectiveFocalLength(newFOVW);
        saveNewEffectiveFOV(newFOVW, newFOVH);

        m_EffectiveFOVPending = false;
    }

    m_AlignCoord.setRA0(ra / 15.0);  // set catalog coordinates
    m_AlignCoord.setDec0(dec);

    // Convert to JNow
    m_AlignCoord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
    // Get horizontal coords
    m_AlignCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    // Do not update diff if we are performing load & slew.
    if (!m_SolveFromFile)
    {
        pixScaleOut->setText(QString::number(pixscale, 'f', 2));
        calculateAlignTargetDiff();
    }

    // TODO 2019-11-06 JM: KStars needs to support "upside-down" displays since this is a hack.
    // Because astrometry reads image upside-down (bottom to top), the orientation is rotated 180 degrees when compared to PA
    // PA = Orientation + 180
    double solverPA = KSUtils::rotationToPositionAngle(orientation);
    solverFOV->setCenter(m_AlignCoord);
    solverFOV->setPA(solverPA);
    solverFOV->setImageDisplay(Options::astrometrySolverOverlay());
    // Sensor FOV as well
    sensorFOV->setPA(solverPA);

    PAOut->setText(QString::number(solverPA, 'f', 2)); // Two decimals are reasonable

    QString ra_dms, dec_dms;
    getFormattedCoords(m_AlignCoord.ra().Hours(), m_AlignCoord.dec().Degrees(), ra_dms, dec_dms);

    SolverRAOut->setText(ra_dms);
    SolverDecOut->setText(dec_dms);

    if (Options::astrometrySolverWCS())
    {
        auto ccdRotation = m_Camera->getNumber("CCD_ROTATION");
        if (ccdRotation)
        {
            auto rotation = ccdRotation->findWidgetByName("CCD_ROTATION_VALUE");
            if (rotation)
            {
                auto clientManager = m_Camera->getDriverInfo()->getClientManager();
                rotation->setValue(orientation);
                clientManager->sendNewProperty(ccdRotation);

                if (m_wcsSynced == false)
                {
                    appendLogText(
                        i18n("WCS information updated. Images captured from this point forward shall have valid WCS."));

                    // Just send telescope info in case the CCD driver did not pick up before.
                    auto telescopeInfo = m_Mount->getNumber("TELESCOPE_INFO");
                    if (telescopeInfo)
                        clientManager->sendNewProperty(telescopeInfo);

                    m_wcsSynced = true;
                }
            }
        }
    }

    m_CaptureErrorCounter = 0;
    m_SlewErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;

    appendLogText(
        i18n("Solution coordinates: RA (%1) DEC (%2) Telescope Coordinates: RA (%3) DEC (%4) Target Coordinates: RA (%5) DEC (%6)",
             m_AlignCoord.ra().toHMSString(),
             m_AlignCoord.dec().toDMSString(),
             m_TelescopeCoord.ra().toHMSString(),
             m_TelescopeCoord.dec().toDMSString(),
             m_TargetCoord.ra().toHMSString(),
             m_TargetCoord.dec().toDMSString()));

    if (!m_SolveFromFile && m_CurrentGotoMode == GOTO_SLEW)
    {
        dms diffDeg(m_TargetDiffTotal / 3600.0);
        appendLogText(i18n("Target is within %1 degrees of solution coordinates.", diffDeg.toDMSString()));
    }

    if (rememberUploadMode != m_Camera->getUploadMode())
        m_Camera->setUploadMode(rememberUploadMode);

    // Remember to reset fast exposure
    if (m_RememberCameraFastExposure)
    {
        m_RememberCameraFastExposure = false;
        m_Camera->setFastExposureEnabled(true);
    }

    //This block of code along with some sections in the switch below will set the status report in the solution table for this item.
    std::unique_ptr<QTableWidgetItem> statusReport(new QTableWidgetItem());
    int currentRow = solutionTable->rowCount() - 1;
    if (!m_SolveFromFile) // [Capture & Solve]
    {
        stopProgressAnimation();
        solutionTable->setCellWidget(currentRow, 3, new QWidget());
        statusReport->setFlags(Qt::ItemIsSelectable);
        // Calibration: determine camera offset from capture
        if (m_Rotator != nullptr && m_Rotator->isConnected())
        {
            if (auto absAngle = m_Rotator->getNumber("ABS_ROTATOR_ANGLE"))
                // if (absAngle && std::isnan(m_TargetPositionAngle) == true)
            {
                sRawAngle = absAngle->at(0)->getValue();
                double OffsetAngle = RotatorUtils::Instance()->calcOffsetAngle(sRawAngle, solverPA);
                RotatorUtils::Instance()->updateOffset(OffsetAngle);
                // Debug info
                auto reverseStatus = "Unknown";
                auto reverseProperty = m_Rotator->getSwitch("ROTATOR_REVERSE");
                if (reverseProperty)
                {
                    if (reverseProperty->at(0)->getState() == ISS_ON)
                        reverseStatus = "Reversed Direction";
                    else
                        reverseStatus = "Normal Direction";
                }
                qCDebug(KSTARS_EKOS_ALIGN) << "Raw Rotator Angle:" << sRawAngle << "Rotator PA:" << solverPA
                                           << "Rotator Offset:" << OffsetAngle << "Direction:" << reverseStatus;
                // Flow is: newSolverResults() -> capture: setAlignresult() -> RotatorSettings: refresh()
                emit newSolverResults(solverPA, ra, dec, pixscale);
                // appendLogText(i18n("Camera offset angle is %1 degrees.", OffsetAngle));
                appendLogText(i18n("Camera position angle is %1 degrees.", RotatorUtils::Instance()->calcCameraAngle(sRawAngle, false)));
            }
        }
    }

    QJsonObject solution =
    {
        {"camera", m_Camera->getDeviceName()},
        {"ra", SolverRAOut->text()},
        {"de", SolverDecOut->text()},
        {"dRA", m_TargetDiffRA},
        {"dDE", m_TargetDiffDE},
        {"targetDiff", m_TargetDiffTotal},
        {"pix", pixscale},
        {"PA", solverPA},
        {"fov", FOVOut->text()},
    };
    emit newSolution(solution.toVariantMap());

    setState(ALIGN_SUCCESSFUL);
    emit newStatus(state);
    solverIterations = 0;
    KSNotification::event(QLatin1String("AlignSuccessful"), i18n("Astrometry alignment completed successfully"),
                          KSNotification::Align);

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
            if (m_SolveFromFile || m_TargetDiffTotal > static_cast<double>(alignAccuracyThreshold->value()))
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

            stopProgressAnimation();
            statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
            solutionTable->setItem(currentRow, 3, statusReport.release());

            appendLogText(i18n("Target is within acceptable range."));
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

    solverFOV->setProperty("visible", true);

    if (!matchPAHStage(PAA::PAH_IDLE))
        m_PolarAlignmentAssistant->processPAHStage(orientation, ra, dec, pixscale, eastToTheRight,
                m_StellarSolver->getSolutionHealpix(),
                m_StellarSolver->getSolutionIndexNumber());
    else
    {

        if (checkIfRotationRequired())
        {
            solveB->setEnabled(false);
            loadSlewB->setEnabled(false);
            return;
        }

        // We are done!
        setState(ALIGN_COMPLETE);
        emit newStatus(state);

        solveB->setEnabled(true);
        loadSlewB->setEnabled(true);
    }
}

void Align::solverFailed()
{

    // If failed-align logging is enabled, let's save the frame.
    if (Options::saveFailedAlignImages())
    {
        QDir dir;
        QDateTime now = KStarsData::Instance()->lt();
        QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("align/failed");
        dir.mkpath(path);
        QString extraFilenameInfo;
        if (m_UsedScale)
            extraFilenameInfo.append(QString("_s%1u%2").arg(m_ScaleUsed, 0, 'f', 3)
                                     .arg(Options::astrometryImageScaleUnits()));
        if (m_UsedPosition)
            extraFilenameInfo.append(QString("_r%1_d%2").arg(m_RAUsed, 0, 'f', 5).arg(m_DECUsed, 0, 'f', 5));

        // IS8601 contains colons but they are illegal under Windows OS, so replacing them with '-'
        // The timestamp is no longer ISO8601 but it should solve interoperality issues between different OS hosts
        QString name     = "failed_align_frame_" + now.toString("yyyy-MM-dd-HH-mm-ss") + extraFilenameInfo + ".fits";
        QString filename = path + QStringLiteral("/") + name;
        if (m_ImageData)
        {
            m_ImageData->saveImage(filename);
            appendLogText(i18n("Saving failed solver image to %1", filename));
        }

    }
    if (state != ALIGN_ABORTED)
    {
        // Try to solve with scale turned off, if not turned off already
        if (Options::astrometryUseImageScale() && useBlindScale == BLIND_IDLE)
        {
            appendLogText(i18n("Solver failed. Retrying without scale constraint."));
            useBlindScale = BLIND_ENGAGNED;
            setAlignTableResult(ALIGN_RESULT_FAILED);
            captureAndSolve(false);
            return;
        }

        // Try to solve with the position turned off, if not turned off already
        if (Options::astrometryUsePosition() && useBlindPosition == BLIND_IDLE)
        {
            appendLogText(i18n("Solver failed. Retrying without position constraint."));
            useBlindPosition = BLIND_ENGAGNED;
            setAlignTableResult(ALIGN_RESULT_FAILED);
            captureAndSolve(false);
            return;
        }


        appendLogText(i18n("Solver Failed."));
        if(!Options::alignmentLogging())
            appendLogText(
                i18n("Please check you have sufficient stars in the image, the indicated FOV is correct, and the necessary index files are installed. Enable Alignment Logging in Setup Tab -> Logs to get detailed information on the failure."));

        KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed"),
                              KSNotification::Align, KSNotification::Alert);
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

    setState(ALIGN_FAILED);
    emit newStatus(state);

    solverFOV->setProperty("visible", false);

    setAlignTableResult(ALIGN_RESULT_FAILED);
}

bool Align::checkIfRotationRequired()
{
    // Check if we need to perform any rotations.
    if (Options::astrometryUseRotator())
    {
        if (m_SolveFromFile) // [Load & Slew] Program flow never lands here!?
        {
            m_TargetPositionAngle = solverFOV->PA();
            // We are not done yet.
            qCDebug(KSTARS_EKOS_ALIGN) << "Solving from file: Setting target PA to:" << m_TargetPositionAngle;
        }
        else // [Capture & Solve]: "direct" or within [Load & Slew]
        {
            currentRotatorPA = solverFOV->PA();
            if (std::isnan(m_TargetPositionAngle) == false) // [Load & Slew] only
            {
                // If image pierside versus mount pierside is different and policy is lenient ...
                if (RotatorUtils::Instance()->Instance()->checkImageFlip() && (Options::astrometryFlipRotationAllowed()))
                {
                    // ... calculate "flipped" PA ...
                    sRawAngle = RotatorUtils::Instance()->calcRotatorAngle(m_TargetPositionAngle);
                    m_TargetPositionAngle = RotatorUtils::Instance()->calcCameraAngle(sRawAngle, true);
                    RotatorUtils::Instance()->setImagePierside(ISD::Mount::PIER_UNKNOWN); // ... once!
                }
                // Match the position angle with rotator
                if  (m_Rotator != nullptr && m_Rotator->isConnected())
                {
                    if(fabs(KSUtils::rangePA(currentRotatorPA - m_TargetPositionAngle)) * 60 >
                            Options::astrometryRotatorThreshold())
                    {
                        // Signal flow: newSolverResults() -> capture: setAlignresult() -> RS: refresh()
                        emit newSolverResults(m_TargetPositionAngle, 0, 0, 0);
                        appendLogText(i18n("Setting camera position angle to %1 degrees ...", m_TargetPositionAngle));
                        setState(ALIGN_ROTATING);
                        emit newStatus(state); // Evoke 'updateProperty()' (where the same check is executed again)
                        return true;
                    }
                    else
                    {
                        appendLogText(i18n("Camera position angle is within acceptable range."));
                        // We're done! (Opposed to 'updateProperty()')
                        m_TargetPositionAngle = std::numeric_limits<double>::quiet_NaN();
                    }
                }
                //  Match the position angle manually
                else
                {
                    double current = currentRotatorPA;
                    double target = m_TargetPositionAngle;

                    double diff = KSUtils::rangePA(current - target);
                    double threshold = Options::astrometryRotatorThreshold() / 60.0;

                    appendLogText(i18n("Current PA is %1; Target PA is %2; diff: %3", current, target, diff));

                    emit manualRotatorChanged(current, target, threshold);

                    m_ManualRotator->setRotatorDiff(current, target, diff);
                    if (fabs(diff) > threshold)
                    {
                        targetAccuracyNotMet = true;
                        m_ManualRotator->show();
                        m_ManualRotator->raise();
                        setState(ALIGN_ROTATING);
                        emit newStatus(state);
                        return true;
                    }
                    else
                    {
                        m_TargetPositionAngle = std::numeric_limits<double>::quiet_NaN();
                        targetAccuracyNotMet = false;
                    }
                }
            }
        }
    }
    return false;
}

void Align::stop(Ekos::AlignState mode)
{
    m_CaptureTimer.stop();
    if (solverModeButtonGroup->checkedId() == SOLVER_LOCAL)
        m_StellarSolver->abort();
    else if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser)
        remoteParser->stopSolver();
    //parser->stopSolver();
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);
    loadSlewB->setEnabled(true);

    m_SolveFromFile = false;
    solverIterations = 0;
    m_CaptureErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;
    m_SlewErrorCounter = 0;
    m_AlignTimer.stop();

    disconnect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Align::processData);
    disconnect(m_Camera, &ISD::Camera::newExposureValue, this, &Ekos::Align::checkCameraExposureProgress);

    if (rememberUploadMode != m_Camera->getUploadMode())
        m_Camera->setUploadMode(rememberUploadMode);

    // Remember to reset fast exposure
    if (m_RememberCameraFastExposure)
    {
        m_RememberCameraFastExposure = false;
        m_Camera->setFastExposureEnabled(true);
    }

    auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

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
            if (elapsed > 0)
                appendLogText(i18n("Solver aborted after %1 seconds.", QString::number(elapsed, 'f', 2)));
        }
    }

    setState(mode);
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

void Align::updateProperty(INDI::Property prop)
{
    if (prop.isNameMatch("EQUATORIAL_EOD_COORD") || prop.isNameMatch("EQUATORIAL_COORD"))
    {
        auto nvp = prop.getNumber();
        QString ra_dms, dec_dms;

        getFormattedCoords(m_TelescopeCoord.ra().Hours(), m_TelescopeCoord.dec().Degrees(), ra_dms, dec_dms);

        ScopeRAOut->setText(ra_dms);
        ScopeDecOut->setText(dec_dms);

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
                            appendLogText(i18n("Mount is synced to solution coordinates."));

                            /* if (checkIfRotationRequired())
                                return; */

                            KSNotification::event(QLatin1String("AlignSuccessful"),
                                                  i18n("Astrometry alignment completed successfully"), KSNotification::Align);
                            setState(ALIGN_COMPLETE);
                            emit newStatus(state);
                            solverIterations = 0;
                            loadSlewB->setEnabled(true);
                        }
                    }
                    break;

                    case ALIGN_SLEWING:

                        if (!didSlewStart())
                        {
                            // If mount has not started slewing yet, then skip
                            qCDebug(KSTARS_EKOS_ALIGN) << "Mount slew planned, but not started slewing yet...";
                            break;
                        }

                        //qCDebug(KSTARS_EKOS_ALIGN) << "Mount slew completed.";
                        m_wasSlewStarted = false;
                        if (m_SolveFromFile)
                        {
                            m_SolveFromFile = false;
                            m_TargetPositionAngle = solverFOV->PA();
                            qCDebug(KSTARS_EKOS_ALIGN) << "Solving from file: Setting target PA to" << m_TargetPositionAngle;

                            setState(ALIGN_PROGRESS);
                            emit newStatus(state);

                            if (alignSettlingTime->value() >= DELAY_THRESHOLD_NOTIFY)
                                appendLogText(i18n("Settling..."));
                            m_resetCaptureTimeoutCounter = true; // Enable rotator time frame estimate in 'captureandsolve()'
                            m_CaptureTimer.start(alignSettlingTime->value());
                            return;
                        }
                        else if (m_CurrentGotoMode == GOTO_SLEW || (m_MountModel && m_MountModel->isRunning()))
                        {
                            if (targetAccuracyNotMet)
                                appendLogText(i18n("Slew complete. Target accuracy is not met, running solver again..."));
                            else
                                appendLogText(i18n("Slew complete. Solving Alignment Point. . ."));

                            targetAccuracyNotMet = false;

                            setState(ALIGN_PROGRESS);
                            emit newStatus(state);

                            if (alignSettlingTime->value() >= DELAY_THRESHOLD_NOTIFY)
                                appendLogText(i18n("Settling..."));
                            m_resetCaptureTimeoutCounter = true; // Enable rotator time frame estimate in 'captureandsolve()'
                            m_CaptureTimer.start(alignSettlingTime->value());
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

        RUN_PAH(processMountRotation(m_TelescopeCoord.ra(), alignSettlingTime->value()));
    }
    else if (prop.isNameMatch("ABS_ROTATOR_ANGLE"))
    {
        auto nvp = prop.getNumber();
        double RAngle = nvp->np[0].value;
        currentRotatorPA = RotatorUtils::Instance()->calcCameraAngle(RAngle, false);
        /*QString logtext = "Alignstate: " + QString::number(state)
                        + " IPSstate: " + QString::number(nvp->s)
                        + " Raw Rotator Angle:" + QString::number(nvp->np[0].value)
                        + " Current PA:" + QString::number(currentRotatorPA)
                        + " Target PA:" + QString::number(m_TargetPositionAngle)
                        + " Offset:" + QString::number(Options::pAOffset());
        appendLogText(logtext);*/
        // loadSlewTarget defined if activation through [Load & Slew] and rotator just reached position
        if (std::isnan(m_TargetPositionAngle) == false && state == ALIGN_ROTATING && nvp->s == IPS_OK)
        {
            auto diff = fabs(RotatorUtils::Instance()->DiffPA(currentRotatorPA - m_TargetPositionAngle)) * 60;
            qCDebug(KSTARS_EKOS_ALIGN) << "Raw Rotator Angle:" << RAngle << "Current PA:" << currentRotatorPA
                                       << "Target PA:" << m_TargetPositionAngle << "Diff (arcmin):" << diff << "Offset:"
                                       << Options::pAOffset();

            if (diff <= Options::astrometryRotatorThreshold())
            {
                appendLogText(i18n("Rotator reached camera position angle."));
                // Check angle once again (no slew -> no settle time)
                // QTimer::singleShot(alignSettlingTime->value(), this, &Ekos::Align::executeGOTO);
                RotatorGOTO = true; // Flag for SlewToTarget()
                executeGOTO();
            }
            else
            {
                // Sometimes update of "nvp->s" is a bit slow, so check state again, if it's really ok
                QTimer::singleShot(300, [ = ]
                {
                    if (nvp->s == IPS_OK)
                    {
                        appendLogText(i18n("Rotator failed to arrive at the requested position angle (Deviation %1 arcmin).", diff));
                        setState(ALIGN_FAILED);
                        emit newStatus(state);
                        solveB->setEnabled(true);
                        loadSlewB->setEnabled(true);
                    }
                });
            }
        }
        else if (m_estimateRotatorTimeFrame) // Estimate time frame during first timeout
        {
            m_RotatorTimeFrame = RotatorUtils::Instance()->calcTimeFrame(RAngle);
        }
    }
    else if (prop.isNameMatch("TELESCOPE_MOTION_NS") || prop.isNameMatch("TELESCOPE_MOTION_WE"))
    {
        switch (prop.getState())
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
        }

        setState(ALIGN_SLEWING);
    }
}

void Align::handleMountStatus()
{
    auto nvp = m_Mount->getNumber(m_Mount->isJ2000() ? "EQUATORIAL_COORD" :
                                  "EQUATORIAL_EOD_COORD");

    if (nvp)
        updateProperty(nvp);
}


void Align::executeGOTO()
{
    if (m_SolveFromFile)
    {
        // Target coords will move the scope and in case of differential align the destination get lost.
        // Thus we have to save these coords for later use (-> SlewToTarget()).
        // This does not affect normal align, where destination is not used.
        m_DestinationCoord = m_AlignCoord;
        m_TargetCoord = m_AlignCoord;

        qCDebug(KSTARS_EKOS_ALIGN) << "Solving from file. Setting Target Coordinates align coords. RA:"
                                   << m_TargetCoord.ra().toHMSString()
                                   << "DE:" << m_TargetCoord.dec().toDMSString();

        SlewToTarget();
    }
    else if (m_CurrentGotoMode == GOTO_SYNC)
        Sync();
    else if (m_CurrentGotoMode == GOTO_SLEW)
        SlewToTarget();
}

void Align::Sync()
{
    setState(ALIGN_SYNCING);

    if (m_Mount->Sync(&m_AlignCoord))
    {
        emit newStatus(state);
        appendLogText(
            i18n("Syncing to RA (%1) DEC (%2)", m_AlignCoord.ra().toHMSString(), m_AlignCoord.dec().toDMSString()));
    }
    else
    {
        setState(ALIGN_IDLE);
        emit newStatus(state);
        appendLogText(i18n("Syncing failed."));
    }
}

void Align::Slew()
{
    setState(ALIGN_SLEWING);
    emit newStatus(state);

    //qCDebug(KSTARS_EKOS_ALIGN) << "## Before SLEW command: wasSlewStarted -->" << m_wasSlewStarted;
    //m_wasSlewStarted = currentTelescope->Slew(&m_targetCoord);
    //qCDebug(KSTARS_EKOS_ALIGN) << "## After SLEW command: wasSlewStarted -->" << m_wasSlewStarted;

    // JM 2019-08-23: Do not assume that slew was started immediately. Wait until IPS_BUSY state is triggered
    // from Goto
    if (m_Mount->Slew(&m_TargetCoord))
    {
        slewStartTimer.start();
        appendLogText(i18n("Slewing to target coordinates: RA (%1) DEC (%2).", m_TargetCoord.ra().toHMSString(),
                           m_TargetCoord.dec().toDMSString()));
    }
    else // inform user about failure
    {
        appendLogText(i18n("Slewing to target coordinates: RA (%1) DEC (%2) is rejected. (see notification)",
                           m_TargetCoord.ra().toHMSString(),
                           m_TargetCoord.dec().toDMSString()));
        setState(ALIGN_FAILED);
        emit newStatus(state);
        solveB->setEnabled(true);
        loadSlewB->setEnabled(true);
    }
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

        if (Options::astrometryDifferentialSlewing())  // Remember: Target coords will move the telescope
        {
            if (!RotatorGOTO) // Only for mount movements
            {
                m_TargetCoord.setRA(m_TargetCoord.ra() - m_AlignCoord.ra().deltaAngle(m_DestinationCoord.ra()));
                m_TargetCoord.setDec(m_TargetCoord.dec() - m_AlignCoord.dec().deltaAngle(m_DestinationCoord.dec()));
                qCDebug(KSTARS_EKOS_ALIGN) << "Differential slew - Target RA:" << m_TargetCoord.ra().toHMSString()
                                           << " DE:" << m_TargetCoord.dec().toDMSString();
            }
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
                                               "Images (*.fits *.fits.fz *.fit *.fts *.xisf "
                                               "*.jpg *.jpeg *.png *.gif *.bmp "
                                               "*.cr2 *.cr3 *.crw *.nef *.raf *.dng *.arw *.orf)");

    if (fileURL.isEmpty())
        return false;

    QFileInfo fileInfo(fileURL);
    if (fileInfo.exists() == false)
        return false;

    dirPath = fileInfo.absolutePath();

    RotatorGOTO = false;

    m_SolveFromFile = true;

    if (m_PolarAlignmentAssistant)
        m_PolarAlignmentAssistant->stopPAHProcess();

    slewR->setChecked(true);
    m_CurrentGotoMode = GOTO_SLEW;

    solveB->setEnabled(false);
    loadSlewB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && m_RemoteParserDevice == nullptr)
    {
        appendLogText(i18n("No remote astrometry driver detected, switching to StellarSolver."));
        setSolverMode(SOLVER_LOCAL);
    }

    m_ImageData.clear();

    m_AlignView->loadFile(fileURL);
    //m_FileToSolve = fileURL;
    connect(m_AlignView.get(), &FITSView::loaded, this, &Align::startSolving);

    return true;
}

bool Align::loadAndSlew(const QByteArray &image, const QString &extension)
{
    RotatorGOTO = false;
    m_SolveFromFile = true;
    RUN_PAH(stopPAHProcess());
    slewR->setChecked(true);
    m_CurrentGotoMode = GOTO_SLEW;
    solveB->setEnabled(false);
    loadSlewB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    // Must clear image data so we are forced to read the
    // image data again from align view when solving begins.
    m_ImageData.clear();
    QSharedPointer<FITSData> data;
    data.reset(new FITSData(), &QObject::deleteLater);
    data->setExtension(extension);
    data->loadFromBuffer(image);
    m_AlignView->loadData(data);
    startSolving();
    return true;
}

void Align::setExposure(double value)
{
    alignExposure->setValue(value);
}

void Align::setBinningIndex(int binIndex)
{
    // If sender is not our combo box, then we need to update the combobox itself
    if (dynamic_cast<QComboBox *>(sender()) != alignBinning)
    {
        alignBinning->blockSignals(true);
        alignBinning->setCurrentIndex(binIndex);
        alignBinning->blockSignals(false);
    }

    // Need to calculate FOV and args for APP
    if (Options::astrometryImageScaleUnits() == SSolver::ARCSEC_PER_PIX)
        calculateFOV();
}

bool Align::setFilterWheel(ISD::FilterWheel * device)
{
    if (m_FilterWheel && m_FilterWheel == device)
    {
        checkFilter();
        return false;
    }

    if (m_FilterWheel)
        m_FilterWheel->disconnect(this);

    m_FilterWheel = device;

    if (m_FilterWheel)
    {
        connect(m_FilterWheel, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            FilterPosLabel->setEnabled(true);
            alignFilter->setEnabled(true);
        });
        connect(m_FilterWheel, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            FilterPosLabel->setEnabled(false);
            alignFilter->setEnabled(false);
        });
    }

    auto isConnected = m_FilterWheel && m_FilterWheel->isConnected();
    FilterPosLabel->setEnabled(isConnected);
    alignFilter->setEnabled(isConnected);

    checkFilter();
    return true;
}

QString Align::filterWheel()
{
    if (m_FilterWheel)
        return m_FilterWheel->getDeviceName();

    return QString();
}

bool Align::setFilter(const QString &filter)
{
    if (m_FilterWheel)
    {
        alignFilter->setCurrentText(filter);
        return true;
    }

    return false;
}


QString Align::filter()
{
    return alignFilter->currentText();
}

void Align::checkFilter()
{
    alignFilter->clear();

    if (!m_FilterWheel)
    {
        FilterPosLabel->setEnabled(false);
        alignFilter->setEnabled(false);
        return;
    }

    auto isConnected = m_FilterWheel->isConnected();
    FilterPosLabel->setEnabled(isConnected);
    alignFilter->setEnabled(alignUseCurrentFilter->isChecked() == false);

    setupFilterManager();

    alignFilter->addItems(m_FilterManager->getFilterLabels());
    currentFilterPosition = m_FilterManager->getFilterPosition();

    if (alignUseCurrentFilter->isChecked())
    {
        // use currently selected filter
        alignFilter->setCurrentIndex(currentFilterPosition - 1);
    }
    else
    {
        // use the fixed filter
        auto filter = m_Settings["alignFilter"];
        if (filter.isValid())
            alignFilter->setCurrentText(filter.toString());
    }
}

void Align::setRotator(ISD::Rotator * Device)
{
    if ((Manager::Instance()->existRotatorController()) && (!m_Rotator || !(Device == m_Rotator)))
    {
        rotatorB->setEnabled(false);
        if (m_Rotator)
        {
            m_Rotator->disconnect(this);
            m_RotatorControlPanel->close();
        }
        m_Rotator = Device;
        if (m_Rotator)
        {
            if (Manager::Instance()->getRotatorController(m_Rotator->getDeviceName(), m_RotatorControlPanel))
            {
                connect(m_Rotator, &ISD::Rotator::propertyUpdated, this, &Ekos::Align::updateProperty, Qt::UniqueConnection);
                connect(rotatorB, &QPushButton::clicked, this, [this]()
                {
                    m_RotatorControlPanel->show();
                    m_RotatorControlPanel->raise();
                });
                rotatorB->setEnabled(true);
            }
        }
    }
}

void Align::setWCSEnabled(bool enable)
{
    if (!m_Camera)
        return;

    auto wcsControl = m_Camera->getSwitch("WCS_CONTROL");

    if (!wcsControl)
        return;

    auto wcs_enable  = wcsControl->findWidgetByName("WCS_ENABLE");
    auto wcs_disable = wcsControl->findWidgetByName("WCS_DISABLE");

    if (!wcs_enable || !wcs_disable)
        return;

    if ((wcs_enable->getState() == ISS_ON && enable) || (wcs_disable->getState() == ISS_ON && !enable))
        return;

    wcsControl->reset();
    if (enable)
    {
        appendLogText(i18n("World Coordinate System (WCS) is enabled."));
        wcs_enable->setState(ISS_ON);
    }
    else
    {
        appendLogText(i18n("World Coordinate System (WCS) is disabled."));
        wcs_disable->setState(ISS_ON);
        m_wcsSynced    = false;
    }

    auto clientManager = m_Camera->getDriverInfo()->getClientManager();
    if (clientManager)
        clientManager->sendNewProperty(wcsControl);
}

void Align::checkCameraExposureProgress(ISD::CameraChip * targetChip, double remaining, IPState state)
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
        captureAndSolve(false);
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

void Align::setCaptureStatus(CaptureState newState)
{
    switch (newState)
    {
        case CAPTURE_ALIGNING:
            if (m_Mount && m_Mount->hasAlignmentModel() && Options::resetMountModelAfterMeridian())
            {
                qCDebug(KSTARS_EKOS_ALIGN) << "Post meridian flip mount model reset" << (m_Mount->clearAlignmentModel() ?
                                           "successful." : "failed.");
            }
            if (alignSettlingTime->value() >= DELAY_THRESHOLD_NOTIFY)
                appendLogText(i18n("Settling..."));
            m_resetCaptureTimeoutCounter = true; // Enable rotator time frame estimate in 'captureandsolve()'
            m_CaptureTimer.start(alignSettlingTime->value());
            break;
        // Is this needed anymore with new flip policy? (sb 2023-10-20)
        // On meridian flip, reset Target Position Angle to fully rotated value
        // expected after MF so that we do not end up with reversed camera rotation
        case CAPTURE_MERIDIAN_FLIP:
            if (std::isnan(m_TargetPositionAngle) == false)
                m_TargetPositionAngle = KSUtils::rangePA(m_TargetPositionAngle + 180.0);
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
            connect(fv.get(), &FITSViewer::terminated, this, [this]()
            {
                fv.clear();
            });
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

void Align::setMountStatus(ISD::Mount::Status newState)
{
    switch (newState)
    {
        case ISD::Mount::MOUNT_PARKED:
            solveB->setEnabled(false);
            loadSlewB->setEnabled(false);
            break;
        case ISD::Mount::MOUNT_IDLE:
            solveB->setEnabled(true);
            loadSlewB->setEnabled(true);
            break;
        case ISD::Mount::MOUNT_PARKING:
            solveB->setEnabled(false);
            loadSlewB->setEnabled(false);
            break;
        case ISD::Mount::MOUNT_SLEWING:
        case ISD::Mount::MOUNT_MOVING:
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

void Align::setAstrometryDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    m_RemoteParserDevice = device;

    remoteSolverR->setEnabled(true);
    if (remoteParser.get() != nullptr)
    {
        remoteParser->setAstrometryDevice(m_RemoteParserDevice);
        connect(remoteParser.get(), &AstrometryParser::solverFinished, this, &Ekos::Align::solverFinished, Qt::UniqueConnection);
        connect(remoteParser.get(), &AstrometryParser::solverFailed, this, &Ekos::Align::solverFailed, Qt::UniqueConnection);
    }
}

void Align::refreshAlignOptions()
{
    solverFOV->setImageDisplay(Options::astrometrySolverWCS());
    m_AlignTimer.setInterval(Options::astrometryTimeout() * 1000);
    if (m_Rotator)
        m_RotatorControlPanel->updateFlipPolicy(Options::astrometryFlipRotationAllowed());
}

void Align::setupOptions()
{
    KConfigDialog *dialog = new KConfigDialog(this, "alignsettings", Options::self());

#ifdef Q_OS_MACOS
    dialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    opsAlign = new OpsAlign(this);
    connect(opsAlign, &OpsAlign::settingsUpdated, this, &Ekos::Align::refreshAlignOptions);
    KPageWidgetItem *page = dialog->addPage(opsAlign, i18n("StellarSolver Options"));
    page->setIcon(QIcon(":/icons/StellarSolverIcon.png"));
    // connect(rotatorB, &QPushButton::clicked, dialog, &KConfigDialog::show);

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

    connect(opsAlign, &OpsAlign::needToLoadProfile, this, [this, dialog, page](const QString & profile)
    {
        optionsProfileEditor->loadProfile(profile);
        dialog->setCurrentPage(page);
    });

    opsAstrometryIndexFiles = new OpsAstrometryIndexFiles(this);
    connect(opsAstrometryIndexFiles, &OpsAstrometryIndexFiles::newDownloadProgress, this, &Align::newDownloadProgress);
    m_IndexFilesPage = dialog->addPage(opsAstrometryIndexFiles, i18n("Index Files"));
    m_IndexFilesPage->setIcon(QIcon::fromTheme("map-flat"));
}

void Align::setupSolutionTable()
{
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
}

void Align::setupPlot()
{
    double accuracyRadius = alignAccuracyThreshold->value();

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
}

void Align::setupFilterManager()
{
    // Do we have an existing filter manager?
    if (m_FilterManager)
        m_FilterManager->disconnect(this);

    // Create new or refresh device
    Ekos::Manager::Instance()->createFilterManager(m_FilterWheel);

    // Return global filter manager for this filter wheel.
    Ekos::Manager::Instance()->getFilterManager(m_FilterWheel->getDeviceName(), m_FilterManager);

    connect(m_FilterManager.get(), &FilterManager::ready, this, [this]()
    {
        if (filterPositionPending)
        {
            m_FocusState = FOCUS_IDLE;
            filterPositionPending = false;
            captureAndSolve(false);
        }
    });

    connect(m_FilterManager.get(), &FilterManager::failed, this, [this]()
    {
        if (filterPositionPending)
        {
            appendLogText(i18n("Filter operation failed."));
            abort();
        }
    });

    connect(m_FilterManager.get(), &FilterManager::newStatus, this, [this](Ekos::FilterState filterState)
    {
        if (filterPositionPending)
        {
            switch (filterState)
            {
                case FILTER_OFFSET:
                    appendLogText(i18n("Changing focus offset by %1 steps...", m_FilterManager->getTargetFilterOffset()));
                    break;

                case FILTER_CHANGE:
                {
                    const int filterComboIndex = m_FilterManager->getTargetFilterPosition() - 1;
                    if (filterComboIndex >= 0 && filterComboIndex < alignFilter->count())
                        appendLogText(i18n("Changing filter to %1...", alignFilter->itemText(filterComboIndex)));
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

    connect(m_FilterManager.get(), &FilterManager::labelsChanged, this, &Align::checkFilter);
    connect(m_FilterManager.get(), &FilterManager::positionChanged, this, &Align::checkFilter);
}

QVariantMap Align::getEffectiveFOV()
{
    KStarsData::Instance()->userdb()->GetAllEffectiveFOVs(effectiveFOVs);

    m_FOVWidth = m_FOVHeight = 0;

    for (auto &map : effectiveFOVs)
    {
        if (map["Profile"].toString() == m_ActiveProfile->name && map["Train"].toString() == opticalTrain())
        {
            if (isEqual(map["Width"].toInt(), m_CameraWidth) &&
                    isEqual(map["Height"].toInt(), m_CameraHeight) &&
                    isEqual(map["PixelW"].toDouble(), m_CameraPixelWidth) &&
                    isEqual(map["PixelH"].toDouble(), m_CameraPixelHeight) &&
                    isEqual(map["FocalLength"].toDouble(), m_FocalLength) &&
                    isEqual(map["FocalRedcuer"].toDouble(), m_Reducer) &&
                    isEqual(map["FocalRatio"].toDouble(), m_FocalRatio))
            {
                m_FOVWidth = map["FovW"].toDouble();
                m_FOVHeight = map["FovH"].toDouble();
                return map;
            }
        }
    }

    return QVariantMap();
}

void Align::saveNewEffectiveFOV(double newFOVW, double newFOVH)
{
    if (newFOVW < 0 || newFOVH < 0 || (isEqual(newFOVW, m_FOVWidth) && isEqual(newFOVH, m_FOVHeight)))
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
    effectiveMap["Train"] = opticalTrainCombo->currentText();
    effectiveMap["Width"] = m_CameraWidth;
    effectiveMap["Height"] = m_CameraHeight;
    effectiveMap["PixelW"] = m_CameraPixelWidth;
    effectiveMap["PixelH"] = m_CameraPixelHeight;
    effectiveMap["FocalLength"] = m_FocalLength;
    effectiveMap["FocalReducer"] = m_Reducer;
    effectiveMap["FocalRatio"] = m_FocalRatio;
    effectiveMap["FovW"] = newFOVW;
    effectiveMap["FovH"] = newFOVH;

    KStarsData::Instance()->userdb()->AddEffectiveFOV(effectiveMap);

    calculateFOV();

}

void Align::zoomAlignView()
{
    m_AlignView->ZoomDefault();

    // Frame update is not immediate to reduce too many refreshes
    // So emit updated frame in 500ms
    QTimer::singleShot(500, this, [this]()
    {
        emit newFrame(m_AlignView);
    });
}

void Align::setAlignZoom(double scale)
{
    if (scale > 1)
        m_AlignView->ZoomIn();
    else if (scale < 1)
        m_AlignView->ZoomOut();

    // Frame update is not immediate to reduce too many refreshes
    // So emit updated frame in 500ms
    QTimer::singleShot(500, this, [this]()
    {
        emit newFrame(m_AlignView);
    });
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

void Align::setTargetCoords(double ra0, double de0)
{
    SkyPoint target;
    target.setRA0(ra0);
    target.setDec0(de0);
    target.updateCoordsNow(KStarsData::Instance()->updateNum());
    setTarget(target);
}

void Align::setTarget(const SkyPoint &targetCoord)
{
    m_TargetCoord = targetCoord;
    qCInfo(KSTARS_EKOS_ALIGN) << "Target coordinates updated to JNow RA:" << m_TargetCoord.ra().toHMSString()
                              << "DE:" << m_TargetCoord.dec().toDMSString();
}

QList<double> Align::getTargetCoords()
{
    return QList<double>() << m_TargetCoord.ra0().Hours() << m_TargetCoord.dec0().Degrees();
}

void Align::setTargetPositionAngle(double value)
{
    m_TargetPositionAngle =  value;
    qCDebug(KSTARS_EKOS_ALIGN) << "Target PA updated to: " << m_TargetPositionAngle;
}

void Align::calculateAlignTargetDiff()
{
    if (matchPAHStage(PAA::PAH_FIRST_CAPTURE) ||
            matchPAHStage(PAA::PAH_SECOND_CAPTURE) ||
            matchPAHStage(PAA::PAH_THIRD_CAPTURE) ||
            matchPAHStage(PAA::PAH_FIRST_SOLVE) ||
            matchPAHStage(PAA::PAH_SECOND_SOLVE) ||
            matchPAHStage(PAA::PAH_THIRD_SOLVE) ||
            nothingR->isChecked() ||
            syncR->isChecked())
        return;

    if (!Options::astrometryDifferentialSlewing()) // Normal align: Target coords are destinations coords
    {
        m_TargetDiffRA = (m_AlignCoord.ra().deltaAngle(m_TargetCoord.ra())).Degrees() * 3600;  // arcsec
        m_TargetDiffDE = (m_AlignCoord.dec().deltaAngle(m_TargetCoord.dec())).Degrees() * 3600;  // arcsec
    }
    else // Differential slewing: Target coords are new position coords
    {
        m_TargetDiffRA = (m_AlignCoord.ra().deltaAngle(m_DestinationCoord.ra())).Degrees() * 3600;  // arcsec
        m_TargetDiffDE = (m_AlignCoord.dec().deltaAngle(m_DestinationCoord.dec())).Degrees() * 3600;  // arcsec
        qCDebug(KSTARS_EKOS_ALIGN) << "Differential slew - Solution RA:" << m_AlignCoord.ra().toHMSString()
                                   << " DE:" << m_AlignCoord.dec().toDMSString();
        qCDebug(KSTARS_EKOS_ALIGN) << "Differential slew - Destination RA:" << m_DestinationCoord.ra().toHMSString()
                                   << " DE:" << m_DestinationCoord.dec().toDMSString();
    }

    m_TargetDiffTotal = sqrt(m_TargetDiffRA * m_TargetDiffRA + m_TargetDiffDE * m_TargetDiffDE);

    errOut->setText(QString("%1 arcsec. RA:%2 DE:%3").arg(
                        QString::number(m_TargetDiffTotal, 'f', 0),
                        QString::number(m_TargetDiffRA, 'f', 0),
                        QString::number(m_TargetDiffDE, 'f', 0)));
    if (m_TargetDiffTotal <= static_cast<double>(alignAccuracyThreshold->value()))
        errOut->setStyleSheet("color:green");
    else if (m_TargetDiffTotal < 1.5 * alignAccuracyThreshold->value())
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

QStringList Align::getStellarSolverProfiles()
{
    QStringList profiles;
    for (auto &param : m_StellarSolverProfiles)
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
              << "),RA (degrees),DE (degrees),Name,RA Error (arcsec),DE Error (arcsec)" << Qt::endl;

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
                  << deErrorCell->text().remove('\"') << Qt::endl;
    }
    emit newLog(i18n("Solution Points Saved as: %1", path));
    file.close();
}

void Align::setupPolarAlignmentAssistant()
{
    // Create PAA instance
    m_PolarAlignmentAssistant = new PolarAlignmentAssistant(this, m_AlignView);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::captureAndSolve, this, [this]()
    {
        captureAndSolve(true);
    });
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newAlignTableResult, this, &Ekos::Align::setAlignTableResult);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newFrame, this, &Ekos::Align::newFrame);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newPAHStage, this, &Ekos::Align::processPAHStage);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newLog, this, &Ekos::Align::appendLogText);

    tabWidget->addTab(m_PolarAlignmentAssistant, i18n("Polar Alignment"));
}

void Align::setupManualRotator()
{
    if (m_ManualRotator)
        return;

    m_ManualRotator = new ManualRotator(this);
    connect(m_ManualRotator, &Ekos::ManualRotator::captureAndSolve, this, [this]()
    {
        captureAndSolve(false);
    });
    // If user cancel manual rotator, reset load slew target PA, otherwise it will keep popping up
    // for any subsequent solves.
    connect(m_ManualRotator, &Ekos::ManualRotator::rejected, this, [this]()
    {
        m_TargetPositionAngle = std::numeric_limits<double>::quiet_NaN();
        // If in progress stop it
        if (state > ALIGN_COMPLETE)
            stop(ALIGN_IDLE);
    });
}

void Align::setuptDarkProcessor()
{
    if (m_DarkProcessor)
        return;

    m_DarkProcessor = new DarkProcessor(this);
    connect(m_DarkProcessor, &DarkProcessor::newLog, this, &Ekos::Align::appendLogText);
    connect(m_DarkProcessor, &DarkProcessor::darkFrameCompleted, this, [this](bool completed)
    {
        alignDarkFrame->setChecked(completed);
        m_AlignView->setProperty("suspended", false);
        if (completed)
        {
            m_AlignView->rescale(ZOOM_KEEP_LEVEL);
            m_AlignView->updateFrame();
        }
        setCaptureComplete();
    });
}

void Align::processPAHStage(int stage)
{
    switch (stage)
    {
        case PAA::PAH_IDLE:
            // Abort any solver that might be running.
            // Assumes this state change won't happen randomly (e.g. in the middle of align).
            // Alternatively could just let the stellarsolver finish naturally.
            if (m_StellarSolver && m_StellarSolver->isRunning())
                m_StellarSolver->abort();
            break;
        case PAA::PAH_POST_REFRESH:
        {
            Options::setAstrometrySolverWCS(rememberSolverWCS);
            Options::setAutoWCS(rememberAutoWCS);
            stop(ALIGN_IDLE);
        }
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
            if (alignSettlingTime->value() >= DELAY_THRESHOLD_NOTIFY)
                emit newLog(i18n("Settling..."));
            m_CaptureTimer.start(alignSettlingTime->value());
            break;

        default:
            break;
    }

    emit newPAAStage(stage);
}

bool Align::matchPAHStage(uint32_t stage)
{
    return m_PolarAlignmentAssistant && m_PolarAlignmentAssistant->getPAHStage() == stage;
}

void Align::toggleManualRotator(bool toggled)
{
    if (toggled)
    {
        m_ManualRotator->show();
        m_ManualRotator->raise();
    }
    else
        m_ManualRotator->close();
}

void Align::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &Align::refreshOpticalTrain);
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        ProfileSettings::Instance()->setOneSetting(ProfileSettings::AlignOpticalTrain,
                OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index)));
        refreshOpticalTrain();
        emit trainChanged();
    });
}

void Align::refreshOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(true);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::AlignOpticalTrain);

    if (trainID.isValid())
    {
        auto id = trainID.toUInt();

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            qCWarning(KSTARS_EKOS_ALIGN) << "Optical train doesn't exist for id" << id;
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
        }

        auto name = OpticalTrainManager::Instance()->name(id);

        opticalTrainCombo->setCurrentText(name);

        auto scope = OpticalTrainManager::Instance()->getScope(name);
        m_FocalLength = scope["focal_length"].toDouble(-1);
        m_Aperture = scope["aperture"].toDouble(-1);
        m_FocalRatio = scope["focal_ratio"].toDouble(-1);
        m_Reducer = OpticalTrainManager::Instance()->getReducer(name);

        // DSLR Lens Aperture
        if (m_Aperture < 0 && m_FocalRatio > 0)
            m_Aperture = m_FocalLength / m_FocalRatio;

        auto mount = OpticalTrainManager::Instance()->getMount(name);
        setMount(mount);

        auto camera = OpticalTrainManager::Instance()->getCamera(name);
        if (camera)
        {
            camera->setScopeInfo(m_FocalLength * m_Reducer, m_Aperture);
            opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(camera->getDeviceName(), scope["name"].toString()));
        }
        setCamera(camera);

        syncTelescopeInfo();

        auto filterWheel = OpticalTrainManager::Instance()->getFilterWheel(name);
        setFilterWheel(filterWheel);

        auto rotator = OpticalTrainManager::Instance()->getRotator(name);
        setRotator(rotator);

        // Load train settings
        OpticalTrainSettings::Instance()->setOpticalTrainID(id);
        auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Align);
        if (settings.isValid())
        {
            auto map = settings.toJsonObject().toVariantMap();
            if (map != m_Settings)
            {
                m_Settings.clear();
                setAllSettings(map);
            }
        }
        else
            m_Settings = m_GlobalSettings;

        // Need to save information used for Mosaic planner
        Options::setTelescopeFocalLength(m_FocalLength);
        Options::setCameraPixelWidth(m_CameraPixelWidth);
        Options::setCameraPixelHeight(m_CameraPixelHeight);
        Options::setCameraWidth(m_CameraWidth);
        Options::setCameraHeight(m_CameraHeight);
    }

    opticalTrainCombo->blockSignals(false);
}

void Align::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QComboBox *cbox = nullptr;
    QRadioButton *cradio = nullptr;

    QString key;
    QVariant value;

    if ( (dsb = qobject_cast<QDoubleSpinBox * >(sender())))
    {
        key = dsb->objectName();
        value = dsb->value();

    }
    else if ( (sb = qobject_cast<QSpinBox * >(sender())))
    {
        key = sb->objectName();
        value = sb->value();
    }
    else if ( (cb = qobject_cast<QCheckBox * >(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (cbox = qobject_cast<QComboBox * >(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (cradio = qobject_cast<QRadioButton * >(sender())))
    {
        key = cradio->objectName();
        // Discard false requests
        if (cradio->isChecked() == false)
        {
            m_Settings.remove(key);
            return;
        }
        value = true;
    }

    // Save immediately
    Options::self()->setProperty(key.toLatin1(), value);

    m_Settings[key] = value;
    m_GlobalSettings[key] = value;
    m_DebounceTimer.start();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Align::settleSettings()
{
    emit settingsUpdated(getAllSettings());
    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Align, m_Settings);
}

void Align::loadGlobalSettings()
{
    QString key;
    QVariant value;

    QVariantMap settings;
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
    {
        if (oneWidget->objectName() == "opticalTrainCombo")
            continue;

        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid() && oneWidget->count() > 0)
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
    }

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toDouble());
            settings[key] = value;
        }
    }

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toInt());
            settings[key] = value;
        }
    }

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
    }

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
    }

    m_GlobalSettings = m_Settings = settings;
}


void Align::connectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Align::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
        connect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Align::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
        connect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Align::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::Align::syncSettings);

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
        connect(oneWidget, &QRadioButton::toggled, this, &Ekos::Align::syncSettings);

    // Train combo box should NOT be synced.
    disconnect(opticalTrainCombo, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Align::syncSettings);
}

void Align::disconnectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Align::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
        disconnect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Align::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
        disconnect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Align::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::Align::syncSettings);

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
        disconnect(oneWidget, &QRadioButton::toggled, this, &Ekos::Align::syncSettings);

}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
QVariantMap Align::getAllSettings() const
{
    QVariantMap settings;

    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
        settings.insert(oneWidget->objectName(), oneWidget->currentText());

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    return settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Align::setAllSettings(const QVariantMap &settings)
{
    // Disconnect settings that we don't end up calling syncSettings while
    // performing the changes.
    disconnectSettings();

    for (auto &name : settings.keys())
    {
        // Combo
        auto comboBox = findChild<QComboBox*>(name);
        if (comboBox)
        {
            syncControl(settings, name, comboBox);
            continue;
        }

        // Double spinbox
        auto doubleSpinBox = findChild<QDoubleSpinBox*>(name);
        if (doubleSpinBox)
        {
            syncControl(settings, name, doubleSpinBox);
            continue;
        }

        // spinbox
        auto spinBox = findChild<QSpinBox*>(name);
        if (spinBox)
        {
            syncControl(settings, name, spinBox);
            continue;
        }

        // checkbox
        auto checkbox = findChild<QCheckBox*>(name);
        if (checkbox)
        {
            syncControl(settings, name, checkbox);
            continue;
        }

        // Radio button
        auto radioButton = findChild<QRadioButton*>(name);
        if (radioButton)
        {
            syncControl(settings, name, radioButton);
            continue;
        }
    }

    // Sync to options
    for (auto &key : settings.keys())
    {
        auto value = settings[key];
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);
        Options::self()->save();

        m_Settings[key] = value;
        m_GlobalSettings[key] = value;
    }

    emit settingsUpdated(getAllSettings());

    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Align, m_Settings);

    // Restablish connections
    connectSettings();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool Align::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
    QRadioButton *pRadioButton = nullptr;
    bool ok = false;

    if ((pSB = qobject_cast<QSpinBox *>(widget)))
    {
        const int value = settings[key].toInt(&ok);
        if (ok)
        {
            pSB->setValue(value);
            return true;
        }
    }
    else if ((pDSB = qobject_cast<QDoubleSpinBox *>(widget)))
    {
        const double value = settings[key].toDouble(&ok);
        if (ok)
        {
            pDSB->setValue(value);
            return true;
        }
    }
    else if ((pCB = qobject_cast<QCheckBox *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value != pCB->isChecked())
            pCB->setChecked(value);
        return true;
    }
    // ONLY FOR STRINGS, not INDEX
    else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
    {
        const QString value = settings[key].toString();
        pComboBox->setCurrentText(value);
        return true;
    }
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
            pRadioButton->setChecked(true);
        return true;
    }

    return false;
}

void Align::setState(AlignState value)
{
    qCDebug(KSTARS_EKOS_ALIGN) << "Align state changed to" << getAlignStatusString(value);
    state = value;
};

void Align::processCaptureTimeout()
{
    if (m_CaptureTimeoutCounter++ > 3)
    {
        appendLogText(i18n("Capture timed out."));
        m_CaptureTimer.stop();
        abort();
    }
    else
    {
        ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        if (targetChip->isCapturing())
        {
            appendLogText(i18n("Capturing still running, Retrying in %1 seconds...", m_CaptureTimer.interval() / 500));
            targetChip->abortExposure();
            m_CaptureTimer.start( m_CaptureTimer.interval() * 2);
        }
        else
        {
            setAlignTableResult(ALIGN_RESULT_FAILED);
            if (m_resetCaptureTimeoutCounter)
            {
                m_resetCaptureTimeoutCounter = false;
                m_CaptureTimeoutCounter = 0;
            }
            captureAndSolve(false);
        }
    }
}
}

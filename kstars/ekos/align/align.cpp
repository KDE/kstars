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
#include "pushtoassistant.h"

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
#include "qtcompat.h"
#include <algorithm>

//Qt Includes
#include <QToolTip>
#include <QFileDialog>

// Qt version calming
#include <qtendl.h>

#include "align_p.h"

namespace Ekos
{

using PAA = PolarAlignmentAssistant;

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
    m_RemoteAlignTimer.setSingleShot(true);
    m_RemoteAlignTimer.setInterval(Options::astrometryTimeout() * 1000);
    connect(&m_RemoteAlignTimer, &QTimer::timeout, this, &Ekos::Align::checkRemoteAlignmentTimeout);

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

    // Initialize dynamic threshold after profiles are loaded
    resetDynamicThreshold();

    // Connect to options profile changes
    connect(Options::self(), &Options::SolveOptionsProfileChanged, this,
            &Ekos::Align::resetDynamicThreshold);


    setupPolarAlignmentAssistant();
    setupPushToAssistant();
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

void Align::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_ALIGN) << text;

    Q_EMIT newLog(text);
}

void Align::clearLog()
{
    m_LogText.clear();
    Q_EMIT newLog(QString());
}

QStringList Align::getStellarSolverProfiles()
{
    QStringList profiles;
    for (auto &param : m_StellarSolverProfiles)
        profiles << param.listName;

    return profiles;
}

void Align::setState(AlignState value)
{
    qCDebug(KSTARS_EKOS_ALIGN) << "Align state changed to" << getAlignStatusString(value);
    state = value;
};

} // namespace Ekos

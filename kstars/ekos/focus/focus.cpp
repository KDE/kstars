/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "adaptivefocus.h"
#include "focusadvisor.h"
#include "focusadaptor.h"
#include "focusalgorithms.h"
#include "focusfwhm.h"
#if defined(HAVE_OPENCV)
#include "focusblurriness.h"
#endif
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "stellarsolver.h"
#undef Const

// Modules
#include "ekos/guide/guide.h"
#include "ekos/manager.h"

// KStars Auxiliary
#include "auxiliary/kspaths.h"
#include "auxiliary/ksmessagebox.h"

// Ekos Auxiliary
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/darkprocessor.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/auxiliary/filtermanager.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"

// FITS
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsviewer.h"

// Devices
#include "indi/indifilterwheel.h"
#include "ksnotification.h"
#include "kconfigdialog.h"

#include <basedevice.h>
#include <gsl/gsl_fit.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_min.h>

#include <ekos_focus_debug.h>
#include <config-kstars.h>

#include <cmath>

#ifdef HAVE_DATAVISUALIZATION
#include "aberrationinspector.h"
#include "aberrationinspectorutils.h"
#endif

#define MAXIMUM_ABS_ITERATIONS     51
#define MAXIMUM_RESET_ITERATIONS   3
#define MAXIMUM_RESTART_ITERATIONS 3
#define AUTO_STAR_TIMEOUT          45000
#define MINIMUM_PULSE_TIMER        32
#define MAX_RECAPTURE_RETRIES      3
#define MINIMUM_POLY_SOLUTIONS     2
#define MAX_STAR_DETECT_FAILS      5

namespace Ekos
{
Focus::Focus(int id) : QWidget()
{
    // #1 Set the UI
    setupUi(this);
    m_focuserId = id;
    // create a dedicated dialog name for each focuser
    m_opsDialogName = QString("focussettings %1").arg(id);

    // #1a Prepare UI
    prepareGUI();

    // #3 Init connections
    initConnections();

    // #4 Init Plots
    initPlots();

    // #5 Init View
    initView();

    // #6 Init helper objects
    initHelperObjects();

    // #7 Reset all buttons to default states
    resetButtons();

    // #8 Load All settings
    loadGlobalSettings();

    // #9 Init Setting Connection now
    connectSyncSettings();

    connect(focusAdvisor.get(), &FocusAdvisor::newStage, this, &Focus::newFocusAdvisorStage);
    connect(focusAdvisor.get(), &FocusAdvisor::newMessage, this, &Focus::newFocusAdvisorMessage);

    connect(&m_StarFinderWatcher, &QFutureWatcher<bool>::finished, this, &Focus::starDetectionFinished);

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    appendLogText(i18n("Idle."));

    // Focus motion timeout
    m_FocusMotionTimer.setInterval(m_OpsFocusMechanics->focusMotionTimeout->value() * 1000);
    m_FocusMotionTimer.setSingleShot(true);
    connect(&m_FocusMotionTimer, &QTimer::timeout, this, &Focus::handleFocusMotionTimeout);

    m_OpsFocusProcess->editFocusProfile->setIcon(QIcon::fromTheme("document-edit"));
    m_OpsFocusProcess->editFocusProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(m_OpsFocusProcess->editFocusProfile, &QAbstractButton::clicked, this, [this]()
    {
        KConfigDialog *optionsEditor = new KConfigDialog(this, "OptionsProfileEditor", Options::self());
        optionsProfileEditor = new StellarSolverProfileEditor(this, Ekos::FocusProfiles, optionsEditor);
#ifdef Q_OS_MACOS
        optionsEditor->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
        KPageWidgetItem *mainPage = optionsEditor->addPage(optionsProfileEditor, i18n("Focus Options Profile Editor"));
        mainPage->setIcon(QIcon::fromTheme("configure"));
        connect(optionsProfileEditor, &StellarSolverProfileEditor::optionsProfilesUpdated, this, &Focus::loadStellarSolverProfiles);
        optionsProfileEditor->loadProfile(m_OpsFocusProcess->focusSEPProfile->currentText());
        optionsEditor->show();
    });

    loadStellarSolverProfiles();

    // connect HFR plot widget
    connect(this, &Ekos::Focus::initHFRPlot, HFRPlot, &FocusHFRVPlot::init);
    connect(this, &Ekos::Focus::redrawHFRPlot, HFRPlot, &FocusHFRVPlot::redraw);
    connect(this, &Ekos::Focus::newHFRPlotPosition, HFRPlot, &FocusHFRVPlot::addPosition);
    connect(this, &Ekos::Focus::drawPolynomial, HFRPlot, &FocusHFRVPlot::drawPolynomial);
    // connect signal/slot for the curve plotting to the V-Curve widget
    connect(this, &Ekos::Focus::drawCurve, HFRPlot, &FocusHFRVPlot::drawCurve);
    connect(this, &Ekos::Focus::setTitle, HFRPlot, &FocusHFRVPlot::setTitle);
    connect(this, &Ekos::Focus::finalUpdates, HFRPlot, &FocusHFRVPlot::finalUpdates);
    connect(this, &Ekos::Focus::minimumFound, HFRPlot, &FocusHFRVPlot::drawMinimum);
    connect(this, &Ekos::Focus::drawCFZ, HFRPlot, &FocusHFRVPlot::drawCFZ);

    m_DarkProcessor = new DarkProcessor(this);
    connect(m_DarkProcessor, &DarkProcessor::newLog, this, &Ekos::Focus::appendLogText);
    connect(m_DarkProcessor, &DarkProcessor::darkFrameCompleted, this, [this](bool completed)
    {
        m_OpsFocusSettings->useFocusDarkFrame->setChecked(completed);
        m_FocusView->setProperty("suspended", false);
        if (completed)
        {
            m_FocusView->rescale(ZOOM_KEEP_LEVEL);
            m_FocusView->updateFrame();
        }
        setCaptureComplete();
        resetButtons();
    });

    // connect navigation
    connect(m_FocusView->m_focusHistoryNavigation->historyFirstB, &QPushButton::clicked, this, &Focus::showFirstFrame);
    connect(m_FocusView->m_focusHistoryNavigation->historyLastB, &QPushButton::clicked, this, &Focus::showLastFrame);
    connect(m_FocusView->m_focusHistoryNavigation->historyBackwardB, &QPushButton::clicked, this, &Focus::showPreviousFrame);
    connect(m_FocusView->m_focusHistoryNavigation->historyForwardB, &QPushButton::clicked, this, &Focus::showNextFrame);
    connect(m_FocusView->m_focusHistoryNavigation->historyPreviousRunB, &QPushButton::clicked, this, &Focus::showPreviousAFRun);
    connect(m_FocusView->m_focusHistoryNavigation->historyNextRunB, &QPushButton::clicked, this, &Focus::showNextAFRun);

    setupOpticalTrainManager();
    initOpticalTrain();

    // Needs to be done once
    connectFilterManager();
}

// Do once only preparation of GUI
void Focus::prepareGUI()
{

    // Parameters are handled by a dedicated KConfigDialog per focuser invoked by pressing the "Options..." button
    // on the Focus window. There are 3 pages of options.
    // Parameters are persisted per Optical Train, so when the user changes OT, the last persisted
    // parameters for the new OT are loaded. In addition the "current" parameter values are also
    // persisted locally using kstars.kcfg
    // KConfigDialog has the ability to persist parameters to kstars.kcfg but this functionality
    // is not used in Focus
    KConfigDialog *dialog = new KConfigDialog(this, opsDialogName(), Options::self());
    m_OpsFocusSettings = new OpsFocusSettings(opsDialogName());
#ifdef Q_OS_MACOS
    dialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    KPageWidgetItem *page = dialog->addPage(m_OpsFocusSettings, i18n("Settings"), nullptr, i18n("Focus Settings"), false);
    page->setIcon(QIcon::fromTheme("configure"));

    m_OpsFocusProcess = new OpsFocusProcess(opsDialogName());
    page = dialog->addPage(m_OpsFocusProcess, i18n("Process"), nullptr, i18n("Focus Process"), false);
    page->setIcon(QIcon::fromTheme("transform-move"));

    m_OpsFocusMechanics = new OpsFocusMechanics(opsDialogName());
    page = dialog->addPage(m_OpsFocusMechanics, i18n("Mechanics"), nullptr, i18n("Focus Mechanics"), false);
    page->setIcon(QIcon::fromTheme("tool-measure"));

    // The CFZ is a tool so has its own dialog box.
    m_CFZDialog = new QDialog(this);
    m_CFZUI.reset(new Ui::focusCFZDialog());
    m_CFZUI->setupUi(m_CFZDialog);
#ifdef Q_OS_MACOS
    m_CFZDialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    // Remove all widgets from the temporary bucket. These will then be loaded as required
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusMultiRowAverageLabel);
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusMultiRowAverage);
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusGaussianSigmaLabel);
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusGaussianSigma);
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusThresholdLabel);
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusThreshold);
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusGaussianKernelSizeLabel);
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusGaussianKernelSize);
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusToleranceLabel);
    m_OpsFocusProcess->gridLayoutProcessBucket->removeWidget(m_OpsFocusProcess->focusTolerance);
    delete m_OpsFocusProcess->gridLayoutProcessBucket;

    // Setup the Walk fields. OutSteps and NumSteps are either/or widgets so co-locate them
    m_OpsFocusMechanics->gridLayoutMechanics->replaceWidget(m_OpsFocusMechanics->focusOutStepsLabel,
            m_OpsFocusMechanics->focusNumStepsLabel);
    m_OpsFocusMechanics->gridLayoutMechanics->replaceWidget(m_OpsFocusMechanics->focusOutSteps,
            m_OpsFocusMechanics->focusNumSteps);

    // Some combo-boxes have changeable values depending on other settings so store the full list of options from the .ui
    // This helps keep some synchronisation with the .ui
    for (int i = 0; i < m_OpsFocusProcess->focusStarMeasure->count(); i++)
        m_StarMeasureText.append(m_OpsFocusProcess->focusStarMeasure->itemText(i));
    for (int i = 0; i < m_OpsFocusProcess->focusCurveFit->count(); i++)
        m_CurveFitText.append(m_OpsFocusProcess->focusCurveFit->itemText(i));
    for (int i = 0; i < m_OpsFocusMechanics->focusWalk->count(); i++)
        m_FocusWalkText.append(m_OpsFocusMechanics->focusWalk->itemText(i));
}

void Focus::loadStellarSolverProfiles()
{
    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(
                                            QStandardPaths::AppLocalDataLocation)).filePath("SavedFocusProfiles.ini");
    if(QFileInfo::exists(savedOptionsProfiles))
    {
        m_StellarSolverProfiles = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
        addMissingStellarSolverProfile(savedOptionsProfiles, FOCUS_DEFAULT_NAME);
        addMissingStellarSolverProfile(savedOptionsProfiles, FOCUS_DEFAULT_DONUT_NAME);
    }
    else
        m_StellarSolverProfiles = getDefaultFocusOptionsProfiles();
    m_OpsFocusProcess->focusSEPProfile->clear();
    for(auto &param : m_StellarSolverProfiles)
        m_OpsFocusProcess->focusSEPProfile->addItem(param.listName);
    auto profile = m_Settings["focusSEPProfile"];
    if (profile.isValid())
        m_OpsFocusProcess->focusSEPProfile->setCurrentText(profile.toString());
}

void Focus::addMissingStellarSolverProfile(const QString profilePath, const QString profile)
{
    for(auto params : m_StellarSolverProfiles)
    {
        if (params.listName == profile)
            // Profile already exists so nothing more to do
            return;
    }

    QSettings settings(profilePath, QSettings::IniFormat);
    SSolver::Parameters params;
    if (profile == FOCUS_DEFAULT_DONUT_NAME)
        params = getFocusOptionsProfileDefaultDonut();
    else if (profile == FOCUS_DEFAULT_NAME)
        params = getFocusOptionsProfileDefault();
    else
        return;

    settings.beginGroup(params.listName);
    QMap<QString, QVariant> map = SSolver::Parameters::convertToMap(params);
    QMapIterator<QString, QVariant> it(map);
    while(it.hasNext())
    {
        it.next();
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();

    m_StellarSolverProfiles.append(params);
}

QStringList Focus::getStellarSolverProfiles()
{
    QStringList profiles;
    for (auto param : m_StellarSolverProfiles)
        profiles << param.listName;

    return profiles;
}


Focus::~Focus()
{
    if (focusingWidget->parent() == nullptr)
        toggleFocusingWidgetFullScreen();
}

void Focus::resetFrame()
{
    if (m_Camera && m_Camera->isConnected())
    {
        ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);

        if (targetChip)
        {
            //fx=fy=fw=fh=0;
            targetChip->resetFrame();

            int x, y, w, h;
            targetChip->getFrame(&x, &y, &w, &h);

            qCDebug(KSTARS_EKOS_FOCUS) << "Frame is reset. X:" << x << "Y:" << y << "W:" << w << "H:" << h << "binX:" << 1 << "binY:" <<
                                       1;

            QVariantMap settings;
            settings["x"]             = x;
            settings["y"]             = y;
            settings["w"]             = w;
            settings["h"]             = h;
            settings["binx"]          = 1;
            settings["biny"]          = 1;
            frameSettings[targetChip] = settings;

            starSelected = false;
            starCenter   = QVector3D();
            subFramed    = false;

            m_FocusView->setTrackingBox(QRect());
            checkMosaicMaskLimits();
        }
    }
}

QString Focus::camera()
{
    if (m_Camera)
        return m_Camera->getDeviceName();

    return QString();
}

void Focus::checkCamera()
{
    if (!m_Camera)
        return;

    // Do NOT perform checks when the camera is capturing or busy as this may result
    // in signals/slots getting disconnected.
    switch (state())
    {
        // Idle, can change camera.
        case FOCUS_IDLE:
        case FOCUS_COMPLETE:
        case FOCUS_FAILED:
        case FOCUS_ABORTED:
            break;

        // Busy, cannot change camera.
        case FOCUS_WAITING:
        case FOCUS_PROGRESS:
        case FOCUS_FRAMING:
        case FOCUS_CHANGING_FILTER:
            return;
    }


    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    if (targetChip && targetChip->isCapturing())
        return;

    if (targetChip)
    {
        focusBinning->setEnabled(targetChip->canBin());
        m_OpsFocusSettings->focusSubFrame->setEnabled(targetChip->canSubframe());
        if (targetChip->canBin())
        {
            int subBinX = 1, subBinY = 1;
            focusBinning->clear();
            targetChip->getMaxBin(&subBinX, &subBinY);
            for (int i = 1; i <= subBinX; i++)
                focusBinning->addItem(QString("%1x%2").arg(i).arg(i));

            auto binning = m_Settings["focusBinning"];
            if (binning.isValid())
                focusBinning->setCurrentText(binning.toString());
        }

        connect(m_Camera, &ISD::Camera::videoStreamToggled, this, &Ekos::Focus::setVideoStreamEnabled, Qt::UniqueConnection);
        liveVideoB->setEnabled(m_Camera->hasVideoStream());
        if (m_Camera->hasVideoStream())
            setVideoStreamEnabled(m_Camera->isStreamingEnabled());
        else
            liveVideoB->setIcon(QIcon::fromTheme("camera-off"));

    }

    syncCCDControls();
    syncCameraInfo();
}

void Focus::syncCCDControls()
{
    if (m_Camera == nullptr)
        return;

    auto targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    if (targetChip == nullptr || (targetChip && targetChip->isCapturing()))
        return;

    auto isoList = targetChip->getISOList();
    focusISO->clear();

    if (isoList.isEmpty())
    {
        focusISO->setEnabled(false);
        ISOLabel->setEnabled(false);
    }
    else
    {
        focusISO->setEnabled(true);
        ISOLabel->setEnabled(true);
        focusISO->addItems(isoList);
        focusISO->setCurrentIndex(targetChip->getISOIndex());
    }

    bool hasGain = m_Camera->hasGain();
    gainLabel->setEnabled(hasGain);
    focusGain->setEnabled(hasGain && m_Camera->getGainPermission() != IP_RO);
    if (hasGain)
    {
        double gain = 0, min = 0, max = 0, step = 1;
        m_Camera->getGainMinMaxStep(&min, &max, &step);
        if (m_Camera->getGain(&gain))
        {
            // Allow the possibility of no gain value at all.
            GainSpinSpecialValue = min - step;
            focusGain->setRange(GainSpinSpecialValue, max);
            focusGain->setSpecialValueText(i18n("--"));
            if (step > 0)
                focusGain->setSingleStep(step);

            auto defaultGain = m_Settings["focusGain"];
            if (defaultGain.isValid())
                focusGain->setValue(defaultGain.toDouble());
            else
                focusGain->setValue(GainSpinSpecialValue);
        }
    }
    else
        focusGain->clear();
}

void Focus::syncCameraInfo()
{
    if (m_Camera == nullptr)
        return;

    auto targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    if (targetChip == nullptr || (targetChip && targetChip->isCapturing()))
        return;

    m_OpsFocusSettings->focusSubFrame->setEnabled(targetChip->canSubframe());

    if (frameSettings.contains(targetChip) == false)
    {
        int x, y, w, h;
        if (targetChip->getFrame(&x, &y, &w, &h))
        {
            int binx = 1, biny = 1;
            targetChip->getBinning(&binx, &biny);
            if (w > 0 && h > 0)
            {
                int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
                targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

                QVariantMap settings;

                settings["x"]    = m_OpsFocusSettings->focusSubFrame->isChecked() ? x : minX;
                settings["y"]    = m_OpsFocusSettings->focusSubFrame->isChecked() ? y : minY;
                settings["w"]    = m_OpsFocusSettings->focusSubFrame->isChecked() ? w : maxW;
                settings["h"]    = m_OpsFocusSettings->focusSubFrame->isChecked() ? h : maxH;
                settings["binx"] = binx;
                settings["biny"] = biny;

                frameSettings[targetChip] = settings;
            }
        }
    }
}

bool Focus::setFilterWheel(ISD::FilterWheel *device)
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
            focusFilter->setEnabled(true);
            filterManagerB->setEnabled(true);
        });
        connect(m_FilterWheel, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            FilterPosLabel->setEnabled(false);
            focusFilter->setEnabled(false);
            filterManagerB->setEnabled(false);
        });
    }

    auto isConnected = m_FilterWheel && m_FilterWheel->isConnected();
    FilterPosLabel->setEnabled(isConnected);
    focusFilter->setEnabled(isConnected);
    filterManagerB->setEnabled(isConnected);

    FilterPosLabel->setEnabled(true);
    focusFilter->setEnabled(true);

    checkFilter();
    return true;
}

void Focus::updateTemperatureSources(const QList<QSharedPointer<ISD::GenericDevice >> &temperatureSources)
{
    defaultFocusTemperatureSource->blockSignals(true);
    m_TemperatureSources = temperatureSources;

    defaultFocusTemperatureSource->clear();
    for (auto device : temperatureSources)
    {
        if (device.isNull())
            break;
        defaultFocusTemperatureSource->addItem(device->getDeviceName());
    }
    defaultFocusTemperatureSource->blockSignals(false);

    auto targetSource = m_Settings["defaultFocusTemperatureSource"];
    if (targetSource.isValid())
        checkTemperatureSource(targetSource.toString());
}

void Focus::checkTemperatureSource(const QString &name )
{
    auto source = name;
    if (name.isEmpty())
    {
        source = defaultFocusTemperatureSource->currentText();
        if (source.isEmpty())
            return;
    }

    QSharedPointer<ISD::GenericDevice> currentSource;

    for (auto &oneSource : m_TemperatureSources)
    {
        if (oneSource->getDeviceName() == source)
        {
            currentSource = oneSource;
            break;
        }
    }

    m_LastSourceDeviceAutofocusTemperature = currentSource;

    // No valid device found
    if (!currentSource)
        return;

    // Disconnect all existing signals
    for (const auto &oneSource : m_TemperatureSources)
        disconnect(oneSource.get(), &ISD::GenericDevice::propertyUpdated, this, &Ekos::Focus::processTemperatureSource);


    if (findTemperatureElement(currentSource))
    {
        defaultFocusTemperatureSource->setCurrentText(name);
        m_LastSourceAutofocusTemperature = currentTemperatureSourceElement->value;
        absoluteTemperatureLabel->setText(QString("%1 °C").arg(currentTemperatureSourceElement->value, 0, 'f', 2));
        deltaTemperatureLabel->setText(QString("%1 °C").arg(0.0, 0, 'f', 2));
    }
    else
    {
        m_LastSourceAutofocusTemperature = INVALID_VALUE;
    }

    connect(currentSource.get(), &ISD::GenericDevice::propertyUpdated, this, &Ekos::Focus::processTemperatureSource,
            Qt::UniqueConnection);
}

bool Focus::findTemperatureElement(const QSharedPointer<ISD::GenericDevice> &device)
{
    // protect for nullptr
    if (device.isNull())
        return false;

    auto temperatureProperty = device->getProperty("FOCUS_TEMPERATURE");
    if (!temperatureProperty)
        temperatureProperty = device->getProperty("CCD_TEMPERATURE");
    if (temperatureProperty)
    {
        currentTemperatureSourceElement = temperatureProperty.getNumber()->at(0);
        return true;
    }

    temperatureProperty = device->getProperty("WEATHER_PARAMETERS");
    if (temperatureProperty)
    {
        for (int i = 0; i < temperatureProperty.getNumber()->count(); i++)
        {
            if (strstr(temperatureProperty.getNumber()->at(i)->getName(), "_TEMPERATURE"))
            {
                currentTemperatureSourceElement = temperatureProperty.getNumber()->at(i);
                return true;
            }
        }
    }

    return false;
}

QString Focus::filterWheel()
{
    if (m_FilterWheel)
        return m_FilterWheel->getDeviceName();

    return QString();
}


bool Focus::setFilter(const QString &filter)
{
    if (m_FilterWheel)
    {
        focusFilter->setCurrentText(filter);
        return true;
    }

    return false;
}

QString Focus::filter()
{
    return focusFilter->currentText();
}

void Focus::checkFilter()
{
    focusFilter->clear();

    if (!m_FilterWheel)
    {
        FilterPosLabel->setEnabled(false);
        focusFilter->setEnabled(false);
        filterManagerB->setEnabled(false);

        if (m_FilterManager)
        {
            m_FilterManager->disconnect(this);
            disconnect(m_FilterManager.get());
            m_FilterManager.reset();
        }
        return;
    }

    FilterPosLabel->setEnabled(true);
    focusFilter->setEnabled(true);
    filterManagerB->setEnabled(true);

    setupFilterManager();

    focusFilter->addItems(m_FilterManager->getFilterLabels());

    currentFilterPosition = m_FilterManager->getFilterPosition();

    focusFilter->setCurrentIndex(currentFilterPosition - 1);

    focusExposure->setValue(m_FilterManager->getFilterExposure());
}

bool Focus::setFocuser(ISD::Focuser *device)
{
    if (m_Focuser && m_Focuser == device)
    {
        checkFocuser();
        return false;
    }

    if (m_Focuser)
        m_Focuser->disconnect(this);

    m_Focuser = device;

    if (m_Focuser)
    {
        connect(m_Focuser, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            resetButtons();
        });
        connect(m_Focuser, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            resetButtons();
        });
    }

    checkFocuser();
    emit focuserChanged(m_focuserId, device != nullptr);
    return true;
}

QString Focus::focuser()
{
    if (m_Focuser)
        return m_Focuser->getDeviceName();

    return QString();
}

void Focus::checkFocuser()
{
    if (!m_Focuser)
    {
        if (m_FilterManager)
            m_FilterManager->setFocusReady(false);
        canAbsMove = canRelMove = canTimerMove = false;
        resetButtons();
        setFocusAlgorithm(static_cast<Algorithm> (m_OpsFocusProcess->focusAlgorithm->currentIndex()));
        return;
    }

    if (m_FilterManager)
        m_FilterManager->setFocusReady(m_Focuser->isConnected());

    hasDeviation = m_Focuser->hasDeviation();

    canAbsMove = m_Focuser->canAbsMove();

    if (canAbsMove)
    {
        getAbsFocusPosition();

        if (m_FilterManager)
            m_FilterManager->setFocusAbsolutePosition(currentPosition);

        absTicksSpin->setEnabled(true);
        absTicksLabel->setEnabled(true);
        startGotoB->setEnabled(true);

        // Now that Optical Trains are managing settings, no need to set absTicksSpin, except if it has a bad value
        if (absTicksSpin->value() <= 0)
            absTicksSpin->setValue(currentPosition);
    }
    else
    {
        absTicksSpin->setEnabled(false);
        absTicksLabel->setEnabled(false);
        startGotoB->setEnabled(false);
    }

    canRelMove = m_Focuser->canRelMove();

    // In case we have a purely relative focuser, we pretend
    // it is an absolute focuser with initial point set at 50,000.
    // This is done we can use the same algorithm used for absolute focuser.
    if (canAbsMove == false && canRelMove == true)
    {
        currentPosition = 50000;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }

    canTimerMove = m_Focuser->canTimerMove();

    // In case we have a timer-based focuser and using the linear focus algorithm,
    // we pretend it is an absolute focuser with initial point set at 50,000.
    // These variables don't have in impact on timer-based focusers if the algorithm
    // is not the linear focus algorithm.
    if (!canAbsMove && !canRelMove && canTimerMove)
    {
        currentPosition = 50000;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }

    m_FocusType = (canRelMove || canAbsMove || canTimerMove) ? FOCUS_AUTO : FOCUS_MANUAL;
    profilePlot->setFocusAuto(m_FocusType == FOCUS_AUTO);

    bool hasBacklash = m_Focuser->hasBacklash();
    m_OpsFocusMechanics->focusBacklash->setEnabled(hasBacklash);
    m_OpsFocusMechanics->focusBacklash->disconnect(this);
    if (hasBacklash)
    {
        double min = 0, max = 0, step = 0;
        m_Focuser->getMinMaxStep("FOCUS_BACKLASH_STEPS", "FOCUS_BACKLASH_VALUE", &min, &max, &step);
        m_OpsFocusMechanics->focusBacklash->setMinimum(min);
        m_OpsFocusMechanics->focusBacklash->setMaximum(max);
        m_OpsFocusMechanics->focusBacklash->setSingleStep(step);
        m_OpsFocusMechanics->focusBacklash->setValue(m_Focuser->getBacklash());
        connect(m_OpsFocusMechanics->focusBacklash, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this, [this](int value)
        {
            if (m_Focuser)
            {
                if (m_Focuser->getBacklash() == value)
                    // Prevent an event storm where a fast update of the backlash field, e.g. changing
                    // the value to "12" results in 2 events; "1", "12". As these events get
                    // processed in the driver and persisted, they in turn update backlash and start
                    // to conflict with this callback, getting stuck forever: "1", "12", "1', "12"
                    return;
                m_Focuser->setBacklash(value);
            }
        });
        // Re-esablish connection to sync settings. Only need to reconnect if the focuser
        // has backlash as the value can't be changed if the focuser doesn't have the backlash property.
        connect(m_OpsFocusMechanics->focusBacklash, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Focus::syncSettings);
    }
    else
    {
        m_OpsFocusMechanics->focusBacklash->setValue(0);
    }

    connect(m_Focuser, &ISD::Focuser::propertyUpdated, this, &Ekos::Focus::updateProperty, Qt::UniqueConnection);

    resetButtons();
    setFocusAlgorithm(static_cast<Algorithm> (m_OpsFocusProcess->focusAlgorithm->currentIndex()));
}

bool Focus::setCamera(ISD::Camera *device)
{
    if (m_Camera && m_Camera == device)
    {
        checkCamera();
        return false;
    }

    m_captureInProgress = false;

    if (m_Camera)
        m_Camera->disconnect(this);

    m_Camera = device;

    if (m_Camera)
    {
        connect(m_Camera, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            resetButtons();
        });
        connect(m_Camera, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            resetButtons();
        });
    }

    auto isConnected = m_Camera && m_Camera->isConnected();
    focuserGroup->setEnabled(isConnected || (m_Focuser && m_Focuser->isConnected()));
    ccdGroup->setEnabled(isConnected);
    toolsGroup->setEnabled(isConnected);

    if (!m_Camera)
        return false;

    resetFrame();

    checkCamera();
    checkMosaicMaskLimits();
    return true;
}

void Focus::getAbsFocusPosition()
{
    if (!canAbsMove)
        return;

    auto absMove = m_Focuser->getNumber("ABS_FOCUS_POSITION");

    if (absMove)
    {
        const auto &it = absMove->at(0);
        currentPosition = static_cast<int>(it->getValue());
        absMotionMax    = it->getMax();
        absMotionMin    = it->getMin();

        absTicksSpin->setMinimum(it->getMin());
        absTicksSpin->setMaximum(it->getMax());
        absTicksSpin->setSingleStep(it->getStep());

        // Restrict the travel if needed
        double const travel = std::abs(it->getMax() - it->getMin());
        m_OpsFocusMechanics->focusMaxTravel->setMaximum(travel);;

        absTicksLabel->setText(QString::number(currentPosition));

        m_OpsFocusMechanics->focusTicks->setMaximum(it->getMax() / 2);
    }
}

void Focus::processTemperatureSource(INDI::Property prop)
{
    if (m_LastSourceAutofocusTemperature == INVALID_VALUE && m_LastSourceDeviceAutofocusTemperature
            && !currentTemperatureSourceElement )
    {
        if( findTemperatureElement( m_LastSourceDeviceAutofocusTemperature ) )
        {
            appendLogText(i18n("Finally found temperature source %1", QString(currentTemperatureSourceElement->nvp->name)));
            m_LastSourceAutofocusTemperature = currentTemperatureSourceElement->value;
        }
    }

    double delta = 0;
    if (currentTemperatureSourceElement && currentTemperatureSourceElement->nvp->name == prop.getName())
    {
        if (m_LastSourceAutofocusTemperature != INVALID_VALUE)
        {
            delta = currentTemperatureSourceElement->value - m_LastSourceAutofocusTemperature;
            emit newFocusTemperatureDelta(abs(delta), currentTemperatureSourceElement->value, opticalTrain());
        }
        else
        {
            emit newFocusTemperatureDelta(0, currentTemperatureSourceElement->value, opticalTrain());
        }

        absoluteTemperatureLabel->setText(QString("%1 °C").arg(currentTemperatureSourceElement->value, 0, 'f', 2));
        deltaTemperatureLabel->setText(QString("%1%2 °C").arg((delta > 0.0 ? "+" : "")).arg(delta, 0, 'f', 2));
        if (delta == 0)
            deltaTemperatureLabel->setStyleSheet("color: lightgreen");
        else if (delta > 0)
            deltaTemperatureLabel->setStyleSheet("color: lightcoral");
        else
            deltaTemperatureLabel->setStyleSheet("color: lightblue");
    }
}

void Focus::setLastFocusTemperature()
{
    m_LastSourceAutofocusTemperature = currentTemperatureSourceElement ? currentTemperatureSourceElement->value : INVALID_VALUE;

    // Reset delta to zero now that we're just done with autofocus
    deltaTemperatureLabel->setText(QString("0 °C"));
    deltaTemperatureLabel->setStyleSheet("color: lightgreen");

    emit newFocusTemperatureDelta(0, -1e6, opticalTrain());
}

void Focus::setLastFocusAlt()
{
    if (mountAlt < 0.0 || mountAlt > 90.0)
        m_LastSourceAutofocusAlt = INVALID_VALUE;
    else
        m_LastSourceAutofocusAlt = mountAlt;
}

// Reset Adaptive Focus parameters.
void Focus::resetAdaptiveFocus(bool enabled)
{
    // Set the focusAdaptive switch so other modules such as Capture can access it
    Options::setFocusAdaptive(enabled);

    if (enabled)
        adaptFocus.reset(new AdaptiveFocus(this));
    adaptFocus->resetAdaptiveFocusCounters();
}

// Capture has signalled an Adaptive Focus run
void Focus::adaptiveFocus()
{
    // Invoke runAdaptiveFocus
    adaptFocus->runAdaptiveFocus(currentPosition, filter());
}

void Focus::showFirstFrame()
{
    if (m_FocusView->showFirstFrame())
        refreshMeasuresDisplay();
}

void Focus::showLastFrame()
{
    if (m_FocusView->showLastFrame())
        refreshMeasuresDisplay();
}

void Focus::showPreviousFrame()
{
    if (m_FocusView->showPreviousFrame())
        refreshMeasuresDisplay();
}

void Focus::showNextFrame()
{
    if (m_FocusView->showNextFrame())
        refreshMeasuresDisplay();
}

void Focus::showPreviousAFRun()
{
    if (m_FocusView->showPreviousAFRun())
        refreshMeasuresDisplay();
}

void Focus::showNextAFRun()
{
    if (m_FocusView->showNextAFRun())
        refreshMeasuresDisplay();
}

// Run Aberration Inspector
void Focus::startAbIns()
{
    m_abInsOn = canAbInsStart();
    runAutoFocus(AutofocusReason::FOCUS_ABERRATION_INSPECTOR, "");
}

// User pressed the Autofocus button
void Focus::manualStart()
{
    runAutoFocus(AutofocusReason::FOCUS_MANUAL, "");
}

void Focus::clearDataRequested()
{
    // ask to delete the focus frames
    // prepare a warning dialog
    // move to trash or delete permanently
    QCheckBox *permanentlyDeleteCB = new QCheckBox(i18n("Delete directly, do not move to trash."));
    permanentlyDeleteCB->setChecked(m_permanentlyDelete);
    KSMessageBox::Instance()->setCheckBox(permanentlyDeleteCB);
    connect(permanentlyDeleteCB, &QCheckBox::toggled, this, [this](bool checked)
    {
        this->m_permanentlyDelete = checked;
    });

    // Yes, delete the focus frames
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
    {
        clearCurrentRun(true, m_permanentlyDelete);
        // clean up
        KSMessageBox::Instance()->disconnect(this);
        // clear the check box
        KSMessageBox::Instance()->setCheckBox(nullptr);
    });
    // No, don't delete
    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
    {
        clearCurrentRun(false, m_permanentlyDelete);
        // clean up
        KSMessageBox::Instance()->disconnect(this);
        // clear the check box
        KSMessageBox::Instance()->setCheckBox(nullptr);
    });

    KSMessageBox::Instance()->questionYesNo(i18n("Delete the corresponding focus frames?"), i18n("Delete focus frames?"), 0,
                                            false);
}

// An Autofocus start request over DBUS (most likely from the Scheduler)
void Focus::start()
{
    runAutoFocus(AutofocusReason::FOCUS_SCHEDULER, "");
}

// Start an Autofocus run. This is called from Build Offsets, FilterManger and Capture
void Focus::runAutoFocus(AutofocusReason autofocusReason, const QString &reasonInfo)
{
    m_AutofocusReason = autofocusReason;
    m_AutofocusReasonInfo = reasonInfo;
    inSingleCaptureMode = false;

    if (m_Focuser == nullptr)
    {
        appendLogText(i18n("No Focuser connected."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_NO_FOCUSER);
        return;
    }

    if (m_Camera == nullptr)
    {
        appendLogText(i18n("No CCD connected."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_NO_CAMERA);
        return;
    }

    if (!canAbsMove && !canRelMove && m_OpsFocusMechanics->focusTicks->value() <= MINIMUM_PULSE_TIMER)
    {
        appendLogText(i18n("Starting pulse step is too low. Increase the step size to %1 or higher...",
                           MINIMUM_PULSE_TIMER * 5));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_LOW_PULSE);
        return;
    }

    if (inAutoFocus)
    {
        // If Autofocus is already running, just ignore this request. This condition should not happen
        // There is no point signalling Autofocus failure as that will trigger actions based on the
        // currently running Autofocus failing (whilst it is still running).
        appendLogText(i18n("Autofocus is already running, discarding start request."));
        return;
    }

    if (inBuildOffsets)
    {
        // If Build Offsets is running, reject the Autofocus request.
        appendLogText(i18n("Build Offset is already running, Autofocus rejected."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_INTERNAL);
        return;
    }

    if (inFocusLoop)
    {
        // If inFocusLoop is already running, reject this Autofocus request
        appendLogText(i18n("In Focus Loop, Autofocus rejected."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_INTERNAL);
        return;
    }

    if (inAdjustFocus || adaptFocus->inAdaptiveFocus())
    {
        QString str = inAdjustFocus ? i18n("Adjust Focus") : i18n("Adaptive Focus");
        if (++m_StartRetries < MAXIMUM_RESTART_ITERATIONS)
        {
            appendLogText(i18n("Autofocus start request - Waiting 10sec for %1 to complete.", str));
            QTimer::singleShot(10 * 1000, this, [this]()
            {
                runAutoFocus(m_AutofocusReason, m_AutofocusReasonInfo);
            });
            return;
        }

        appendLogText(i18n("Discarding Autofocus start request - %1 in progress.", str));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_INTERNAL);
        return;
    }

    inBuildOffsets = (autofocusReason == AutofocusReason::FOCUS_FILTER_OFFSETS);
    if (autofocusReason == AutofocusReason::FOCUS_USER_REQUEST)
    {
        forceInSeqAF->setChecked(false);
        emit inSequenceAF(false, opticalTrain());
    }

    inAutoFocus = true;
    // Check for AF optimisation. The purpose of this to optimise out automated AF requests, e.g., on startup where an
    // AF can be run prior to alignment and then repeated a few seconds later at the start of the schedule. In the case
    // that the AF run is optimised out, don't signal AF event or Analyze but we do need to signal Capture / Scheduler
    // and do a proper AF close down. We can do this now if no focuser move is required, otherwise we'll wait for the
    // focuser move to complete before completing the Autofocus.
    if ((m_FocusAlgorithm == FOCUS_LINEAR || m_FocusAlgorithm == FOCUS_LINEAR1PASS) && checkAFOptimisation(autofocusReason))
    {
        appendLogText(i18n("Autofocus request [%1] optimized out.", filter()));
        if (!inAFOptimise)
            completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_OPTIMISED_OUT);
        return;
    }

    m_FocusView->addRun();

    m_StartRetries = 0;
    m_LastFocusDirection = FOCUS_NONE;

    waitStarSelectTimer.stop();

    // Reset the focus motion timer
    m_FocusMotionTimerCounter = 0;
    m_FocusMotionTimer.stop();
    m_FocusMotionTimer.setInterval(m_OpsFocusMechanics->focusMotionTimeout->value() * 1000);

    // Reset focuser reconnect counter
    m_FocuserReconnectCounter = 0;

    // Reset focuser reconnect counter
    m_FocuserReconnectCounter = 0;
    m_DebugFocuserCounter = 0;

    starsHFR.clear();

    lastHFR = 0;

    // Reset state variable that deals with retrying and aborting aurofocus
    m_RestartState = RESTART_NONE;

    // Keep the  last focus temperature, it can still be useful in case the autofocus fails
    // lastFocusTemperature

    if (canAbsMove)
    {
        absIterations = 0;
        getAbsFocusPosition();
        pulseDuration = m_OpsFocusMechanics->focusTicks->value();
    }
    else if (canRelMove)
    {
        //appendLogText(i18n("Setting dummy central position to 50000"));
        absIterations   = 0;
        pulseDuration   = m_OpsFocusMechanics->focusTicks->value();
        //currentPosition = 50000;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }
    else
    {
        pulseDuration   = m_OpsFocusMechanics->focusTicks->value();
        absIterations   = 0;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }

    focuserAdditionalMovement = 0;
    starMeasureFrames.clear();

    resetButtons();

    // clear the history
    refreshMeasuresDisplay();

    reverseDir = false;

    /*if (fw > 0 && fh > 0)
        starSelected= true;
    else
        starSelected= false;*/

    clearDataPoints();
    profilePlot->clear();
    FWHMOut->setText("--");

    qCInfo(KSTARS_EKOS_FOCUS)  << "Starting Autofocus " << lastAFRun()
                               << " on" << opticalTrain()
                               << " Reason: " << AutofocusReasonStr[m_AutofocusReason]
                               << " Reason Info: " << m_AutofocusReasonInfo
                               << " CanAbsMove: " << (canAbsMove ? "yes" : "no" )
                               << " CanRelMove: " << (canRelMove ? "yes" : "no" )
                               << " CanTimerMove: " << (canTimerMove ? "yes" : "no" )
                               << " Position:" << currentPosition
                               << " Filter:" << filter()
                               << " Exp:" << focusExposure->value()
                               << " Bin:" << focusBinning->currentText()
                               << " Gain:" << focusGain->value()
                               << " ISO:" << focusISO->currentText();
    qCInfo(KSTARS_EKOS_FOCUS)  << "Settings Tab."
                               << " Auto Select Star:" << ( m_OpsFocusSettings->focusAutoStarEnabled->isChecked() ? "yes" : "no" )
                               << " Dark Frame:" << ( m_OpsFocusSettings->useFocusDarkFrame->isChecked() ? "yes" : "no" )
                               << " Sub Frame:" << ( m_OpsFocusSettings->focusSubFrame->isChecked() ? "yes" : "no" )
                               << " Box:" << m_OpsFocusSettings->focusBoxSize->value()
                               << " Full frame:" << ( m_OpsFocusSettings->focusUseFullField->isChecked() ? "yes" : "no " )
                               << " Focus Mask: " << (m_OpsFocusSettings->focusNoMaskRB->isChecked() ? "Use all stars" :
                                       (m_OpsFocusSettings->focusRingMaskRB->isChecked() ? QString("Ring Mask: [%1%, %2%]").
                                        arg(m_OpsFocusSettings->focusFullFieldInnerRadius->value()).arg(m_OpsFocusSettings->focusFullFieldOuterRadius->value()) :
                                        QString("Mosaic Mask: [%1%, space=%2px]").
                                        arg(m_OpsFocusSettings->focusMosaicTileWidth->value()).arg(m_OpsFocusSettings->focusMosaicSpace->value())))
                               << " Suspend Guiding:" << ( m_OpsFocusSettings->focusSuspendGuiding->isChecked() ? "yes" : "no " )
                               << " Guide Settle:" << m_OpsFocusSettings->focusGuideSettleTime->value()
                               << " Display Units:" << m_OpsFocusSettings->focusUnits->currentText()
                               << " AF Optimize:" << m_OpsFocusSettings->focusAFOptimize->value()
                               << " Adaptive Focus:" << ( m_OpsFocusSettings->focusAdaptive->isChecked() ? "yes" : "no" )
                               << " Min Move:" << m_OpsFocusSettings->focusAdaptiveMinMove->value()
                               << " Adapt Start:" << ( m_OpsFocusSettings->focusAdaptStart->isChecked() ? "yes" : "no" )
                               << " Max Total Move:" << m_OpsFocusSettings->focusAdaptiveMaxMove->value();
    qCInfo(KSTARS_EKOS_FOCUS)  << "Process Tab."
                               << " Detection:" << m_OpsFocusProcess->focusDetection->currentText()
                               << " SEP Profile:" << m_OpsFocusProcess->focusSEPProfile->currentText()
                               << " Algorithm:" << m_OpsFocusProcess->focusAlgorithm->currentText()
                               << " Curve Fit:" << m_OpsFocusProcess->focusCurveFit->currentText()
                               << " Measure:" << m_OpsFocusProcess->focusStarMeasure->currentText()
                               << " PSF:" << m_OpsFocusProcess->focusStarPSF->currentText()
                               << " Use Weights:" << ( m_OpsFocusProcess->focusUseWeights->isChecked() ? "yes" : "no" )
                               << " R2 Limit:" << m_OpsFocusProcess->focusR2Limit->value()
                               << " Refine Curve Fit:" << ( m_OpsFocusProcess->focusRefineCurveFit->isChecked() ? "yes" : "no" )
                               << " Average Over:" << m_OpsFocusProcess->focusFramesCount->value()
                               << " Average HFR Check Over:" << m_OpsFocusProcess->focusHFRFramesCount->value()
                               << " Num.of Rows:" << m_OpsFocusProcess->focusMultiRowAverage->value()
                               << " Sigma:" << m_OpsFocusProcess->focusGaussianSigma->value()
                               << " Threshold:" << m_OpsFocusProcess->focusThreshold->value()
                               << " Kernel size:" << m_OpsFocusProcess->focusGaussianKernelSize->value()
                               << " Tolerance:" << m_OpsFocusProcess->focusTolerance->value()
                               << " Denoise:" << ( m_OpsFocusProcess->focusDenoise->isChecked() ? "yes" : "no" )
                               << " Donut Buster:" << ( m_OpsFocusProcess->focusDonut->isChecked() ? "yes" : "no" )
                               << " Donut Time Dilation:" << m_OpsFocusProcess->focusTimeDilation->value()
                               << " Outlier Rejection:" << m_OpsFocusProcess->focusOutlierRejection->value()
                               << " Scan Start Pos:" << ( m_OpsFocusProcess->focusScanStartPos->isChecked() ? "yes" : "no" )
                               << " Scan Always On:" << ( m_OpsFocusProcess->focusScanAlwaysOn->isChecked() ? "yes" : "no" )
                               << " Num Datapoints:" << m_OpsFocusProcess->focusScanDatapoints->value()
                               << " Scan Step Factor:" << m_OpsFocusProcess->focusScanStepSizeFactor->value();
    qCInfo(KSTARS_EKOS_FOCUS)  << "Mechanics Tab."
                               << " Initial Step Size:" << m_OpsFocusMechanics->focusTicks->value()
                               << " Out Step Multiple:" << m_OpsFocusMechanics->focusOutSteps->value()
                               << " Number Steps:" << m_OpsFocusMechanics->focusNumSteps->value()
                               << " Max Travel:" << m_OpsFocusMechanics->focusMaxTravel->value()
                               << " Max Step Size:" << m_OpsFocusMechanics->focusMaxSingleStep->value()
                               << " Driver Backlash:" << m_OpsFocusMechanics->focusBacklash->value()
                               << " AF Overscan:" << m_OpsFocusMechanics->focusAFOverscan->value()
                               << " Overscan Delay:" << m_OpsFocusMechanics->focusOverscanDelay->value()
                               << " Focuser Settle:" << m_OpsFocusMechanics->focusSettleTime->value()
                               << " Walk:" << m_OpsFocusMechanics->focusWalk->currentText()
                               << " Capture Timeout:" << m_OpsFocusMechanics->focusCaptureTimeout->value()
                               << " Motion Timeout:" << m_OpsFocusMechanics->focusMotionTimeout->value();
    qCInfo(KSTARS_EKOS_FOCUS)  << "CFZ Tab."
                               << " Algorithm:" << m_CFZUI->focusCFZAlgorithm->currentText()
                               << " Tolerance:" << m_CFZUI->focusCFZTolerance->value()
                               << " Tolerance (τ):" << m_CFZUI->focusCFZTau->value()
                               << " Display:" << ( m_CFZUI->focusCFZDisplayVCurve->isChecked() ? "yes" : "no" )
                               << " Wavelength (λ):" << m_CFZUI->focusCFZWavelength->value()
                               << " Aperture (A):" << m_CFZUI->focusCFZAperture->value()
                               << " Focal Ratio (f):" << m_CFZUI->focusCFZFNumber->value()
                               << " Step Size:" << m_CFZUI->focusCFZStepSize->value()
                               << " FWHM (θ):" << m_CFZUI->focusCFZSeeing->value();

    const double temperature = (currentTemperatureSourceElement) ? currentTemperatureSourceElement->value : INVALID_VALUE;
    emit autofocusStarting(temperature, filter(), m_AutofocusReason, m_AutofocusReasonInfo);

    appendLogText(i18n("Autofocus starting..."));

    // Only suspend when we have Off-Axis Guider
    // If the guide camera is operating on a different OTA
    // then no need to suspend.
    if (m_GuidingSuspended == false && m_OpsFocusSettings->focusSuspendGuiding->isChecked())
    {
        m_GuidingSuspended = true;
        emit suspendGuiding();
    }

    //emit statusUpdated(true);
    setState(Ekos::FOCUS_PROGRESS);

    KSNotification::event(QLatin1String("FocusStarted"), i18n("Autofocus operation started"), KSNotification::Focus);

    // Used for all the focuser types.
    if (m_FocusAlgorithm == FOCUS_LINEAR || m_FocusAlgorithm == FOCUS_LINEAR1PASS)
    {
        m_AFfilter = filter();
        int position = adaptFocus->adaptStartPosition(currentPosition, m_AFfilter);

        curveFitting.reset(new CurveFitting());

        if (m_FocusAlgorithm == FOCUS_LINEAR1PASS)
        {
            // Curve fitting for stars and FWHM processing
            starFitting.reset(new CurveFitting());
            focusFWHM.reset(new FocusFWHM(m_ScaleCalc));
            focusFourierPower.reset(new FocusFourierPower(m_ScaleCalc));
#if defined(HAVE_OPENCV)
            focusBlurriness.reset(new FocusBlurriness());
#endif
            // Donut Buster
            initDonutProcessing();
            // Scan for Start Position
            if (initScanStartPos(false, currentPosition))
                return;
            // Focus Advisor
            if (m_AutofocusReason == FOCUS_FOCUS_ADVISOR)
            {
                focusAdvisor->initControl();
                return;
            }
        }

        // Setup the focuser
        setupLinearFocuser(position);
        if (!changeFocus(linearRequestedPosition - currentPosition))
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_NO_MOVE);

        // Avoid the capture below.
        return;
    }
    capture();
}

bool Focus::checkAFOptimisation(const AutofocusReason autofocusReason)
{
    bool dontRunAF = false;
    inAFOptimise = false;

    // Check that the optimisation control has been set, if not then nothing to do
    if (!m_FilterManager || m_OpsFocusSettings->focusAFOptimize->value() <= 0)
        return dontRunAF;

    // Now check why Autofocus is being requested... only optimise automatically requested AF runs
    // so manual runs, Build Filter Offsets, and AF after Meridian flip are always performed.
    if (autofocusReason != FOCUS_FILTER &&
            autofocusReason != FOCUS_TIME &&
            autofocusReason != FOCUS_TEMPERATURE &&
            autofocusReason != FOCUS_HFR_CHECK &&
            autofocusReason != FOCUS_SCHEDULER)
        return dontRunAF;

    // Get the filter we would use for Autofocus where we to run it
    QString filterToUse = filter();
    QString lockFilter = m_FilterManager->getFilterLock(filterToUse);
    if (lockFilter != NULL_FILTER && lockFilter != filterToUse)
        filterToUse = lockFilter;

    // Check when the last successful Autofocus was run on filterToUse
    QDateTime lastAFDatetime;
    if (!m_FilterManager->getAFDatetime(filterToUse, lastAFDatetime))
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 unable to get last Autofocus run timestamp on %2")
                                   .arg(__FUNCTION__).arg(filterToUse);
        return dontRunAF;
    }

    // Check if the last Autofocus run is within the user defined optimisation zone
    QDateTime now = KStarsData::Instance()->lt();
    auto lastAFDelta = lastAFDatetime.secsTo(now);
    if (lastAFDelta > m_OpsFocusSettings->focusAFOptimize->value())
        return dontRunAF;

    // If Adapt Start Pos is set then we need to adjust the focuser position based on the settings (temp, alt)
    int position = currentPosition;
    if (m_OpsFocusSettings->focusAdaptStart->isChecked())
        position = adaptFocus->adaptStartPosition(currentPosition, filterToUse);
    else
    {
        // Start with the last AF run result for the Autofocus filter
        double lastTemp, lastAlt;
        if(!m_FilterManager->getFilterAbsoluteFocusDetails(filterToUse, position, lastTemp, lastAlt))
        {
            // Unable to get the last AF run information for the filter
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 unable to get last Autofocus info on %2")
                                       .arg(__FUNCTION__).arg(filterToUse);
            return dontRunAF;
        }
        // Do some sanity checks on lastPos
        int minTravelLimit = qMax(0.0, currentPosition - m_OpsFocusMechanics->focusMaxTravel->value());
        int maxTravelLimit = qMin(absMotionMax, currentPosition + m_OpsFocusMechanics->focusMaxTravel->value());
        if (position < minTravelLimit || position > maxTravelLimit)
        {
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 Bad last Autofocus solution found on %2")
                                       .arg(__FUNCTION__).arg(position);
            return dontRunAF;
        }

    }

    // Add on any filter offset between the requested filter() and Autofocus filterToUse
    if (filterToUse != filter())
    {
        int filterOffset = m_FilterManager->getFilterOffset(filter());
        int filterToUseOffset = m_FilterManager->getFilterOffset(filterToUse);
        if (filterOffset != INVALID_VALUE && filterToUseOffset != INVALID_VALUE)
            position += filterOffset - filterToUseOffset;
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 unable to calculate filter offsets").arg(__FUNCTION__);
            return dontRunAF;
        }
    }

    // We now have all the information required to move the focuser
    if (abs(position - currentPosition) <= 1)
    {
        // Focuser at correct position so nothing more to do
        dontRunAF = true;
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Autofocus (%1) on %2 optimised out by Autofocus at %3")
                                   .arg(AutofocusReasonStr[autofocusReason]).arg(filterToUse).arg(lastAFDatetime.toString());
    }
    else
    {
        // Need to wait for the focuser to move before signalling DONE (controlled by inAFOptimise flag)
        if (changeFocus(position - currentPosition))
        {
            inAFOptimise = dontRunAF = true;
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Autofocus (%1) on %2 optimised out by Autofocus at %3."
                                                  " Current Position %4, Target Position %5")
                                       .arg(AutofocusReasonStr[autofocusReason]).arg(filterToUse)
                                       .arg(lastAFDatetime.toString()).arg(currentPosition).arg(position);
        }
        else
            qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 unable to move focuser... trying full Autofocus").arg(__FUNCTION__);
    }
    return dontRunAF;
}

double Focus::calculateCurrentHFR()
{
    if (isStarMeasureStarBased())
    {
        appendLogText(i18n("Detection complete."));

        if (m_StarFinderWatcher.result() == false)
        {
            qCWarning(KSTARS_EKOS_FOCUS) << "Failed to extract any stars.";
            return INVALID_STAR_MEASURE;
        }
        else
        {
            if (m_OpsFocusSettings->focusUseFullField->isChecked())
            {
                m_FocusView->filterStars();

                // Get the average HFR of the whole frame
                return m_ImageData->getHFR(m_StarMeasure == FOCUS_STAR_HFR_ADJ ? HFR_ADJ_AVERAGE : HFR_AVERAGE);
            }
            else
            {
                m_FocusView->setTrackingBoxEnabled(true);

                // JM 2020-10-08: Try to get first the same HFR star already selected before
                // so that it doesn't keep jumping around
                double hfr = INVALID_STAR_MEASURE;
                if (starCenter.isNull() == false)
                    hfr = m_ImageData->getHFR(starCenter.x(), starCenter.y());

                // If not found, then get the MAX or MEDIAN depending on the selected algorithm.
                if (hfr < 0)
                    hfr = m_ImageData->getHFR(m_FocusDetection == ALGORITHM_SEP ? HFR_HIGH : HFR_MAX);

                return hfr;
            }
        }
    }
    return INVALID_STAR_MEASURE;
}

CaptureHistory::FrameData Focus::calculateCurrentMeasureAndWeight()
{
    CaptureHistory::FrameData frameData;
    double newFWHM = INVALID_STAR_MEASURE;
    double newWeight = INVALID_STAR_MEASURE;
    double newFourierPower = INVALID_STAR_MEASURE;
    double newBlurriness = INVALID_STAR_MEASURE;

    frameData.hfr = calculateCurrentHFR();
    frameData.numStars = m_ImageData->getDetectedStars();

    // Setup with measure we are using (HFR, FWHM, etc)
    if (m_StarMeasure == FOCUS_STAR_NUM_STARS)
    {
        frameData.measure = lastFrame().numStars;
        frameData.weight = 1.0;
    }
    else if (m_StarMeasure == FOCUS_STAR_FWHM)
    {
        getFWHM(m_ImageData->getStarCenters(), &newFWHM, &newWeight);
        frameData.fwhm = newFWHM;
        frameData.measure = newFWHM;
    }
    else if (m_StarMeasure == FOCUS_STAR_FOURIER_POWER)
    {

        getFourierPower(&newFourierPower, &newWeight);
        frameData.measure = newFourierPower;
    }
    else if (m_StarMeasure == FOCUS_STAR_STDDEV || m_StarMeasure == FOCUS_STAR_SOBEL ||
             m_StarMeasure == FOCUS_STAR_LAPLASSIAN || m_StarMeasure == FOCUS_STAR_CANNY)
    {
        QRect roi = QRect();
        if (m_OpsFocusSettings->focusSubFrame->isChecked())
            roi = m_FocusView->isTrackingBoxEnabled() ? m_FocusView->getTrackingBox() : QRect();
        getBlurriness(m_StarMeasure, m_OpsFocusProcess->focusDenoise->isChecked(), &newBlurriness, &newWeight, roi);
        frameData.measure = newBlurriness;
    }
    else
    {
        frameData.measure = frameData.hfr;
        QList<Edge*> stars = m_ImageData->getStarCenters();
        std::vector<double> hfrs(stars.size());
        std::transform(stars.constBegin(), stars.constEnd(), hfrs.begin(), [](Edge * edge)
        {
            return edge->HFR;
        });
        frameData.weight = calculateStarWeight(m_OpsFocusProcess->focusUseWeights->isChecked(), hfrs);
    }

    return frameData;
}

void Focus::setupLinearFocuser(int initialPosition)
{
    FocusAlgorithmInterface::FocusParams params(curveFitting.get(),
            m_OpsFocusMechanics->focusMaxTravel->value(), m_OpsFocusMechanics->focusTicks->value(), initialPosition, absMotionMin,
            absMotionMax, MAXIMUM_ABS_ITERATIONS, m_OpsFocusProcess->focusTolerance->value() / 100.0, m_AFfilter,
            currentTemperatureSourceElement ? currentTemperatureSourceElement->value : INVALID_VALUE,
            m_OpsFocusMechanics->focusOutSteps->value(), m_OpsFocusMechanics->focusNumSteps->value(),
            m_FocusAlgorithm, m_OpsFocusMechanics->focusBacklash->value(), m_CurveFit, m_OpsFocusProcess->focusUseWeights->isChecked(),
            m_StarMeasure, m_StarPSF, m_OpsFocusProcess->focusRefineCurveFit->isChecked(), m_FocusWalk,
            m_OpsFocusProcess->focusDonut->isChecked(), m_OpsFocusProcess->focusOutlierRejection->value(), m_OptDir, m_ScaleCalc);

    if (canAbsMove)
        initialFocuserAbsPosition = initialPosition;
    linearFocuser.reset(MakeLinearFocuser(params));
    linearRequestedPosition = linearFocuser->initialPosition();
}

// Initialise donut buster
void Focus::initDonutProcessing()
{
    if (!m_OpsFocusProcess->focusDonut->isChecked())
        return;

    m_donutOrigExposure = focusExposure->value();
}

// Reset donut buster
void Focus::resetDonutProcessing()
{
    // If donut busting variable focus exposures have been used, reset to starting value
    if (m_OpsFocusProcess->focusDonut->isChecked() && inAutoFocus)
        focusExposure->setValue(m_donutOrigExposure);
}

int Focus::adjustLinearPosition(int position, int newPosition, int overscan, bool updateDir)
{
    if (overscan > 0 && newPosition > position)
    {
        // If user has set an overscan value then use it
        int adjustment = overscan;
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: extending outward movement by overscan %1").arg(adjustment);

        if (newPosition + adjustment > absMotionMax)
            adjustment = static_cast<int>(absMotionMax) - newPosition;

        focuserAdditionalMovement = adjustment;
        focuserAdditionalMovementUpdateDir = updateDir;

        return newPosition + adjustment;
    }
    return newPosition;
}

// This function checks whether capture is in progress or HFR processing is in progress. If so, it waits until complete before
// stopping processing and signalling to Capture and Scheduler. This prevents Capture / Scheduler from starting the next action
// before Focus is done.
void Focus::checkStopFocus(bool abort)
{
    m_abortInProgress = true;
    // if abort, avoid try to restart
    if (abort)
        resetFocusIteration = MAXIMUM_RESET_ITERATIONS + 1;

    if (captureTimer.isActive())
        // Capture is waiting for the settling period before starting, so just kill the timer which will prevent capture starting
        captureTimer.stop();

    if (m_captureInProgress)
    {
        if (inAutoFocus == false && inFocusLoop == false)
        {
            captureB->setEnabled(true);
            stopFocusB->setEnabled(false);

            appendLogText(i18n("Capture aborted."));
        }
        else if (inAutoFocus)
        {
            stopFocusB->setEnabled(false);
            appendLogText(i18n("Capture in progress, retrying in 1s..."));
            QTimer::singleShot(1000, this, [ &, abort]()
            {
                checkStopFocus(abort);
            });
            return;
        }
    }

    if (m_starDetectInProgress)
    {
        stopFocusB->setEnabled(false);
        if (++m_starDetectFailCounter <= MAX_STAR_DETECT_FAILS)
            // Give star detection a few seconds complete
            appendLogText(i18n("Star detection in progress, retrying in 1s..."));
        else
        {
            // Star detection is long running so for some reason so force an abort
            appendLogText(i18n("Star detection still in progress, aborting..."));
            if (m_ImageData)
                m_ImageData->abortStarDetection();
            m_starDetectInProgress = false;
            m_starDetectFailCounter = 0;
        }
        QTimer::singleShot(1000, this, [ &, abort]()
        {
            checkStopFocus(abort);
        });
        return;
    }

    completeFocusProcedure(abort ? Ekos::FOCUS_ABORTED : Ekos::FOCUS_FAILED, Ekos::FOCUS_FAIL_ABORT);
}

void Focus::meridianFlipStarted()
{
    // if focusing is not running, do nothing
    if (state() == FOCUS_IDLE || state() == FOCUS_COMPLETE || state() == FOCUS_FAILED || state() == FOCUS_ABORTED)
        return;

    // store current focus iteration counter since abort() sets it to the maximal value to avoid restarting
    int old = resetFocusIteration;
    // abort focusing
    abort();
    // restore iteration counter
    resetFocusIteration = old;
}

void Focus::abort()
{
    // No need to "abort" if not already in progress.
    if (state() <= FOCUS_ABORTED)
        return;

    // If Focus Advisor is running let it handle the abort
    if (focusAdvisor->inFocusAdvisor())
    {
        focusAdvisor->stop();
        return;
    }
    processAbort();
}

void Focus::processAbort()
{
    bool focusLoop = inFocusLoop;
    checkStopFocus(true);
    appendLogText(i18n("Autofocus aborted."));
    if (!focusLoop)
        // For looping leave focuser where it is
        // Otherwise try to shift the focuser back to its initial position
        resetFocuser();
}

void Focus::stop(Ekos::FocusState completionState)
{
    Q_UNUSED(completionState);
    qCDebug(KSTARS_EKOS_FOCUS) << "Stopping Focus";

    captureTimeout.stop();
    m_FocusMotionTimer.stop();
    m_FocusMotionTimerCounter = 0;
    m_FocuserReconnectCounter = 0;

    opticalTrainCombo->setEnabled(true);
    trainB->setEnabled(true);
    resetDonutProcessing();
    inAutoFocus = false;
    inAdjustFocus = false;
    adaptFocus->setInAdaptiveFocus(false);
    inBuildOffsets = false;
    inAFOptimise = false;
    inScanStartPos = false;
    focusAdvisor->reset();
    m_AutofocusReason = AutofocusReason::FOCUS_NONE;
    m_AutofocusReasonInfo = "";
    focuserAdditionalMovement = 0;
    focuserAdditionalMovementUpdateDir = true;
    inFocusLoop = false;
    m_captureInProgress = false;
    m_abortInProgress = false;
    m_starDetectInProgress = false;
    isVShapeSolution = false;
    captureFailureCounter = 0;
    minimumRequiredHFR = INVALID_STAR_MEASURE;
    noStarCount = 0;
    starMeasureFrames.clear();
    m_abInsOn = false;
    m_StartRetries = 0;
    m_AFRerun = false;
    m_R2Retries = 0;

    // Check if CCD was not removed due to crash or other reasons.
    if (m_Camera)
    {
        disconnect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Focus::processData);
        disconnect(m_Camera, &ISD::Camera::error, this, &Ekos::Focus::processCaptureError);

        if (rememberUploadMode != m_Camera->getUploadMode())
            m_Camera->setUploadMode(rememberUploadMode);

        // Remember to reset fast exposure if it was enabled before.
        if (m_RememberCameraFastExposure)
        {
            m_RememberCameraFastExposure = false;
            m_Camera->setFastExposureEnabled(true);
        }

        ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
        targetChip->abortExposure();
    }

    resetButtons();

    absIterations = 0;
    HFRInc        = 0;
    reverseDir    = false;

    if (m_GuidingSuspended)
    {
        emit resumeGuiding();
        m_GuidingSuspended = false;
    }
}

void Focus::capture(double settleTime)
{
    if (m_abortInProgress)
    {
        // We are trying to abort Autofocus so do no further processing
        m_captureInProgress = false;
        qCDebug(KSTARS_EKOS_FOCUS) << "Abort AF: discarding capture request.";
        return;
    }

    // If capturing should be delayed by a given settling time, we start the capture timer.
    // This is intentionally designed re-entrant, i.e. multiple calls with settle time > 0 takes the last delay
    if (settleTime > 0 && m_captureInProgress == false)
    {
        captureTimer.start(static_cast<int>(settleTime * 1000));
        return;
    }

    if (m_captureInProgress)
    {
        qCWarning(KSTARS_EKOS_FOCUS) << "Capture called while already in progress. Capture is ignored.";
        return;
    }

    if (m_Camera == nullptr)
    {
        appendLogText(i18n("Error: No Camera detected."));
        checkStopFocus(true);
        return;
    }

    if (m_Camera->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to Camera."));
        checkStopFocus(true);
        return;
    }

    // reset timeout for receiving an image
    captureTimeout.stop();
    // reset timeout for focus star selection
    waitStarSelectTimer.stop();

    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);

    if (m_Camera->isBLOBEnabled() == false)
    {
        m_Camera->setBLOBEnabled(true);
    }

    if (focusFilter->currentIndex() != -1)
    {
        if (m_FilterWheel == nullptr)
        {
            appendLogText(i18n("Error: No Filter Wheel detected."));
            checkStopFocus(true);
            return;
        }
        if (m_FilterWheel->isConnected() == false)
        {
            appendLogText(i18n("Error: Lost connection to Filter Wheel."));
            checkStopFocus(true);
            return;
        }

        // In "regular" autofocus mode if the chosen filter has an associated lock filter then we need
        // to swap to the lock filter before running autofocus. However, AF is also called from build
        // offsets to run AF. In this case we need to ignore the lock filter and use the chosen filter
        int targetPosition = focusFilter->currentIndex() + 1;
        if (!inBuildOffsets)
        {
            QString lockedFilter = m_FilterManager->getFilterLock(filter());

            // We change filter if:
            // 1. Target position is not equal to current position.
            // 2. Locked filter of CURRENT filter is a different filter.
            if (lockedFilter != "--" && lockedFilter != filter())
            {
                int lockedFilterIndex = focusFilter->findText(lockedFilter);
                if (lockedFilterIndex >= 0)
                {
                    // Go back to this filter once we are done
                    fallbackFilterPending = true;
                    fallbackFilterPosition = targetPosition;
                    targetPosition = lockedFilterIndex + 1;
                }
            }
        }

        filterPositionPending = (targetPosition != currentFilterPosition);
        if (filterPositionPending)
        {
            // Change the filter. When done this will signal to update the focusFilter combo
            // Apply filter change policy if in Autofocus; otherwise apply change and offsets
            // Note that Autofocus doesn't need the change policy as Adapt Start Pos takes care of this
            FilterManager::FilterPolicy policy = (inAutoFocus) ?
                                                 static_cast<FilterManager::FilterPolicy>(FilterManager::CHANGE_POLICY) :
                                                 static_cast<FilterManager::FilterPolicy>(FilterManager::CHANGE_POLICY | FilterManager::OFFSET_POLICY);
            m_FilterManager->setFilterPosition(targetPosition, policy);
            return;
        }
        else if (targetPosition != focusFilter->currentIndex() + 1)
            focusFilter->setCurrentIndex(targetPosition - 1);
    }

    m_FocusView->setProperty("suspended", m_OpsFocusSettings->useFocusDarkFrame->isChecked());
    prepareCapture(targetChip);

    connect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Focus::processData, Qt::UniqueConnection);
    connect(m_Camera, &ISD::Camera::error, this, &Ekos::Focus::processCaptureError, Qt::UniqueConnection);

    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        targetChip->setFrame(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(),
                             settings["h"].toInt());
        settings["binx"]          = (focusBinning->currentIndex() + 1);
        settings["biny"]          = (focusBinning->currentIndex() + 1);
        frameSettings[targetChip] = settings;
    }

    m_captureInProgress = true;
    if (state() != FOCUS_PROGRESS)
        setState(FOCUS_PROGRESS);

    m_FocusView->setBaseSize(focusingWidget->size());

    if (targetChip->capture(focusExposure->value()))
    {
        // Timeout is exposure duration + timeout threshold in seconds
        //long const timeout = lround(ceil(focusExposure->value() * 1000)) + FOCUS_TIMEOUT_THRESHOLD;
        captureTimeout.start( (focusExposure->value() + m_OpsFocusMechanics->focusCaptureTimeout->value()) * 1000);

        if (inFocusLoop == false)
            appendLogText(i18n("Capturing image..."));

        resetButtons();
    }
    else if (inAutoFocus)
    {
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_CAPTURE_FAILED);
    }
}

void Focus::prepareCapture(ISD::CameraChip *targetChip)
{
    if (m_Camera->getUploadMode() == ISD::Camera::UPLOAD_REMOTE)
    {
        rememberUploadMode = ISD::Camera::UPLOAD_REMOTE;
        m_Camera->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
    }

    // We cannot use fast exposure in focus.
    if (m_Camera->isFastExposureEnabled())
    {
        m_RememberCameraFastExposure = true;
        m_Camera->setFastExposureEnabled(false);
    }

    m_Camera->setEncodingFormat("FITS");
    targetChip->setBatchMode(false);
    targetChip->setBinning((focusBinning->currentIndex() + 1), (focusBinning->currentIndex() + 1));
    targetChip->setCaptureMode(FITS_FOCUS);
    targetChip->setFrameType(FRAME_LIGHT);
    targetChip->setCaptureFilter(FITS_NONE);

    if (isFocusISOEnabled() && focusISO->currentIndex() != -1 &&
            targetChip->getISOIndex() != focusISO->currentIndex())
        targetChip->setISOIndex(focusISO->currentIndex());

    if (isFocusGainEnabled() && focusGain->value() != GainSpinSpecialValue)
        m_Camera->setGain(focusGain->value());
}

bool Focus::focusIn(int ms, int speedFactor)
{
    if (currentPosition == absMotionMin)
    {
        appendLogText(i18n("At minimum focus position %1...", absMotionMin));
        return false;
    }
    focusInB->setEnabled(false);
    focusOutB->setEnabled(false);
    startGotoB->setEnabled(false);
    if (ms <= 0)
        ms = m_OpsFocusMechanics->focusTicks->value();

    // apply speed factor
    ms *= speedFactor;

    if (currentPosition - ms <= absMotionMin)
    {
        ms = currentPosition - absMotionMin;
        appendLogText(i18n("Moving to minimum focus position %1...", absMotionMin));
    }
    return changeFocus(-ms);
}

bool Focus::focusOut(int ms, int speedFactor)
{
    if (currentPosition == absMotionMax)
    {
        appendLogText(i18n("At maximum focus position %1...", absMotionMax));
        return false;
    }
    focusInB->setEnabled(false);
    focusOutB->setEnabled(false);
    startGotoB->setEnabled(false);
    if (ms <= 0)
        ms = m_OpsFocusMechanics->focusTicks->value();
    ms = m_OpsFocusMechanics->focusTicks->value();

    // apply speed factor
    ms *= speedFactor;

    if (currentPosition + ms >= absMotionMax)
    {
        ms = absMotionMax - currentPosition;
        appendLogText(i18n("Moving to maximum focus position %1...", absMotionMax));
    }
    return changeFocus(ms);
}

// Routine to manage focus movements. All moves are now subject to overscan
// + amount indicates a movement out; - amount indictaes a movement in
bool Focus::changeFocus(int amount, bool updateDir)
{
    // Retry capture if we stay at the same position
    // Allow 1 step of tolerance--Have seen stalls with amount==1.
    if (inAutoFocus && abs(amount) <= 1)
    {
        if (m_RestartState == RESTART_NOW)
            // Restart in progress which is normally triggered by the completion of the focuser
            // reset to its starting position but in this case the focuser is already at its starting
            // position... so create a small movement. As above comment amount=1 can cause a stall, so use 2.
            amount = -2;
        else
        {
            capture(m_OpsFocusMechanics->focusSettleTime->value());
            return true;
        }
    }

    if (m_Focuser == nullptr)
    {
        appendLogText(i18n("Error: No Focuser detected."));
        checkStopFocus(true);
        return false;
    }

    if (m_Focuser->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to Focuser."));
        checkStopFocus(true);
        return false;
    }

    const int newPosition = adjustLinearPosition(currentPosition, currentPosition + amount,
                            m_OpsFocusMechanics->focusAFOverscan->value(),
                            updateDir);
    if (newPosition == currentPosition)
        return true;

    const int newAmount = newPosition - currentPosition;
    const int absNewAmount = abs(newAmount);
    const bool focusingOut = newAmount > 0;
    const QString dirStr = focusingOut ? i18n("outward") : i18n("inward");
    // update the m_LastFocusDirection unless in Iterative / Polynomial which controls this variable itself.
    if (updateDir)
        m_LastFocusDirection = (focusingOut) ? FOCUS_OUT : FOCUS_IN;

    if (focusingOut)
        m_Focuser->focusOut();
    else
        m_Focuser->focusIn();

    // Keep track of motion in case it gets stuck.
    m_FocusMotionTimer.start();

    if (canAbsMove)
    {
        m_LastFocusSteps = newPosition;
        m_Focuser->moveAbs(newPosition);
        appendLogText(i18n("Focusing %2 by %1 steps...", abs(absNewAmount), dirStr));
    }
    else if (canRelMove)
    {
        m_LastFocusSteps = absNewAmount;
        m_Focuser->moveRel(absNewAmount);
        appendLogText(i18np("Focusing %2 by %1 step...", "Focusing %2 by %1 steps...", absNewAmount, dirStr));
    }
    else
    {
        m_LastFocusSteps = absNewAmount;
        m_Focuser->moveByTimer(absNewAmount);
        appendLogText(i18n("Focusing %2 by %1 ms...", absNewAmount, dirStr));
    }

    return true;
}

void Focus::handleFocusMotionTimeout()
{
    // handleFocusMotionTimeout is called when the focus motion timer times out. This is only
    // relevant to AutoFocus runs which could be unattended so make an attempt to recover. Other
    // types of focuser movement issues are logged and the user is expected to take action.
    // If set correctly, say 30 secs, this should only occur when there are comms issues
    // with the focuser.
    // Step 1: Just retry the last requested move. If the issue is a transient one-off issue
    //         this should resolve it. Try this twice.
    // Step 2: Step 1 didn't resolve the issue so try to restart the focus driver. In this case
    //         abort the inflight autofocus run and let it retry from the start. It will try to
    //         return the focuser to the start (last successful autofocus) position. Try twice.
    // Step 3: Step 2 didn't work either because the driver restart wasn't successful or we are
    //         still getting timeouts. In this case just stop the autoFocus process and return
    //         control to either the Scheduer or GUI. Note that here we cannot reset the focuser
    //         to the previous good position so if the focuser miraculously fixes itself the
    //         next autofocus run won't start from the best place.

    if (!inAutoFocus)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "handleFocusMotionTimeout() called while not in AutoFocus";
        return;
    }

    m_FocusMotionTimerCounter++;

    if (m_FocusMotionTimerCounter > 4)
    {
        // We've tried everything but still get timeouts so abort...
        appendLogText(i18n("Focuser is still timing out. Aborting..."));
        stop(Ekos::FOCUS_ABORTED);
        return;
    }
    else if (m_FocusMotionTimerCounter > 2)
    {
        QString focuser = m_Focuser->getDeviceName();
        appendLogText(i18n("Focus motion timed out (%1). Restarting focus driver %2", m_FocusMotionTimerCounter, focuser));
        emit focuserTimedout(focuser);

        QTimer::singleShot(5000, this, [ &, focuser]()
        {
            Focus::reconnectFocuser(focuser);
        });
        return;
    }

    if (!changeFocus(m_LastFocusSteps - currentPosition))
        appendLogText(i18n("Focus motion timed out (%1). Focusing to %2 steps...", m_FocusMotionTimerCounter, m_LastFocusSteps));
}

void Focus::selectImageMask()
{
    const bool useFullField = m_OpsFocusSettings->focusUseFullField->isChecked();
    ImageMaskType masktype;
    if (m_OpsFocusSettings->focusRingMaskRB->isChecked())
        masktype = FOCUS_MASK_RING;
    else if (m_OpsFocusSettings->focusMosaicMaskRB->isChecked())
        masktype = FOCUS_MASK_MOSAIC;
    else
        masktype = FOCUS_MASK_NONE;

    // mask selection only enabled if full field should be used for focusing
    m_OpsFocusSettings->focusRingMaskRB->setEnabled(useFullField);
    m_OpsFocusSettings->focusMosaicMaskRB->setEnabled(useFullField);
    // ring mask
    m_OpsFocusSettings->focusFullFieldInnerRadius->setEnabled(useFullField && masktype == FOCUS_MASK_RING);
    m_OpsFocusSettings->focusFullFieldOuterRadius->setEnabled(useFullField && masktype == FOCUS_MASK_RING);
    // aberration inspector mosaic
    m_OpsFocusSettings->focusMosaicTileWidth->setEnabled(useFullField && masktype == FOCUS_MASK_MOSAIC);
    m_OpsFocusSettings->focusSpacerLabel->setEnabled(useFullField && masktype == FOCUS_MASK_MOSAIC);
    m_OpsFocusSettings->focusMosaicSpace->setEnabled(useFullField && masktype == FOCUS_MASK_MOSAIC);

    // create the type specific mask
    if (masktype == FOCUS_MASK_RING)
        m_FocusView->setImageMask(new ImageRingMask(m_OpsFocusSettings->focusFullFieldInnerRadius->value() / 100.0,
                                  m_OpsFocusSettings->focusFullFieldOuterRadius->value() / 100.0));
    else if (masktype == FOCUS_MASK_MOSAIC)
        m_FocusView->setImageMask(new ImageMosaicMask(m_OpsFocusSettings->focusMosaicTileWidth->value(),
                                  m_OpsFocusSettings->focusMosaicSpace->value()));
    else
        m_FocusView->setImageMask(nullptr);

    checkMosaicMaskLimits();
    m_currentImageMask = masktype;
    startAbInsB->setEnabled(canAbInsStart());
}

void Focus::reconnectFocuser(const QString &focuser)
{
    m_FocuserReconnectCounter++;

    if (m_Focuser && m_Focuser->getDeviceName() == focuser)
    {
        appendLogText(i18n("Attempting to reconnect focuser: %1", focuser));
        initOpticalTrain();
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_ERROR);
        return;
    }

    if (m_FocuserReconnectCounter > 12)
    {
        // We've waited a minute and can't reconnect the focuser so abort...
        appendLogText(i18n("Cannot reconnect focuser: %1. Aborting...", focuser));
        stop(Ekos::FOCUS_ABORTED);
        return;
    }

    QTimer::singleShot(5000, this, [ &, focuser]()
    {
        reconnectFocuser(focuser);
    });
}

void Focus::processData(const QSharedPointer<FITSData> &data)
{
    // Ignore guide head if there is any.
    if (data->property("chip").toInt() == ISD::CameraChip::GUIDE_CCD)
        return;

    if (data)
    {
        m_FocusView->loadData(data);
        m_ImageData = data;
    }
    else
        m_ImageData.reset();

    captureTimeout.stop();
    captureTimeoutCounter = 0;
    m_MissingCameraCounter = 0;

    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    disconnect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Focus::processData);
    disconnect(m_Camera, &ISD::Camera::error, this, &Ekos::Focus::processCaptureError);

    if (m_ImageData && m_OpsFocusSettings->useFocusDarkFrame->isChecked())
    {
        QVariantMap settings = frameSettings[targetChip];
        uint16_t offsetX     = settings["x"].toInt() / settings["binx"].toInt();
        uint16_t offsetY     = settings["y"].toInt() / settings["biny"].toInt();

        m_DarkProcessor->denoise(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()),
                                 targetChip, m_ImageData, focusExposure->value(), offsetX, offsetY);
        return;
    }

    setCaptureComplete();
    resetButtons();
}

void Focus::starDetectionFinished()
{
    m_starDetectInProgress = false;
    if (m_abortInProgress)
    {
        // We are trying to abort Autofocus so do no further processing
        qCDebug(KSTARS_EKOS_FOCUS) << "Abort AF: discarding results from " << __FUNCTION__;
        return;
    }

    appendLogText(i18n("Detection complete."));

    updateMeasurements();
}

// Returns whether the star measure is star based.
// FOCUS_STAR_SOBEL, FOCUS_STAR_LAPLASSIAN & FOCUS_STAR_CANNY work on non-star fields such as lunar fields where there may be no stars
// FOCUS_STAR_STDDEV can be used on star fields and non-star fields and does not require star detection.
bool Focus::isStarMeasureStarBased()
{
    if (m_StarMeasure == FOCUS_STAR_STDDEV || m_StarMeasure == FOCUS_STAR_SOBEL || m_StarMeasure == FOCUS_STAR_LAPLASSIAN
            || m_StarMeasure == FOCUS_STAR_CANNY)
        return false;
    else
        return true;
}
// The image has been processed for star centroids and HFRs so now process it for star FWHMs
void Focus::getFWHM(const QList<Edge *> &stars, double *FWHM, double *weight)
{
    *FWHM = INVALID_STAR_MEASURE;
    *weight = 0.0;

    auto imageBuffer = m_ImageData->getImageBuffer();

    switch (m_ImageData->getStatistics().dataType)
    {
        case TBYTE:
            focusFWHM->processFWHM(reinterpret_cast<uint8_t const *>(imageBuffer), stars, m_ImageData, starFitting, FWHM, weight);
            break;

        case TSHORT: // Don't think short is used as its recorded as unsigned short
            focusFWHM->processFWHM(reinterpret_cast<short const *>(imageBuffer), stars, m_ImageData, starFitting, FWHM, weight);
            break;

        case TUSHORT:
            focusFWHM->processFWHM(reinterpret_cast<unsigned short const *>(imageBuffer), stars, m_ImageData, starFitting, FWHM,
                                   weight);
            break;

        case TLONG:  // Don't think long is used as its recorded as unsigned long
            focusFWHM->processFWHM(reinterpret_cast<long const *>(imageBuffer), stars, m_ImageData, starFitting, FWHM, weight);
            break;

        case TULONG:
            focusFWHM->processFWHM(reinterpret_cast<unsigned long const *>(imageBuffer), stars, m_ImageData, starFitting, FWHM, weight);
            break;

        case TFLOAT:
            focusFWHM->processFWHM(reinterpret_cast<float const *>(imageBuffer), stars, m_ImageData, starFitting, FWHM, weight);
            break;

        case TLONGLONG:
            focusFWHM->processFWHM(reinterpret_cast<long long const *>(imageBuffer), stars, m_ImageData, starFitting, FWHM, weight);
            break;

        case TDOUBLE:
            focusFWHM->processFWHM(reinterpret_cast<double const *>(imageBuffer), stars, m_ImageData, starFitting, FWHM, weight);
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << "Unknown image buffer datatype " << m_ImageData->getStatistics().dataType <<
                                       " Cannot calc FWHM";
            break;
    }
}

// The image has been processed for star centroids and HFRs so now process it for star FWHMs
void Focus::getFourierPower(double *fourierPower, double *weight, const int mosaicTile)
{
    *fourierPower = INVALID_STAR_MEASURE;
    *weight = 1.0;

    auto imageBuffer = m_ImageData->getImageBuffer();

    switch (m_ImageData->getStatistics().dataType)
    {
        case TBYTE:
            focusFourierPower->processFourierPower(reinterpret_cast<uint8_t const *>(imageBuffer), m_ImageData,
                                                   m_FocusView->imageMask(), mosaicTile, fourierPower, weight);
            break;

        case TSHORT: // Don't think short is used as its recorded as unsigned short
            focusFourierPower->processFourierPower(reinterpret_cast<short const *>(imageBuffer), m_ImageData,
                                                   m_FocusView->imageMask(), mosaicTile, fourierPower, weight);
            break;

        case TUSHORT:
            focusFourierPower->processFourierPower(reinterpret_cast<unsigned short const *>(imageBuffer), m_ImageData,
                                                   m_FocusView->imageMask(), mosaicTile, fourierPower, weight);
            break;

        case TLONG:  // Don't think long is used as its recorded as unsigned long
            focusFourierPower->processFourierPower(reinterpret_cast<long const *>(imageBuffer), m_ImageData,
                                                   m_FocusView->imageMask(), mosaicTile, fourierPower, weight);
            break;

        case TULONG:
            focusFourierPower->processFourierPower(reinterpret_cast<unsigned long const *>(imageBuffer), m_ImageData,
                                                   m_FocusView->imageMask(), mosaicTile, fourierPower, weight);
            break;

        case TFLOAT:
            focusFourierPower->processFourierPower(reinterpret_cast<float const *>(imageBuffer), m_ImageData,
                                                   m_FocusView->imageMask(), mosaicTile, fourierPower, weight);
            break;

        case TLONGLONG:
            focusFourierPower->processFourierPower(reinterpret_cast<long long const *>(imageBuffer), m_ImageData,
                                                   m_FocusView->imageMask(), mosaicTile, fourierPower, weight);
            break;

        case TDOUBLE:
            focusFourierPower->processFourierPower(reinterpret_cast<double const *>(imageBuffer), m_ImageData,
                                                   m_FocusView->imageMask(), mosaicTile, fourierPower, weight);
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << "Unknown image buffer datatype " << m_ImageData->getStatistics().dataType <<
                                       " Cannot calc Fourier Power";
            break;
    }
}

// The image has been processed for star centroids and HFRs so now process it for the Laplassian factor
void Focus::getBlurriness(const StarMeasure starMeasure, const bool denoise, double *blurriness, double *weight,
                          const QRect &roi, const int mosaicTile)
{
    *blurriness = INVALID_STAR_MEASURE;
    *weight = 1.0;

#if defined(HAVE_OPENCV)

    auto imageBuffer = m_ImageData->getImageBuffer();

    switch (m_ImageData->getStatistics().dataType)
    {
        case TBYTE:
            focusBlurriness->processBlurriness(reinterpret_cast<uint8_t const *>(imageBuffer), m_ImageData, denoise,
                                               m_FocusView->imageMask(), mosaicTile, roi, starMeasure, blurriness, weight);
            break;

        case TSHORT: // Don't think short is used as its recorded as unsigned short
            focusBlurriness->processBlurriness(reinterpret_cast<short const *>(imageBuffer), m_ImageData, denoise,
                                               m_FocusView->imageMask(), mosaicTile, roi, starMeasure, blurriness, weight);
            break;

        case TUSHORT:
            focusBlurriness->processBlurriness(reinterpret_cast<unsigned short const *>(imageBuffer), m_ImageData, denoise,
                                               m_FocusView->imageMask(), mosaicTile, roi, starMeasure, blurriness, weight);
            break;

        case TLONG:  // Don't think long is used as its recorded as unsigned long
            focusBlurriness->processBlurriness(reinterpret_cast<long const *>(imageBuffer), m_ImageData, denoise,
                                               m_FocusView->imageMask(), mosaicTile, roi, starMeasure, blurriness, weight);
            break;

        case TULONG:
            focusBlurriness->processBlurriness(reinterpret_cast<unsigned long const *>(imageBuffer), m_ImageData, denoise,
                                               m_FocusView->imageMask(), mosaicTile, roi, starMeasure, blurriness, weight);
            break;

        case TFLOAT:
            focusBlurriness->processBlurriness(reinterpret_cast<float const *>(imageBuffer), m_ImageData, denoise,
                                               m_FocusView->imageMask(), mosaicTile, roi, starMeasure, blurriness, weight);
            break;

        case TLONGLONG:
            focusBlurriness->processBlurriness(reinterpret_cast<long long const *>(imageBuffer), m_ImageData, denoise,
                                               m_FocusView->imageMask(), mosaicTile, roi, starMeasure, blurriness, weight);
            break;

        case TDOUBLE:
            focusBlurriness->processBlurriness(reinterpret_cast<double const *>(imageBuffer), m_ImageData, denoise,
                                               m_FocusView->imageMask(), mosaicTile, roi, starMeasure, blurriness, weight);
            break;

        default:
            qCDebug(KSTARS_EKOS_FOCUS) << "Unknown image buffer datatype " << m_ImageData->getStatistics().dataType <<
                                       " Cannot calc Blurriness";
            break;
    }
#else
    Q_UNUSED(starMeasure);
    Q_UNUSED(denoise);
    Q_UNUSED(roi);
    Q_UNUSED(mosaicTile);
    // This shouldn't occur in normal operation
    qCDebug(KSTARS_EKOS_FOCUS) << "getBlurriness called when openCV not installed";
#endif
}

double Focus::calculateStarWeight(const bool useWeights, const std::vector<double> values)
{
    if (!useWeights || values.size() <= 0)
        // If we can't calculate weight set to 1 = equal weights
        // Also if we are using numStars as the measure - don't use weights
        return 1.0f;

    return Mathematics::RobustStatistics::ComputeWeight(m_ScaleCalc, values);
}

void Focus::analyzeSources()
{
    appendLogText(i18n("Detecting sources..."));
    m_starDetectInProgress = true;

    QVariantMap extractionSettings;
    extractionSettings["optionsProfileIndex"] = m_OpsFocusProcess->focusSEPProfile->currentIndex();
    extractionSettings["optionsProfileGroup"] =  static_cast<int>(Ekos::FocusProfiles);
    m_ImageData->setSourceExtractorSettings(extractionSettings);
    // When we're using FULL field view, we always use either CENTROID algorithm which is the default
    // standard algorithm in KStars, or SEP. The other algorithms are too inefficient to run on full frames and require
    // a bounding box for them to be effective in near real-time application.
    if (m_OpsFocusSettings->focusUseFullField->isChecked())
    {
        m_FocusView->setTrackingBoxEnabled(false);

        if (m_FocusDetection != ALGORITHM_CENTROID && m_FocusDetection != ALGORITHM_SEP)
            m_StarFinderWatcher.setFuture(m_ImageData->findStars(ALGORITHM_CENTROID));
        else
            m_StarFinderWatcher.setFuture(m_ImageData->findStars(m_FocusDetection));
    }
    else
    {
        QRect searchBox = m_FocusView->isTrackingBoxEnabled() ? m_FocusView->getTrackingBox() : QRect();
        // If star is already selected then use whatever algorithm currently selected.
        if (starSelected)
        {
            m_StarFinderWatcher.setFuture(m_ImageData->findStars(m_FocusDetection, searchBox));
        }
        else
        {
            // Disable tracking box
            m_FocusView->setTrackingBoxEnabled(false);

            // If algorithm is set something other than Centeroid or SEP, then force Centroid
            // Since it is the most reliable detector when nothing was selected before.
            if (m_FocusDetection != ALGORITHM_CENTROID && m_FocusDetection != ALGORITHM_SEP)
                m_StarFinderWatcher.setFuture(m_ImageData->findStars(ALGORITHM_CENTROID));
            else
                // Otherwise, continue to find use using the selected algorithm
                m_StarFinderWatcher.setFuture(m_ImageData->findStars(m_FocusDetection, searchBox));
        }
    }
}

bool Focus::appendMeasure()
{
    CaptureHistory::FrameData frameData = calculateCurrentMeasureAndWeight();
    double newMeasure = frameData.measure;

    // Add new star measure (e.g. HFR, FWHM, etc) to existing values, even if invalid
    starMeasureFrames.append(newMeasure);

    // Prepare a work vector with valid HFR values
    QVector <double> samples(starMeasureFrames);
    samples.erase(std::remove_if(samples.begin(), samples.end(), [](const double measure)
    {
        return measure == INVALID_STAR_MEASURE;
    }), samples.end());

    // Consolidate the average star measure. Sigma clips outliers and averages remainder.
    if (!samples.isEmpty())
    {
        double currentMeasure = Mathematics::RobustStatistics::ComputeLocation(
                                    Mathematics::RobustStatistics::LOCATION_SIGMACLIPPING, std::vector<double>(samples.begin(), samples.end()));

        frameData.measure = currentMeasure;
        switch(m_StarMeasure)
        {
            case FOCUS_STAR_HFR:
            case FOCUS_STAR_HFR_ADJ:
                frameData.hfr = currentMeasure;
                break;
            case FOCUS_STAR_FWHM:
                frameData.fwhm = currentMeasure;
                break;
            case FOCUS_STAR_NUM_STARS:
                frameData.numStars = currentMeasure;
                break;
            case FOCUS_STAR_FOURIER_POWER:
                frameData.fourierPower = currentMeasure;
                break;
            case FOCUS_STAR_STDDEV:
            case FOCUS_STAR_SOBEL:
            case FOCUS_STAR_LAPLASSIAN:
            case FOCUS_STAR_CANNY:
                frameData.blurriness = currentMeasure;
                break;
            default:
                break;
        }

    }

    // Save the focus frame
    frameData.filename = saveFocusFrame();
    // clone the stars since they get cleared
    for (Edge* star : m_ImageData->getStarCenters())
    {
        QSharedPointer<Edge> clonedStar;
        clonedStar.reset(star->clone());
        frameData.starCenters.append(clonedStar);
    }

    // do not keep the history while looping
    if (inFocusLoop)
        captureHistory(lastAFRun()).reset();
    else if (!inAutoFocus && !inSingleCaptureMode)
    {
        // track the history of single frames
        m_FocusView->addRun();
        inSingleCaptureMode = true;
        // initialize the HFR view
        clearDataPoints();
    }

    // update the history
    m_FocusView->addFrame(frameData);

    // Return whether we need more frame based on user requirement
    int framesCount = m_OpsFocusProcess->focusFramesCount->value();
    if ((minimumRequiredHFR > 0) || (inAutoFocus && !inBuildOffsets && !inScanStartPos && !focusAdvisor->inFocusAdvisor()
                                     && m_FocusAlgorithm == FOCUS_LINEAR1PASS
                                     && linearFocuser && !linearFocuser->isInFirstPass()))
        // If in-sequence HFR Check or L1P autofocus and doing the last (in focus) datapoint use focusHFRFramesCount
        framesCount = m_OpsFocusProcess->focusHFRFramesCount->value();

    return starMeasureFrames.count() < framesCount;
}

void Focus::settle(const FocusState completionState, const bool autoFocusUsed, const bool buildOffsetsUsed,
                   const AutofocusFailReason failCode, const QString failCodeInfo)
{
    if (autoFocusUsed && failCode != FOCUS_FAIL_OPTIMISED_OUT)
    {
        if (completionState == Ekos::FOCUS_COMPLETE)
        {
            if (failCode != FOCUS_FAIL_ADVISOR_COMPLETE)
                KSNotification::event(QLatin1String("FocusSuccessful"), i18n("Autofocus operation completed successfully"),
                                      KSNotification::Focus);
            // Pass consistent Autofocus temperature to analyze
            if (m_FocusAlgorithm == FOCUS_LINEAR1PASS && curveFitting != nullptr)
                emit autofocusComplete(m_LastSourceAutofocusTemperature, filter(), getAnalyzeData(),
                                       m_OpsFocusProcess->focusUseWeights->isChecked(),
                                       curveFitting->serialize(), linearFocuser->getTextStatus(R2));
            else
                emit autofocusComplete(m_LastSourceAutofocusTemperature, filter(), getAnalyzeData(),
                                       m_OpsFocusProcess->focusUseWeights->isChecked());
        }
        else
        {
            if (failCode != FOCUS_FAIL_ADVISOR_COMPLETE)
                KSNotification::event(QLatin1String("FocusFailed"), i18n("Autofocus operation failed"),
                                      KSNotification::Focus, KSNotification::Alert);
            emit autofocusAborted(filter(), getAnalyzeData(), m_OpsFocusProcess->focusUseWeights->isChecked(), failCode, failCodeInfo);
        }
    }

    if (completionState == Ekos::FOCUS_COMPLETE)
    {
        if (autoFocusUsed && fallbackFilterPending)
        {
            // Save the solution details for the filter used for the AF run before changing
            // filers to the fallback filter. Details for the fallback filter will be saved once that
            // filter has been processed and the offset applied
            m_FilterManager->setFilterAbsoluteFocusDetails(focusFilter->currentIndex(), currentPosition,
                    m_LastSourceAutofocusTemperature, m_LastSourceAutofocusAlt);
        }
    }

    // Delay state notification if we have a locked filter pending return to original filter
    if (fallbackFilterPending)
    {
        m_pendingState = completionState;
        FilterManager::FilterPolicy policy = static_cast<FilterManager::FilterPolicy>(FilterManager::CHANGE_POLICY |
                                             FilterManager::OFFSET_POLICY);
        m_FilterManager->setFilterPosition(fallbackFilterPosition, policy);
    }
    else
        setState(completionState, failCode != FOCUS_FAIL_OPTIMISED_OUT);

    if (autoFocusUsed && buildOffsetsUsed)
        // If we are building filter offsets signal AF run is complete
        m_FilterManager->autoFocusComplete(completionState, currentPosition, m_LastSourceAutofocusTemperature,
                                           m_LastSourceAutofocusAlt);

    resetButtons();
}

QString Focus::getAnalyzeData()
{
    QString analysis_results = "";

    for (int i = 0; i < plot_position.size(); ++i)
    {
        analysis_results.append(QString("%1%2|%3|%4|%5")
                                .arg(i == 0 ? "" : "|" )
                                .arg(QString::number(plot_position[i], 'f', 0))
                                .arg(QString::number(plot_value[i], 'f', 3))
                                .arg(QString::number(plot_weight[i], 'f', 3))
                                .arg(QString::number(plot_outlier[i])));
    }
    return analysis_results;
}

void Focus::completeFocusProcedure(FocusState completionState, AutofocusFailReason failCode, QString failCodeInfo,
                                   bool plot)
{
    // On Advisor complete or Optimised out, Autofocus wasn't run so don't update values / modules as per normal
    if (inAutoFocus && failCode != FOCUS_FAIL_ADVISOR_COMPLETE && failCode != FOCUS_FAIL_OPTIMISED_OUT)
    {
        // Update the plot vectors (used by Analyze)
        updatePlotPosition();

        if (completionState == Ekos::FOCUS_COMPLETE)
        {
            // Usually we'll record the AF information except for certain FocusAdvisor activities that are not complete AF runs
            if (plot)
                emit redrawHFRPlot(polynomialFit.get(), currentPosition, lastFrame().hfr);

            appendLogText(i18np("Focus procedure completed after %1 iteration.",
                                "Focus procedure completed after %1 iterations.", plot_position.count()));

            setLastFocusTemperature();
            setLastFocusAlt();
            resetAdaptiveFocus(m_OpsFocusSettings->focusAdaptive->isChecked());

            // CR add auto focus position, temperature and filter to log in CSV format
            // this will help with setting up focus offsets and temperature compensation
            qCInfo(KSTARS_EKOS_FOCUS) << "Autofocus values: position," << currentPosition << ", temperature,"
                                      << m_LastSourceAutofocusTemperature << ", filter," << filter()
                                      << ", HFR," << lastFrame().hfr << ", altitude," << m_LastSourceAutofocusAlt;

            if (m_FocusAlgorithm == FOCUS_POLYNOMIAL)
            {
                // Add the final polynomial values to the signal sent to Analyze.
                plot_position.append(currentPosition);
                plot_value.append(lastFrame().hfr);
                plot_weight.append(1.0);
                plot_outlier.append(false);
            }

            appendFocusLogText(QString("%1, %2, %3, %4, %5\n")
                               .arg(QString::number(currentPosition))
                               .arg(QString::number(m_LastSourceAutofocusTemperature, 'f', 1))
                               .arg(filter())
                               .arg(QString::number(lastFrame().hfr, 'f', 3))
                               .arg(QString::number(m_LastSourceAutofocusAlt, 'f', 1)));
            // Replace user position with optimal position
            absTicksSpin->setValue(currentPosition);
        }
        // In case of failure, go back to last position if the focuser is absolute
        else if (canAbsMove && initialFocuserAbsPosition >= 0 && resetFocusIteration <= MAXIMUM_RESET_ITERATIONS &&
                 m_RestartState != RESTART_ABORT)
        {
            // If we're doing in-sequence focusing using an absolute focuser, retry focusing once, starting from last known good position
            bool const retry_focusing = m_RestartState == RESTART_NONE &&
                                        (failCode == Ekos::FOCUS_FAIL_ADVISOR_RERUN || ++resetFocusIteration < MAXIMUM_RESET_ITERATIONS);

            // If retrying, before moving, reset focus frame in case the star in subframe was lost
            if (retry_focusing)
            {
                m_RestartState = RESTART_NOW;
                resetFrame();
            }

            resetFocuser();

            // Bypass the rest of the function if we retry - we will fail if we could not move the focuser
            if (retry_focusing)
            {
                emit autofocusAborted(filter(), getAnalyzeData(), m_OpsFocusProcess->focusUseWeights->isChecked(), failCode, failCodeInfo);
                return;
            }
            else
            {
                // We're in Autofocus and we've hit our max retry limit, so...
                // resetFocuser will have initiated a focuser reset back to its starting position
                // so we need to wait for that move to complete before returning control.
                // This is important if the scheduler is running autofocus as it will likely
                // immediately retry. The startup process will take the current focuser position
                // as the start position and so the focuser needs to have arrived at its starting
                // position before this. So set m_RestartState to log this.
                m_RestartState = RESTART_ABORT;
                return;
            }
        }

        // Reset the retry count on success or maximum count
        resetFocusIteration = 0;
    }

    const bool autoFocusUsed = inAutoFocus;
    const bool inBuildOffsetsUsed = inBuildOffsets;

    // Refresh display if needed
    if (m_FocusAlgorithm == FOCUS_POLYNOMIAL && plot)
        emit drawPolynomial(polynomialFit.get(), isVShapeSolution, true);

    // Enforce settling duration. Note stop resets m_GuidingSuspended
    int const settleTime = m_GuidingSuspended ? m_OpsFocusSettings->focusGuideSettleTime->value() : 0;

    // Reset the autofocus flags
    stop(completionState);

    if (settleTime > 0)
        appendLogText(i18n("Settling for %1s...", settleTime));

    QTimer::singleShot(settleTime * 1000, this, [ &, settleTime, completionState, autoFocusUsed, inBuildOffsetsUsed, failCode,
                          failCodeInfo]()
    {
        settle(completionState, autoFocusUsed, inBuildOffsetsUsed, failCode, failCodeInfo);

        if (settleTime > 0)
            appendLogText(i18n("Settling complete."));
    });
}

void Focus::resetFocuser()
{
    // If we are able to and need to, move the focuser back to the initial position and let the procedure restart from its termination
    if (m_Focuser && m_Focuser->isConnected() && initialFocuserAbsPosition >= 0)
    {
        // HACK: If the focuser will not move, cheat a little to get the notification - see processNumber
        if (currentPosition == initialFocuserAbsPosition)
            currentPosition--;

        appendLogText(i18n("Autofocus failed, moving back to initial focus position %1.", initialFocuserAbsPosition));
        changeFocus(initialFocuserAbsPosition - currentPosition);
        /* Restart will be executed by the end-of-move notification from the device if needed by resetFocus */
    }
}

void Focus::updateMeasurements()
{
    // Let's now report the current HFR
    qCDebug(KSTARS_EKOS_FOCUS) << "Focus newFITS #" << starMeasureFrames.count() + 1 << ": Current HFR " <<
                               lastFrame().hfr <<
                               " Num stars "
                               << (starSelected ? 1 : lastFrame().numStars);

    // Take the new HFR into account, eventually continue to stack samples
    if (appendMeasure())
    {
        capture();
        return;
    }
    else starMeasureFrames.clear();

    // Let signal the current HFR now depending on whether the focuser is absolute or relative
    // Outside of Focus we continue to rely on HFR and independent of which measure the user selected we always calculate HFR
    if (canAbsMove)
        emit newHFR(lastFrame().hfr, currentPosition, inAutoFocus, opticalTrain());
    else
        emit newHFR(lastFrame().hfr, -1, inAutoFocus, opticalTrain());

    // update display of the measured values
    refreshMeasuresDisplay();
    // Display message in case _last_ HFR was invalid
    if (lastHFR == INVALID_STAR_MEASURE)
        appendLogText(i18n("FITS received. No stars detected."));

    // If we have a valid HFR value
    if (!isStarMeasureStarBased() || lastFrame().hfr > 0)
    {
        // Check if we're done from polynomial fitting algorithm
        if (m_FocusAlgorithm == FOCUS_POLYNOMIAL && isVShapeSolution)
        {
            completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_NONE);
            return;
        }

        Edge selectedHFRStarHFR = m_ImageData->getSelectedHFRStar();

        // Center tracking box around selected star (if it valid) either in:
        // 1. Autofocus
        // 2. CheckFocus (minimumHFRCheck)
        // The starCenter _must_ already be defined, otherwise, we proceed until
        // the latter half of the function searches for a star and define it.
        if (starCenter.isNull() == false && (inAutoFocus || minimumRequiredHFR >= 0))
        {
            // Now we have star selected in the frame
            starSelected = true;
            starCenter.setX(qMax(0, static_cast<int>(selectedHFRStarHFR.x)));
            starCenter.setY(qMax(0, static_cast<int>(selectedHFRStarHFR.y)));

            syncTrackingBoxPosition();

            // Record the star information (X, Y, lastFrame().currentHFR)
            QVector3D oneStar = starCenter;
            oneStar.setZ(lastFrame().hfr);
            starsHFR.append(oneStar);
        }
        else
        {
            // Record the star information (X, Y, lastFrame().currentHFR)
            QVector3D oneStar(starCenter.x(), starCenter.y(), lastFrame().hfr);
            starsHFR.append(oneStar);
        }

        if (lastFrame().hfr > maxHFR)
            maxHFR = lastFrame().hfr;

        // Append point to the #Iterations vs #HFR chart in case of looping or in case in autofocus with a focus
        // that does not support position feedback.

        // If inAutoFocus is true without canAbsMove and without canRelMove, canTimerMove must be true.
        // We'd only want to execute this if the focus linear algorithm is not being used, as that
        // algorithm simulates a position-based system even for timer-based focusers.
        if (inFocusLoop || inSingleCaptureMode || (inAutoFocus && ! isPositionBased()))
        {
            int pos = plot_position.empty() ? 1 : plot_position.last() + 1;
            addPlotPosition(pos, getLastMeasure());
        }
    }
    else
    {
        // Let's record an invalid star result
        QVector3D oneStar(starCenter.x(), starCenter.y(), INVALID_STAR_MEASURE);
        starsHFR.append(oneStar);
    }

    // First check that we haven't already search for stars
    // Since star-searching algorithm are time-consuming, we should only search when necessary
    m_FocusView->updateFrame();

    setHFRComplete();

    if (m_abInsOn && !inScanStartPos && !focusAdvisor->inFocusAdvisor())
        calculateAbInsData();
}

// Save off focus frame during Autofocus for later debugging
QString Ekos::Focus::focusFramePath()
{
    QDateTime now = KStarsData::Instance()->lt();
    return QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("autofocus/" +
            now.toString("yyyy-MM-dd"));
}

QString Focus::saveFocusFrame()
{
    QString filename = "";
    if (!inFocusLoop && Options::maxFocusFrameFiles() != 0)
    {
        QDir dir;
        QString path = focusFramePath();
        dir.mkpath(path);

        // To help identify focus frames add run number, step and frame (for multiple frames at each step)
        // If FocusAdvisor is manipuling Autofocus then annote frame appropriately
        QString prefix;
        if (focusAdvisor->inFocusAdvisor())
            prefix = focusAdvisor->getFocusFramePrefix();
        else if (inScanStartPos)
            prefix = QString("_ssp_%1_%2").arg(lastAFRun()).arg(absIterations + 1);
        else if (m_FocusAlgorithm == FOCUS_LINEAR1PASS && linearFocuser)
        {
            const int currentStep = linearFocuser->currentStep() + 1;
            prefix = QString("_%1_%2").arg(lastAFRun()).arg(currentStep);
        }

        if (!prefix.isEmpty())
            // Add the frame count
            prefix.append(QString("_%1").arg(starMeasureFrames.count()));

        // IS8601 contains colons but they are illegal under Windows OS, so replacing them with '-'
        // The timestamp is no longer ISO8601 but it should solve interoperality issues between different OS hosts
        QDateTime now    = KStarsData::Instance()->lt();
        QString  name    = "autofocus_frame_" + now.toString("HH-mm-ss") + prefix + ".fits";
        filename = path + QStringLiteral("/") + name;
        m_ImageData->saveImage(filename);
        // keep the autofocus file count limit
        if (Options::maxFocusFrameFiles() > 0)
            enforceMaxFilesRecursive(QFileInfo(path).dir().absolutePath(), Options::maxFocusFrameFiles());
    }
    return filename;
}

int Focus::enforceMaxFilesRecursive(const QString &path, int maxCount)
{
    QFileInfoList files;
    // recursive search for files
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        it.next();
        files << it.fileInfo();
    }

    if (files.size() <= maxCount)
        return 0;

    // oldest files first
    std::sort(files.begin(), files.end(),
              [](const QFileInfo & a, const QFileInfo & b)
    {
        return a.lastModified() < b.lastModified();
    });

    int removed = 0;
    for (int i = 0; i < files.size() - maxCount; ++i)
    {
        const QString filePath = files[i].absoluteFilePath();
        if (QFile::remove(filePath))
        {
            ++removed;
            // clear empty directory
            const QString dirPath  = files[i].absolutePath();
            QDir dir(dirPath);
            if (dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty())
            {
                if (dir.rmdir(dirPath))
                    qCInfo(KSTARS_EKOS_FOCUS) << "Empty directory removed:" << dirPath;
                else
                    qCDebug(KSTARS_EKOS_FOCUS) << "Cannot remove empty directory:" << dirPath;

            }
        }
        else
            qCWarning(KSTARS_EKOS_FOCUS) << "Deleting focus frame failed: " << filePath;

    }
    return removed;

}

void Focus::calculateAbInsData()
{
    ImageMosaicMask *mosaicmask = dynamic_cast<ImageMosaicMask *>(m_FocusView->imageMask().get());
    const QVector<QRect> tiles = mosaicmask->tiles();
    auto stars = m_ImageData->getStarCenters();
    QVector<QList<Edge * >> tileStars(NUM_TILES);

    for (int star = 0; star < stars.count(); star++)
    {
        const int x = stars[star]->x;
        const int y = stars[star]->y;
        for (int tile = 0; tile < NUM_TILES; tile++)
        {
            QRect thisTile = tiles[tile];
            if (thisTile.contains(x, y))
            {
                tileStars[tile].append(stars[star]);
                break;
            }
        }
    }

    // Get the measure for each tile
    for (int tile = 0; tile < tileStars.count(); tile++)
    {
        double measure, weight;
        std::vector<double> HFRs;

        switch (m_StarMeasure)
        {
            case FOCUS_STAR_HFR:
            case FOCUS_STAR_HFR_ADJ:
                for (int star = 0; star < tileStars[tile].count(); star++)
                {
                    HFRs.push_back(tileStars[tile][star]->HFR);
                }
                measure = Mathematics::RobustStatistics::ComputeLocation(Mathematics::RobustStatistics::LOCATION_SIGMACLIPPING, HFRs, 2);
                weight = calculateStarWeight(m_OpsFocusProcess->focusUseWeights->isChecked(), HFRs);
                break;
            case FOCUS_STAR_FWHM:
                getFWHM(tileStars[tile], &measure, &weight);
                break;
            case FOCUS_STAR_NUM_STARS:
                measure = tileStars[tile].count();
                weight = 1.0;
                break;
            case FOCUS_STAR_FOURIER_POWER:
                getFourierPower(&measure, &weight, tile);
                break;
            case FOCUS_STAR_STDDEV:
            case FOCUS_STAR_SOBEL:
            case FOCUS_STAR_LAPLASSIAN:
            case FOCUS_STAR_CANNY:
                getBlurriness(m_StarMeasure, m_OpsFocusProcess->focusDenoise->isChecked(), &measure, &weight, QRect(), tile);
                break;
            default:
                qCDebug(KSTARS_EKOS_FOCUS) << "Unable to calculate Aberration Inspector data";
                return;
        }

        m_abInsMeasure[tile].append(measure);
        m_abInsWeight[tile].append(weight);
        m_abInsNumStars[tile].append(tileStars[tile].count());

        if (!linearFocuser->isInFirstPass())
        {
            // This is the last datapoint so calculate average star position in the tile from the tile center
            // FOCUS_STAR_FOURIER_POWER doesn't use stars directly so no need to calculate offset. The other
            // measures all use stars: FOCUS_STAR_NUM_STARS, FOCUS_STAR_FWHM, FOCUS_STAR_HFR, FOCUS_STAR_HFR_ADJ
            int xAv = 0, yAv = 0;
            if (m_StarMeasure != FOCUS_STAR_FOURIER_POWER)
            {
                QPoint tileCenter = tiles[tile].center();
                int xSum = 0.0, ySum = 0.0;
                for (int star = 0; star < tileStars[tile].count(); star++)
                {
                    xSum += tileStars[tile][star]->x - tileCenter.x();
                    ySum += tileStars[tile][star]->y - tileCenter.y();
                }

                xAv = (tileStars[tile].count() <= 0) ? 0 : xSum / tileStars[tile].count();
                yAv = (tileStars[tile].count() <= 0) ? 0 : ySum / tileStars[tile].count();
            }
            m_abInsTileCenterOffset.append(QPoint(xAv, yAv));
        }
    }
    m_abInsPosition.append(currentPosition);
}

void Focus::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    // If we have a box, sync the bounding box to its position.
    syncTrackingBoxPosition();

    // if we're not looping
    if (inFocusLoop == false)
    {
        // notify user
        appendLogText(i18n("Image received."));
        // show the history navigation panel unless no frames are stored
        refreshHistoryNavigation(Options::maxFocusFrameFiles() != 0, true);
    }

    if (m_captureInProgress && inFocusLoop == false && inAutoFocus == false)
        m_Camera->setUploadMode(rememberUploadMode);

    if (m_RememberCameraFastExposure && inFocusLoop == false && inAutoFocus == false)
    {
        m_RememberCameraFastExposure = false;
        m_Camera->setFastExposureEnabled(true);
    }

    m_captureInProgress = false;
    if (m_abortInProgress)
    {
        // We are trying to abort autofocus so do no further processing.
        // Note this will cover both the original capture and any dark frame processing
        qCDebug(KSTARS_EKOS_FOCUS) << "Abort AF: discarding frame results in " << __FUNCTION__;
        return;
    }

    // update the limits from the real values
    checkMosaicMaskLimits();

    // Emit the whole image
    emit newImage(m_FocusView);
    // Emit the tracking (bounding) box view. Used in Summary View
    emit newStarPixmap(m_FocusView->getTrackingBoxPixmap(10));

    // If we are not looping; OR
    // If we are looping but we already have tracking box enabled; OR
    // If we are asked to analyze _all_ the stars within the field
    // THEN let's find stars in the image and get current HFR
    // Unless the Star Measure assumes a non-star field, e.g. lunar
    // in which case circumvent star detection
    if (!isStarMeasureStarBased())
        starDetectionFinished();
    else if (inFocusLoop == false || (inFocusLoop && (m_FocusView->isTrackingBoxEnabled()
                                      || m_OpsFocusSettings->focusUseFullField->isChecked())))
        analyzeSources();
    else
        setHFRComplete();
}

void Focus::setHFRComplete()
{
    // If we are just framing, let's capture again
    if (inFocusLoop)
    {
        capture();
        return;
    }

    // Get target chip
    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);

    // Get target chip binning
    int subBinX = 1, subBinY = 1;
    if (!targetChip->getBinning(&subBinX, &subBinY))
        qCDebug(KSTARS_EKOS_FOCUS) << "Warning: target chip is reporting no binning property, using 1x1.";

    if (m_OpsFocusSettings->focusSubFrame->isChecked() && !isStarMeasureStarBased() && !starSelected)
    {
        appendLogText(i18n("Capture complete. Select a region of interest."));

        syncTrackingBoxPosition();

        // Now we wait
        setState(Ekos::FOCUS_WAITING);

        // If the user does not select for a timeout period, we abort.
        waitStarSelectTimer.start();
        return;
    }
    else if (m_OpsFocusSettings->focusSubFrame->isChecked() && isStarMeasureStarBased() && starCenter.isNull())
    {
        // If star is NOT yet selected in a non-full-frame situation
        // then let's now try to find the star. This step is skipped for full frames
        // since there isn't a single star to select as we are only interested in the overall average HFR.
        // We need to check if we can find the star right away, or if we need to _subframe_ around the
        // selected star.
        int x = 0, y = 0, w = 0, h = 0;

        // Let's get the stored frame settings for this particular chip
        if (frameSettings.contains(targetChip))
        {
            QVariantMap settings = frameSettings[targetChip];
            x                    = settings["x"].toInt();
            y                    = settings["y"].toInt();
            w                    = settings["w"].toInt();
            h                    = settings["h"].toInt();
        }
        else
            // Otherwise let's get the target chip frame coordinates.
            targetChip->getFrame(&x, &y, &w, &h);

        // In case auto star is selected.
        if (m_OpsFocusSettings->focusAutoStarEnabled->isChecked())
        {
            // Do we have a valid star detected?
            const Edge selectedHFRStar = m_ImageData->getSelectedHFRStar();

            if (selectedHFRStar.x == -1)
            {
                appendLogText(i18n("Failed to automatically select a star. Please select a star manually."));

                // Center the tracking box in the frame and display it
                m_FocusView->setTrackingBox(QRect(w - m_OpsFocusSettings->focusBoxSize->value() / (subBinX * 2),
                                                  h - m_OpsFocusSettings->focusBoxSize->value() / (subBinY * 2),
                                                  m_OpsFocusSettings->focusBoxSize->value() / subBinX, m_OpsFocusSettings->focusBoxSize->value() / subBinY));
                m_FocusView->setTrackingBoxEnabled(true);

                // Use can now move it to select the desired star
                setState(Ekos::FOCUS_WAITING);

                // Start the wait timer so we abort after a timeout if the user does not make a choice
                waitStarSelectTimer.start();

                return;
            }

            // set the tracking box on selectedHFRStar
            starCenter.setX(selectedHFRStar.x);
            starCenter.setY(selectedHFRStar.y);
            starCenter.setZ(subBinX);
            starSelected = true;
            syncTrackingBoxPosition();

            // Do we need to subframe?
            if (subFramed == false && isFocusSubFrameEnabled() && m_OpsFocusSettings->focusSubFrame->isChecked())
            {
                int offset = (static_cast<double>(m_OpsFocusSettings->focusBoxSize->value()) / subBinX) * 1.5;
                int subX   = (selectedHFRStar.x - offset) * subBinX;
                int subY   = (selectedHFRStar.y - offset) * subBinY;
                int subW   = offset * 2 * subBinX;
                int subH   = offset * 2 * subBinY;

                int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
                targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

                // Try to limit the subframed selection
                if (subX < minX)
                    subX = minX;
                if (subY < minY)
                    subY = minY;
                if ((subW + subX) > maxW)
                    subW = maxW - subX;
                if ((subH + subY) > maxH)
                    subH = maxH - subY;

                // Now we store the subframe coordinates in the target chip frame settings so we
                // reuse it later when we capture again.
                QVariantMap settings = frameSettings[targetChip];
                settings["x"]        = subX;
                settings["y"]        = subY;
                settings["w"]        = subW;
                settings["h"]        = subH;
                settings["binx"]     = subBinX;
                settings["biny"]     = subBinY;

                qCDebug(KSTARS_EKOS_FOCUS) << "Frame is subframed. X:" << subX << "Y:" << subY << "W:" << subW << "H:" << subH << "binX:" <<
                                           subBinX << "binY:" << subBinY;

                starsHFR.clear();

                frameSettings[targetChip] = settings;

                // Set the star center in the center of the subframed coordinates
                starCenter.setX(subW / (2 * subBinX));
                starCenter.setY(subH / (2 * subBinY));
                starCenter.setZ(subBinX);

                subFramed = true;

                m_FocusView->setFirstLoad(true);

                // Now let's capture again for the actual requested subframed image.
                capture();
                return;
            }
            // If we're subframed or don't need subframe, let's record the max star coordinates
            else
            {
                starCenter.setX(selectedHFRStar.x);
                starCenter.setY(selectedHFRStar.y);
                starCenter.setZ(subBinX);

                // Let's now capture again if we're autofocusing
                if (inAutoFocus)
                {
                    capture();
                    return;
                }
            }
        }
        // If manual selection is enabled then let's ask the user to select the focus star
        else
        {
            appendLogText(i18n("Capture complete. Select a star to focus."));

            starSelected = false;

            // Let's now display and set the tracking box in the center of the frame
            // so that the user moves it around to select the desired star.
            int subBinX = 1, subBinY = 1;
            targetChip->getBinning(&subBinX, &subBinY);

            m_FocusView->setTrackingBox(QRect((w - m_OpsFocusSettings->focusBoxSize->value()) / (subBinX * 2),
                                              (h - m_OpsFocusSettings->focusBoxSize->value()) / (2 * subBinY),
                                              m_OpsFocusSettings->focusBoxSize->value() / subBinX, m_OpsFocusSettings->focusBoxSize->value() / subBinY));
            m_FocusView->setTrackingBoxEnabled(true);

            // Now we wait
            setState(Ekos::FOCUS_WAITING);

            // If the user does not select for a timeout period, we abort.
            waitStarSelectTimer.start();
            return;
        }
    }

    // Check if the focus module is requested to verify if the minimum HFR value is met.
    if (minimumRequiredHFR >= 0)
    {
        // In case we failed to detected, we capture again.
        if (lastFrame().hfr == INVALID_STAR_MEASURE)
        {
            if (noStarCount++ < MAX_RECAPTURE_RETRIES)
            {
                appendLogText(i18n("No stars detected while testing HFR, capturing again..."));
                // On Last Attempt reset focus frame to capture full frame and recapture star if possible
                if (noStarCount == MAX_RECAPTURE_RETRIES)
                    resetFrame();
                capture();
                return;
            }
            // If we exceeded maximum tries we abort
            else
            {
                noStarCount = 0;
                completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_NO_STARS);
            }
        }
        // If the detect current HFR is more than the minimum required HFR
        // then we should start the autofocus process now to bring it down.
        else if (lastFrame().hfr > minimumRequiredHFR)
        {
            qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR:" << lastFrame().hfr << "is above required minimum HFR:" <<
                                       minimumRequiredHFR <<
                                       ". Starting AutoFocus...";
            QString reasonInfo = i18n("HFR %1 > Limit %2", QString::number(lastFrame().hfr, 'f', 2),
                                      QString::number(minimumRequiredHFR, 'f',
                                              2));
            minimumRequiredHFR = INVALID_STAR_MEASURE;
            runAutoFocus(AutofocusReason::FOCUS_HFR_CHECK, reasonInfo);
        }
        // Otherwise, the current HFR is fine and lower than the required minimum HFR so we announce success.
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR:" << lastFrame().hfr << "is below required minimum HFR:" <<
                                       minimumRequiredHFR <<
                                       ". Autofocus successful.";
            completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_NONE);
        }

        // Nothing more for now
        return;
    }

    // If we are not in autofocus process, we're done.
    if (inAutoFocus == false)
    {
        // If we are done and there is no further autofocus,
        // we reset state to IDLE
        if (state() != Ekos::FOCUS_IDLE)
            setState(Ekos::FOCUS_IDLE);

        resetButtons();
        return;
    }

    // Set state to progress
    if (state() != Ekos::FOCUS_PROGRESS)
        setState(Ekos::FOCUS_PROGRESS);

    // Now let's kick in the algorithms
    if (inScanStartPos)
        scanStartPos();
    else if(focusAdvisor->inFocusAdvisor())
        focusAdvisor->control();
    else if(m_FocusAlgorithm == FOCUS_LINEAR || m_FocusAlgorithm == FOCUS_LINEAR1PASS)
        autoFocusLinear();
    else if (canAbsMove || canRelMove)
        // Position-based algorithms
        autoFocusAbs();
    else
        // Time open-looped algorithms
        autoFocusRel();
}

QString Focus::getyAxisLabel(StarMeasure starMeasure)
{
    QString str = "HFR";
    m_StarUnits == FOCUS_UNITS_ARCSEC ? str += " (\")" : str += " (pix)";

    switch (starMeasure)
    {
        case FOCUS_STAR_HFR:
            break;
        case FOCUS_STAR_HFR_ADJ:
            str = "HFR Adj";
            m_StarUnits == FOCUS_UNITS_ARCSEC ? str += " (\")" : str += " (pix)";
            break;
        case FOCUS_STAR_FWHM:
            str = "FWHM";
            m_StarUnits == FOCUS_UNITS_ARCSEC ? str += " (\")" : str += " (pix)";
            break;
        case FOCUS_STAR_NUM_STARS:
            str = "# Stars";
            break;
        case FOCUS_STAR_FOURIER_POWER:
            str = "Fourier Power";
            break;
        case FOCUS_STAR_STDDEV:
            str = "Std Dev";
            break;
        case FOCUS_STAR_SOBEL:
            str = "Sobel Factor";
            break;
        case FOCUS_STAR_LAPLASSIAN:
            str = "Laplassian Factor";
            break;
        case FOCUS_STAR_CANNY:
            str = "Canny Factor";
            break;
        default:
            break;
    }
    return str;
}

void Ekos::Focus::resetHFRPlot()
{
    emit initHFRPlot(getyAxisLabel(m_StarMeasure), getStarUnits(m_StarMeasure, m_StarUnits),
                     m_OptDir == CurveFitting::OPTIMISATION_MINIMISE, m_OpsFocusProcess->focusUseWeights->isChecked(),
                     inFocusLoop == false && inSingleCaptureMode == false && isPositionBased());
}

void Focus::clearDataPoints()
{
    maxHFR = 1;
    polynomialFit.reset();
    plot_position.clear();
    plot_value.clear();
    plot_weight.clear();
    plot_outlier.clear();
    isVShapeSolution = false;
    m_abInsPosition.clear();
    m_abInsTileCenterOffset.clear();
    if (m_abInsMeasure.count() != NUM_TILES)
    {
        m_abInsMeasure.resize(NUM_TILES);
        m_abInsWeight.resize(NUM_TILES);
        m_abInsNumStars.resize(NUM_TILES);
    }
    else
    {
        for (int i = 0; i < m_abInsMeasure.count(); i++)
        {
            m_abInsMeasure[i].clear();
            m_abInsWeight[i].clear();
            m_abInsNumStars[i].clear();
        }
    }

    resetHFRPlot();
}

void Focus::clearCurrentRun(bool deleteFiles, bool useTrash)
{
    clearDataPoints();
    m_FocusView->removeRun(m_FocusView->currentAFRun(), deleteFiles, useTrash);
    inSingleCaptureMode = false;
    // check visibility of the history navigation (enable if possible, but do not force)
    refreshHistoryNavigation(true, false);
    refreshMeasuresDisplay();
}

bool Focus::autoFocusChecks()
{
    if (++absIterations > MAXIMUM_ABS_ITERATIONS)
    {
        appendLogText(i18n("Autofocus failed: exceeded max iterations %1", MAXIMUM_ABS_ITERATIONS));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_MAX_ITERS);
        return false;
    }

    // No stars detected, try to capture again
    if (isStarMeasureStarBased() && lastFrame().hfr == INVALID_STAR_MEASURE)
    {
        if (noStarCount < MAX_RECAPTURE_RETRIES)
        {
            noStarCount++;
            appendLogText(i18n("No stars detected, capturing again..."));
            capture();
            return false;
        }
        else if (m_FocusAlgorithm == FOCUS_LINEAR)
        {
            appendLogText(i18n("Failed to detect any stars at position %1. Continuing...", currentPosition));
            noStarCount = 0;
        }
        else if(!m_OpsFocusProcess->focusDonut->isChecked())
        {
            // Carry on for donut detection
            appendLogText(i18n("Failed to detect any stars. Reset frame and try again."));
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_NO_STARS);
            return false;
        }
    }
    else
        noStarCount = 0;

    return true;
}

void Focus::plotLinearFocus()
{
    // I was hoping to avoid intermediate plotting, just set everything up then plot,
    // but this isn't working. For now, with plt=true, plot on every intermediate update.
    bool plt = true;

    // Get the data to plot.
    QVector<double> values, weights;
    QVector<int> positions;
    linearFocuser->getMeasurements(&positions, &values, &weights);
    const FocusAlgorithmInterface::FocusParams &params = linearFocuser->getParams();

    // As an optimization for slower machines, e.g. RPi4s, if the points are the same except for
    // the last point, just emit the last point instead of redrawing everything.
    static QVector<double> lastValues;
    static QVector<int> lastPositions;

    bool incrementalChange = false;
    if (positions.size() > 1 && positions.size() == lastPositions.size() + 1)
    {
        bool ok = true;
        for (int i = 0; i < positions.size() - 1; ++i)
            if (positions[i] != lastPositions[i] || values[i] != lastValues[i])
            {
                ok = false;
                break;
            }
        incrementalChange = ok;
    }
    lastPositions = positions;
    lastValues = values;

    const bool outlier = false;
    if (incrementalChange)
        emit newHFRPlotPosition(static_cast<double>(positions.last()), values.last(), (pow(weights.last(), -0.5)),
                                outlier, params.initialStepSize, plt);
    else
    {
        emit initHFRPlot(getyAxisLabel(m_StarMeasure), getStarUnits(m_StarMeasure, m_StarUnits),
                         m_OptDir == CurveFitting::OPTIMISATION_MINIMISE, params.useWeights, plt);
        for (int i = 0; i < positions.size(); ++i)
            emit newHFRPlotPosition(static_cast<double>(positions[i]), values[i], (pow(weights.last(), -0.5)),
                                    outlier, params.initialStepSize, plt);
    }

    // Plot the polynomial, if there are enough points.
    if (values.size() > 3)
    {
        // The polynomial should only reflect 1st-pass samples.
        QVector<double> pass1Values, pass1Weights;
        QVector<int> pass1Positions;
        QVector<bool> pass1Outliers;
        double minPosition, minValue;
        double searchMin = std::max(params.minPositionAllowed, params.startPosition - params.maxTravel);
        double searchMax = std::min(params.maxPositionAllowed, params.startPosition + params.maxTravel);

        linearFocuser->getPass1Measurements(&pass1Positions, &pass1Values, &pass1Weights, &pass1Outliers);
        if (m_FocusAlgorithm == FOCUS_LINEAR || m_CurveFit == CurveFitting::FOCUS_QUADRATIC)
        {
            // TODO: Need to determine whether to change LINEAR over to the LM solver in CurveFitting
            // This will be determined after L1P's first release has been deemed successful.
            polynomialFit.reset(new PolynomialFit(2, pass1Positions, pass1Values));

            if (polynomialFit->findMinimum(params.startPosition, searchMin, searchMax, &minPosition, &minValue))
            {
                emit drawPolynomial(polynomialFit.get(), true, true, plt);

                // Only plot the first pass' min position if we're not done.
                // Once we have a result, we don't want to display an intermediate minimum.
                if (linearFocuser->isDone())
                    emit minimumFound(-1, -1, plt);
                else
                    emit minimumFound(minPosition, minValue, plt);
            }
            else
            {
                // Didn't get a good polynomial fit.
                emit drawPolynomial(polynomialFit.get(), false, false, plt);
                emit minimumFound(-1, -1, plt);
            }

        }
        else // Linear 1 Pass
        {
            if (curveFitting->findMinMax(params.startPosition, searchMin, searchMax, &minPosition, &minValue, params.curveFit,
                                         params.optimisationDirection))
            {
                R2 = curveFitting->calculateR2(static_cast<CurveFitting::CurveFit>(params.curveFit));
                emit drawCurve(curveFitting.get(), true, true, plt);

                // For Linear 1 Pass always display the minimum on the graph
                emit minimumFound(minPosition, minValue, plt);
            }
            else
            {
                // Didn't get a good fit.
                emit drawCurve(curveFitting.get(), false, false, plt);
                emit minimumFound(-1, -1, plt);
            }
        }
    }

    // Linear focuser might change the latest hfr with its relativeHFR scheme.
    HFROut->setText(QString("%1").arg(lastFrame().hfr * getStarUnits(m_StarMeasure, m_StarUnits), 0, 'f', 2));

    emit setTitle(linearFocuser->getTextStatus(R2));

    if (!plt) HFRPlot->replot();
}

// Get the curve fitting goal
CurveFitting::FittingGoal Focus::getGoal(int numSteps)
{
    // The classic walk needs the STANDARD curve fitting
    if (m_FocusWalk == FOCUS_WALK_CLASSIC)
        return CurveFitting::FittingGoal::STANDARD;

    // Fixed step walks will use C, except for the last step which should be BEST
    return (numSteps >= m_OpsFocusMechanics->focusNumSteps->value()) ? CurveFitting::FittingGoal::BEST :
           CurveFitting::FittingGoal::STANDARD;
}

// Called after the first pass is complete and we're moving to the final focus position
// Calculate R2 for the curve and update the graph.
// Add the CFZ to the graph
void Focus::plotLinearFinalUpdates()
{
    bool plt = true;
    if (!m_OpsFocusProcess->focusRefineCurveFit->isChecked())
    {
        // Display the CFZ on the graph
        emit drawCFZ(linearFocuser->solution(), linearFocuser->solutionValue(), m_cfzSteps,
                     m_CFZUI->focusCFZDisplayVCurve->isChecked());
        // Final updates to the graph title
        emit finalUpdates(linearFocuser->getTextStatus(R2), plt);
    }
    else
    {
        // v-curve needs to be redrawn to reflect the data from the refining process
        // Get the data to plot.
        QVector<double> pass1Values, pass1Weights;
        QVector<int> pass1Positions;
        QVector<bool> pass1Outliers;

        linearFocuser->getPass1Measurements(&pass1Positions, &pass1Values, &pass1Weights, &pass1Outliers);
        const FocusAlgorithmInterface::FocusParams &params = linearFocuser->getParams();

        emit initHFRPlot(getyAxisLabel(m_StarMeasure), getStarUnits(m_StarMeasure, m_StarUnits),
                         m_OptDir == CurveFitting::OPTIMISATION_MINIMISE, m_OpsFocusProcess->focusUseWeights->isChecked(), plt);

        for (int i = 0; i < pass1Positions.size(); ++i)
            emit newHFRPlotPosition(static_cast<double>(pass1Positions[i]), pass1Values[i], (pow(pass1Weights[i], -0.5)),
                                    pass1Outliers[i], params.initialStepSize, plt);

        R2 = curveFitting->calculateR2(static_cast<CurveFitting::CurveFit>(params.curveFit));
        emit drawCurve(curveFitting.get(), true, true, false);

        // For Linear 1 Pass always display the minimum on the graph
        emit minimumFound(linearFocuser->solution(), linearFocuser->solutionValue(), plt);
        // Display the CFZ on the graph
        emit drawCFZ(linearFocuser->solution(), linearFocuser->solutionValue(), m_cfzSteps,
                     m_CFZUI->focusCFZDisplayVCurve->isChecked());
        // Update the graph title
        emit setTitle(linearFocuser->getTextStatus(R2), plt);
    }
}

void Focus::startAberrationInspector()
{
#ifdef HAVE_DATAVISUALIZATION
    // Fill in the data structure to be passed to the Aberration Inspector
    AberrationInspector::abInsData data;

    ImageMosaicMask *mosaicmask = dynamic_cast<ImageMosaicMask *>(m_FocusView->imageMask().get());
    if (!mosaicmask)
    {
        appendLogText(i18n("Unable to launch Aberration Inspector run %1...", m_abInsRun));
        return;
    }


    data.run = ++m_abInsRun;
    data.curveFit = m_CurveFit;
    data.useWeights = m_OpsFocusProcess->focusUseWeights->isChecked();
    data.optDir = m_OptDir;
    data.sensorWidth = m_CcdWidth;
    data.sensorHeight = m_CcdHeight;
    data.pixelSize = m_CcdPixelSizeX;
    data.tileWidth = mosaicmask->tiles()[0].width();
    data.focuserStepMicrons = m_CFZUI->focusCFZStepSize->value();
    data.yAxisLabel = getyAxisLabel(m_StarMeasure);
    data.starUnits = getStarUnits(m_StarMeasure, m_StarUnits);
    data.cfzSteps = m_cfzSteps;
    data.isPositionBased = isPositionBased();

    // Launch the Aberration Inspector.
    appendLogText(i18n("Launching Aberration Inspector run %1...", m_abInsRun));
    QPointer<AberrationInspector> abIns(new AberrationInspector(data, m_abInsPosition, m_abInsMeasure, m_abInsWeight,
                                        m_abInsNumStars, m_abInsTileCenterOffset));
    abIns->setAttribute(Qt::WA_DeleteOnClose);
    abIns->show();
#endif
}

// Initialise the Scan Start Position algorithm
bool Focus::initScanStartPos(const bool force, const int initialPosition)
{
    if (!force)
    {
        if (!m_OpsFocusProcess->focusScanStartPos->isChecked())
            return false;

        if (!m_OpsFocusProcess->focusScanAlwaysOn->isChecked() && !m_AFRerun)
            // Only activate Scan Start Pos if "Always On" is set or we are rerunning after an AF failure
            return false;
    }

    inScanStartPos = true;
    initialFocuserAbsPosition = initialPosition;
    m_scanMeasure.clear();
    m_scanPosition.clear();

    appendLogText(i18n("Starting scan for initial focuser position."));
    emit setTitle(QString(i18n("Scanning for starting position...")), true);

    if (!changeFocus(initialPosition - currentPosition))
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_NO_MOVE);
    return true;
}

// Algorithm to scan for an optimum start position for Autofocus
void Focus::scanStartPos()
{
    // Firstly do we have any stars?
    if (lastFrame().hfr == INVALID_STAR_MEASURE)
    {
        if (noStarCount < MAX_RECAPTURE_RETRIES)
        {
            noStarCount++;
            appendLogText(i18n("No stars detected, capturing again..."));
            capture();
            return;
        }
        else
        {
            appendLogText(i18n("Failed to detect any stars. Aborting..."));
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_NO_STARS);
            return;
        }
    }

    // Cap the maximum number of iterations before failing
    if (++absIterations > MAXIMUM_ABS_ITERATIONS)
    {
        appendLogText(i18n("Scan Start Pos: exceeded max iterations %1", MAXIMUM_ABS_ITERATIONS));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_MAX_ITERS);
        return;
    }

    noStarCount = 0;
    m_scanPosition.push_back(currentPosition);
    m_scanMeasure.push_back(getLastMeasure());

    int deltaPos;
    const int step = m_scanPosition.size();
    const int maxSteps = m_OpsFocusProcess->focusScanDatapoints->value();
    const int stepSize = m_OpsFocusMechanics->focusTicks->value() * m_OpsFocusProcess->focusScanStepSizeFactor->value();
    emit newHFRPlotPosition(static_cast<double>(currentPosition), getLastMeasure(), pow(getLastMeasure(), -0.5), false,
                            stepSize,
                            true);
    if (step < maxSteps)
    {
        // Collect more data
        emit setTitle(QString(i18n("Scanning for starting position %1/%2", step, maxSteps)), true);
        deltaPos = step * stepSize * std::pow(-1, step + 1);
    }
    else
    {
        // We have enough data
        auto it = std::min_element(std::begin(m_scanMeasure), std::end(m_scanMeasure));
        auto min = std::distance(std::begin(m_scanMeasure), it);
        if (min >= m_OpsFocusProcess->focusScanDatapoints->value() - 2)
        {
            // No minimum so we need to move and rerun the scan
            deltaPos = m_scanPosition[min] - currentPosition;
            m_scanPosition.clear();
            m_scanMeasure.clear();
            emit setTitle(QString(i18n("No scan minimum - widening search...")), true);
        }
        else
        {
            // We have a minimum so kick in Autofocus
            if (m_AutofocusReason == FOCUS_FOCUS_ADVISOR)
                focusAdvisor->setInFocusAdvisor(false);

            const int initialPosition = m_scanPosition[min];
            setupLinearFocuser(initialPosition);
            inScanStartPos = false;
            absIterations = 0;
            deltaPos = linearRequestedPosition - currentPosition;
            emit setTitle(QString(i18n("Scan Start Position %1 Found", initialPosition)), true);
            appendLogText(i18n("Scan Start Position %1 found", initialPosition));
        }
    }
    if (!changeFocus(deltaPos))
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_NO_MOVE);
}

void Focus::autoFocusLinear()
{
    if (!autoFocusChecks())
        return;

    if (!canAbsMove && !canRelMove && canTimerMove)
    {
        //const bool kFixPosition = true;
        if (linearRequestedPosition != currentPosition)
            //if (kFixPosition && (linearRequestedPosition != currentPosition))
        {
            qCDebug(KSTARS_EKOS_FOCUS) << "Linear: warning, changing position " << currentPosition << " to "
                                       << linearRequestedPosition;

            currentPosition = linearRequestedPosition;
        }
    }

    // Only use the relativeHFR algorithm if full field is enabled with one capture/measurement.
    bool useFocusStarsHFR = m_OpsFocusSettings->focusUseFullField->isChecked()
                            && m_OpsFocusProcess->focusFramesCount->value() == 1;
    auto focusStars = useFocusStarsHFR || (m_FocusAlgorithm == FOCUS_LINEAR1PASS) ? &(m_ImageData->getStarCenters()) : nullptr;

    linearRequestedPosition = linearFocuser->newMeasurement(currentPosition, getLastMeasure(), getLastMeasure(), focusStars);
    if (m_FocusAlgorithm == FOCUS_LINEAR1PASS && linearFocuser->isDone() && linearFocuser->solution() != -1)
    {
        // Linear 1 Pass is done, graph is drawn, so just move to the focus position, and update the graph.
        plotLinearFinalUpdates();
        if (m_abInsOn)
            startAberrationInspector();
    }
    else
        // Update the graph with the next datapoint, draw the curve, etc.
        plotLinearFocus();

    if (linearFocuser->isDone())
    {
        if (linearFocuser->solution() != -1)
        {
            // If we're running under Focus Advisor analyse results and, if necessary, rerun
            if (m_AutofocusReason == FOCUS_FOCUS_ADVISOR)
            {
                if (focusAdvisor->analyseAF())
                    completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_ADVISOR_RERUN, "", false);
                else
                    completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_NONE, "", false);
            }
            else if (m_CurveFit == CurveFitting::FOCUS_QUADRATIC)
                // Linear only uses Quadratic so no need to do the R2 check, just complete
                completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_NONE, "", false);
            else if (R2 >= m_OpsFocusProcess->focusR2Limit->value())
            {
                // Now test that the curve fit was acceptable. If not retry the focus process using standard retry process
                // R2 check is only available for Linear 1 Pass for Hyperbola and Parabola
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear Curve Fit check passed R2=%1 focusR2Limit=%2").arg(R2).arg(
                                               m_OpsFocusProcess->focusR2Limit->value());
                completeFocusProcedure(Ekos::FOCUS_COMPLETE, FOCUS_FAIL_NONE, "", false);
                m_R2Retries = 0;
            }
            else if (m_R2Retries == 0)
            {
                // Failed the R2 check for the first time so retry...
                appendLogText(i18n("Curve Fit check failed R2=%1 focusR2Limit=%2 retrying...", R2,
                                   m_OpsFocusProcess->focusR2Limit->value()));
                QString failCodeInfo = i18n("R2=%1 < Limit=%2", QString::number(R2, 'f', 2),
                                            QString::number(m_OpsFocusProcess->focusR2Limit->value(), 'f', 2));
                completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_R2, failCodeInfo, false);
                m_R2Retries++;
            }
            else
            {
                // Retried after an R2 check fail but failed again so... log msg and continue
                appendLogText(i18n("Curve Fit check failed again R2=%1 focusR2Limit=%2 but continuing...", R2,
                                   m_OpsFocusProcess->focusR2Limit->value()));
                completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_NONE, "", false);
                m_R2Retries = 0;
            }
        }
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << linearFocuser->doneReason();
            AutofocusFailReason failCode = linearFocuser->getFailCode();
            appendLogText(i18n("Linear autofocus algorithm aborted: %1", AutofocusFailReasonStr[failCode]));
            completeFocusProcedure(Ekos::FOCUS_ABORTED, failCode, "", false);
        }
        return;
    }
    else
    {
        const int delta = linearRequestedPosition - currentPosition;

        if (!changeFocus(delta))
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_NO_MOVE, "", false);

        return;
    }
}

void Focus::autoFocusAbs()
{
    // Q_ASSERT_X(canAbsMove || canRelMove, __FUNCTION__, "Prerequisite: only absolute and relative focusers");

    static int minHFRPos = 0, focusOutLimit = 0, focusInLimit = 0, lastHFRPos = 0, fluctuations = 0;
    static double minHFR = 0, lastDelta = 0;
    double targetPosition = 0;
    bool ignoreLimitedDelta = false;

    QString deltaTxt = QString("%1").arg(fabs(lastFrame().hfr - minHFR) * 100.0, 0, 'g', 3);
    QString HFRText  = QString("%1").arg(lastFrame().hfr, 0, 'g', 3);

    qCDebug(KSTARS_EKOS_FOCUS) << "===============================";
    qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR: " << lastFrame().hfr << " Current Position: " << currentPosition;
    qCDebug(KSTARS_EKOS_FOCUS) << "Last minHFR: " << minHFR << " Last MinHFR Pos: " << minHFRPos;
    qCDebug(KSTARS_EKOS_FOCUS) << "Delta: " << deltaTxt << "%";
    qCDebug(KSTARS_EKOS_FOCUS) << "========================================";

    if (minHFR)
        appendLogText(i18n("FITS received. HFR %1 @ %2. Delta (%3%)", HFRText, currentPosition, deltaTxt));
    else
        appendLogText(i18n("FITS received. HFR %1 @ %2.", HFRText, currentPosition));

    if (!autoFocusChecks())
        return;

    addPlotPosition(currentPosition, lastFrame().hfr);

    switch (m_LastFocusDirection)
    {
        case FOCUS_NONE:
            lastHFR                   = lastFrame().hfr;
            initialFocuserAbsPosition = currentPosition;
            minHFR                    = lastFrame().hfr;
            minHFRPos                 = currentPosition;
            HFRDec                    = 0;
            HFRInc                    = 0;
            focusOutLimit             = 0;
            focusInLimit              = 0;
            lastDelta                 = 0;
            fluctuations              = 0;

            // This is the first step, so clamp the initial target position to the device limits
            // If the focuser cannot move because it is at one end of the interval, try the opposite direction next
            if (absMotionMax < currentPosition + pulseDuration)
            {
                if (currentPosition < absMotionMax)
                {
                    pulseDuration = absMotionMax - currentPosition;
                }
                else
                {
                    pulseDuration = 0;
                    m_LastFocusDirection = FOCUS_IN;
                }
            }
            else if (currentPosition + pulseDuration < absMotionMin)
            {
                if (absMotionMin < currentPosition)
                {
                    pulseDuration = currentPosition - absMotionMin;
                }
                else
                {
                    pulseDuration = 0;
                    m_LastFocusDirection = FOCUS_OUT;
                }
            }

            m_LastFocusDirection = (pulseDuration > 0) ? FOCUS_OUT : FOCUS_IN;
            if (!changeFocus(pulseDuration, false))
                completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_NO_MOVE);

            break;

        case FOCUS_IN:
        case FOCUS_OUT:
            if (reverseDir && focusInLimit && focusOutLimit &&
                    fabs(lastFrame().hfr - minHFR) < (m_OpsFocusProcess->focusTolerance->value() / 100.0) && HFRInc == 0)
            {
                if (absIterations <= 2)
                {
                    QString message = i18n("Change in HFR is too small. Try increasing the step size or decreasing the tolerance.");
                    appendLogText(message);
                    KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                    completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_SMALL_HFR);
                }
                else if (noStarCount > 0)
                {
                    QString message = i18n("Failed to detect focus star in frame. Capture and select a focus star.");
                    appendLogText(message);
                    KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                    completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_NO_STARS);
                }
                else
                {
                    completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_NONE);
                }
                break;
            }
            else if (lastFrame().hfr < lastHFR)
            {
                // Let's now limit the travel distance of the focuser
                if (HFRInc >= 1 && m_LastFocusDirection == FOCUS_OUT && lastHFRPos < focusInLimit
                        && fabs(lastFrame().hfr - lastHFR) > 0.1)
                {
                    focusInLimit = lastHFRPos;
                    qCDebug(KSTARS_EKOS_FOCUS) << "New FocusInLimit " << focusInLimit;
                }
                else if (HFRInc >= 1 && m_LastFocusDirection == FOCUS_IN && lastHFRPos > focusOutLimit &&
                         fabs(lastFrame().hfr - lastHFR) > 0.1)
                {
                    focusOutLimit = lastHFRPos;
                    qCDebug(KSTARS_EKOS_FOCUS) << "New FocusOutLimit " << focusOutLimit;
                }

                double factor = std::max(1.0, HFRDec / 2.0);
                if (m_LastFocusDirection == FOCUS_IN)
                    targetPosition = currentPosition - (pulseDuration * factor);
                else
                    targetPosition = currentPosition + (pulseDuration * factor);

                qCDebug(KSTARS_EKOS_FOCUS) << "current Position" << currentPosition << " targetPosition " << targetPosition;

                lastHFR = lastFrame().hfr;

                // Let's keep track of the minimum HFR
                if (lastHFR < minHFR)
                {
                    minHFR    = lastHFR;
                    minHFRPos = currentPosition;
                    qCDebug(KSTARS_EKOS_FOCUS) << "new minHFR " << minHFR << " @ position " << minHFRPos;
                }

                lastHFRPos = currentPosition;

                // HFR is decreasing, we are on the right direction
                HFRDec++;
                if (HFRInc > 0)
                {
                    // Remove bad data point and mark fluctuation
                    if (plot_position.count() >= 2)
                    {
                        plot_position.remove(plot_position.count() - 2);
                        plot_value.remove(plot_value.count() - 2);
                    }
                    fluctuations++;
                }
                HFRInc = 0;
            }
            else
            {
                // HFR increased, let's deal with it.
                HFRInc++;
                if (HFRDec > 0)
                    fluctuations++;
                HFRDec = 0;

                lastHFR      = lastFrame().hfr;
                lastHFRPos   = currentPosition;

                // Keep moving in same direction (even if bad) for one more iteration to gather data points.
                if (HFRInc > 1)
                {
                    // Looks like we're going away from optimal HFR
                    reverseDir   = true;
                    HFRInc       = 0;

                    qCDebug(KSTARS_EKOS_FOCUS) << "Focus is moving away from optimal HFR.";

                    // Let's set new limits
                    if (m_LastFocusDirection == FOCUS_IN)
                    {
                        focusInLimit = currentPosition;
                        qCDebug(KSTARS_EKOS_FOCUS) << "Setting focus IN limit to " << focusInLimit;
                    }
                    else
                    {
                        focusOutLimit = currentPosition;
                        qCDebug(KSTARS_EKOS_FOCUS) << "Setting focus OUT limit to " << focusOutLimit;
                    }

                    if (m_FocusAlgorithm == FOCUS_POLYNOMIAL && plot_position.count() > 4)
                    {
                        polynomialFit.reset(new PolynomialFit(2, 5, plot_position, plot_value));
                        double a = *std::min_element(plot_position.constBegin(), plot_position.constEnd());
                        double b = *std::max_element(plot_position.constBegin(), plot_position.constEnd());
                        double min_position = 0, min_hfr = 0;
                        isVShapeSolution = polynomialFit->findMinimum(minHFRPos, a, b, &min_position, &min_hfr);
                        qCDebug(KSTARS_EKOS_FOCUS) << "Found Minimum?" << (isVShapeSolution ? "Yes" : "No");
                        if (isVShapeSolution)
                        {
                            ignoreLimitedDelta = true;
                            qCDebug(KSTARS_EKOS_FOCUS) << "Minimum Solution:" << min_hfr << "@" << min_position;
                            targetPosition = round(min_position);
                            appendLogText(i18n("Found polynomial solution @ %1", QString::number(min_position, 'f', 0)));

                            emit drawPolynomial(polynomialFit.get(), isVShapeSolution, true);
                            emit minimumFound(min_position, min_hfr);
                        }
                        else
                        {
                            emit drawPolynomial(polynomialFit.get(), isVShapeSolution, false);
                        }
                    }
                }

                if (HFRInc == 1)
                {
                    // Keep going at same stride even if we are going away from CFZ
                    // This is done to gather data points are the trough.
                    if (std::abs(lastDelta) > 0)
                        targetPosition = currentPosition + lastDelta;
                    else
                        targetPosition = currentPosition + pulseDuration;
                }
                else if (isVShapeSolution == false)
                {
                    ignoreLimitedDelta = true;
                    // Let's get close to the minimum HFR position so far detected
                    if (m_LastFocusDirection == FOCUS_OUT)
                        targetPosition = minHFRPos - pulseDuration / 2;
                    else
                        targetPosition = minHFRPos + pulseDuration / 2;
                }

                qCDebug(KSTARS_EKOS_FOCUS) << "new targetPosition " << targetPosition;
            }

            // Limit target Pulse to algorithm limits
            if (focusInLimit != 0 && m_LastFocusDirection == FOCUS_IN && targetPosition < focusInLimit)
            {
                targetPosition = focusInLimit;
                qCDebug(KSTARS_EKOS_FOCUS) << "Limiting target pulse to focus in limit " << targetPosition;
            }
            else if (focusOutLimit != 0 && m_LastFocusDirection == FOCUS_OUT && targetPosition > focusOutLimit)
            {
                targetPosition = focusOutLimit;
                qCDebug(KSTARS_EKOS_FOCUS) << "Limiting target pulse to focus out limit " << targetPosition;
            }

            // Limit target pulse to focuser limits
            if (targetPosition < absMotionMin)
                targetPosition = absMotionMin;
            else if (targetPosition > absMotionMax)
                targetPosition = absMotionMax;

            // We cannot go any further because of the device limits, this is a failure
            if (targetPosition == currentPosition)
            {
                // If case target position happens to be the minimal historical
                // HFR position, accept this value instead of bailing out.
                if (targetPosition == minHFRPos || isVShapeSolution)
                {
                    appendLogText("Stopping at minimum recorded HFR position.");
                    completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_NONE);
                }
                else
                {
                    QString message = i18n("Focuser cannot move further, device limits reached. Autofocus aborted.");
                    appendLogText(message);
                    KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                    completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_OOB);
                }
                return;
            }

            // Too many fluctuatoins
            if (fluctuations >= MAXIMUM_FLUCTUATIONS)
            {
                QString message = i18n("Unstable fluctuations. Try increasing initial step size or exposure time.");
                appendLogText(message);
                KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FLUCTUATIONS);
                return;
            }

            // Ops, deadlock
            if (focusOutLimit && focusOutLimit == focusInLimit)
            {
                QString message = i18n("Deadlock reached. Please try again with different settings.");
                appendLogText(message);
                KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_DEADLOCK);
                return;
            }

            // Restrict the target position even more with the maximum travel option
            if (fabs(targetPosition - initialFocuserAbsPosition) > m_OpsFocusMechanics->focusMaxTravel->value())
            {
                int minTravelLimit = qMax(0.0, initialFocuserAbsPosition - m_OpsFocusMechanics->focusMaxTravel->value());
                int maxTravelLimit = qMin(absMotionMax, initialFocuserAbsPosition + m_OpsFocusMechanics->focusMaxTravel->value());

                // In case we are asked to go below travel limit, but we are not there yet
                // let us go there and see the result before aborting
                if (fabs(currentPosition - minTravelLimit) > 10 && targetPosition < minTravelLimit)
                {
                    targetPosition = minTravelLimit;
                }
                // Same for max travel
                else if (fabs(currentPosition - maxTravelLimit) > 10 && targetPosition > maxTravelLimit)
                {
                    targetPosition = maxTravelLimit;
                }
                else
                {
                    qCDebug(KSTARS_EKOS_FOCUS) << "targetPosition (" << targetPosition << ") - initHFRAbsPos ("
                                               << initialFocuserAbsPosition << ") exceeds maxTravel distance of " << m_OpsFocusMechanics->focusMaxTravel->value();

                    QString message = i18n("Maximum travel limit reached. Autofocus aborted.");
                    appendLogText(message);
                    KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                    completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_OOB);
                    break;
                }
            }

            // Get delta for next move
            lastDelta = (targetPosition - currentPosition);

            qCDebug(KSTARS_EKOS_FOCUS) << "delta (targetPosition - currentPosition) " << lastDelta;

            // Limit to Maximum permitted delta (Max Single Step Size)
            if (ignoreLimitedDelta == false)
            {
                double limitedDelta = qMax(-1.0 * m_OpsFocusMechanics->focusMaxSingleStep->value(),
                                           qMin(1.0 * m_OpsFocusMechanics->focusMaxSingleStep->value(), lastDelta));
                if (std::fabs(limitedDelta - lastDelta) > 0)
                {
                    qCDebug(KSTARS_EKOS_FOCUS) << "Limited delta to maximum permitted single step " <<
                                               m_OpsFocusMechanics->focusMaxSingleStep->value();
                    lastDelta = limitedDelta;
                }
            }

            m_LastFocusDirection = (lastDelta > 0) ? FOCUS_OUT : FOCUS_IN;
            if (!changeFocus(lastDelta, false))
                completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_NO_MOVE);

            break;
    }
}

void Focus::addPlotPosition(int pos, double value, bool plot)
{
    plot_position.append(pos);
    plot_value.append(value);
    if (plot)
        emit newHFRPlotPosition(static_cast<double>(pos), value, 1.0, false, pulseDuration);
}

void Focus::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // ensure that underlying changes to Options are reflected in the UI
    if (isVisible())
        resetButtons();

}

// Synchronises the plot_position and plot_value vectors with the data used by linearFocuser
// This keeps the 2 sets of data in sync for the Linear and Linear1Pass algorithms.
// For Iterative and Polynomial these vectors are built during the focusing cycle so nothing to do here
void Focus::updatePlotPosition()
{
    if (m_FocusAlgorithm == FOCUS_LINEAR1PASS)
    {
        QVector<int> positions;
        if (inScanStartPos || focusAdvisor->inFocusAdvisor())
        {
            // If we are inScanStartPos, or inFocusAdvisor then there is no focus data to save off
            plot_position.clear();
            plot_value.clear();
            plot_weight.clear();
            plot_outlier.clear();
        }
        else
        {
            linearFocuser->getPass1Measurements(&positions, &plot_value, &plot_weight, &plot_outlier);
            plot_position.clear();
            for (int i = 0; i < positions.count(); i++)
                plot_position.append(positions[i]);

            // For L1P add in the solution datapoint (if there is one). Linear already has this included.
            if (linearFocuser && linearFocuser->isDone() && linearFocuser->solution() != -1)
            {
                plot_position.append(linearFocuser->solution());
                plot_value.append(linearFocuser->solutionValue());
                plot_weight.append(linearFocuser->solutionWeight());
                plot_outlier.append(false);
            }
        }
    }
    else if (m_FocusAlgorithm == FOCUS_LINEAR)
    {
        QVector<int> positions;
        linearFocuser->getMeasurements(&positions, &plot_value, &plot_weight);
        plot_position.clear();
        plot_outlier.clear();
        for (int i = 0; i < positions.count(); i++)
        {
            plot_position.append(positions[i]);
            // Outlier functionality isn't supported for Linear so make sure no outliers in the data
            plot_outlier.append(false);
        }
    }
    else
    {
        // Polynomial and Iterative don't support weights or outliers so fill in this data.
        for (int i = 0; i < plot_position.count(); i++)
        {
            plot_weight.append(1.0);
            plot_outlier.append(false);
        }
    }
}

void Focus::autoFocusRel()
{
    static int noStarCount = 0;
    static double minHFR   = 1e6;
    QString deltaTxt       = QString("%1").arg(fabs(lastFrame().hfr - minHFR) * 100.0, 0, 'g', 2);
    QString minHFRText     = QString("%1").arg(minHFR, 0, 'g', 3);
    QString HFRText        = QString("%1").arg(lastFrame().hfr, 0, 'g', 3);

    appendLogText(i18n("FITS received. HFR %1. Delta (%2%) Min HFR (%3)", HFRText, deltaTxt, minHFRText));

    if (pulseDuration <= MINIMUM_PULSE_TIMER)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try adjusting the tolerance value."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_TOLERANCE);
        return;
    }

    // No stars detected, try to capture again
    if (lastFrame().hfr == INVALID_STAR_MEASURE)
    {
        if (noStarCount < MAX_RECAPTURE_RETRIES)
        {
            noStarCount++;
            appendLogText(i18n("No stars detected, capturing again..."));
            capture();
            return;
        }
        else if (m_FocusAlgorithm == FOCUS_LINEAR || m_FocusAlgorithm == FOCUS_LINEAR1PASS)
        {
            appendLogText(i18n("Failed to detect any stars at position %1. Continuing...", currentPosition));
            noStarCount = 0;
        }
        else if(!m_OpsFocusProcess->focusDonut->isChecked())
        {
            // Carry on for donut detection
            appendLogText(i18n("Failed to detect any stars. Reset frame and try again."));
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_NO_STARS);
            return;
        }
    }
    else
        noStarCount = 0;

    switch (m_LastFocusDirection)
    {
        case FOCUS_NONE:
            lastHFR = lastFrame().hfr;
            minHFR  = 1e6;
            m_LastFocusDirection = FOCUS_IN;
            changeFocus(-pulseDuration, false);
            break;

        case FOCUS_IN:
        case FOCUS_OUT:
            if (fabs(lastFrame().hfr - minHFR) < (m_OpsFocusProcess->focusTolerance->value() / 100.0) && HFRInc == 0)
            {
                completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_NONE);
            }
            else if (lastFrame().hfr < lastHFR)
            {
                if (lastFrame().hfr < minHFR)
                    minHFR = lastFrame().hfr;

                lastHFR = lastFrame().hfr;
                changeFocus(m_LastFocusDirection == FOCUS_IN ? -pulseDuration : pulseDuration, false);
                HFRInc = 0;
            }
            else
            {
                //HFRInc++;

                lastHFR = lastFrame().hfr;

                HFRInc = 0;

                pulseDuration *= 0.75;

                if (!changeFocus(m_LastFocusDirection == FOCUS_IN ? pulseDuration : -pulseDuration, false))
                    completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_NO_MOVE);

                // HFR getting worse so reverse direction
                m_LastFocusDirection = (m_LastFocusDirection == FOCUS_IN) ? FOCUS_OUT : FOCUS_IN;
            }
            break;
    }
}

void Focus::handleFocusButtonEvent()
{
    bool inward = (sender() == focusInB);
    int speedup = 1;
    bool shift = QApplication::keyboardModifiers() & Qt::ShiftModifier;
    bool ctrl  = QApplication::keyboardModifiers() & Qt::ControlModifier;
    updateButtonColors(inward ? focusInB : focusOutB, shift, ctrl);

    if (shift)
        speedup *= m_OpsFocusMechanics->speedupShift->value();
    if (ctrl)
        speedup *= m_OpsFocusMechanics->speedupCtrl->value();

    if (inward)
        focusIn(-1, speedup);
    else
        focusOut(-1, speedup);
}

void Focus::autoFocusProcessPositionChange(IPState state)
{
    if (state == IPS_OK)
    {
        // Normally, if we are auto-focusing, after we move the focuser we capture an image.
        // However, the Linear algorithm, at the start of its passes, requires two
        // consecutive focuser moves--the first out further than we want, and a second
        // move back in, so that we eliminate backlash and are always moving in before a capture.
        if (focuserAdditionalMovement > 0)
        {
            int temp = focuserAdditionalMovement;
            focuserAdditionalMovement = 0;

            qCDebug(KSTARS_EKOS_FOCUS) << QString("Undoing overscan extension. Moving back in by %1 ticks in %2s")
                                       .arg(temp).arg(m_OpsFocusMechanics->focusOverscanDelay->value());

            QTimer::singleShot(m_OpsFocusMechanics->focusOverscanDelay->value() * 1000, this, [this, temp]()
            {
                if (!changeFocus(-temp, focuserAdditionalMovementUpdateDir))
                {
                    appendLogText(i18n("Focuser error, check INDI panel."));
                    completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_NO_MOVE);
                }
            });
        }
        else if (inAutoFocus && !inAFOptimise)
        {
            // Add a check that the current position matches the requested position (within a tolerance)
            if (m_FocusAlgorithm == FOCUS_LINEAR || m_FocusAlgorithm == FOCUS_LINEAR1PASS)
                if (abs(linearRequestedPosition - currentPosition) > m_OpsFocusMechanics->focusTicks->value())
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Focus positioning error: requested position %1, current position %2")
                                               .arg(linearRequestedPosition).arg(currentPosition);

            qCDebug(KSTARS_EKOS_FOCUS) << QString("Focus position reached at %1, starting capture in %2 seconds.").arg(
                                           currentPosition).arg(m_OpsFocusMechanics->focusSettleTime->value());
            // Adjust exposure if Donut Buster activated
            if (m_OpsFocusProcess->focusDonut->isChecked())
                donutTimeDilation();
            capture(m_OpsFocusMechanics->focusSettleTime->value());
        }
    }
    else if (state == IPS_ALERT)
    {
        appendLogText(i18n("Focuser error, check INDI panel."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FOCUSER_ERROR);
    }
    else
        qCDebug(KSTARS_EKOS_FOCUS) <<
                                   QString("autoFocusProcessPositionChange called with state %1 (%2), focuserAdditionalMovement=%3, inAutoFocus=%4, m_captureInProgress=%5, currentPosition=%6")
                                   .arg(state).arg(pstateStr(state)).arg(focuserAdditionalMovement).arg(inAutoFocus).arg(m_captureInProgress)
                                   .arg(currentPosition);
}

// This routine adjusts capture exposure during Autofocus depending on donut parameter settings
void Focus::donutTimeDilation()
{
    if (m_OpsFocusProcess->focusTimeDilation->value() == 1.0 || inScanStartPos || focusAdvisor->inFocusAdvisor())
        return;

    // Get the max distance from focus to outer points
    const double centre = (m_OpsFocusMechanics->focusNumSteps->value() + 1.0) / 2.0;
    // Get the current step - treat the final
    const int currentStep = linearFocuser->currentStep() + 1;
    double distance;
    if (currentStep <= m_OpsFocusMechanics->focusNumSteps->value())
        distance = std::abs(centre - currentStep);
    else
        // Last step is always back to focus
        distance = 0.0;
    double multiplier = distance / (centre - 1.0) * m_OpsFocusProcess->focusTimeDilation->value();
    multiplier = std::max(multiplier, 1.0);
    const double exposure = multiplier * m_donutOrigExposure;
    focusExposure->setValue(exposure);
    qCDebug(KSTARS_EKOS_FOCUS) << "Donut time dilation for point " << currentStep << " from " << m_donutOrigExposure << " to "
                               << exposure;

}

void Focus::updateProperty(INDI::Property prop)
{
    if (m_Focuser == nullptr || prop.getType() != INDI_NUMBER || prop.getDeviceName() != m_Focuser->getDeviceName())
        return;

    auto nvp = prop.getNumber();

    // Only process focus properties
    if (QString(nvp->getName()).contains("focus", Qt::CaseInsensitive) == false)
        return;

    if (nvp->isNameMatch("FOCUS_BACKLASH_STEPS"))
    {
        m_OpsFocusMechanics->focusBacklash->setValue(nvp->np[0].value);
        return;
    }

    if (nvp->isNameMatch("ABS_FOCUS_POSITION"))
    {
        if (m_DebugFocuser)
        {
            // Simulate focuser comms issues every 5 moves
            if (m_DebugFocuserCounter++ >= 10 && m_DebugFocuserCounter <= 14)
            {
                if (m_DebugFocuserCounter == 14)
                    m_DebugFocuserCounter = 0;
                appendLogText(i18n("Simulate focuser comms failure..."));
                return;
            }
        }

        m_FocusMotionTimer.stop();
        INumber *pos = IUFindNumber(nvp, "FOCUS_ABSOLUTE_POSITION");
        IPState newState = nvp->s;

        // FIXME: We should check state validity, but some focusers do not care - make ignore an option!
        if (pos)
        {
            int newPosition = static_cast<int>(pos->value);

            // Some absolute focuser constantly report the position without a state change.
            // Therefore we ignore it if both value and state are the same as last time.
            // HACK: This would shortcut the autofocus procedure reset, see completeFocusProcedure for the small hack
            if (currentPosition == newPosition && currentPositionState == newState)
            {
                if (logPositionAndState)
                {
                    qCDebug(KSTARS_EKOS_FOCUS) << "Focuser position " << currentPosition << " and state:"
                                               << pstateStr(currentPositionState) << " unchanged";
                    logPositionAndState = false;
                }
                return;
            }
            else
            {
                logPositionAndState = true;
            }

            currentPositionState = newState;

            if (currentPosition != newPosition)
            {
                currentPosition = newPosition;
                qCDebug(KSTARS_EKOS_FOCUS) << "Abs Focuser position changed to " << currentPosition << "State:" << pstateStr(
                                               currentPositionState);
                absTicksLabel->setText(QString::number(currentPosition));
                emit absolutePositionChanged(currentPosition);
            }
        }
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "Can't access FOCUS_ABSOLUTE_POSITION. Current state:"
                                       << pstateStr(currentPositionState) << " New state:" << pstateStr(newState);

        if (newState != IPS_OK)
        {
            if (inAutoFocus || inAdjustFocus || adaptFocus->inAdaptiveFocus())
            {
                // We had something back from the focuser but we're not done yet, so
                // restart motion timer in case focuser gets stuck.
                qCDebug(KSTARS_EKOS_FOCUS) << "Restarting focus motion timer, state " << pstateStr(newState);
                m_FocusMotionTimer.start();
            }
        }
        else
        {
            // Systematically reset UI when focuser finishes moving
            if (focuserAdditionalMovement == 0)
                resetButtons();

            if (inAdjustFocus)
            {
                if (focuserAdditionalMovement == 0)
                {
                    inAdjustFocus = false;
                    emit focusPositionAdjusted();
                    return;
                }
            }

            if (inAFOptimise)
            {
                if (focuserAdditionalMovement == 0)
                {
                    inAFOptimise = false;
                    emit focusAFOptimise();
                    return;
                }
            }

            if (adaptFocus->inAdaptiveFocus())
            {
                if (focuserAdditionalMovement == 0)
                {
                    adaptFocus->adaptiveFocusAdmin(currentPosition, true, true);
                    return;
                }
            }

            if (m_RestartState == RESTART_NOW && status() != Ekos::FOCUS_ABORTED)
            {
                if (focuserAdditionalMovement == 0)
                {
                    m_RestartState = RESTART_NONE;
                    m_AFRerun = true;
                    inAutoFocus = inBuildOffsets = inAFOptimise = inAdjustFocus = inScanStartPos = false;
                    adaptFocus->setInAdaptiveFocus(false);
                    focusAdvisor->setInFocusAdvisor(false);
                    appendLogText(i18n("Restarting autofocus process..."));
                    runAutoFocus(m_AutofocusReason, m_AutofocusReasonInfo);
                    return;
                }
            }
            else if (m_RestartState == RESTART_ABORT && focuserAdditionalMovement == 0)
            {
                // We are trying to abort an autofocus run
                // This event means that the focuser has been reset and arrived at its starting point
                // so we can finish processing the abort. Set inAutoFocus to avoid repeating
                // processing already done in completeFocusProcedure
                completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FORCE_ABORT);
                m_RestartState = RESTART_NONE;
                inAutoFocus = inBuildOffsets = inAFOptimise = inAdjustFocus = inScanStartPos = false;
                adaptFocus->setInAdaptiveFocus(false);
                focusAdvisor->setInFocusAdvisor(false);
                return;
            }
        }

        if (canAbsMove)
            autoFocusProcessPositionChange(newState);
        else if (newState == IPS_ALERT)
            appendLogText(i18n("Focuser error, check INDI panel."));
        return;
    }

    if (canAbsMove)
        return;

    if (nvp->isNameMatch("manualfocusdrive"))
    {
        if (m_DebugFocuser)
        {
            // Simulate focuser comms issues every 5 moves
            if (m_DebugFocuserCounter++ >= 10 && m_DebugFocuserCounter <= 14)
            {
                if (m_DebugFocuserCounter == 14)
                    m_DebugFocuserCounter = 0;
                appendLogText(i18n("Simulate focuser comms failure..."));
                return;
            }
        }

        m_FocusMotionTimer.stop();

        INumber *pos = IUFindNumber(nvp, "manualfocusdrive");
        IPState newState = nvp->s;
        if (pos && newState == IPS_OK)
        {
            if (focuserAdditionalMovement == 0)
                resetButtons();

            currentPosition += pos->value;
            absTicksLabel->setText(QString::number(static_cast<int>(currentPosition)));
            emit absolutePositionChanged(currentPosition);
        }

        if (inAdjustFocus && newState == IPS_OK)
        {
            if (focuserAdditionalMovement == 0)
            {
                inAdjustFocus = false;
                emit focusPositionAdjusted();
                return;
            }
        }

        if (inAFOptimise && newState == IPS_OK)
        {
            if (focuserAdditionalMovement == 0)
            {
                inAFOptimise = false;
                emit focusAFOptimise();
                return;
            }
        }

        if (adaptFocus->inAdaptiveFocus() && newState == IPS_OK)
        {
            if (focuserAdditionalMovement == 0)
            {
                adaptFocus->adaptiveFocusAdmin(currentPosition, true, true);
                return;
            }
        }

        // restart if focus movement has finished
        if (m_RestartState == RESTART_NOW && newState == IPS_OK && status() != Ekos::FOCUS_ABORTED)
        {
            if (focuserAdditionalMovement == 0)
            {
                m_RestartState = RESTART_NONE;
                m_AFRerun = true;
                inAutoFocus = inBuildOffsets = inAFOptimise = inAdjustFocus = inScanStartPos = false;
                adaptFocus->setInAdaptiveFocus(false);
                focusAdvisor->setInFocusAdvisor(false);
                appendLogText(i18n("Restarting autofocus process..."));
                runAutoFocus(m_AutofocusReason, m_AutofocusReasonInfo);
                return;
            }
        }
        else if (m_RestartState == RESTART_ABORT && newState == IPS_OK)
        {
            // Abort the autofocus run now the focuser has finished moving to its start position
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FORCE_ABORT);
            m_RestartState = RESTART_NONE;
            inAutoFocus = inBuildOffsets = inAFOptimise = inAdjustFocus = inScanStartPos = false;
            adaptFocus->setInAdaptiveFocus(false);
            focusAdvisor->setInFocusAdvisor(false);
            return;
        }

        if (canRelMove)
            autoFocusProcessPositionChange(newState);
        else if (newState == IPS_ALERT)
            appendLogText(i18n("Focuser error, check INDI panel."));

        return;
    }

    if (nvp->isNameMatch("REL_FOCUS_POSITION"))
    {
        m_FocusMotionTimer.stop();

        INumber *pos = IUFindNumber(nvp, "FOCUS_RELATIVE_POSITION");
        IPState newState = nvp->s;
        if (pos && newState == IPS_OK)
        {
            if (focuserAdditionalMovement == 0)
                resetButtons();

            currentPosition += pos->value * (m_LastFocusDirection == FOCUS_IN ? -1 : 1);
            qCDebug(KSTARS_EKOS_FOCUS)
                    << QString("Rel Focuser position moved %1 by %2 to %3")
                    .arg((m_LastFocusDirection == FOCUS_IN) ? "in" : "out").arg(pos->value).arg(currentPosition);
            absTicksLabel->setText(QString::number(static_cast<int>(currentPosition)));
            emit absolutePositionChanged(currentPosition);
        }

        if (inAdjustFocus && newState == IPS_OK)
        {
            if (focuserAdditionalMovement == 0)
            {
                inAdjustFocus = false;
                emit focusPositionAdjusted();
                return;
            }
        }

        if (inAFOptimise && newState == IPS_OK)
        {
            if (focuserAdditionalMovement == 0)
            {
                inAFOptimise = false;
                emit focusAFOptimise();
                return;
            }
        }

        if (adaptFocus->inAdaptiveFocus() && newState == IPS_OK)
        {
            if (focuserAdditionalMovement == 0)
            {
                adaptFocus->adaptiveFocusAdmin(currentPosition, true, true);
                return;
            }
        }

        // restart if focus movement has finished
        if (m_RestartState == RESTART_NOW && newState == IPS_OK && status() != Ekos::FOCUS_ABORTED)
        {
            if (focuserAdditionalMovement == 0)
            {
                m_RestartState = RESTART_NONE;
                m_AFRerun = true;
                inAutoFocus = inBuildOffsets = inAFOptimise = inAdjustFocus = inScanStartPos = false;
                adaptFocus->setInAdaptiveFocus(false);
                focusAdvisor->setInFocusAdvisor(false);
                appendLogText(i18n("Restarting autofocus process..."));
                runAutoFocus(m_AutofocusReason, m_AutofocusReasonInfo);
                return;
            }
        }
        else if (m_RestartState == RESTART_ABORT && newState == IPS_OK)
        {
            // Abort the autofocus run now the focuser has finished moving to its start position
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FORCE_ABORT);
            m_RestartState = RESTART_NONE;
            inAutoFocus = inBuildOffsets = inAFOptimise = inAdjustFocus = inScanStartPos = false;
            adaptFocus->setInAdaptiveFocus(false);
            focusAdvisor->setInFocusAdvisor(false);
            return;
        }

        if (canRelMove)
            autoFocusProcessPositionChange(newState);
        else if (newState == IPS_ALERT)
            appendLogText(i18n("Focuser error, check INDI panel."));

        return;
    }

    if (canRelMove)
        return;

    if (nvp->isNameMatch("FOCUS_TIMER"))
    {
        IPState newState = nvp->s;
        m_FocusMotionTimer.stop();
        // restart if focus movement has finished
        if (m_RestartState == RESTART_NOW && newState == IPS_OK && status() != Ekos::FOCUS_ABORTED)
        {
            if (focuserAdditionalMovement == 0)
            {
                m_RestartState = RESTART_NONE;
                m_AFRerun = true;
                inAutoFocus = inBuildOffsets = inAFOptimise = inAdjustFocus = inScanStartPos = false;
                adaptFocus->setInAdaptiveFocus(false);
                focusAdvisor->setInFocusAdvisor(false);
                appendLogText(i18n("Restarting autofocus process..."));
                runAutoFocus(m_AutofocusReason, m_AutofocusReasonInfo);
                return;
            }
        }
        else if (m_RestartState == RESTART_ABORT && newState == IPS_OK)
        {
            // Abort the autofocus run now the focuser has finished moving to its start position
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FORCE_ABORT);
            m_RestartState = RESTART_NONE;
            inAutoFocus = inBuildOffsets = inAFOptimise = inAdjustFocus = inScanStartPos = false;
            adaptFocus->setInAdaptiveFocus(false);
            focusAdvisor->setInFocusAdvisor(false);
            return;
        }

        if (canAbsMove == false && canRelMove == false)
        {
            // Used by the linear focus algorithm. Ignored if that's not in use for the timer-focuser.
            INumber *pos = IUFindNumber(nvp, "FOCUS_TIMER_VALUE");
            if (pos)
            {
                currentPosition += pos->value * (m_LastFocusDirection == FOCUS_IN ? -1 : 1);
                qCDebug(KSTARS_EKOS_FOCUS)
                        << QString("Timer Focuser position moved %1 by %2 to %3")
                        .arg((m_LastFocusDirection == FOCUS_IN) ? "in" : "out").arg(pos->value).arg(currentPosition);
            }

            if (pos && newState == IPS_OK && focuserAdditionalMovement == 0)
                resetButtons();

            if (inAdjustFocus && newState == IPS_OK)
            {
                if (focuserAdditionalMovement == 0)
                {
                    inAdjustFocus = false;
                    emit focusPositionAdjusted();
                    return;
                }
            }

            if (inAFOptimise && newState == IPS_OK)
            {
                if (focuserAdditionalMovement == 0)
                {
                    inAFOptimise = false;
                    emit focusAFOptimise();
                    return;
                }
            }

            if (adaptFocus->inAdaptiveFocus() && newState == IPS_OK)
            {
                if (focuserAdditionalMovement == 0)
                {
                    adaptFocus->adaptiveFocusAdmin(true, true, true);
                    return;
                }
            }

            autoFocusProcessPositionChange(newState);
        }
        else if (newState == IPS_ALERT)
            appendLogText(i18n("Focuser error, check INDI panel."));

        return;
    }
}

void Focus::appendLogText(const QString &text)
{
    const QString logtext(m_Focuser == nullptr ? text : QString("[%1] %2").arg(m_Focuser->getDeviceName()).arg(text));
    emit newLog(logtext);
}

void Focus::appendFocusLogText(const QString &text)
{
    if (Options::focusLogging())
    {
        const QString logtext(m_Focuser == nullptr ? text : QString("[%1] %2").arg(m_Focuser->getDeviceName()).arg(text));
        emit newFocusLog(logtext);
    }
}

void Focus::startFraming()
{
    if (m_Camera == nullptr)
    {
        appendLogText(i18n("No CCD connected."));
        return;
    }

    waitStarSelectTimer.stop();

    inFocusLoop = true;
    starMeasureFrames.clear();

    clearDataPoints();

    //emit statusUpdated(true);
    setState(Ekos::FOCUS_FRAMING);

    resetButtons();

    appendLogText(i18n("Starting continuous exposure..."));

    capture();
}

void Focus::resetButtons()
{
    // clear focus button colors
    updateButtonColors(focusInB, false, false);
    updateButtonColors(focusOutB, false, false);

    if (inFocusLoop)
    {
        startFocusB->setEnabled(false);
        startAbInsB->setEnabled(false);
        startLoopB->setEnabled(false);
        focusOutB->setEnabled(true);
        focusInB->setEnabled(true);
        startGotoB->setEnabled(canAbsMove);
        stopFocusB->setEnabled(true);
        captureB->setEnabled(false);
        opticalTrainCombo->setEnabled(false);
        trainB->setEnabled(false);
        return;
    }

    if (inAutoFocus)
    {
        // During an Autofocus run we need to disable input widgets to stop the user changing them
        // We need to disable the widgets currently enabled and save a QVector of these to be
        // reinstated once the AF run completes.
        // Certain widgets (highlighted below) have the isEnabled() property used in the code to
        // determine functionality. So the isEnabled state for these is saved off before the
        // interface is disabled and these saved states used to control the code path and preserve
        // functionality.
        // Since this function can be called several times only load up widgets once
        if (disabledWidgets.empty())
        {
            AFDisable(trainLabel, false);
            AFDisable(opticalTrainCombo, false);
            AFDisable(trainB, false);
            AFDisable(focuserGroup, true);
            AFDisable(clearDataB, false);

            // In the ccdGroup save the enabled state of Gain and ISO
            m_FocusGainAFEnabled = focusGain->isEnabled();
            m_FocusISOAFEnabled = focusISO->isEnabled();
            AFDisable(ccdGroup, false);

            AFDisable(toolsGroup, false);

            // Save the enabled state of SubFrame
            m_FocusSubFrameAFEnabled = m_OpsFocusSettings->focusSubFrame->isEnabled();

            // Disable parameters and tools to prevent changes while Autofocus is running
            AFDisable(m_OpsFocusSettings, false);
            AFDisable(m_OpsFocusProcess, false);
            AFDisable(m_OpsFocusMechanics, false);
            AFDisable(focusAdvisor->focusAdvGroupBox, false);
            AFDisable(m_CFZDialog, false);

            // Disable the navigation buttons
            AFDisable(m_FocusView->m_focusHistoryNavigation->historyFirstB, false);
            AFDisable(m_FocusView->m_focusHistoryNavigation->historyBackwardB, false);
            AFDisable(m_FocusView->m_focusHistoryNavigation->historyForwardB, false);
            AFDisable(m_FocusView->m_focusHistoryNavigation->historyLastB, false);

            // Enable the "stop" button so the user can abort an AF run
            stopFocusB->setEnabled(true);
        }
        return;
    }

    // Restore widgets that were disabled when Autofocus started
    for(int i = 0 ; i < disabledWidgets.size() ; i++)
        disabledWidgets[i]->setEnabled(true);
    disabledWidgets.clear();

    bool cameraConnected = m_Camera && m_Camera->isConnected();
    bool enableCaptureButtons = cameraConnected && (!m_captureInProgress && !m_starDetectInProgress);

    captureB->setEnabled(enableCaptureButtons);
    resetFrameB->setEnabled(enableCaptureButtons);
    startLoopB->setEnabled(enableCaptureButtons);
    startFocusB->setEnabled(enableCaptureButtons);

    refreshMeasuresDisplay();

    if (cameraConnected)
    {
        ccdGroup->setEnabled(true);
        toolsGroup->setEnabled(true);
    }
    else
    {
        ccdGroup->setEnabled(false);
        toolsGroup->setEnabled(false);
    }

    if (m_Focuser && m_Focuser->isConnected())
    {
        focusOutB->setEnabled(true);
        focusInB->setEnabled(true);
        startAbInsB->setEnabled(canAbInsStart());
        stopFocusB->setEnabled(!enableCaptureButtons);
        startGotoB->setEnabled(canAbsMove);
        stopGotoB->setEnabled(true);
        focuserGroup->setEnabled(true);
    }
    else
    {
        focusOutB->setEnabled(false);
        focusInB->setEnabled(false);
        startAbInsB->setEnabled(false);
        stopFocusB->setEnabled(false);
        startGotoB->setEnabled(false);
        stopGotoB->setEnabled(false);
        focuserGroup->setEnabled(cameraConnected);
    }
}

void Focus::updateButtonColors(QPushButton *button, bool shift, bool ctrl)
{
    QString stylesheet = "";

    if (shift && ctrl)
        stylesheet = "background-color: #D32F2F";
    else if (shift)
        stylesheet = "background-color: #FFC107";
    else if (ctrl)
        stylesheet = "background-color: #FF5722";

    button->setStyleSheet(stylesheet);
}

void Ekos::Focus::refreshHistoryNavigation(bool enable, bool force)
{
    // remove limit exceeding frames
    if (Options::maxFocusFrameFiles() > 0)
    {
        QString path = focusFramePath();
        int removed = enforceMaxFilesRecursive(QFileInfo(path).dir().absolutePath(), Options::maxFocusFrameFiles());

        // check the history and clear not existing file name entries
        if (removed > 0)
            m_FocusView->m_focusHistoryNavigation->refreshHistory();
    }

    m_FocusView->showNavigation((m_FocusView->isNavigationVisible() || force) &&
                                enable && m_FocusView->m_focusHistoryNavigation->lastRun() > 0);
}

void Focus::refreshMeasuresDisplay()
{
    if (captureHistory(m_FocusView->currentAFRun()).size() <= 0)
    {
        m_FocusView->m_focusHistoryNavigation->iterOut->setText("--");
        HFROut->setText("--");
        FWHMOut->setText("--");
        starsOut->setText("--");
    }
    else
    {
        // display the iteration count during autofocus and afterwards
        if (inAutoFocus || captureHistory(m_FocusView->currentAFRun()).size() > 0)
            m_FocusView->m_focusHistoryNavigation->iterOut->setText(QString("Run #%1: %2/%3").arg(m_FocusView->currentAFRun()).arg(
                        captureHistory(
                            m_FocusView->currentAFRun()).position() + 1).arg(captureHistory(m_FocusView->currentAFRun()).size()));
        else
            m_FocusView->m_focusHistoryNavigation->iterOut->setText("--");

        HFROut->setText(QString("%1").arg(currentFrame().hfr * getStarUnits(m_StarMeasure, m_StarUnits), 0, 'f', 2));
        if (m_StarMeasure == FOCUS_STAR_FWHM)
            FWHMOut->setText(QString("%1").arg(currentFrame().fwhm * getStarUnits(m_StarMeasure, m_StarUnits), 0, 'f', 2));
        starsOut->setText(QString("%1").arg(currentFrame().numStars));
    }
    // disable focus history navigation if no history is present or no focus frames are stored
    refreshHistoryNavigation(m_FocusView->lastAFRun() > 0 && Options::maxFocusFrameFiles() != 0, true);
}

// Return whether the Aberration Inspector Start button should be enabled. The pre-requisties are:
// - Absolute position focuser
// - Mosaic Mask is on
// - Algorithm is LINEAR 1 PASS
// - Curve fit is not Quadratic (which uses an old curve fitting approach not enabled for Aberration Inspector)
bool Focus::canAbInsStart()
{
    return canAbsMove && m_FocusAlgorithm == FOCUS_LINEAR1PASS && m_currentImageMask == FOCUS_MASK_MOSAIC
           && m_CurveFit != CurveFitting::FOCUS_QUADRATIC;
}

// Disable input widgets during an Autofocus run. Keep a record so after the AF run, widgets can be re-enabled
void Focus::AFDisable(QWidget * widget, const bool children)
{
    if (children)
    {
        // The parent widget has been passed in so disable any enabled child widgets
        for(auto *wid : widget->findChildren<QWidget *>())
        {
            if (wid->isEnabled())
            {
                wid->setEnabled(false);
                disabledWidgets.push_back(wid);
            }
        }

    }
    else if (widget->isEnabled())
    {
        // Base level widget or group of widgets, so just disable the passed what was passed in
        widget->setEnabled(false);
        disabledWidgets.push_back(widget);
    }
}

bool Focus::isFocusGainEnabled()
{
    return (inAutoFocus) ? m_FocusGainAFEnabled : focusGain->isEnabled();
}

bool Focus::isFocusISOEnabled()
{
    return (inAutoFocus) ?  m_FocusISOAFEnabled : focusISO->isEnabled();
}

bool Focus::isFocusSubFrameEnabled()
{
    return (inAutoFocus) ? m_FocusSubFrameAFEnabled : m_OpsFocusSettings->focusSubFrame->isEnabled();
}

void Focus::updateBoxSize(int value)
{
    if (m_Camera == nullptr)
        return;

    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);

    if (targetChip == nullptr)
        return;

    int subBinX, subBinY;
    targetChip->getBinning(&subBinX, &subBinY);

    QRect trackBox = m_FocusView->getTrackingBox();
    QPoint center(trackBox.x() + (trackBox.width() / 2), trackBox.y() + (trackBox.height() / 2));

    trackBox =
        QRect(center.x() - value / (2 * subBinX), center.y() - value / (2 * subBinY), value / subBinX, value / subBinY);

    m_FocusView->setTrackingBox(trackBox);
}

void Focus::selectFocusStarFraction(double x, double y)
{
    if (m_ImageData.isNull())
        return;

    focusStarSelected(x * m_ImageData->width(), y * m_ImageData->height());
    // Focus view timer takes 50ms second to update, so let's emit afterwards.
    QTimer::singleShot(250, this, [this]()
    {
        emit newImage(m_FocusView);
    });
}

void Focus::focusStarSelected(int x, int y)
{
    if (state() == Ekos::FOCUS_PROGRESS)
        return;

    if (subFramed == false)
    {
        rememberStarCenter.setX(x);
        rememberStarCenter.setY(y);
    }

    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);

    int subBinX, subBinY;
    targetChip->getBinning(&subBinX, &subBinY);

    // If binning was changed outside of the focus module, recapture
    if (subBinX != (focusBinning->currentIndex() + 1))
    {
        capture();
        return;
    }

    int offset = (static_cast<double>(m_OpsFocusSettings->focusBoxSize->value()) / subBinX) * 1.5;

    QRect starRect;

    bool squareMovedOutside = false;

    if (subFramed == false && m_OpsFocusSettings->focusSubFrame->isChecked() && targetChip->canSubframe())
    {
        // JEE - for now just save the x, y of the mouse click. Should check that its within bounds of the chip.
        if (!isStarMeasureStarBased())
        {
            starCenter.setX(x);
            starCenter.setY(y);
        }

        int minX, maxX, minY, maxY, minW, maxW, minH, maxH; //, fx,fy,fw,fh;

        targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);
        //targetChip->getFrame(&fx, &fy, &fw, &fy);

        x     = (x - offset) * subBinX;
        y     = (y - offset) * subBinY;
        int w = offset * 2 * subBinX;
        int h = offset * 2 * subBinY;

        if (x < minX)
            x = minX;
        if (y < minY)
            y = minY;
        if ((x + w) > maxW)
            w = maxW - x;
        if ((y + h) > maxH)
            h = maxH - y;

        //fx += x;
        //fy += y;
        //fw = w;
        //fh = h;

        //targetChip->setFocusFrame(fx, fy, fw, fh);
        //frameModified=true;

        QVariantMap settings = frameSettings[targetChip];
        settings["x"]        = x;
        settings["y"]        = y;
        settings["w"]        = w;
        settings["h"]        = h;
        settings["binx"]     = subBinX;
        settings["biny"]     = subBinY;

        frameSettings[targetChip] = settings;

        subFramed = true;

        qCDebug(KSTARS_EKOS_FOCUS) << "Frame is subframed. X:" << x << "Y:" << y << "W:" << w << "H:" << h << "binX:" << subBinX <<
                                   "binY:" << subBinY;

        m_FocusView->setFirstLoad(true);

        capture();

        if (isStarMeasureStarBased())
        {
            starCenter.setX(w / (2 * subBinX));
            starCenter.setY(h / (2 * subBinY));
        }
    }
    else
    {
        // JEE Probbaly need to call syncTrackingBoxPosition() for non-star based measures
        //starRect = QRect(x-focusBoxSize->value()/(subBinX*2), y-focusBoxSize->value()/(subBinY*2), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY);
        double dist = sqrt((starCenter.x() - x) * (starCenter.x() - x) + (starCenter.y() - y) * (starCenter.y() - y));

        squareMovedOutside = (dist > (static_cast<double>(m_OpsFocusSettings->focusBoxSize->value()) / subBinX));
        starCenter.setX(x);
        starCenter.setY(y);
        //starRect = QRect( starCenter.x()-focusBoxSize->value()/(2*subBinX), starCenter.y()-focusBoxSize->value()/(2*subBinY), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY);
        starRect = QRect(starCenter.x() - m_OpsFocusSettings->focusBoxSize->value() / (2 * subBinX),
                         starCenter.y() - m_OpsFocusSettings->focusBoxSize->value() / (2 * subBinY),
                         m_OpsFocusSettings->focusBoxSize->value() / subBinX,
                         m_OpsFocusSettings->focusBoxSize->value() / subBinY);
        m_FocusView->setTrackingBox(starRect);
    }

    starsHFR.clear();

    starCenter.setZ(subBinX);

    if (squareMovedOutside && inAutoFocus == false && m_OpsFocusSettings->focusAutoStarEnabled->isChecked())
    {
        m_OpsFocusSettings->focusAutoStarEnabled->blockSignals(true);
        m_OpsFocusSettings->focusAutoStarEnabled->setChecked(false);
        m_OpsFocusSettings->focusAutoStarEnabled->blockSignals(false);
        appendLogText(i18n("Disabling Auto Star Selection as star selection box was moved manually."));
        starSelected = false;
    }
    else if (starSelected == false)
    {
        if (isStarMeasureStarBased())
            appendLogText(i18n("Focus star is selected."));
        else
            appendLogText(i18n("Region of interest is selected."));

        starSelected = true;
        capture();
    }

    waitStarSelectTimer.stop();
    FocusState nextState = inAutoFocus ? FOCUS_PROGRESS : FOCUS_IDLE;
    if (nextState != state())
    {
        setState(nextState);
    }
}

void Focus::checkFocus(double requiredHFR)
{
    if (inAutoFocus || inAdjustFocus || adaptFocus->inAdaptiveFocus())
    {
        QString str;
        if (inAutoFocus)
            str = i18n("Autofocus");
        else if (inAdjustFocus)
            str = i18n("Adjust Focus");
        else
            str = i18n("Adaptive Focus");

        if (++m_StartRetries < MAXIMUM_RESTART_ITERATIONS)
        {
            appendLogText(i18n("Check focus request - Waiting 10sec for %1 to complete.", str));
            QTimer::singleShot(10 * 1000, this, [this, requiredHFR]()
            {
                checkFocus(requiredHFR);
            });
            return;
        }

        appendLogText(i18n("Discarding Check Focus request - %1 in progress.", str));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_INTERNAL);
        return;
    }

    m_StartRetries = 0;

    if (inFocusLoop || inBuildOffsets)
    {
        QString str = inFocusLoop ? "Focus Looping" : " Build Offsets";
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Check Focus rejected, %1 is already running.").arg(str);
    }
    else if (!isStarMeasureStarBased())
        appendLogText(i18n("Check Focus ignored - Star Measure must be star based"));
    else
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "Check Focus requested with minimum required HFR" << requiredHFR;
        minimumRequiredHFR = requiredHFR;
        appendLogText(i18n("Capturing to check HFR..."));
        capture();
    }
}

void Focus::toggleSubframe(bool enable)
{
    if (enable == false)
        resetFrame();

    m_FocusView->setTrackingBoxEnabled(enable);
    starSelected = false;
    starCenter   = QVector3D();

    if (enable)
        // sub frame focusing - disable focus image mask
        m_OpsFocusSettings->focusNoMaskRB->setChecked(true);

    // update image mask controls
    selectImageMask();
    // enable focus mask selection if full field is selected
    m_OpsFocusSettings->focusRingMaskRB->setEnabled(!enable);
    m_OpsFocusSettings->focusMosaicMaskRB->setEnabled(!enable);

    setUseWeights();
    setDenoise();
    setAutoStarAndBox();
}

// Set the useWeights widget based on various other user selected parameters
// 1. weights are only available with the LM solver used by Hyperbola, Parabola & Gaussian
// 2. weights are only used for multiple stars so only if full frame is selected
// 3. weights are only used for star measures involving multiple star measures: HFR, HFR_ADJ and FWHM
void Focus::setUseWeights()
{
    if (m_CurveFit == CurveFitting::FOCUS_QUADRATIC || !m_OpsFocusSettings->focusUseFullField->isChecked()
            || m_StarMeasure == FOCUS_STAR_NUM_STARS || m_StarMeasure == FOCUS_STAR_FOURIER_POWER ||
            m_StarMeasure == FOCUS_STAR_STDDEV || m_StarMeasure == FOCUS_STAR_SOBEL ||
            m_StarMeasure == FOCUS_STAR_LAPLASSIAN || m_StarMeasure == FOCUS_STAR_CANNY)
    {
        m_OpsFocusProcess->focusUseWeights->setEnabled(false);
        m_OpsFocusProcess->focusUseWeights->setChecked(false);
    }
    else
        m_OpsFocusProcess->focusUseWeights->setEnabled(true);

}

// Set the focusDenoise widget based on various other user selected parameters
// 1. denoise is only availabel for the openCV star measures
// 2. denoise and useWeights are mutually exclusive
void Focus::setDenoise()
{
    if (m_StarMeasure == FOCUS_STAR_STDDEV || m_StarMeasure == FOCUS_STAR_SOBEL ||
            m_StarMeasure == FOCUS_STAR_LAPLASSIAN || m_StarMeasure == FOCUS_STAR_CANNY)
        m_OpsFocusProcess->focusDenoise->setEnabled(true);
    else
    {
        m_OpsFocusProcess->focusDenoise->setEnabled(false);
        m_OpsFocusProcess->focusDenoise->setChecked(false);
    }
}

// Set the setDonutBuster widget based on various other user selected parameters
// 1. Donut Buster is only available for algorithm: Linear 1 Pass
// 2. Donut Buster is available for measures: HFR, HFR Adj and FWHM
// 3. Donut Buster is available for walks: Fixed and CFZ Shuffle
// 4. Donut Buster is available for curves fits: Hyperbola and Parabola
void Focus::setDonutBuster()
{
    if (m_FocusAlgorithm != FOCUS_LINEAR1PASS)
    {
        m_OpsFocusProcess->focusDonut->hide();
        m_OpsFocusProcess->focusDonut->setEnabled(false);
        m_OpsFocusProcess->focusDonut->setChecked(false);
    }
    else
    {
        m_OpsFocusProcess->focusDonut->show();
        if ((m_StarMeasure == FOCUS_STAR_HFR || m_StarMeasure == FOCUS_STAR_HFR_ADJ || m_StarMeasure == FOCUS_STAR_FWHM) &&
                (m_FocusWalk == FOCUS_WALK_FIXED_STEPS || m_FocusWalk == FOCUS_WALK_CFZ_SHUFFLE) &&
                (m_CurveFit != CurveFitting::FOCUS_QUADRATIC))
            m_OpsFocusProcess->focusDonut->setEnabled(true);
        else
        {
            m_OpsFocusProcess->focusDonut->setEnabled(false);
            m_OpsFocusProcess->focusDonut->setChecked(false);
        }
    }
}

void Focus::setExposure(double value)
{
    focusExposure->setValue(value);
}

void Focus::setBinning(int subBinX, int subBinY)
{
    INDI_UNUSED(subBinY);
    focusBinning->setCurrentIndex(subBinX - 1);
}

void Focus::setAutoStarEnabled(bool enable)
{
    m_OpsFocusSettings->focusAutoStarEnabled->setChecked(enable);
}

void Focus::setAutoSubFrameEnabled(bool enable)
{
    m_OpsFocusSettings->focusSubFrame->setChecked(enable);
}

void Focus::setAutoFocusParameters(int boxSize, int stepSize, int maxTravel, double tolerance)
{
    m_OpsFocusSettings->focusBoxSize->setValue(boxSize);
    m_OpsFocusMechanics->focusTicks->setValue(stepSize);
    m_OpsFocusMechanics->focusMaxTravel->setValue(maxTravel);
    m_OpsFocusProcess->focusTolerance->setValue(tolerance);
}

void Focus::checkAutoStarTimeout()
{
    //if (starSelected == false && inAutoFocus)
    if (starCenter.isNull() && (inAutoFocus || minimumRequiredHFR > 0))
    {
        if (inAutoFocus)
        {
            if (rememberStarCenter.isNull() == false)
            {
                focusStarSelected(rememberStarCenter.x(), rememberStarCenter.y());
                appendLogText(i18n("No star was selected. Using last known position..."));
                return;
            }
        }

        initialFocuserAbsPosition = -1;
        appendLogText(i18n("No star was selected. Aborting..."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_NO_STARS);
    }
    else if (state() == FOCUS_WAITING)
        setState(FOCUS_IDLE);
}

void Focus::setAbsoluteFocusTicks()
{
    if (absTicksSpin->value() == currentPosition)
    {
        appendLogText(i18n("Focuser already at %1...", currentPosition));
        return;
    }
    focusInB->setEnabled(false);
    focusOutB->setEnabled(false);
    startGotoB->setEnabled(false);
    if (!changeFocus(absTicksSpin->value() - currentPosition))
        qCDebug(KSTARS_EKOS_FOCUS) << "setAbsoluteFocusTicks unable to move focuser.";
}

void Focus::syncTrackingBoxPosition()
{
    if (m_OpsFocusSettings->focusUseFullField->isChecked() || starCenter.isNull())
        return;

    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    Q_ASSERT(targetChip);

    int subBinX = 1, subBinY = 1;
    targetChip->getBinning(&subBinX, &subBinY);

    double boxSize = m_OpsFocusSettings->focusBoxSize->value();
    int x, y, w, h;
    targetChip->getFrame(&x, &y, &w, &h);
    // If box size is larger than image size, set it to lower index
    if (boxSize / subBinX >= w || boxSize / subBinY >= h)
    {
        m_OpsFocusSettings->focusBoxSize->setValue((boxSize / subBinX >= w) ? w : h);
        return;
    }

    // If binning changed, update coords accordingly
    if (subBinX != starCenter.z())
    {
        if (starCenter.z() > 0)
        {
            starCenter.setX(starCenter.x() * (starCenter.z() / subBinX));
            starCenter.setY(starCenter.y() * (starCenter.z() / subBinY));
        }

        starCenter.setZ(subBinX);
    }

    QRect starRect = QRect();
    if (isStarMeasureStarBased())
        starRect = QRect(starCenter.x() - boxSize / (2 * subBinX), starCenter.y() - boxSize / (2 * subBinY),
                         boxSize / subBinX, boxSize / subBinY);
    else
    {
        // get frame parameters and ensure tracking box remains within limits
        int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
        targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);
        int boxTLX = starCenter.x() - boxSize / (2 * subBinX);
        boxTLX = std::max(boxTLX, minX);
        int boxTLY = starCenter.y() - boxSize / (2 * subBinY);
        boxTLY = std::max(boxTLY, minY);
        const int boxBRX = boxTLX + (boxSize / subBinX);
        if (boxBRX > maxX)
            boxTLX -= boxBRX - maxX;
        const int boxBRY = boxTLY + (boxSize / subBinY);
        if (boxBRY > maxY)
            boxTLY -= boxBRY - maxY;

        starRect = QRect(boxTLX, boxTLY, boxSize / subBinX, boxSize / subBinY);
    }
    m_FocusView->setTrackingBoxEnabled(true);
    m_FocusView->setTrackingBox(starRect);
}

void Focus::showFITSViewer()
{
    static int lastFVTabID = -1;
    if (m_ImageData)
    {
        QUrl url = QUrl::fromLocalFile("focus.fits");
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

void Focus::adjustFocusOffset(int value, bool useAbsoluteOffset)
{
    // Allow focuser adjustments during looping & autofocus to honour Filter Offsets
    if (inAdjustFocus || adaptFocus->inAdaptiveFocus())
    {
        QString str = inAdjustFocus ? i18n("Adjust Focus") : i18n("Adaptive Focus");
        if (++m_StartRetries < MAXIMUM_RESTART_ITERATIONS)
        {
            appendLogText(i18n("Adjust focus request - Waiting 10sec for %1 to complete.", str));
            QTimer::singleShot(10 * 1000, this, [this, value, useAbsoluteOffset] ()
            {
                adjustFocusOffset(value, useAbsoluteOffset);
            });
            return;
        }

        qCDebug(KSTARS_EKOS_FOCUS) << "adjustFocusOffset called whilst" << str << "in progress. Ignoring...";
        emit focusPositionAdjusted();
        return;
    }

    m_StartRetries = 0;
    inAdjustFocus = true;

    // Get the new position
    int newPosition = (useAbsoluteOffset) ? value : value + currentPosition;

    if (!changeFocus(newPosition - currentPosition))
        qCDebug(KSTARS_EKOS_FOCUS) << "adjustFocusOffset unable to move focuser";
}

void Focus::toggleFocusingWidgetFullScreen()
{
    if (focusingWidget->parent() == nullptr)
    {
        focusingWidget->setParent(this);
        rightLayout->insertWidget(0, focusingWidget);
        focusingWidget->showNormal();
    }
    else
    {
        focusingWidget->setParent(nullptr);
        focusingWidget->setWindowTitle(i18nc("@title:window", "Focus Frame"));
        focusingWidget->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
        focusingWidget->showMaximized();
        focusingWidget->show();
    }
}

void Focus::setMountStatus(ISD::Mount::Status newState)
{
    switch (newState)
    {
        case ISD::Mount::MOUNT_PARKING:
        case ISD::Mount::MOUNT_SLEWING:
        case ISD::Mount::MOUNT_MOVING:
            captureB->setEnabled(false);
            startFocusB->setEnabled(false);
            startAbInsB->setEnabled(false);
            startLoopB->setEnabled(false);

            // If mount is moved while we have a star selected and subframed
            // let us reset the frame.
            if (subFramed)
                resetFrame();

            break;

        default:
            resetButtons();
            break;
    }
}

void Focus::setMountCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha)
{
    Q_UNUSED(pierSide)
    Q_UNUSED(ha)
    mountAlt = position.alt().Degrees();
}

void Focus::removeDevice(const QSharedPointer<ISD::GenericDevice> &deviceRemoved)
{
    auto name = deviceRemoved->getDeviceName();

    // Check in Focusers

    if (m_Focuser && m_Focuser->getDeviceName() == name)
    {
        m_Focuser->disconnect(this);
        m_Focuser = nullptr;
        QTimer::singleShot(1000, this, [this]()
        {
            checkFocuser();
            resetButtons();
        });
    }

    // Check in Temperature Sources.
    for (auto &oneSource : m_TemperatureSources)
    {
        if (oneSource->getDeviceName() == name)
        {
            // clear reference to avoid runtime exception in checkTemperatureSource()
            if (m_LastSourceDeviceAutofocusTemperature && m_LastSourceDeviceAutofocusTemperature->getDeviceName() == name)
                m_LastSourceDeviceAutofocusTemperature.reset(nullptr);

            m_TemperatureSources.removeAll(oneSource);
            QTimer::singleShot(1000, this, [this, name]()
            {
                defaultFocusTemperatureSource->removeItem(defaultFocusTemperatureSource->findText(name));
            });
            break;
        }
    }

    // Check camera
    if (m_Camera && m_Camera->getDeviceName() == name)
    {
        m_Camera->disconnect(this);
        m_Camera = nullptr;

        QTimer::singleShot(1000, this, [this]()
        {
            checkCamera();
            resetButtons();
        });
    }

    // Check Filter
    if (m_FilterWheel && m_FilterWheel->getDeviceName() == name)
    {
        m_FilterWheel->disconnect(this);
        m_FilterWheel = nullptr;

        QTimer::singleShot(1000, this, [this]()
        {
            checkFilter();
            resetButtons();
        });
    }
}

void Focus::setupFilterManager()
{
    // Do we have an existing filter manager?
    if (m_FilterManager)
        m_FilterManager->disconnect(this);

    // Create new or refresh device
    Ekos::Manager::Instance()->createFilterManager(m_FilterWheel);

    // Return global filter manager for this filter wheel.
    Ekos::Manager::Instance()->getFilterManager(m_FilterWheel->getDeviceName(), m_FilterManager);

    // Focus Module ----> Filter Manager connections

    // Update focuser absolute position.
    connect(this, &Focus::absolutePositionChanged, m_FilterManager.get(), &FilterManager::setFocusAbsolutePosition);
    // Set initial focuser position if we have one
    if (m_Focuser && m_Focuser->isConnected() && currentPosition >= 0)
        m_FilterManager->setFocusAbsolutePosition(currentPosition);

    // Update Filter Manager state
    connect(this, &Focus::newStatus, this, [this](Ekos::FocusState state, const QString trainname, const bool update)
    {
        Q_UNUSED(trainname);
        if (m_FilterManager)
        {
            m_FilterManager->setFocusStatus(state);
            if (update && focusFilter->currentIndex() != -1 && canAbsMove && state == Ekos::FOCUS_COMPLETE)
            {
                m_FilterManager->setFilterAbsoluteFocusDetails(focusFilter->currentIndex(), currentPosition,
                        m_LastSourceAutofocusTemperature, m_LastSourceAutofocusAlt);
            }
        }
    });

    // Filter Manager ----> Focus Module connections

    // Suspend guiding if filter offset is change with OAG
    connect(m_FilterManager.get(), &FilterManager::newStatus, this, [this](Ekos::FilterState filterState)
    {
        // If we are changing filter offset while idle, then check if we need to suspend guiding.
        if (filterState == FILTER_OFFSET && state() != Ekos::FOCUS_PROGRESS)
        {
            if (m_GuidingSuspended == false && m_OpsFocusSettings->focusSuspendGuiding->isChecked())
            {
                m_GuidingSuspended = true;
                emit suspendGuiding();
            }
        }
    });

    // Take action once filter manager completes filter position
    connect(m_FilterManager.get(), &FilterManager::ready, this, [this]()
    {
        // Keep the focusFilter widget consistent with the filter wheel
        if (focusFilter->currentIndex() != currentFilterPosition - 1)
            focusFilter->setCurrentIndex(currentFilterPosition - 1);

        if (filterPositionPending)
        {
            filterPositionPending = false;
            capture();
        }
        else if (fallbackFilterPending)
        {
            fallbackFilterPending = false;
            setState(m_pendingState);
            m_pendingState = FOCUS_IDLE;
        }
    });

    // Take action when filter operation fails
    connect(m_FilterManager.get(), &FilterManager::failed, this, [this]()
    {
        if (filterPositionPending)
        {
            appendLogText(i18n("Filter operation failed."));
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_FILTER_MANAGER);
        }
    });

    // Run Autofocus if required by filter manager
    connect(m_FilterManager.get(), &FilterManager::runAutoFocus, this, &Focus::runAutoFocus);

    // Abort Autofocus if required by filter manager
    connect(m_FilterManager.get(), &FilterManager::abortAutoFocus, this, &Focus::abort);

    // Adjust focus offset
    connect(m_FilterManager.get(), &FilterManager::newFocusOffset, this, &Focus::adjustFocusOffset);

    // Update labels
    connect(m_FilterManager.get(), &FilterManager::labelsChanged, this, [this]()
    {
        focusFilter->clear();
        focusFilter->addItems(m_FilterManager->getFilterLabels());
        currentFilterPosition = m_FilterManager->getFilterPosition();
        focusFilter->setCurrentIndex(currentFilterPosition - 1);
    });

    // Position changed
    connect(m_FilterManager.get(), &FilterManager::positionChanged, this, [this]()
    {
        currentFilterPosition = m_FilterManager->getFilterPosition();
        focusFilter->setCurrentIndex(currentFilterPosition - 1);
    });

    // Exposure Changed
    connect(m_FilterManager.get(), &FilterManager::exposureChanged, this, [this]()
    {
        focusExposure->setValue(m_FilterManager->getFilterExposure());
    });

    // Wavelength Changed
    connect(m_FilterManager.get(), &FilterManager::wavelengthChanged, this, [this]()
    {
        wavelengthChanged();
    });
}

void Focus::connectFilterManager()
{
    // Show filter manager if toggled.
    connect(filterManagerB, &QPushButton::clicked, this, [this]()
    {
        if (m_FilterManager)
        {
            m_FilterManager->refreshFilterModel();
            m_FilterManager->show();
            m_FilterManager->raise();
        }
    });

    // Resume guiding if suspended after focus position is adjusted.
    connect(this, &Focus::focusPositionAdjusted, this, [this]()
    {
        if (m_FilterManager)
            m_FilterManager->setFocusOffsetComplete();
        if (m_GuidingSuspended && state() != Ekos::FOCUS_PROGRESS)
        {
            QTimer::singleShot(m_OpsFocusMechanics->focusSettleTime->value() * 1000, this, [this]()
            {
                m_GuidingSuspended = false;
                emit resumeGuiding();
            });
        }
    });

    // Focuser move as part of AF Optimisation just completed so signal the end of the Autofocus
    connect(this, &Focus::focusAFOptimise, this, [this]()
    {
        completeFocusProcedure(Ekos::FOCUS_COMPLETE, Ekos::FOCUS_FAIL_OPTIMISED_OUT);
    });

    // Save focus exposure for a particular filter
    connect(focusExposure, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        // Don't save if donut processing is changing this field
        if (inAutoFocus && m_OpsFocusProcess->focusDonut->isEnabled())
            return;

        if (m_FilterManager)
            m_FilterManager->setFilterExposure(focusFilter->currentIndex(), focusExposure->value());
    });

    // Load exposure if filter is changed.
    connect(focusFilter, &QComboBox::currentTextChanged, this, [this](const QString & text)
    {
        if (m_FilterManager)
        {
            focusExposure->setValue(m_FilterManager->getFilterExposure(text));
            // Update the CFZ for the new filter - force all data back to OT & filter values
            resetCFZToOT();
        }
    });

}

void Focus::toggleVideo(bool enabled)
{
    if (m_Camera == nullptr)
        return;

    if (m_Camera->isBLOBEnabled() == false)
    {

        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL)
            m_Camera->setBLOBEnabled(true);
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, enabled]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                m_Camera->setVideoStreamEnabled(enabled);
            });
            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"));
        }
    }
    else
        m_Camera->setVideoStreamEnabled(enabled);
}

//void Focus::setWeatherData(const std::vector<ISD::Weather::WeatherData> &data)
//{
//    auto pos = std::find_if(data.begin(), data.end(), [](ISD::Weather::WeatherData oneEntry)
//    {
//        return (oneEntry.name == "WEATHER_TEMPERATURE");
//    });

//    if (pos != data.end())
//    {
//        updateTemperature(OBSERVATORY_TEMPERATURE, pos->value);
//    }
//}

void Focus::setVideoStreamEnabled(bool enabled)
{
    if (enabled)
    {
        liveVideoB->setChecked(true);
        liveVideoB->setIcon(QIcon::fromTheme("camera-on"));
    }
    else
    {
        liveVideoB->setChecked(false);
        liveVideoB->setIcon(QIcon::fromTheme("camera-ready"));
    }
}

void Focus::processCaptureTimeout()
{
    m_captureInProgress = false;

    if (m_abortInProgress)
    {
        // Timeout occured whilst trying to abort AF. So do no further processing - stop function will call abortExposure()
        qCDebug(KSTARS_EKOS_FOCUS) << "Abort AF: discarding frame in " << __FUNCTION__;
        return;
    }

    captureTimeoutCounter++;

    if (captureTimeoutCounter >= 3)
    {
        captureTimeoutCounter = 0;
        m_MissingCameraCounter = 0;
        captureTimeout.stop();
        appendLogText(i18n("Exposure timeout. Aborting..."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_CAPTURE_TIMEOUT);
    }
    else if (m_Camera)
    {
        appendLogText(i18n("Exposure timeout. Restarting exposure..."));
        ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
        targetChip->abortExposure();

        capture(focusExposure->value());
    }
    // Don't allow this to happen all night. We will repeat checking (most likely for activeCamera()
    // another 200s = 40 * 5s, but after that abort capture.
    else if (m_MissingCameraCounter < 40)
    {
        m_MissingCameraCounter++;
        qCDebug(KSTARS_EKOS_FOCUS) << "Unable to restart focus exposure as camera is missing, trying again in 5 seconds...";
        QTimer::singleShot(5000, this, &Focus::processCaptureTimeout);
    }
    else
    {
        m_MissingCameraCounter = 0;
        captureTimeoutCounter = 0;
        captureTimeout.stop();
        appendLogText(i18n("Exposure timeout. Aborting..."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_CAPTURE_TIMEOUT);
    }

}

void Focus::processCaptureErrorDefault()
{
    processCaptureError(ISD::Camera::ERROR_CAPTURE);
}

void Focus::processCaptureError(ISD::Camera::ErrorType type)
{
    m_captureInProgress = false;
    captureTimeout.stop();

    if (m_abortInProgress)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "Abort AF: discarding frame in " << __FUNCTION__;
        return;
    }

    if (type == ISD::Camera::ERROR_SAVE)
    {
        appendLogText(i18n("Failed to save image. Aborting..."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_CAPTURE_FAILED);
        return;
    }


    if (m_Camera)
    {
        captureFailureCounter++;

        if (captureFailureCounter >= 3)
        {
            captureFailureCounter = 0;
            captureTimeoutCounter = 0;
            m_MissingCameraCounter = 0;
            appendLogText(i18n("Exposure failure. Aborting..."));
            completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_CAPTURE_FAILED);
            return;
        }

        ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
        targetChip->abortExposure();
        capture(focusExposure->value());
    }
    // Don't allow this to happen all night. We will repeat checking (most likely for activeCamera()
    // another 200s = 40 * 5s, but after that abort capture.
    else if (m_MissingCameraCounter < 40)
    {
        m_MissingCameraCounter++;
        qCDebug(KSTARS_EKOS_FOCUS) << "Unable to restart focus exposure as camera is missing, trying again in 5 seconds...";
        // We already know type is not ERROR_SAVE, so the value of type isn't used
        // and we can let it default to ERROR_CAPTURE.
        QTimer::singleShot(5000, this, &Focus::processCaptureErrorDefault);
    }
    else
    {
        m_MissingCameraCounter = 0;
        captureFailureCounter = 0;
        captureTimeoutCounter = 0;
        captureTimeout.stop();
        appendLogText(i18n("Exposure failure. Aborting..."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED, Ekos::FOCUS_FAIL_CAPTURE_FAILED);
    }
}

void Focus::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QGroupBox *gb = nullptr;
    QRadioButton *rb = nullptr;
    QComboBox *cbox = nullptr;
    QSplitter *s = nullptr;

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
        // refresh the visibility of the history navigation
        if (key == "maxFocusFrameFiles")
        {
            // enforce immediate setting the value
            Options::setMaxFocusFrameFiles(value.Int);
            refreshHistoryNavigation(value != 0);
        }
    }
    else if ( (cb = qobject_cast<QCheckBox * >(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (gb = qobject_cast<QGroupBox * >(sender())))
    {
        key = gb->objectName();
        value = gb->isChecked();
    }
    else if ( (rb = qobject_cast<QRadioButton * >(sender())))
    {
        key = rb->objectName();
        // Discard false requests
        if (rb->isChecked() == false)
        {
            m_Settings.remove(key);
            return;
        }
        value = true;
    }
    else if ( (cbox = qobject_cast<QComboBox * >(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (s = qobject_cast<QSplitter * >(sender())))
    {
        key = s->objectName();
        // Convert from the QByteArray to QString using Base64
        value = QString::fromUtf8(s->saveState().toBase64());
    }

    // Save immediately
    Options::self()->setProperty(key.toLatin1(), value);

    m_Settings[key] = value;
    m_GlobalSettings[key] = value;
    // propagate image mask attributes
    selectImageMask();

    m_DebounceTimer.start();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Focus::settleSettings()
{
    emit settingsUpdated(getAllSettings());
    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Focus, m_Settings);
}

void Focus::loadGlobalSettings()
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
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "Option" << key << "not found!";
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
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "Option" << key << "not found!";
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
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "Option" << key << "not found!";
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
        else if (key != forceInSeqAF->objectName())
            qCDebug(KSTARS_EKOS_FOCUS) << "Option" << key << "not found!";
    }

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox * >())
    {
        if (oneWidget->isCheckable())
        {
            key = oneWidget->objectName();
            value = Options::self()->property(key.toLatin1());
            if (value.isValid())
            {
                oneWidget->setChecked(value.toBool());
                settings[key] = value;
            }
            else
                qCDebug(KSTARS_EKOS_FOCUS) << "Option" << key << "not found!";
        }
    }

    // All Splitters
    for (auto &oneWidget : findChildren<QSplitter * >())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            // Convert the saved QString to a QByteArray using Base64
            auto valueBA = QByteArray::fromBase64(value.toString().toUtf8());
            oneWidget->restoreState(valueBA);
            settings[key] = valueBA;
        }
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "Option" << key << "not found!";
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
    // propagate image mask attributes
    selectImageMask();
    m_GlobalSettings = m_Settings = settings;
}

void Focus::checkMosaicMaskLimits()
{
    if (m_Camera == nullptr || m_Camera->isConnected() == false)
        return;
    auto targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    if (targetChip == nullptr || frameSettings.contains(targetChip) == false)
        return;
    auto settings = frameSettings[targetChip];

    // Watch out for invalid values.
    auto width = settings["w"].toInt();
    auto height = settings["h"].toInt();
    if (width == 0 || height == 0)
        return;

    // determine maximal square size
    auto min = std::min(width, height);
    // now check if the tile size is below this limit
    m_OpsFocusSettings->focusMosaicTileWidth->setMaximum(100 * min / (3 * width));
}

void Focus::connectSyncSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
        // Don't sync Optical Train combo
        if (oneWidget != opticalTrainCombo)
            connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Focus::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
        connect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Focus::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
        connect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Focus::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::Focus::syncSettings);

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox * >())
        if (oneWidget->isCheckable())
            connect(oneWidget, &QGroupBox::toggled, this, &Ekos::Focus::syncSettings);

    // All Splitters
    for (auto &oneWidget : findChildren<QSplitter * >())
        connect(oneWidget, &QSplitter::splitterMoved, this, &Ekos::Focus::syncSettings);

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
        connect(oneWidget, &QRadioButton::toggled, this, &Ekos::Focus::syncSettings);
}

void Focus::disconnectSyncSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Focus::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
        disconnect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Focus::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
        disconnect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Focus::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::Focus::syncSettings);

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox * >())
        if (oneWidget->isCheckable())
            disconnect(oneWidget, &QGroupBox::toggled, this, &Ekos::Focus::syncSettings);

    // All Splitters
    for (auto &oneWidget : findChildren<QSplitter * >())
        disconnect(oneWidget, &QSplitter::splitterMoved, this, &Ekos::Focus::syncSettings);

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
        disconnect(oneWidget, &QRadioButton::toggled, this, &Ekos::Focus::syncSettings);
}

void Focus::initPlots()
{
    connect(clearDataB, &QPushButton::clicked, this, &Ekos::Focus::clearDataRequested);

    profileDialog = new QDialog(this);
    profileDialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    QVBoxLayout *profileLayout = new QVBoxLayout(profileDialog);
    profileDialog->setWindowTitle(i18nc("@title:window", "Relative Profile"));
    profilePlot = new FocusProfilePlot(profileDialog);

    profileLayout->addWidget(profilePlot);
    profileDialog->setLayout(profileLayout);
    profileDialog->resize(400, 300);

    connect(relativeProfileB, &QPushButton::clicked, profileDialog, &QDialog::show);
    connect(this, &Ekos::Focus::newHFR, [this](double currentHFR, int pos)
    {
        Q_UNUSED(pos) profilePlot->drawProfilePlot(currentHFR);
    });
}

void Focus::initConnections()
{
    // How long do we wait until the user select a star?
    waitStarSelectTimer.setInterval(AUTO_STAR_TIMEOUT);
    connect(&waitStarSelectTimer, &QTimer::timeout, this, &Ekos::Focus::checkAutoStarTimeout);
    connect(liveVideoB, &QPushButton::clicked, this, &Ekos::Focus::toggleVideo);

    // Setup Debounce timer to limit over-activation of settings changes
    m_DebounceTimer.setInterval(500);
    m_DebounceTimer.setSingleShot(true);
    connect(&m_DebounceTimer, &QTimer::timeout, this, &Focus::settleSettings);

    // Show FITS Image in a new window
    showFITSViewerB->setIcon(QIcon::fromTheme("kstars_fitsviewer"));
    showFITSViewerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(showFITSViewerB, &QPushButton::clicked, this, &Ekos::Focus::showFITSViewer);

    // Toggle FITS View to full screen
    toggleFullScreenB->setIcon(QIcon::fromTheme("view-fullscreen"));
    toggleFullScreenB->setShortcut(Qt::Key_F4);
    toggleFullScreenB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(toggleFullScreenB, &QPushButton::clicked, this, &Ekos::Focus::toggleFocusingWidgetFullScreen);

    // delayed capturing for waiting the scope to settle
    captureTimer.setSingleShot(true);
    connect(&captureTimer, &QTimer::timeout, this, [this]()
    {
        capture();
    });

    // How long do we wait until an exposure times out and needs a retry?
    captureTimeout.setSingleShot(true);
    connect(&captureTimeout, &QTimer::timeout, this, &Ekos::Focus::processCaptureTimeout);

    // Start/Stop focus
    connect(startFocusB, &QPushButton::clicked, this, &Ekos::Focus::manualStart);
    connect(stopFocusB, &QPushButton::clicked, this, &Ekos::Focus::abort);

    // Focus IN/OUT
    connect(focusOutB, &QPushButton::clicked, this, &Ekos::Focus::handleFocusButtonEvent);
    connect(focusInB, &QPushButton::clicked, this, &Ekos::Focus::handleFocusButtonEvent);

    // Capture a single frame
    connect(captureB, &QPushButton::clicked, this, &Ekos::Focus::capture);
    // Start continuous capture
    connect(startLoopB, &QPushButton::clicked, this, &Ekos::Focus::startFraming);
    // Use a subframe when capturing
    connect(m_OpsFocusSettings->focusSubFrame, &QRadioButton::toggled, this, &Ekos::Focus::toggleSubframe);
    // Reset frame dimensions to default
    connect(resetFrameB, &QPushButton::clicked, this, &Ekos::Focus::resetFrame);

    // handle frame size changes
    connect(focusBinning, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Focus::checkMosaicMaskLimits);

    // Sync settings if the temperature source selection is updated.
    connect(defaultFocusTemperatureSource, &QComboBox::currentTextChanged, this, &Ekos::Focus::checkTemperatureSource);

    // Set focuser absolute position
    connect(startGotoB, &QPushButton::clicked, this, &Ekos::Focus::setAbsoluteFocusTicks);
    connect(stopGotoB, &QPushButton::clicked, this, [this]()
    {
        if (m_Focuser)
            m_Focuser->stop();
    });
    // Update the focuser box size used to enclose a star
    connect(m_OpsFocusSettings->focusBoxSize, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            &Ekos::Focus::updateBoxSize);

    // Setup the tools buttons
    connect(startAbInsB, &QPushButton::clicked, this, &Ekos::Focus::startAbIns);
    connect(cfzB, &QPushButton::clicked, this, [this]()
    {
        m_CFZDialog->show();
        m_CFZDialog->raise();
    });
    connect(advisorB, &QPushButton::clicked, this, [this]()
    {
        if (focusAdvisor)
        {
            focusAdvisor->setButtons(false);
            focusAdvisor->show();
            focusAdvisor->raise();
        }
    });

    connect(forceInSeqAF, &QCheckBox::toggled, this, [&](bool enabled)
    {
        emit inSequenceAF(enabled, opticalTrain());
    });

    // Update the focuser star detection if the detection algorithm selection changes.
    connect(m_OpsFocusProcess->focusDetection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&](int index)
    {
        setFocusDetection(static_cast<StarAlgorithm>(index));
    });

    // Update the focuser solution algorithm if the selection changes.
    connect(m_OpsFocusProcess->focusAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&](int index)
    {
        setFocusAlgorithm(static_cast<Algorithm>(index));
    });

    // Update the curve fit if the selection changes. Use the currentIndexChanged method rather than
    // activated as the former fires when the index is changed by the user AND if changed programmatically
    connect(m_OpsFocusProcess->focusCurveFit, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [&](int index)
    {
        setCurveFit(static_cast<CurveFitting::CurveFit>(index));
    });

    // Update the star measure if the selection changes
    connect(m_OpsFocusProcess->focusStarMeasure, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [&](int index)
    {
        setStarMeasure(static_cast<StarMeasure>(index));
    });

    // Update the star PSF if the selection changes
    connect(m_OpsFocusProcess->focusStarPSF, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [&](int index)
    {
        setStarPSF(static_cast<StarPSF>(index));
    });

    // Update the units (pixels or arcsecs) if the selection changes
    connect(m_OpsFocusSettings->focusUnits, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [&](int index)
    {
        setStarUnits(static_cast<StarUnits>(index));
    });

    // Update the walk if the selection changes
    connect(m_OpsFocusMechanics->focusWalk, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [&](int index)
    {
        setWalk(static_cast<FocusWalk>(index));
    });

    // Adaptive Focus on/off switch toggled
    connect(m_OpsFocusSettings->focusAdaptive, &QCheckBox::toggled, this, &Ekos::Focus::resetAdaptiveFocus);

    // Reset star center on auto star check toggle
    connect(m_OpsFocusSettings->focusAutoStarEnabled, &QCheckBox::toggled, this, [&](bool enabled)
    {
        if (enabled)
        {
            starCenter   = QVector3D();
            starSelected = false;
            m_FocusView->setTrackingBox(QRect());
        }
    });

    // CFZ Panel
    connect(m_CFZUI->resetToOTB, &QPushButton::clicked, this, &Ekos::Focus::resetCFZToOT);

    connect(m_CFZUI->focusCFZDisplayVCurve, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), this,
            &Ekos::Focus::calcCFZ);

    connect(m_CFZUI->focusCFZAlgorithm, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Focus::calcCFZ);

    connect(m_CFZUI->focusCFZTolerance, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Focus::calcCFZ);

    connect(m_CFZUI->focusCFZTau, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [ = ](double d)
    {
        Q_UNUSED(d);
        calcCFZ();
    });

    connect(m_CFZUI->focusCFZWavelength, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [ = ](int i)
    {
        Q_UNUSED(i);
        calcCFZ();
    });

    connect(m_CFZUI->focusCFZFNumber, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [ = ](double d)
    {
        Q_UNUSED(d);
        calcCFZ();
    });

    connect(m_CFZUI->focusCFZStepSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [ = ](double d)
    {
        Q_UNUSED(d);
        calcCFZ();
    });

    connect(m_CFZUI->focusCFZAperture, QOverload<int>::of(&QSpinBox::valueChanged), [ = ](int i)
    {
        Q_UNUSED(i);
        calcCFZ();
    });

    connect(m_CFZUI->focusCFZSeeing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [ = ](double d)
    {
        Q_UNUSED(d);
        calcCFZ();
    });
}

void Focus::setFocusDetection(StarAlgorithm starAlgorithm)
{
    static bool first = true;
    if (!first && m_FocusDetection == starAlgorithm)
        return;

    first = false;

    m_FocusDetection = starAlgorithm;

    // setFocusAlgorithm displays the appropriate widgets for the selection
    setFocusAlgorithm(m_FocusAlgorithm);
    setAutoStarAndBox();
    setDonutBuster();
}

// Controls the setting of focusAutoStarEnabled and focusBoxSize widgets
// Sub frame is selected (rather than full field)
// All m_FocusDetection except ALGORITHM_BAHTINOV
// m_StarMeasure must be star based
void Focus::setAutoStarAndBox()
{
    if (m_FocusDetection == ALGORITHM_BAHTINOV)
    {
        m_OpsFocusSettings->focusAutoStarEnabled->setEnabled(false);
        m_OpsFocusSettings->focusAutoStarEnabled->setChecked(false);
        m_OpsFocusSettings->focusBoxSize->setEnabled(m_OpsFocusSettings->focusSubFrame->isChecked());
        m_OpsFocusSettings->focusBoxSizeLabel->setEnabled(m_OpsFocusSettings->focusSubFrame->isChecked());
        m_OpsFocusSettings->focusBoxSize->setMaximum(512);
        if (m_OpsFocusSettings->focusBoxSize->value() > 512)
            m_OpsFocusSettings->focusBoxSize->setValue(m_OpsFocusSettings->focusBoxSize->value());
    }
    else if(m_OpsFocusSettings->focusSubFrame->isChecked())
    {
        if(isStarMeasureStarBased())
        {
            m_OpsFocusSettings->focusAutoStarEnabled->setEnabled(true);

            m_OpsFocusSettings->focusBoxSize->setEnabled(true);
            m_OpsFocusSettings->focusBoxSizeLabel->setEnabled(true);
            if (m_OpsFocusSettings->focusBoxSize->value() > 256)
                m_OpsFocusSettings->focusBoxSize->setValue(m_OpsFocusSettings->focusBoxSize->value());
            m_OpsFocusSettings->focusBoxSize->setMaximum(256);
        }
        else
        {
            m_OpsFocusSettings->focusAutoStarEnabled->setEnabled(false);
            m_OpsFocusSettings->focusAutoStarEnabled->setChecked(false);

            // For non-star based measures we cam make the box much bigger as it could encapsulate the moon, etc
            m_OpsFocusSettings->focusBoxSize->setEnabled(true);
            m_OpsFocusSettings->focusBoxSizeLabel->setEnabled(true);
            int maxSize = std::min(m_CcdWidth, m_CcdHeight);
            int step = m_OpsFocusSettings->focusBoxSize->singleStep();
            maxSize = std::max(256, (maxSize / step) * step);
            m_OpsFocusSettings->focusBoxSize->setMaximum(maxSize);
        }
    }
    else
    {
        // Full field is set
        m_OpsFocusSettings->focusAutoStarEnabled->setEnabled(false);
        m_OpsFocusSettings->focusAutoStarEnabled->setChecked(false);
        m_OpsFocusSettings->focusBoxSize->setEnabled(false);
        m_OpsFocusSettings->focusBoxSizeLabel->setEnabled(false);
    }

}

void Focus::setFocusAlgorithm(Algorithm algorithm)
{
    m_FocusAlgorithm = algorithm;
    switch(algorithm)
    {
        case FOCUS_ITERATIVE:
            // Remove unused widgets from the grid and hide them
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusMultiRowAverageLabel);
            m_OpsFocusProcess->focusMultiRowAverageLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusMultiRowAverage);
            m_OpsFocusProcess->focusMultiRowAverage->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianSigmaLabel);
            m_OpsFocusProcess->focusGaussianSigmaLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianSigma);
            m_OpsFocusProcess->focusGaussianSigma->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianKernelSizeLabel);
            m_OpsFocusProcess->focusGaussianKernelSizeLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianKernelSize);
            m_OpsFocusProcess->focusGaussianKernelSize->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarMeasureLabel);
            m_OpsFocusProcess->focusStarMeasureLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarMeasure);
            m_OpsFocusProcess->focusStarMeasure->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSFLabel);
            m_OpsFocusProcess->focusStarPSFLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSF);
            m_OpsFocusProcess->focusStarPSF->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusUseWeights);
            m_OpsFocusProcess->focusUseWeights->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusR2LimitLabel);
            m_OpsFocusProcess->focusR2LimitLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusR2Limit);
            m_OpsFocusProcess->focusR2Limit->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusRefineCurveFit);
            m_OpsFocusProcess->focusRefineCurveFit->hide();
            m_OpsFocusProcess->focusRefineCurveFit->setChecked(false);

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusToleranceLabel);
            m_OpsFocusProcess->focusToleranceLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusTolerance);
            m_OpsFocusProcess->focusTolerance->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusThresholdLabel);
            m_OpsFocusProcess->focusThresholdLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusThreshold);
            m_OpsFocusProcess->focusThreshold->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusCurveFitLabel);
            m_OpsFocusProcess->focusCurveFitLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusCurveFit);
            m_OpsFocusProcess->focusCurveFit->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusDenoise);
            m_OpsFocusProcess->focusDenoise->hide();

            // Donut Buster not available
            m_OpsFocusProcess->focusDonut->hide();
            m_OpsFocusProcess->focusDonut->setChecked(false);

            // Scan Start Pos not available
            m_OpsFocusProcess->focusScanStartPos->hide();
            m_OpsFocusProcess->focusScanStartPos->setChecked(false);

            // Although CurveFit is not used by Iterative setting to Quadratic will configure other widgets
            m_OpsFocusProcess->focusCurveFit->setCurrentIndex(CurveFitting::FOCUS_QUADRATIC);

            // Set Measure to just HFR
            if (m_OpsFocusProcess->focusStarMeasure->count() != 1)
            {
                m_OpsFocusProcess->focusStarMeasure->clear();
                m_OpsFocusProcess->focusStarMeasure->addItem(m_StarMeasureText.at(FOCUS_STAR_HFR));
                m_OpsFocusProcess->focusStarMeasure->setCurrentIndex(FOCUS_STAR_HFR);
            }

            // Add necessary widgets to the grid
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusToleranceLabel, 3, 0);
            m_OpsFocusProcess->focusToleranceLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusTolerance, 3, 1);
            m_OpsFocusProcess->focusTolerance->show();

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusFramesCountLabel, 3, 2);
            m_OpsFocusProcess->focusFramesCountLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusFramesCount, 3, 3);
            m_OpsFocusProcess->focusFramesCount->show();

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusHFRFramesCountLabel, 4, 2);
            m_OpsFocusProcess->focusHFRFramesCountLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusHFRFramesCount, 4, 3);
            m_OpsFocusProcess->focusHFRFramesCount->show();

            if (m_FocusDetection == ALGORITHM_THRESHOLD)
            {
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusThresholdLabel, 5, 0);
                m_OpsFocusProcess->focusThresholdLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusThreshold, 5, 1);
                m_OpsFocusProcess->focusThreshold->show();
            }
            else if (m_FocusDetection == ALGORITHM_BAHTINOV)
            {
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusMultiRowAverageLabel, 5, 0);
                m_OpsFocusProcess->focusMultiRowAverageLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusMultiRowAverage, 5, 1);
                m_OpsFocusProcess->focusMultiRowAverage->show();

                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianSigmaLabel, 5, 2);
                m_OpsFocusProcess->focusGaussianSigmaLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianSigma, 5, 3);
                m_OpsFocusProcess->focusGaussianSigma->show();

                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianKernelSizeLabel, 6, 0);
                m_OpsFocusProcess->focusGaussianKernelSizeLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianKernelSize, 6, 1);
                m_OpsFocusProcess->focusGaussianKernelSize->show();
            }

            // Aberration Inspector button
            startAbInsB->setEnabled(canAbInsStart());

            // Settings changes
            // Disable adaptive focus
            m_OpsFocusSettings->focusAdaptive->setChecked(false);
            m_OpsFocusSettings->focusAdaptStart->setChecked(false);
            m_OpsFocusSettings->adaptiveFocusGroup->setEnabled(false);
            // Disable Autofocus optimisation
            m_OpsFocusSettings->focusAFOptimize->setValue(0);
            m_OpsFocusSettings->focusAFOptimize->setEnabled(false);

            // Mechanics changes
            m_OpsFocusMechanics->focusMaxSingleStep->setEnabled(true);
            m_OpsFocusMechanics->focusOutSteps->setEnabled(false);

            // Set Walk to just Classic on 1st time through
            if (m_OpsFocusMechanics->focusWalk->count() != 1)
            {
                m_OpsFocusMechanics->focusWalk->clear();
                m_OpsFocusMechanics->focusWalk->addItem(m_FocusWalkText.at(FOCUS_WALK_CLASSIC));
                m_OpsFocusMechanics->focusWalk->setCurrentIndex(FOCUS_WALK_CLASSIC);
            }
            break;

        case FOCUS_POLYNOMIAL:
            // Remove unused widgets from the grid and hide them
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusMultiRowAverageLabel);
            m_OpsFocusProcess->focusMultiRowAverageLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusMultiRowAverage);
            m_OpsFocusProcess->focusMultiRowAverage->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianSigmaLabel);
            m_OpsFocusProcess->focusGaussianSigmaLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianSigma);
            m_OpsFocusProcess->focusGaussianSigma->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianKernelSizeLabel);
            m_OpsFocusProcess->focusGaussianKernelSizeLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianKernelSize);
            m_OpsFocusProcess->focusGaussianKernelSize->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSFLabel);
            m_OpsFocusProcess->focusStarPSFLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSF);
            m_OpsFocusProcess->focusStarPSF->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusUseWeights);
            m_OpsFocusProcess->focusUseWeights->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusR2LimitLabel);
            m_OpsFocusProcess->focusR2LimitLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusR2Limit);
            m_OpsFocusProcess->focusR2Limit->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusRefineCurveFit);
            m_OpsFocusProcess->focusRefineCurveFit->hide();
            m_OpsFocusProcess->focusRefineCurveFit->setChecked(false);

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusToleranceLabel);
            m_OpsFocusProcess->focusToleranceLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusTolerance);
            m_OpsFocusProcess->focusTolerance->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusThresholdLabel);
            m_OpsFocusProcess->focusThresholdLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusThreshold);
            m_OpsFocusProcess->focusThreshold->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusDenoise);
            m_OpsFocusProcess->focusDenoise->hide();

            // Donut buster not available
            m_OpsFocusProcess->focusDonut->hide();
            m_OpsFocusProcess->focusDonut->setChecked(false);

            // Scan Start Pos not available
            m_OpsFocusProcess->focusScanStartPos->hide();
            m_OpsFocusProcess->focusScanStartPos->setChecked(false);

            // Set Measure to just HFR
            if (m_OpsFocusProcess->focusStarMeasure->count() != 1)
            {
                m_OpsFocusProcess->focusStarMeasure->clear();
                m_OpsFocusProcess->focusStarMeasure->addItem(m_StarMeasureText.at(FOCUS_STAR_HFR));
                m_OpsFocusProcess->focusStarMeasure->setCurrentIndex(FOCUS_STAR_HFR);
            }

            // Add necessary widgets to the grid
            // Curve fit can only be QUADRATIC so only allow this value
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusCurveFitLabel, 1, 2);
            m_OpsFocusProcess->focusCurveFitLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusCurveFit, 1, 3);
            m_OpsFocusProcess->focusCurveFit->show();
            if (m_OpsFocusProcess->focusCurveFit->count() != 1)
            {
                m_OpsFocusProcess->focusCurveFit->clear();
                m_OpsFocusProcess->focusCurveFit->addItem(m_CurveFitText.at(CurveFitting::FOCUS_QUADRATIC));
                m_OpsFocusProcess->focusCurveFit->setCurrentIndex(CurveFitting::FOCUS_QUADRATIC);
            }

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusToleranceLabel, 3, 0);
            m_OpsFocusProcess->focusToleranceLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusTolerance, 3, 1);
            m_OpsFocusProcess->focusTolerance->show();

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusFramesCountLabel, 3, 2);
            m_OpsFocusProcess->focusFramesCountLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusFramesCount, 3, 3);
            m_OpsFocusProcess->focusFramesCount->show();

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusHFRFramesCountLabel, 4, 2);
            m_OpsFocusProcess->focusHFRFramesCountLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusHFRFramesCount, 4, 3);
            m_OpsFocusProcess->focusHFRFramesCount->show();

            if (m_FocusDetection == ALGORITHM_THRESHOLD)
            {
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusThresholdLabel, 5, 0);
                m_OpsFocusProcess->focusThresholdLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusThreshold, 5, 1);
                m_OpsFocusProcess->focusThreshold->show();
            }
            else if (m_FocusDetection == ALGORITHM_BAHTINOV)
            {
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusMultiRowAverageLabel, 5, 0);
                m_OpsFocusProcess->focusMultiRowAverageLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusMultiRowAverage, 5, 1);
                m_OpsFocusProcess->focusMultiRowAverage->show();

                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianSigmaLabel, 5, 2);
                m_OpsFocusProcess->focusGaussianSigmaLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianSigma, 5, 3);
                m_OpsFocusProcess->focusGaussianSigma->show();

                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianKernelSizeLabel, 6, 0);
                m_OpsFocusProcess->focusGaussianKernelSizeLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianKernelSize, 6, 1);
                m_OpsFocusProcess->focusGaussianKernelSize->show();
            }

            // Aberration Inspector button
            startAbInsB->setEnabled(canAbInsStart());

            // Settings changes
            // Disable adaptive focus
            m_OpsFocusSettings->focusAdaptive->setChecked(false);
            m_OpsFocusSettings->focusAdaptStart->setChecked(false);
            m_OpsFocusSettings->adaptiveFocusGroup->setEnabled(false);
            // Disable Autofocus optimisation
            m_OpsFocusSettings->focusAFOptimize->setValue(0);
            m_OpsFocusSettings->focusAFOptimize->setEnabled(false);

            // Mechanics changes
            m_OpsFocusMechanics->focusMaxSingleStep->setEnabled(true);
            m_OpsFocusMechanics->focusOutSteps->setEnabled(false);

            // Set Walk to just Classic on 1st time through
            if (m_OpsFocusMechanics->focusWalk->count() != 1)
            {
                m_OpsFocusMechanics->focusWalk->clear();
                m_OpsFocusMechanics->focusWalk->addItem(m_FocusWalkText.at(FOCUS_WALK_CLASSIC));
                m_OpsFocusMechanics->focusWalk->setCurrentIndex(FOCUS_WALK_CLASSIC);
            }
            break;

        case FOCUS_LINEAR:
            // Remove unused widgets from the grid and hide them
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusMultiRowAverageLabel);
            m_OpsFocusProcess->focusMultiRowAverageLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusMultiRowAverage);
            m_OpsFocusProcess->focusMultiRowAverage->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianSigmaLabel);
            m_OpsFocusProcess->focusGaussianSigmaLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianSigma);
            m_OpsFocusProcess->focusGaussianSigma->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianKernelSizeLabel);
            m_OpsFocusProcess->focusGaussianKernelSizeLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianKernelSize);
            m_OpsFocusProcess->focusGaussianKernelSize->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusThresholdLabel);
            m_OpsFocusProcess->focusThresholdLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusThreshold);
            m_OpsFocusProcess->focusThreshold->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSFLabel);
            m_OpsFocusProcess->focusStarPSFLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSF);
            m_OpsFocusProcess->focusStarPSF->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusUseWeights);
            m_OpsFocusProcess->focusUseWeights->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusR2LimitLabel);
            m_OpsFocusProcess->focusR2LimitLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusR2Limit);
            m_OpsFocusProcess->focusR2Limit->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusRefineCurveFit);
            m_OpsFocusProcess->focusRefineCurveFit->hide();
            m_OpsFocusProcess->focusRefineCurveFit->setChecked(false);

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusDenoise);
            m_OpsFocusProcess->focusDenoise->hide();

            // Donut buster not available
            m_OpsFocusProcess->focusDonut->hide();
            m_OpsFocusProcess->focusDonut->setChecked(false);

            // Scan Start Pos not available
            m_OpsFocusProcess->focusScanStartPos->hide();
            m_OpsFocusProcess->focusScanStartPos->setChecked(false);

            // Set Measure to just HFR
            if (m_OpsFocusProcess->focusStarMeasure->count() != 1)
            {
                m_OpsFocusProcess->focusStarMeasure->clear();
                m_OpsFocusProcess->focusStarMeasure->addItem(m_StarMeasureText.at(FOCUS_STAR_HFR));
                m_OpsFocusProcess->focusStarMeasure->setCurrentIndex(FOCUS_STAR_HFR);
            }

            // Add necessary widgets to the grid
            // For Linear the only allowable CurveFit is Quadratic
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusCurveFitLabel, 1, 2);
            m_OpsFocusProcess->focusCurveFitLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusCurveFit, 1, 3);
            m_OpsFocusProcess->focusCurveFit->show();
            if (m_OpsFocusProcess->focusCurveFit->count() != 1)
            {
                m_OpsFocusProcess->focusCurveFit->clear();
                m_OpsFocusProcess->focusCurveFit->addItem(m_CurveFitText.at(CurveFitting::FOCUS_QUADRATIC));
                m_OpsFocusProcess->focusCurveFit->setCurrentIndex(CurveFitting::FOCUS_QUADRATIC);
            }

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusToleranceLabel, 3, 0);
            m_OpsFocusProcess->focusToleranceLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusTolerance, 3, 1);
            m_OpsFocusProcess->focusTolerance->show();

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusFramesCountLabel, 3, 2);
            m_OpsFocusProcess->focusFramesCountLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusFramesCount, 3, 3);
            m_OpsFocusProcess->focusFramesCount->show();

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusHFRFramesCountLabel, 4, 2);
            m_OpsFocusProcess->focusHFRFramesCountLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusHFRFramesCount, 4, 3);
            m_OpsFocusProcess->focusHFRFramesCount->show();

            if (m_FocusDetection == ALGORITHM_THRESHOLD)
            {
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusThresholdLabel, 5, 0);
                m_OpsFocusProcess->focusThresholdLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusThreshold, 5, 1);
                m_OpsFocusProcess->focusThreshold->show();
            }
            else if (m_FocusDetection == ALGORITHM_BAHTINOV)
            {
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusMultiRowAverageLabel, 5, 0);
                m_OpsFocusProcess->focusMultiRowAverageLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusMultiRowAverage, 5, 1);
                m_OpsFocusProcess->focusMultiRowAverage->show();

                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianSigmaLabel, 5, 2);
                m_OpsFocusProcess->focusGaussianSigmaLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianSigma, 5, 3);
                m_OpsFocusProcess->focusGaussianSigma->show();

                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianKernelSizeLabel, 6, 0);
                m_OpsFocusProcess->focusGaussianKernelSizeLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianKernelSize, 6, 1);
                m_OpsFocusProcess->focusGaussianKernelSize->show();
            }

            // Aberration Inspector button
            startAbInsB->setEnabled(canAbInsStart());

            // Settings changes
            // Disable adaptive focus
            m_OpsFocusSettings->focusAdaptive->setChecked(false);
            m_OpsFocusSettings->focusAdaptStart->setChecked(false);
            m_OpsFocusSettings->adaptiveFocusGroup->setEnabled(false);
            // Enable Autofocus optimisation if Filter Manager set
            if (m_FilterManager)
                m_OpsFocusSettings->focusAFOptimize->setEnabled(true);
            else
            {
                m_OpsFocusSettings->focusAFOptimize->setValue(0);
                m_OpsFocusSettings->focusAFOptimize->setEnabled(false);
            }

            // Mechanics changes
            m_OpsFocusMechanics->focusMaxSingleStep->setEnabled(false);
            m_OpsFocusMechanics->focusOutSteps->setEnabled(true);

            // Set Walk to just Classic on 1st time through
            if (m_OpsFocusMechanics->focusWalk->count() != 1)
            {
                m_OpsFocusMechanics->focusWalk->clear();
                m_OpsFocusMechanics->focusWalk->addItem(m_FocusWalkText.at(FOCUS_WALK_CLASSIC));
                m_OpsFocusMechanics->focusWalk->setCurrentIndex(FOCUS_WALK_CLASSIC);
            }
            break;

        case FOCUS_LINEAR1PASS:
            // Remove unused widgets from the grid and hide them
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusMultiRowAverageLabel);
            m_OpsFocusProcess->focusMultiRowAverageLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusMultiRowAverage);
            m_OpsFocusProcess->focusMultiRowAverage->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianSigmaLabel);
            m_OpsFocusProcess->focusGaussianSigmaLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianSigma);
            m_OpsFocusProcess->focusGaussianSigma->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianKernelSizeLabel);
            m_OpsFocusProcess->focusGaussianKernelSizeLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusGaussianKernelSize);
            m_OpsFocusProcess->focusGaussianKernelSize->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusThresholdLabel);
            m_OpsFocusProcess->focusThresholdLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusThreshold);
            m_OpsFocusProcess->focusThreshold->hide();

            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusToleranceLabel);
            m_OpsFocusProcess->focusToleranceLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusTolerance);
            m_OpsFocusProcess->focusTolerance->hide();

            // Setup Measure with all options for detection=SEP and curveFit=HYPERBOLA or PARABOLA
            // or just HDR otherwise. Reset on 1st time through only
            if (m_FocusDetection == ALGORITHM_SEP && m_CurveFit != CurveFitting::FOCUS_QUADRATIC)
            {
                if (m_OpsFocusProcess->focusStarMeasure->count() != m_StarMeasureText.count())
                {
                    m_OpsFocusProcess->focusStarMeasure->clear();
                    m_OpsFocusProcess->focusStarMeasure->addItems(m_StarMeasureText);
                    m_OpsFocusProcess->focusStarMeasure->setCurrentIndex(FOCUS_STAR_HFR);
                }
                // If we don't have openCV installed grey out the unavailble star measures. As opposed to just
                // hiding these measures, this will remind folks that want to try these that they need to install openCV
#if !defined(HAVE_OPENCV)
                QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(m_OpsFocusProcess->focusStarMeasure->model());
                model->item(FOCUS_STAR_STDDEV, 0)->setEnabled(false);
                model->item(FOCUS_STAR_SOBEL, 0)->setEnabled(false);
                model->item(FOCUS_STAR_LAPLASSIAN, 0)->setEnabled(false);
                model->item(FOCUS_STAR_CANNY, 0)->setEnabled(false);
#endif
            }
            else if (m_FocusDetection != ALGORITHM_SEP || m_CurveFit == CurveFitting::FOCUS_QUADRATIC)
            {
                if (m_OpsFocusProcess->focusStarMeasure->count() != 1)
                {
                    m_OpsFocusProcess->focusStarMeasure->clear();
                    m_OpsFocusProcess->focusStarMeasure->addItem(m_StarMeasureText.at(FOCUS_STAR_HFR));
                    m_OpsFocusProcess->focusStarMeasure->setCurrentIndex(FOCUS_STAR_HFR);
                }
            }

            // Add necessary widgets to the grid
            // All Curve Fits are available - default to Hyperbola which is the best option
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusCurveFitLabel, 1, 2);
            m_OpsFocusProcess->focusCurveFitLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusCurveFit, 1, 3);
            m_OpsFocusProcess->focusCurveFit->show();
            if (m_OpsFocusProcess->focusCurveFit->count() != m_CurveFitText.count())
            {
                m_OpsFocusProcess->focusCurveFit->clear();
                m_OpsFocusProcess->focusCurveFit->addItems(m_CurveFitText);
                m_OpsFocusProcess->focusCurveFit->setCurrentIndex(CurveFitting::FOCUS_HYPERBOLA);
            }

            if (m_StarMeasure == FOCUS_STAR_STDDEV || m_StarMeasure == FOCUS_STAR_SOBEL ||
                    m_StarMeasure == FOCUS_STAR_LAPLASSIAN || m_StarMeasure == FOCUS_STAR_CANNY)
            {
                m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusUseWeights);
                m_OpsFocusProcess->focusUseWeights->hide();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusDenoise, 3, 0, 1, 2); // Spans 2 columns
                m_OpsFocusProcess->focusDenoise->show();
            }
            else
            {
                m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusDenoise);
                m_OpsFocusProcess->focusDenoise->hide();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusUseWeights, 3, 0, 1, 2); // Spans 2 columns
                m_OpsFocusProcess->focusUseWeights->show();
            }

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusR2LimitLabel, 3, 2);
            m_OpsFocusProcess->focusR2LimitLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusR2Limit, 3, 3);
            m_OpsFocusProcess->focusR2Limit->show();

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusRefineCurveFit, 4, 0, 1, 2); // Spans 2 columns
            m_OpsFocusProcess->focusRefineCurveFit->show();

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusFramesCountLabel, 4, 2);
            m_OpsFocusProcess->focusFramesCountLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusFramesCount, 4, 3);
            m_OpsFocusProcess->focusFramesCount->show();

            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusHFRFramesCountLabel, 5, 2);
            m_OpsFocusProcess->focusHFRFramesCountLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusHFRFramesCount, 5, 3);
            m_OpsFocusProcess->focusHFRFramesCount->show();

            if (m_FocusDetection == ALGORITHM_THRESHOLD)
            {
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusThresholdLabel, 6, 0);
                m_OpsFocusProcess->focusThresholdLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusThreshold, 6, 1);
                m_OpsFocusProcess->focusThreshold->show();
            }
            else if (m_FocusDetection == ALGORITHM_BAHTINOV)
            {
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusMultiRowAverageLabel, 6, 0);
                m_OpsFocusProcess->focusMultiRowAverageLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusMultiRowAverage, 6, 1);
                m_OpsFocusProcess->focusMultiRowAverage->show();

                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianSigmaLabel, 6, 2);
                m_OpsFocusProcess->focusGaussianSigmaLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianSigma, 6, 3);
                m_OpsFocusProcess->focusGaussianSigma->show();

                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianKernelSizeLabel, 7, 0);
                m_OpsFocusProcess->focusGaussianKernelSizeLabel->show();
                m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusGaussianKernelSize, 7, 1);
                m_OpsFocusProcess->focusGaussianKernelSize->show();
            }

            // Donut Buster
            m_OpsFocusProcess->focusDonut->show();
            m_OpsFocusProcess->focusDonut->setEnabled(true);

            // Scan Start Pos
            m_OpsFocusProcess->focusScanStartPos->show();
            m_OpsFocusProcess->focusScanStartPos->setEnabled(true);

            // Aberration Inspector button
            startAbInsB->setEnabled(canAbInsStart());

            // Settings changes
            // Enable adaptive focus for Absolute focusers
            m_OpsFocusSettings->adaptiveFocusGroup->setEnabled(canAbsMove);
            // Enable Autofocus optimisation if Filter Manager set
            if (m_FilterManager)
                m_OpsFocusSettings->focusAFOptimize->setEnabled(true);
            else
            {
                m_OpsFocusSettings->focusAFOptimize->setValue(0);
                m_OpsFocusSettings->focusAFOptimize->setEnabled(false);
            }

            // Mechanics changes
            // Firstly max Single Step is not used by Linear 1 Pass
            m_OpsFocusMechanics->focusMaxSingleStep->setEnabled(false);
            m_OpsFocusMechanics->focusOutSteps->setEnabled(true);

            // Set Walk to just Classic for Quadratic, all options otherwise
            if (m_CurveFit == CurveFitting::FOCUS_QUADRATIC)
            {
                if (m_OpsFocusMechanics->focusWalk->count() != 1)
                {
                    m_OpsFocusMechanics->focusWalk->clear();
                    m_OpsFocusMechanics->focusWalk->addItem(m_FocusWalkText.at(FOCUS_WALK_CLASSIC));
                    m_OpsFocusMechanics->focusWalk->setCurrentIndex(FOCUS_WALK_CLASSIC);
                }
            }
            else
            {
                if (m_OpsFocusMechanics->focusWalk->count() != m_FocusWalkText.count())
                {
                    m_OpsFocusMechanics->focusWalk->clear();
                    m_OpsFocusMechanics->focusWalk->addItems(m_FocusWalkText);
                    m_OpsFocusMechanics->focusWalk->setCurrentIndex(FOCUS_WALK_CLASSIC);
                }
            }
            break;
    }
}

void Focus::setCurveFit(CurveFitting::CurveFit curve)
{
    if (m_OpsFocusProcess->focusCurveFit->currentIndex() == -1)
        return;

    static bool first = true;
    if (!first && m_CurveFit == curve)
        return;

    first = false;

    m_CurveFit = curve;
    setFocusAlgorithm(static_cast<Algorithm> (m_OpsFocusProcess->focusAlgorithm->currentIndex()));
    setUseWeights();
    setDenoise();
    setDonutBuster();

    switch(m_CurveFit)
    {
        case CurveFitting::FOCUS_QUADRATIC:
            m_OpsFocusProcess->focusR2Limit->setEnabled(false);
            m_OpsFocusProcess->focusRefineCurveFit->setChecked(false);
            m_OpsFocusProcess->focusRefineCurveFit->setEnabled(false);
            break;

        case CurveFitting::FOCUS_HYPERBOLA:
            m_OpsFocusProcess->focusR2Limit->setEnabled(true);      // focusR2Limit allowed
            m_OpsFocusProcess->focusRefineCurveFit->setEnabled(true);
            break;

        case CurveFitting::FOCUS_PARABOLA:
            m_OpsFocusProcess->focusR2Limit->setEnabled(true);      // focusR2Limit allowed
            m_OpsFocusProcess->focusRefineCurveFit->setEnabled(true);
            break;

        case CurveFitting::FOCUS_2DGAUSSIAN:
            m_OpsFocusProcess->focusR2Limit->setEnabled(true);      // focusR2Limit allowed
            m_OpsFocusProcess->focusRefineCurveFit->setEnabled(true);
            break;

        default:
            break;
    }
}

void Focus::setStarMeasure(StarMeasure starMeasure)
{
    if (m_OpsFocusProcess->focusStarMeasure->currentIndex() == -1)
        return;

    static bool first = true;
    if (!first && m_StarMeasure == starMeasure)
        return;

    first = false;

    m_StarMeasure = starMeasure;
    setFocusAlgorithm(static_cast<Algorithm> (m_OpsFocusProcess->focusAlgorithm->currentIndex()));
    setUseWeights();
    setDenoise();
    setDonutBuster();
    setAutoStarAndBox();

    // So what is the best estimator of scale to use? Not much to choose from analysis on the sim.
    // Variance is the simplest but isn't robust in the presence of outliers.
    // MAD is probably better but Sn and Qn are theoretically a little better as they are both efficient and
    // able to deal with skew. Have picked Sn.
    switch(m_StarMeasure)
    {
        case FOCUS_STAR_HFR:
            m_OptDir = CurveFitting::OPTIMISATION_MINIMISE;
            m_FocusView->setStarsHFREnabled(true);

            // Don't display the PSF widgets
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSFLabel);
            m_OpsFocusProcess->focusStarPSFLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSF);
            m_OpsFocusProcess->focusStarPSF->hide();
            break;

        case FOCUS_STAR_HFR_ADJ:
            m_OptDir = CurveFitting::OPTIMISATION_MINIMISE;
            m_FocusView->setStarsHFREnabled(false);

            // Don't display the PSF widgets
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSFLabel);
            m_OpsFocusProcess->focusStarPSFLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSF);
            m_OpsFocusProcess->focusStarPSF->hide();
            break;

        case FOCUS_STAR_FWHM:
            m_OptDir = CurveFitting::OPTIMISATION_MINIMISE;
            // Ideally the FITSViewer would display FWHM. Until then disable HFRs to avoid confusion
            m_FocusView->setStarsHFREnabled(false);

            // Display the PSF widgets
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusStarPSFLabel, 2, 2);
            m_OpsFocusProcess->focusStarPSFLabel->show();
            m_OpsFocusProcess->gridLayoutProcess->addWidget(m_OpsFocusProcess->focusStarPSF, 2, 3);
            m_OpsFocusProcess->focusStarPSF->show();
            break;

        case FOCUS_STAR_NUM_STARS:
            m_OptDir = CurveFitting::OPTIMISATION_MAXIMISE;
            m_FocusView->setStarsHFREnabled(true);

            // Don't display the PSF widgets
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSFLabel);
            m_OpsFocusProcess->focusStarPSFLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSF);
            m_OpsFocusProcess->focusStarPSF->hide();
            break;

        case FOCUS_STAR_FOURIER_POWER:
            m_OptDir = CurveFitting::OPTIMISATION_MAXIMISE;
            m_FocusView->setStarsHFREnabled(true);

            // Don't display the PSF widgets
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSFLabel);
            m_OpsFocusProcess->focusStarPSFLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSF);
            m_OpsFocusProcess->focusStarPSF->hide();
            break;

        case FOCUS_STAR_STDDEV:
        case FOCUS_STAR_SOBEL:
        case FOCUS_STAR_LAPLASSIAN:
        case FOCUS_STAR_CANNY:
            m_OptDir = CurveFitting::OPTIMISATION_MAXIMISE;
            m_FocusView->setStarsHFREnabled(true);

            // Don't display the PSF widgets
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSFLabel);
            m_OpsFocusProcess->focusStarPSFLabel->hide();
            m_OpsFocusProcess->gridLayoutProcess->removeWidget(m_OpsFocusProcess->focusStarPSF);
            m_OpsFocusProcess->focusStarPSF->hide();
            break;

        default:
            break;
    }
}

void Focus::setStarPSF(StarPSF starPSF)
{
    m_StarPSF = starPSF;
}

void Focus::setStarUnits(StarUnits starUnits)
{
    m_StarUnits = starUnits;
}

void Focus::setWalk(FocusWalk walk)
{
    if (m_OpsFocusMechanics->focusWalk->currentIndex() == -1)
        return;

    static bool first = true;
    if (!first && m_FocusWalk == walk)
        return;

    first = false;

    m_FocusWalk = walk;

    switch(m_FocusWalk)
    {
        case FOCUS_WALK_CLASSIC:
            m_OpsFocusMechanics->gridLayoutMechanics->replaceWidget(m_OpsFocusMechanics->focusNumStepsLabel,
                    m_OpsFocusMechanics->focusOutStepsLabel);
            m_OpsFocusMechanics->focusNumStepsLabel->hide();
            m_OpsFocusMechanics->focusOutStepsLabel->show();
            m_OpsFocusMechanics->gridLayoutMechanics->replaceWidget(m_OpsFocusMechanics->focusNumSteps,
                    m_OpsFocusMechanics->focusOutSteps);
            m_OpsFocusMechanics->focusNumSteps->hide();
            m_OpsFocusMechanics->focusOutSteps->show();
            break;

        case FOCUS_WALK_FIXED_STEPS:
        case FOCUS_WALK_CFZ_SHUFFLE:
            m_OpsFocusMechanics->gridLayoutMechanics->replaceWidget(m_OpsFocusMechanics->focusOutStepsLabel,
                    m_OpsFocusMechanics->focusNumStepsLabel);
            m_OpsFocusMechanics->focusOutStepsLabel->hide();
            m_OpsFocusMechanics->focusNumStepsLabel->show();
            m_OpsFocusMechanics->gridLayoutMechanics->replaceWidget(m_OpsFocusMechanics->focusOutSteps,
                    m_OpsFocusMechanics->focusNumSteps);
            m_OpsFocusMechanics->focusOutSteps->hide();
            m_OpsFocusMechanics->focusNumSteps->show();
            break;

        default:
            break;
    }
    setDonutBuster();
}

double Focus::getStarUnits(const StarMeasure starMeasure, const StarUnits starUnits)
{
    if (starUnits == FOCUS_UNITS_PIXEL || starMeasure == FOCUS_STAR_NUM_STARS || starMeasure == FOCUS_STAR_FOURIER_POWER
            || starMeasure == FOCUS_STAR_STDDEV || starMeasure == FOCUS_STAR_SOBEL || starMeasure == FOCUS_STAR_LAPLASSIAN
            || starMeasure == FOCUS_STAR_CANNY)
        return 1.0;
    if (m_CcdPixelSizeX <= 0.0 || m_FocalLength <= 0.0)
        return 1.0;

    // Convert to arcsecs from pixels. PixelSize / FocalLength * 206265
    return m_CcdPixelSizeX / m_FocalLength * 206.265;
}

void Focus::calcCFZ()
{
    double cfzMicrons, cfzSteps;
    double cfzCameraSteps = calcCameraCFZ() / m_CFZUI->focusCFZStepSize->value();

    switch(static_cast<Focus::CFZAlgorithm> (m_CFZUI->focusCFZAlgorithm->currentIndex()))
    {
        case Focus::FOCUS_CFZ_CLASSIC:
            // CFZ = 4.88 t λ f2
            cfzMicrons = 4.88f * m_CFZUI->focusCFZTolerance->value() * m_CFZUI->focusCFZWavelength->value() / 1000.0f *
                         pow(m_CFZUI->focusCFZFNumber->value(), 2.0f);
            cfzSteps = cfzMicrons / m_CFZUI->focusCFZStepSize->value();
            m_cfzSteps = std::round(std::max(cfzSteps, cfzCameraSteps));
            m_CFZUI->focusCFZFormula->setText("CFZ = 4.88 t λ f²");
            m_CFZUI->focusCFZ->setText(QString("%1 μm").arg(cfzMicrons, 0, 'f', 0));
            m_CFZUI->focusCFZSteps->setText(QString("%1 steps").arg(cfzSteps, 0, 'f', 0));
            m_CFZUI->focusCFZCameraSteps->setText(QString("%1 steps").arg(cfzCameraSteps, 0, 'f', 0));
            m_CFZUI->focusCFZFinal->setText(QString("%1 steps").arg(m_cfzSteps, 0, 'f', 0));

            // Remove widgets not relevant to this algo
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZTauLabel);
            m_CFZUI->focusCFZTauLabel->hide();
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZTau);
            m_CFZUI->focusCFZTau->hide();
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZSeeingLabel);
            m_CFZUI->focusCFZSeeingLabel->hide();
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZSeeing);
            m_CFZUI->focusCFZSeeing->hide();

            // Add necessary widgets to the grid
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZToleranceLabel, 1, 0);
            m_CFZUI->focusCFZToleranceLabel->show();
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZTolerance, 1, 1);
            m_CFZUI->focusCFZTolerance->show();
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZWavelengthLabel, 2, 0);
            m_CFZUI->focusCFZWavelengthLabel->show();
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZWavelength, 2, 1);
            m_CFZUI->focusCFZWavelength->show();
            break;

        case Focus::FOCUS_CFZ_WAVEFRONT:
            // CFZ = 4 t λ f2
            cfzMicrons = 4.0f * m_CFZUI->focusCFZTolerance->value() * m_CFZUI->focusCFZWavelength->value() / 1000.0f * pow(
                             m_CFZUI->focusCFZFNumber->value(),
                             2.0f);
            cfzSteps = cfzMicrons / m_CFZUI->focusCFZStepSize->value();
            m_cfzSteps = std::round(std::max(cfzSteps, cfzCameraSteps));
            m_CFZUI->focusCFZFormula->setText("CFZ = 4 t λ f²");
            m_CFZUI->focusCFZ->setText(QString("%1 μm").arg(cfzMicrons, 0, 'f', 0));
            m_CFZUI->focusCFZSteps->setText(QString("%1 steps").arg(cfzSteps, 0, 'f', 0));
            m_CFZUI->focusCFZCameraSteps->setText(QString("%1 steps").arg(cfzCameraSteps, 0, 'f', 0));
            m_CFZUI->focusCFZFinal->setText(QString("%1 steps").arg(m_cfzSteps, 0, 'f', 0));

            // Remove widgets not relevant to this algo
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZTauLabel);
            m_CFZUI->focusCFZTauLabel->hide();
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZTau);
            m_CFZUI->focusCFZTau->hide();
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZSeeingLabel);
            m_CFZUI->focusCFZSeeingLabel->hide();
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZSeeing);
            m_CFZUI->focusCFZSeeing->hide();

            // Add necessary widgets to the grid
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZToleranceLabel, 1, 0);
            m_CFZUI->focusCFZToleranceLabel->show();
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZTolerance, 1, 1);
            m_CFZUI->focusCFZTolerance->show();
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZWavelengthLabel, 2, 0);
            m_CFZUI->focusCFZWavelengthLabel->show();
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZWavelength, 2, 1);
            m_CFZUI->focusCFZWavelength->show();
            break;

        case Focus::FOCUS_CFZ_GOLD:
            // CFZ = 0.00225 √τ θ f² A
            cfzMicrons = 0.00225f * pow(m_CFZUI->focusCFZTau->value(), 0.5f) * m_CFZUI->focusCFZSeeing->value()
                         * pow(m_CFZUI->focusCFZFNumber->value(), 2.0f) * m_CFZUI->focusCFZAperture->value();
            cfzSteps = cfzMicrons / m_CFZUI->focusCFZStepSize->value();
            m_cfzSteps = std::round(std::max(cfzSteps, cfzCameraSteps));
            m_CFZUI->focusCFZFormula->setText("CFZ = 0.00225 √τ θ f² A");
            m_CFZUI->focusCFZ->setText(QString("%1 μm").arg(cfzMicrons, 0, 'f', 0));
            m_CFZUI->focusCFZSteps->setText(QString("%1 steps").arg(cfzSteps, 0, 'f', 0));
            m_CFZUI->focusCFZCameraSteps->setText(QString("%1 steps").arg(cfzCameraSteps, 0, 'f', 0));
            m_CFZUI->focusCFZFinal->setText(QString("%1 steps").arg(m_cfzSteps, 0, 'f', 0));

            // Remove widgets not relevant to this algo
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZToleranceLabel);
            m_CFZUI->focusCFZToleranceLabel->hide();
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZTolerance);
            m_CFZUI->focusCFZTolerance->hide();
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZWavelengthLabel);
            m_CFZUI->focusCFZWavelengthLabel->hide();
            m_CFZUI->gridLayoutCFZ->removeWidget(m_CFZUI->focusCFZWavelength);
            m_CFZUI->focusCFZWavelength->hide();

            // Add necessary widgets to the grid
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZTauLabel, 1, 0);
            m_CFZUI->focusCFZTauLabel->show();
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZTau, 1, 1);
            m_CFZUI->focusCFZTau->show();
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZSeeingLabel, 2, 0);
            m_CFZUI->focusCFZSeeingLabel->show();
            m_CFZUI->gridLayoutCFZ->addWidget(m_CFZUI->focusCFZSeeing, 2, 1);
            m_CFZUI->focusCFZSeeing->show();
            break;
    }
    if (linearFocuser != nullptr && linearFocuser->isDone())
        emit drawCFZ(linearFocuser->solution(), linearFocuser->solutionValue(), m_cfzSteps,
                     m_CFZUI->focusCFZDisplayVCurve->isChecked());
}

// Calculate the CFZ of the camera in microns using
// CFZcamera = pixel_size * f^2 * A
// pixel_size in microns and A in mm so divide A by 1000.
double Focus::calcCameraCFZ()
{
    return m_CcdPixelSizeX * pow(m_CFZUI->focusCFZFNumber->value(), 2.0) * m_CFZUI->focusCFZAperture->value() / 1000.0;
}

void Focus::wavelengthChanged()
{
    // The static data for the wavelength held against the filter has been updated so use the new value
    if (m_FilterManager)
    {
        m_CFZUI->focusCFZWavelength->setValue(m_FilterManager->getFilterWavelength(filter()));
        calcCFZ();
    }
}

void Focus::resetCFZToOT()
{
    // Set the aperture and focal ratio for the scope on the connected optical train
    m_CFZUI->focusCFZFNumber->setValue(m_FocalRatio);
    m_CFZUI->focusCFZAperture->setValue(m_Aperture);

    // Set the wavelength from the active filter
    if (m_FilterManager)
    {
        if (m_CFZUI->focusCFZWavelength->value() != m_FilterManager->getFilterWavelength(filter()))
            m_CFZUI->focusCFZWavelength->setValue(m_FilterManager->getFilterWavelength(filter()));
    }
    calcCFZ();
}

void Focus::setState(FocusState newState, const bool update)
{
    qCDebug(KSTARS_EKOS_FOCUS) << "Focus State changes from" << getFocusStatusString(m_state) << "to" << getFocusStatusString(
                                   newState);
    m_state = newState;
    emit newStatus(m_state, opticalTrain(), update);
}

void Focus::initView()
{
    // 1. Create the focus view
    m_FocusView.reset(new FocusFITSView(focusingWidget));
    m_FocusView->setBaseSize(focusingWidget->size());

    // 2. Set layout on the parent
    QVBoxLayout *layout = new QVBoxLayout(focusingWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_FocusView.get());

    // 3. Create overlay widget AS CHILD OF m_FocusView

    connect(m_FocusView.get(), &FITSView::trackingStarSelected, this, &Ekos::Focus::focusStarSelected, Qt::UniqueConnection);
    m_FocusView->setStarsEnabled(true);
    m_FocusView->setStarsHFREnabled(true);
}

void Focus::initHelperObjects()
{
    // Objects to do with focus measures
    starFitting.reset(new CurveFitting());
    focusFWHM.reset(new FocusFWHM(m_ScaleCalc));
    focusFourierPower.reset(new FocusFourierPower(m_ScaleCalc));
#if defined(HAVE_OPENCV)
    focusBlurriness.reset(new FocusBlurriness());
#endif

    // Adaptive Focus
    adaptFocus.reset(new AdaptiveFocus(this));

    // Focus Advisor
    focusAdvisor.reset(new FocusAdvisor(this));
}

QVariantMap Focus::getAllSettings() const
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

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox * >())
        if (oneWidget->isCheckable())
            settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    // All Splitters
    for (auto &oneWidget : findChildren<QSplitter * >())
        settings.insert(oneWidget->objectName(), QString::fromUtf8(oneWidget->saveState().toBase64()));

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    return settings;
}

void Focus::setAllSettings(QVariantMap &settings)
{
    // Disconnect settings that we don't end up calling syncSettings while
    // performing the changes.
    disconnectSyncSettings();

    // The OT Combo is handled outside of focus settings so shouldn't be included in "settings"
    // Scrub the data just in case
    settings.remove(opticalTrainCombo->objectName());

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

        // Checkable Groupboxes
        auto groupbox = findChild<QGroupBox*>(name);
        if (groupbox && groupbox->isCheckable())
        {
            syncControl(settings, name, groupbox);
            continue;
        }

        // Splitters
        auto splitter = findChild<QSplitter*>(name);
        if (splitter)
        {
            syncControl(settings, name, splitter);
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

        m_Settings[key] = value;
        m_GlobalSettings[key] = value;
    }

    emit settingsUpdated(getAllSettings());

    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Focus, m_Settings);

    // Restablish connections
    connectSyncSettings();

    // Once settings have been loaded run through routines to set state variables
    m_CurveFit = static_cast<CurveFitting::CurveFit> (m_OpsFocusProcess->focusCurveFit->currentIndex());
    setFocusDetection(static_cast<StarAlgorithm> (m_OpsFocusProcess->focusDetection->currentIndex()));
    setCurveFit(static_cast<CurveFitting::CurveFit>(m_OpsFocusProcess->focusCurveFit->currentIndex()));
    setStarMeasure(static_cast<StarMeasure>(m_OpsFocusProcess->focusStarMeasure->currentIndex()));
    setWalk(static_cast<FocusWalk>(m_OpsFocusMechanics->focusWalk->currentIndex()));
    selectImageMask();
}



bool Focus::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QGroupBox *pGB = nullptr;
    QComboBox *pComboBox = nullptr;
    QSplitter *pSplitter = nullptr;
    QRadioButton *pRadioButton = nullptr;
    bool ok = true;

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
    else if ((pGB = qobject_cast<QGroupBox *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value != pGB->isChecked())
            pGB->setChecked(value);
        return true;
    }
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
            pRadioButton->setChecked(true);
        return true;
    }
    // ONLY FOR STRINGS, not INDEX
    else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
    {
        const QString value = settings[key].toString();
        pComboBox->setCurrentText(value);
        return true;
    }
    else if ((pSplitter = qobject_cast<QSplitter *>(widget)))
    {
        const auto value = QByteArray::fromBase64(settings[key].toString().toUtf8());
        pSplitter->restoreState(value);
        return true;
    }

    return false;
};

void Focus::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, [this]()
    {
        // remember current train selection
        const int current = opticalTrainCombo->currentIndex();
        initOpticalTrain();
        // restore current train selection if available
        if (current >= 0 && current < opticalTrainCombo->count())
            opticalTrainCombo->setCurrentIndex(current);
    });
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        const int id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index));
        // remember only the train ID from the first Focus tab
        if (m_focuserId == 0)
        {
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::FocusOpticalTrain, id);
        }
        refreshOpticalTrain(id);
        emit trainChanged();
    });
}

void Focus::initOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(true);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::FocusOpticalTrain);

    if (trainID.isValid())
    {
        auto id = trainID.toUInt();

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            qCWarning(KSTARS_EKOS_FOCUS) << "Optical train doesn't exist for id" << id;
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
        }
        refreshOpticalTrain(id);
    }

    opticalTrainCombo->blockSignals(false);
}

void Focus::refreshOpticalTrain(const int id)
{
    bool validSettings = false;
    auto name = OpticalTrainManager::Instance()->name(id);

    opticalTrainCombo->setCurrentText(name);

    // Load train settings
    // This needs to be done near the start of this function as methods further down
    // cause settings to be updated, which in turn interferes with the persistence and
    // setup of settings in OpticalTrainSettings
    OpticalTrainSettings::Instance()->setOpticalTrainID(id);

    auto focuser = OpticalTrainManager::Instance()->getFocuser(name);
    setFocuser(focuser);

    auto scope = OpticalTrainManager::Instance()->getScope(name);
    double reducer = OpticalTrainManager::Instance()->getReducer(name);
    setScopeDetails(scope, reducer);

    auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Focus);
    if (settings.isValid())
    {
        validSettings = true;
        auto map = settings.toJsonObject().toVariantMap();
        if (map != m_Settings)
        {
            m_Settings.clear();
            setAllSettings(map);
        }
    }

    auto camera = OpticalTrainManager::Instance()->getCamera(name);
    if (camera)
    {
        opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(camera->getDeviceName(), scope["name"].toString()));

        // Get the pixel size of the active camera for later calculations
        auto nvp = camera->getNumber("CCD_INFO");
        if (!nvp)
        {
            m_CcdPixelSizeX = 0.0;
            m_CcdWidth = m_CcdHeight = 0;
        }
        else
        {
            auto np = nvp->findWidgetByName("CCD_PIXEL_SIZE_X");
            if (np)
                m_CcdPixelSizeX = np->getValue();
            np = nvp->findWidgetByName("CCD_MAX_X");
            if (np)
                m_CcdWidth = np->getValue();
            np = nvp->findWidgetByName("CCD_MAX_Y");
            if (np)
                m_CcdHeight = np->getValue();
        }
    }
    setCamera(camera);

    auto filterWheel = OpticalTrainManager::Instance()->getFilterWheel(name);
    setFilterWheel(filterWheel);

    // Update calcs for the CFZ based on the new OT
    resetCFZToOT();

    // JM 2024.03.16 Also use focus advisor on new profiles
    if (!validSettings)
    {
        focusAdvisor->setupParams(name);
        focusAdvisor->updateParams();
    }

    resetButtons();
}

// Function to set member variables based on Optical Train's attached scope
void Focus::setScopeDetails(const QJsonObject &scope, const double reducer)
{
    m_Aperture = scope["aperture"].toDouble(-1);
    m_FocalLength = scope["focal_length"].toDouble(-1);
    m_FocalRatio = scope["focal_ratio"].toDouble(-1);
    m_ScopeType = scope["type"].toString();
    m_Reducer = reducer;

    // Adjust telescope FL and F# for any reducer
    if (m_Reducer > 0.0)
        m_FocalLength *= m_Reducer;

    // Use the adjusted focal length to calculate an adjusted focal ratio
    if (m_FocalRatio <= 0.0)
        // For a scope, FL and aperture are specified so calc the F#
        m_FocalRatio = (m_Aperture > 0.001) ? m_FocalLength / m_Aperture : 0.0f;
    else if (m_Aperture < 0.0)
        // DSLR Lens. FL and F# are specified so calc the aperture
        m_Aperture = m_FocalLength / m_FocalRatio;
}

}

/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focus.h"

#include "focusadaptor.h"
#include "focusalgorithms.h"
#include "polynomialfit.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"

// Modules
#include "ekos/guide/guide.h"
#include "ekos/manager.h"

// KStars Auxiliary
#include "auxiliary/kspaths.h"
#include "auxiliary/ksmessagebox.h"

// Ekos Auxiliary
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/auxiliary/filtermanager.h"

// FITS
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"

// Devices
#include "indi/indifilterwheel.h"
#include "ksnotification.h"
#include "kconfigdialog.h"

#include <basedevice.h>
#include <gsl/gsl_fit.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_min.h>

#include <ekos_focus_debug.h>

#include <cmath>

#define MAXIMUM_ABS_ITERATIONS   30
#define MAXIMUM_RESET_ITERATIONS 3
#define AUTO_STAR_TIMEOUT        45000
#define MINIMUM_PULSE_TIMER      32
#define MAX_RECAPTURE_RETRIES    3
#define MINIMUM_POLY_SOLUTIONS   2

namespace Ekos
{
Focus::Focus()
{
    // #1 Set the UI
    setupUi(this);

    // #2 Register DBus
    qRegisterMetaType<Ekos::FocusState>("Ekos::FocusState");
    qDBusRegisterMetaType<Ekos::FocusState>();
    new FocusAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Focus", this);

    // #3 Init connections
    initConnections();

    // #4 Init Plots
    initPlots();

    // #5 Init View
    initView();

    // #6 Reset all buttons to default states
    resetButtons();

    // #7 Load All settings
    loadGlobalSettings();

    // #8 Init Setting Connection now
    connectSettings();

    connect(&m_StarFinderWatcher, &QFutureWatcher<bool>::finished, this, &Focus::calculateHFR);

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    appendLogText(i18n("Idle."));

    // Focus motion timeout
    m_FocusMotionTimer.setInterval(focusMotionTimeout->value() * 1000);
    m_FocusMotionTimer.setSingleShot(true);
    connect(&m_FocusMotionTimer, &QTimer::timeout, this, &Focus::handleFocusMotionTimeout);

    // Create an autofocus CSV file, dated at startup time
    m_FocusLogFileName = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("focuslogs/autofocus-" +
                         QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss") + ".txt");
    m_FocusLogFile.setFileName(m_FocusLogFileName);

    editFocusProfile->setIcon(QIcon::fromTheme("document-edit"));
    editFocusProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(editFocusProfile, &QAbstractButton::clicked, this, [this]()
    {
        KConfigDialog *optionsEditor = new KConfigDialog(this, "OptionsProfileEditor", Options::self());
        optionsProfileEditor = new StellarSolverProfileEditor(this, Ekos::FocusProfiles, optionsEditor);
#ifdef Q_OS_OSX
        optionsEditor->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
        KPageWidgetItem *mainPage = optionsEditor->addPage(optionsProfileEditor, i18n("Focus Options Profile Editor"));
        mainPage->setIcon(QIcon::fromTheme("configure"));
        connect(optionsProfileEditor, &StellarSolverProfileEditor::optionsProfilesUpdated, this, &Focus::loadStellarSolverProfiles);
        optionsProfileEditor->loadProfile(focusSEPProfile->currentText());
        optionsEditor->show();
    });

    loadStellarSolverProfiles();

    // connect HFR plot widget
    connect(this, &Ekos::Focus::initHFRPlot, HFRPlot, &FocusHFRVPlot::init);
    connect(this, &Ekos::Focus::redrawHFRPlot, HFRPlot, &FocusHFRVPlot::redraw);
    connect(this, &Ekos::Focus::newHFRPlotPosition, HFRPlot, &FocusHFRVPlot::addPosition);
    // connect signal/slot for adding a new position with errors to be shown as error bars
    connect(this, &Ekos::Focus::newHFRPlotPositionWithSigma, HFRPlot, &FocusHFRVPlot::addPositionWithSigma);
    connect(this, &Ekos::Focus::drawPolynomial, HFRPlot, &FocusHFRVPlot::drawPolynomial);
    // connect signal/slot for the curve plotting to the V-Curve widget
    connect(this, &Ekos::Focus::drawCurve, HFRPlot, &FocusHFRVPlot::drawCurve);
    connect(this, &Ekos::Focus::setTitle, HFRPlot, &FocusHFRVPlot::setTitle);
    connect(this, &Ekos::Focus::updateTitle, HFRPlot, &FocusHFRVPlot::updateTitle);
    connect(this, &Ekos::Focus::minimumFound, HFRPlot, &FocusHFRVPlot::drawMinimum);

    m_DarkProcessor = new DarkProcessor(this);
    connect(m_DarkProcessor, &DarkProcessor::newLog, this, &Ekos::Focus::appendLogText);
    connect(m_DarkProcessor, &DarkProcessor::darkFrameCompleted, this, [this](bool completed)
    {
        useFocusDarkFrame->setChecked(completed);
        m_FocusView->setProperty("suspended", false);
        if (completed)
        {
            m_FocusView->rescale(ZOOM_KEEP_LEVEL);
            m_FocusView->updateFrame();
        }
        setCaptureComplete();
        resetButtons();
    });

    setupOpticalTrainManager();
    // Needs to be done once
    connectFilterManager();
}

void Focus::loadStellarSolverProfiles()
{
    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(
                                            QStandardPaths::AppLocalDataLocation)).filePath("SavedFocusProfiles.ini");
    if(QFileInfo::exists(savedOptionsProfiles))
        m_StellarSolverProfiles = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        m_StellarSolverProfiles = getDefaultFocusOptionsProfiles();
    focusSEPProfile->clear();
    for(auto &param : m_StellarSolverProfiles)
        focusSEPProfile->addItem(param.listName);
    auto profile = m_Settings["focusSEPProfile"];
    if (profile.isValid())
        focusSEPProfile->setCurrentText(profile.toString());
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

    m_FocusLogFile.close();
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
    switch (state)
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
        focusSubFrame->setEnabled(targetChip->canSubframe());
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

    focusSubFrame->setEnabled(targetChip->canSubframe());

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

                settings["x"]    = focusSubFrame->isChecked() ? x : minX;
                settings["y"]    = focusSubFrame->isChecked() ? y : minY;
                settings["w"]    = focusSubFrame->isChecked() ? w : maxW;
                settings["h"]    = focusSubFrame->isChecked() ? h : maxH;
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

bool Focus::addTemperatureSource(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (device.isNull())
        return false;

    for (auto &oneSource : m_TemperatureSources)
    {
        if (oneSource->getDeviceName() == device->getDeviceName())
            return false;
    }

    m_TemperatureSources.append(device);
    defaultFocusTemperatureSource->addItem(device->getDeviceName());

    auto targetSource = m_Settings["defaultFocusTemperatureSource"];
    if (targetSource.isValid())
        checkTemperatureSource(targetSource.toString());
    return true;
}

void Focus::checkTemperatureSource(const QString &name)
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
        if (oneSource->getDeviceName() == name)
        {
            currentSource = oneSource;
            break;
        }
    }

    // No valid device found
    if (!currentSource)
        return;

    // Disconnect all existing signals
    for (const auto &oneSource : m_TemperatureSources)
        disconnect(oneSource.get(), &ISD::GenericDevice::propertyUpdated, this, &Ekos::Focus::processTemperatureSource);

    if (findTemperatureElement(currentSource))
    {
        m_LastSourceAutofocusTemperature = currentTemperatureSourceElement->value;
        absoluteTemperatureLabel->setText(QString("%1 °C").arg(currentTemperatureSourceElement->value, 0, 'f', 2));
        deltaTemperatureLabel->setText(QString("%1 °C").arg(0.0, 0, 'f', 2));
    }
    else
        m_LastSourceAutofocusTemperature = INVALID_VALUE;

    connect(currentSource.get(), &ISD::GenericDevice::propertyUpdated, this, &Ekos::Focus::processTemperatureSource,
            Qt::UniqueConnection);
}

bool Focus::findTemperatureElement(const QSharedPointer<ISD::GenericDevice> &device)
{
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
        return;
    }
    else
        focuserLabel->setText(m_Focuser->getDeviceName());

    if (m_FilterManager)
        m_FilterManager->setFocusReady(m_Focuser->isConnected());

    hasDeviation = m_Focuser->hasDeviation();

    canAbsMove = m_Focuser->canAbsMove();

    if (canAbsMove)
    {
        getAbsFocusPosition();

        absTicksSpin->setEnabled(true);
        absTicksLabel->setEnabled(true);
        startGotoB->setEnabled(true);

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
    focusBacklash->setEnabled(hasBacklash);
    focusBacklash->disconnect(this);
    if (hasBacklash)
    {
        double min = 0, max = 0, step = 0;
        m_Focuser->getMinMaxStep("FOCUS_BACKLASH_STEPS", "FOCUS_BACKLASH_VALUE", &min, &max, &step);
        focusBacklash->setMinimum(min);
        focusBacklash->setMaximum(max);
        focusBacklash->setSingleStep(step);
        focusBacklash->setValue(m_Focuser->getBacklash());
        connect(focusBacklash, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value)
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


    }
    else
    {
        focusBacklash->setValue(0);
    }

    connect(m_Focuser, &ISD::Focuser::propertyUpdated, this, &Ekos::Focus::updateProperty, Qt::UniqueConnection);

    resetButtons();
}

bool Focus::setCamera(ISD::Camera *device)
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
            controlGroup->setEnabled(true);
            ccdGroup->setEnabled(true);
            tabWidget->setEnabled(true);
        });
        connect(m_Camera, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            controlGroup->setEnabled(false);
            ccdGroup->setEnabled(false);
            tabWidget->setEnabled(false);
        });
    }

    auto isConnected = m_Camera && m_Camera->isConnected();
    controlGroup->setEnabled(isConnected);
    ccdGroup->setEnabled(isConnected);
    tabWidget->setEnabled(isConnected);

    if (!m_Camera)
        return false;

    resetFrame();

    checkCamera();
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
        focusMaxTravel->setMaximum(travel);;

        absTicksLabel->setText(QString::number(currentPosition));

        focusTicks->setMaximum(it->getMax() / 2);
    }
}

void Focus::processTemperatureSource(INDI::Property prop)
{
    double delta = 0;
    if (currentTemperatureSourceElement && currentTemperatureSourceElement->nvp->name == prop.getName())
    {
        if (m_LastSourceAutofocusTemperature != INVALID_VALUE)
        {
            delta = currentTemperatureSourceElement->value - m_LastSourceAutofocusTemperature;
            emit newFocusTemperatureDelta(abs(delta), currentTemperatureSourceElement->value);
        }
        else
        {
            emit newFocusTemperatureDelta(0, currentTemperatureSourceElement->value);
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

    emit newFocusTemperatureDelta(0, -1e6);
}

#if 0
void Focus::initializeFocuserTemperature()
{
    auto temperatureProperty = currentFocuser->getBaseDevice()->getNumber("FOCUS_TEMPERATURE");

    if (temperatureProperty && temperatureProperty->getState() != IPS_ALERT)
    {
        focuserTemperature = temperatureProperty->at(0)->getValue();
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Setting current focuser temperature: %1").arg(focuserTemperature, 0, 'f', 2);
    }
    else
    {
        focuserTemperature = INVALID_VALUE;
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Focuser temperature is not available");
    }
}

void Focus::setLastFocusTemperature()
{
    // The focus temperature is taken by default from the focuser.
    // If unavailable, fallback to the observatory temperature.
    if (focuserTemperature != INVALID_VALUE)
    {
        lastFocusTemperature = focuserTemperature;
        lastFocusTemperatureSource = FOCUSER_TEMPERATURE;
    }
    else if (observatoryTemperature != INVALID_VALUE)
    {
        lastFocusTemperature = observatoryTemperature;
        lastFocusTemperatureSource = OBSERVATORY_TEMPERATURE;
    }
    else
    {
        lastFocusTemperature = INVALID_VALUE;
        lastFocusTemperatureSource = NO_TEMPERATURE;
    }

    emit newFocusTemperatureDelta(0, -1e6);
}


void Focus::updateTemperature(TemperatureSource source, double newTemperature)
{
    if (source == FOCUSER_TEMPERATURE && focuserTemperature != newTemperature)
    {
        focuserTemperature = newTemperature;
        emitTemperatureEvents(source, newTemperature);
    }
    else if (source == OBSERVATORY_TEMPERATURE && observatoryTemperature != newTemperature)
    {
        observatoryTemperature = newTemperature;
        emitTemperatureEvents(source, newTemperature);
    }
}

void Focus::emitTemperatureEvents(TemperatureSource source, double newTemperature)
{
    if (source != lastFocusTemperatureSource)
    {
        return;
    }

    if (lastFocusTemperature != INVALID_VALUE && newTemperature != INVALID_VALUE)
    {
        emit newFocusTemperatureDelta(abs(newTemperature - lastFocusTemperature), newTemperature);
    }
    else
    {
        emit newFocusTemperatureDelta(0, newTemperature);
    }
}
#endif

void Focus::start()
{
    if (m_Focuser == nullptr)
    {
        appendLogText(i18n("No Focuser connected."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
        return;
    }

    if (m_Camera == nullptr)
    {
        appendLogText(i18n("No CCD connected."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
        return;
    }

    if (!canAbsMove && !canRelMove && focusTicks->value() <= MINIMUM_PULSE_TIMER)
    {
        appendLogText(i18n("Starting pulse step is too low. Increase the step size to %1 or higher...",
                           MINIMUM_PULSE_TIMER * 5));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
        return;
    }

    if (inAutoFocus)
    {
        appendLogText(i18n("Autofocus is already running, discarding start request."));
        return;
    }
    else inAutoFocus = true;

    m_LastFocusDirection = FOCUS_NONE;

    waitStarSelectTimer.stop();

    // Reset the focus motion timer
    m_FocusMotionTimerCounter = 0;
    m_FocusMotionTimer.stop();
    m_FocusMotionTimer.setInterval(focusMotionTimeout->value() * 1000);

    // Reset focuser reconnect counter
    m_FocuserReconnectCounter = 0;

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
        pulseDuration = focusTicks->value();
    }
    else if (canRelMove)
    {
        //appendLogText(i18n("Setting dummy central position to 50000"));
        absIterations   = 0;
        pulseDuration   = focusTicks->value();
        //currentPosition = 50000;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }
    else
    {
        pulseDuration   = focusTicks->value();
        absIterations   = 0;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }

    focuserAdditionalMovement = 0;
    HFRFrames.clear();

    resetButtons();

    reverseDir = false;

    /*if (fw > 0 && fh > 0)
        starSelected= true;
    else
        starSelected= false;*/

    clearDataPoints();
    profilePlot->clear();

    qCInfo(KSTARS_EKOS_FOCUS)  << "Starting focus with Detection: " << focusDetection->currentText()
                               << " Algorithm: " << focusAlgorithm->currentText()
                               << " Box size: " << focusBoxSize->value()
                               << " Subframe: " << ( focusSubFrame->isChecked() ? "yes" : "no" )
                               << " Autostar: " << ( focusAutoStarEnabled->isChecked() ? "yes" : "no" )
                               << " Full frame: " << ( focusUseFullField->isChecked() ? "yes" : "no " )
                               << " [" << focusFullFieldInnerRadius->value() << "%," << focusFullFieldOuterRadius->value() << "%]"
                               << " Step Size: " << focusTicks->value()
                               << " Number Steps " << focusOutSteps->value()
                               << " Backlash " << focusBacklash->value()
                               << " AF Overscan " << focusAFOverscan->value()
                               << " Threshold: " << focusThreshold->value()
                               << " Gaussian Sigma: " << focusGaussianSigma->value()
                               << " Gaussian Kernel size: " << focusGaussianKernelSize->value()
                               << " Multi row average: " << focusMultiRowAverage->value()
                               << " Tolerance: " << focusTolerance->value()
                               << " Frames: " << 1 /*focusFramesCount->value()*/ << " Maximum Travel: " << focusMaxTravel->value()
                               << " Curve Fit: " << focusCurveFit->currentText()
                               << " Use Weights: " << ( focusUseWeights->isChecked() ? "yes" : "no" )
                               << " R2 Limit: " << focusR2Limit->value();

    if (currentTemperatureSourceElement)
        emit autofocusStarting(currentTemperatureSourceElement->value, filter());
    else
        // dummy temperature will be ignored
        emit autofocusStarting(INVALID_VALUE, filter());

    if (focusAutoStarEnabled->isChecked())
        appendLogText(i18n("Autofocus in progress..."));
    else if (!inAutoFocus)
        appendLogText(i18n("Please wait until image capture is complete..."));

    // Only suspend when we have Off-Axis Guider
    // If the guide camera is operating on a different OTA
    // then no need to suspend.
    if (m_GuidingSuspended == false && focusSuspendGuiding->isChecked())
    {
        m_GuidingSuspended = true;
        emit suspendGuiding();
    }

    //emit statusUpdated(true);
    state = Ekos::FOCUS_PROGRESS;
    qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
    emit newStatus(state);

    // Denoise with median filter
    //defaultScale = FITS_MEDIAN;

    KSNotification::event(QLatin1String("FocusStarted"), i18n("Autofocus operation started"), KSNotification::Focus);

    // Used for all the focuser types.
    if (m_FocusAlgorithm == FOCUS_LINEAR || m_FocusAlgorithm == FOCUS_LINEAR1PASS)
    {
        const int position = static_cast<int>(currentPosition);
        auto focusOutSteps = m_Settings["focusOutSteps"];
        auto focusOutStepsValue = focusOutSteps.isValid() ? focusOutSteps.toDouble() : 0.0;
        FocusAlgorithmInterface::FocusParams params(
            focusMaxTravel->value(), focusTicks->value(), position, absMotionMin, absMotionMax,
            MAXIMUM_ABS_ITERATIONS, focusTolerance->value() / 100.0, filter(),
            currentTemperatureSourceElement ? currentTemperatureSourceElement->value : INVALID_VALUE,
            focusOutStepsValue,
            m_FocusAlgorithm, focusBacklash->value(), m_CurveFit, focusUseWeights->isChecked());
        if (canAbsMove)
            initialFocuserAbsPosition = position;
        linearFocuser.reset(MakeLinearFocuser(params));
        curveFitting.reset(new CurveFitting());
        linearRequestedPosition = linearFocuser->initialPosition();
        const int newPosition = adjustLinearPosition(position, linearRequestedPosition, focusAFOverscan->value());
        if (newPosition != position)
        {
            if (!changeFocus(newPosition - position))
            {
                completeFocusProcedure(Ekos::FOCUS_ABORTED);
            }
            // Avoid the capture below.
            return;
        }
    }
    capture();
}

int Focus::adjustLinearPosition(int position, int newPosition, int overscan)
{
    if (overscan > 0 && newPosition > position)
    {
        // If user has set an overscan value then use it
        int adjustment = overscan;
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: extending outward movement by overscan %1").arg(adjustment);

        if (newPosition + adjustment > absMotionMax)
            adjustment = static_cast<int>(absMotionMax) - newPosition;

        focuserAdditionalMovement = adjustment;

        return newPosition + adjustment;
    }
    return newPosition;
}

void Focus::checkStopFocus(bool abort)
{
    // if abort, avoid try to restart
    if (abort)
        resetFocusIteration = MAXIMUM_RESET_ITERATIONS + 1;

    if (captureInProgress && inAutoFocus == false && inFocusLoop == false)
    {
        captureB->setEnabled(true);
        stopFocusB->setEnabled(false);

        appendLogText(i18n("Capture aborted."));
    }

    if (hfrInProgress)
    {
        stopFocusB->setEnabled(false);
        appendLogText(i18n("Detection in progress, please wait."));
        QTimer::singleShot(1000, this, [ &, abort]()
        {
            checkStopFocus(abort);
        });
    }
    else
    {
        completeFocusProcedure(abort ? Ekos::FOCUS_ABORTED : Ekos::FOCUS_FAILED);
    }
}

void Focus::meridianFlipStarted()
{
    // if focusing is not running, do nothing
    if (state == FOCUS_IDLE || state == FOCUS_COMPLETE || state == FOCUS_FAILED || state == FOCUS_ABORTED)
        return;

    // store current focus iteration counter since abort() sets it to the maximal value to avoid restarting
    int old = resetFocusIteration;
    // abort focusing
    abort();
    // try to shift the focuser back to its initial position
    resetFocuser();
    // restore iteration counter
    resetFocusIteration = old;
}

void Focus::abort()
{
    // No need to "abort" if not already in progress.
    if (state <= FOCUS_ABORTED)
        return;

    checkStopFocus(true);
    appendLogText(i18n("Autofocus aborted."));
}

void Focus::stop(Ekos::FocusState completionState)
{
    qCDebug(KSTARS_EKOS_FOCUS) << "Stopping Focus";

    captureTimeout.stop();
    m_FocusMotionTimer.stop();
    m_FocusMotionTimerCounter = 0;
    m_FocuserReconnectCounter = 0;

    opticalTrainCombo->setEnabled(true);
    inAutoFocus     = false;
    focuserAdditionalMovement = 0;
    inFocusLoop     = false;
    captureInProgress  = false;
    isVShapeSolution = false;
    captureFailureCounter = 0;
    minimumRequiredHFR = -1;
    noStarCount        = 0;
    HFRFrames.clear();

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

    if (completionState == Ekos::FOCUS_ABORTED || completionState == Ekos::FOCUS_FAILED)
    {
        state = completionState;
        qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
        emit newStatus(state);
    }
}

void Focus::capture(double settleTime)
{
    // If capturing should be delayed by a given settling time, we start the capture timer.
    // This is intentionally designed re-entrant, i.e. multiple calls with settle time > 0 takes the last delay
    if (settleTime > 0 && captureInProgress == false)
    {
        captureTimer.start(static_cast<int>(settleTime * 1000));
        return;
    }

    if (captureInProgress)
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

        int targetPosition = focusFilter->currentIndex() + 1;
        QString lockedFilter = m_FilterManager->getFilterLock(focusFilter->currentText());

        // We change filter if:
        // 1. Target position is not equal to current position.
        // 2. Locked filter of CURRENT filter is a different filter.
        if (lockedFilter != "--" && lockedFilter != focusFilter->currentText())
        {
            int lockedFilterIndex = focusFilter->findText(lockedFilter);
            if (lockedFilterIndex >= 0)
            {
                // Go back to this filter one we are done
                fallbackFilterPending = true;
                fallbackFilterPosition = targetPosition;
                targetPosition = lockedFilterIndex + 1;
            }
        }

        filterPositionPending = (targetPosition != currentFilterPosition);
        // If either the target position is not equal to the current position, OR
        if (filterPositionPending)
        {
            // Apply all policies except autofocus since we are already in autofocus module doh.
            m_FilterManager->setFilterPosition(targetPosition,
                                               static_cast<FilterManager::FilterPolicy>(FilterManager::CHANGE_POLICY | FilterManager::OFFSET_POLICY));
            return;
        }
    }

    m_FocusView->setProperty("suspended", useFocusDarkFrame->isChecked());
    prepareCapture(targetChip);

    connect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Focus::processData);
    connect(m_Camera, &ISD::Camera::error, this, &Ekos::Focus::processCaptureError);

    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        targetChip->setFrame(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(),
                             settings["h"].toInt());
        settings["binx"]          = (focusBinning->currentIndex() + 1);
        settings["biny"]          = (focusBinning->currentIndex() + 1);
        frameSettings[targetChip] = settings;
    }

    captureInProgress = true;
    if (state != FOCUS_PROGRESS)
    {
        state = FOCUS_PROGRESS;
        emit newStatus(state);
    }

    m_FocusView->setBaseSize(focusingWidget->size());

    if (targetChip->capture(focusExposure->value()))
    {
        // Timeout is exposure duration + timeout threshold in seconds
        //long const timeout = lround(ceil(focusExposure->value() * 1000)) + FOCUS_TIMEOUT_THRESHOLD;
        captureTimeout.start(Options::focusCaptureTimeout() * 1000);

        if (inFocusLoop == false)
            appendLogText(i18n("Capturing image..."));

        resetButtons();
    }
    else if (inAutoFocus)
    {
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
    }
}

void Focus::prepareCapture(ISD::CameraChip *targetChip)
{
    if (m_Camera->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
    {
        rememberUploadMode = ISD::Camera::UPLOAD_LOCAL;
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

    if (focusISO->isEnabled() && focusISO->currentIndex() != -1 &&
            targetChip->getISOIndex() != focusISO->currentIndex())
        targetChip->setISOIndex(focusISO->currentIndex());

    if (focusGain->isEnabled() && focusGain->value() != GainSpinSpecialValue)
        m_Camera->setGain(focusGain->value());
}

bool Focus::focusIn(int ms)
{
    if (ms <= 0)
        ms = focusTicks->value();
    return changeFocus(-ms);
}

bool Focus::focusOut(int ms)
{
    if (ms <= 0)
        ms = focusTicks->value();
    return changeFocus(ms);
}

// If amount > 0 we focus out, otherwise in.
bool Focus::changeFocus(int amount)
{
    const int absAmount = abs(amount);

    // Retry capture if we stay at the same position
    // Allow 1 step of tolerance--Have seen stalls with amount==1.
    if (inAutoFocus && absAmount <= 1)
    {
        capture(focusSettleTime->value());
        return true;
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

    const bool focusingOut = amount > 0;
    const QString dirStr = focusingOut ? i18n("outward") : i18n("inward");
    m_LastFocusDirection = focusingOut ? FOCUS_OUT : FOCUS_IN;

    if (focusingOut)
        m_Focuser->focusOut();
    else
        m_Focuser->focusIn();

    // Keep track of motion in case it gets stuck.
    m_FocusMotionTimer.start();

    if (canAbsMove)
    {
        m_LastFocusSteps = currentPosition + amount;
        m_Focuser->moveAbs(currentPosition + amount);
        appendLogText(i18n("Focusing %2 by %1 steps...", absAmount, dirStr));
    }
    else if (canRelMove)
    {
        m_LastFocusSteps = absAmount;
        m_Focuser->moveRel(absAmount);
        appendLogText(i18np("Focusing %2 by %1 step...", "Focusing %2 by %1 steps...", absAmount, dirStr));
    }
    else
    {
        m_LastFocusSteps = absAmount;
        m_Focuser->moveByTimer(absAmount);
        appendLogText(i18n("Focusing %2 by %1 ms...", absAmount, dirStr));
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////////////
void Focus::handleFocusMotionTimeout()
{
    // handleFocusMotionTimeout is called when the focus motion timer times out. This is only
    // relevant to AutoFocus runs.
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

    // We're about to try to move the focuser so restart the timer
    m_FocusMotionTimer.start();

    const QString dirStr = m_LastFocusDirection == FOCUS_OUT ? i18n("outward") : i18n("inward");
    if (canAbsMove)
    {
        m_Focuser->moveAbs(m_LastFocusSteps);
        appendLogText(i18n("Focus motion timed out (%1). Focusing to %2 steps...", m_FocusMotionTimerCounter, m_LastFocusSteps));
    }
    else if (canRelMove)
    {
        m_Focuser->moveRel(m_LastFocusSteps);
        appendLogText(i18n("Focus motion timed out (%1). Focusing %3 by %2 steps...", m_FocusMotionTimerCounter, m_LastFocusSteps,
                           dirStr));
    }
    else
    {
        m_Focuser->moveByTimer(m_LastFocusSteps);
        appendLogText(i18n("Focus motion timed out (%1). Focusing %3 by %2 ms...",
                           m_FocusMotionTimerCounter, m_LastFocusSteps, dirStr));
    }
}

void Focus::reconnectFocuser(const QString &focuser)
{
    m_FocuserReconnectCounter++;

    if (m_Focuser && m_Focuser->getDeviceName() == focuser)
    {
        appendLogText(i18n("Attempting to reconnect focuser: %1", focuser));
        refreshOpticalTrain();
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
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

    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    disconnect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Focus::processData);
    disconnect(m_Camera, &ISD::Camera::error, this, &Ekos::Focus::processCaptureError);

    if (m_ImageData && useFocusDarkFrame->isChecked())
    {
        QVariantMap settings = frameSettings[targetChip];
        uint16_t offsetX     = settings["x"].toInt() / settings["binx"].toInt();
        uint16_t offsetY     = settings["y"].toInt() / settings["biny"].toInt();

        //targetChip->setCaptureFilter(defaultScale);
        m_DarkProcessor->denoise(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()),
                                 targetChip, m_ImageData, focusExposure->value(), offsetX, offsetY);
        return;
    }

    setCaptureComplete();
    resetButtons();
}

void Focus::calculateHFR()
{
    appendLogText(i18n("Detection complete."));

    // Beware as this HFR value is then treated specifically by the graph renderer
    double hfr = FocusAlgorithmInterface::IGNORED_HFR;

    if (m_StarFinderWatcher.result() == false)
    {
        qCWarning(KSTARS_EKOS_FOCUS) << "Failed to extract any stars.";
    }
    else
    {
        if (focusUseFullField->isChecked())
        {
            m_FocusView->setStarFilterRange(static_cast <float> (focusFullFieldInnerRadius->value() / 100.0),
                                            static_cast <float> (focusFullFieldOuterRadius->value() / 100.0));
            m_FocusView->filterStars();

            // Get the average HFR of the whole frame
            hfr = m_ImageData->getHFR(HFR_AVERAGE);
        }
        else
        {
            m_FocusView->setTrackingBoxEnabled(true);

            // JM 2020-10-08: Try to get first the same HFR star already selected before
            // so that it doesn't keep jumping around

            if (starCenter.isNull() == false)
                hfr = m_ImageData->getHFR(starCenter.x(), starCenter.y());

            // If not found, then get the MAX or MEDIAN depending on the selected algorithm.
            if (hfr < 0)
                hfr = m_ImageData->getHFR(m_FocusDetection == ALGORITHM_SEP ? HFR_HIGH : HFR_MAX);
        }
    }

    hfrInProgress = false;
    resetButtons();
    setCurrentHFR(hfr);
}

void Focus::analyzeSources()
{
    appendLogText(i18n("Detecting sources..."));
    hfrInProgress = true;

    QVariantMap extractionSettings;
    extractionSettings["optionsProfileIndex"] = Options::focusOptionsProfile();
    extractionSettings["optionsProfileGroup"] =  static_cast<int>(Ekos::FocusProfiles);
    m_ImageData->setSourceExtractorSettings(extractionSettings);
    // When we're using FULL field view, we always use either CENTROID algorithm which is the default
    // standard algorithm in KStars, or SEP. The other algorithms are too inefficient to run on full frames and require
    // a bounding box for them to be effective in near real-time application.
    if (focusUseFullField->isChecked())
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

bool Focus::appendHFR(double newHFR)
{
    // Add new HFR to existing values, even if invalid
    HFRFrames.append(newHFR);

    // Prepare a work vector with valid HFR values
    QVector <double> samples(HFRFrames);
    samples.erase(std::remove_if(samples.begin(), samples.end(), [](const double HFR)
    {
        return HFR == FocusAlgorithmInterface::IGNORED_HFR;
    }), samples.end());

    // Perform simple sigma clipping if more than a few samples
    if (samples.count() > 3)
    {
        // Sort all HFRs and extract the median
        std::sort(samples.begin(), samples.end());
        const auto median =
            ((samples.size() % 2) ?
             samples[samples.size() / 2] :
             (static_cast<double>(samples[samples.size() / 2 - 1]) + samples[samples.size() / 2]) * .5);

        // Extract the mean
        const auto mean = std::accumulate(samples.begin(), samples.end(), .0) / samples.size();

        // Extract the variance
        double variance = 0;
        foreach (auto val, samples)
            variance += (val - mean) * (val - mean);

        // Deduce the standard deviation
        const double stddev = sqrt(variance / samples.size());

        // Reject those 2 sigma away from median
        const double sigmaHigh = median + stddev * 2;
        const double sigmaLow  = median - stddev * 2;

        // FIXME: why is the first value not considered?
        // FIXME: what if there are less than 3 samples after clipping?
        QMutableVectorIterator<double> i(samples);
        while (i.hasNext())
        {
            auto val = i.next();
            if (val > sigmaHigh || val < sigmaLow)
                i.remove();
        }
    }

    // Consolidate the average HFR
    currentHFR = samples.isEmpty() ? -1 : std::accumulate(samples.begin(), samples.end(), .0) / samples.size();

    // Return whether we need more frame based on user requirement
    return HFRFrames.count() < focusFramesCount->value();
}

void Focus::settle(const FocusState completionState, const bool autoFocusUsed)
{
    state = completionState;
    if (completionState == Ekos::FOCUS_COMPLETE)
    {
        if (autoFocusUsed)
        {
            // Prepare the message for Analyze
            const int size = hfr_position.size();
            QString analysis_results = "";

            for (int i = 0; i < size; ++i)
            {
                analysis_results.append(QString("%1%2|%3")
                                        .arg(i == 0 ? "" : "|" )
                                        .arg(QString::number(hfr_position[i], 'f', 0))
                                        .arg(QString::number(hfr_value[i], 'f', 3)));
            }

            KSNotification::event(QLatin1String("FocusSuccessful"), i18n("Autofocus operation completed successfully"),
                                  KSNotification::Focus);
            emit autofocusComplete(filter(), analysis_results);
        }
    }
    else
    {
        if (autoFocusUsed)
        {
            KSNotification::event(QLatin1String("FocusFailed"), i18n("Autofocus operation failed"),
                                  KSNotification::Focus, KSNotification::Alert);
            emit autofocusAborted(filter(), "");
        }
    }

    qCDebug(KSTARS_EKOS_FOCUS) << "Settled. State:" << Ekos::getFocusStatusString(state);

    // Delay state notification if we have a locked filter pending return to original filter
    if (fallbackFilterPending)
    {
        m_FilterManager->setFilterPosition(fallbackFilterPosition,
                                           static_cast<FilterManager::FilterPolicy>(FilterManager::CHANGE_POLICY | FilterManager::OFFSET_POLICY));
    }
    else
        emit newStatus(state);

    resetButtons();
}

void Focus::completeFocusProcedure(FocusState completionState, bool plot)
{
    if (inAutoFocus)
    {
        if (completionState == Ekos::FOCUS_COMPLETE)
        {
            if (plot)
                emit redrawHFRPlot(polynomialFit.get(), currentPosition, currentHFR);
            appendLogText(i18np("Focus procedure completed after %1 iteration.",
                                "Focus procedure completed after %1 iterations.", hfr_position.count()));

            setLastFocusTemperature();

            // CR add auto focus position, temperature and filter to log in CSV format
            // this will help with setting up focus offsets and temperature compensation
            qCInfo(KSTARS_EKOS_FOCUS) << "Autofocus values: position," << currentPosition << ", temperature,"
                                      << m_LastSourceAutofocusTemperature << ", filter," << filter()
                                      << ", HFR," << currentHFR << ", altitude," << mountAlt;

            if (m_FocusAlgorithm == FOCUS_POLYNOMIAL)
            {
                // Add the final polynomial values to the signal sent to Analyze.
                hfr_position.append(currentPosition);
                hfr_value.append(currentHFR);
            }

            appendFocusLogText(QString("%1, %2, %3, %4, %5\n")
                               .arg(QString::number(currentPosition))
                               .arg(QString::number(m_LastSourceAutofocusTemperature, 'f', 1))
                               .arg(filter())
                               .arg(QString::number(currentHFR, 'f', 3))
                               .arg(QString::number(mountAlt, 'f', 1)));

            // Replace user position with optimal position
            absTicksSpin->setValue(currentPosition);
        }
        // In case of failure, go back to last position if the focuser is absolute
        else if (canAbsMove && initialFocuserAbsPosition >= 0 && resetFocusIteration <= MAXIMUM_RESET_ITERATIONS)
        {
            // If we're doing in-sequence focusing using an absolute focuser, retry focusing once, starting from last known good position
            bool const retry_focusing = m_RestartState == RESTART_NONE && ++resetFocusIteration < MAXIMUM_RESET_ITERATIONS;

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
                emit autofocusAborted(filter(), "");
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
                resetFocusIteration = 0;
                m_RestartState = RESTART_ABORT;
                return;
            }
        }

        // Reset the retry count on success or maximum count
        resetFocusIteration = 0;
    }

    const bool autoFocusUsed = inAutoFocus;

    // Reset the autofocus flags
    stop(completionState);

    // Refresh display if needed
    if (m_FocusAlgorithm == FOCUS_POLYNOMIAL && plot)
        emit drawPolynomial(polynomialFit.get(), isVShapeSolution, true);

    // Enforce settling duration
    int const settleTime = m_GuidingSuspended ? guideSettleTime->value() : 0;

    if (settleTime > 0)
        appendLogText(i18n("Settling for %1s...", settleTime));

    QTimer::singleShot(settleTime * 1000, this, [ &, settleTime, completionState, autoFocusUsed]()
    {
        settle(completionState, autoFocusUsed);

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
        m_FocusMotionTimer.start();
        m_Focuser->moveAbs(initialFocuserAbsPosition);
        /* Restart will be executed by the end-of-move notification from the device if needed by resetFocus */
    }
}

void Focus::setCurrentHFR(double value)
{
    currentHFR = value;

    // Let's now report the current HFR
    qCDebug(KSTARS_EKOS_FOCUS) << "Focus newFITS #" << HFRFrames.count() + 1 << ": Current HFR " << currentHFR << " Num stars "
                               << (starSelected ? 1 : m_ImageData->getDetectedStars());

    // Take the new HFR into account, eventually continue to stack samples
    if (appendHFR(currentHFR))
    {
        capture();
        return;
    }
    else HFRFrames.clear();

    // Let signal the current HFR now depending on whether the focuser is absolute or relative
    if (canAbsMove)
        emit newHFR(currentHFR, currentPosition);
    else
        emit newHFR(currentHFR, -1);

    // Format the HFR value into a string
    QString HFRText = QString("%1").arg(currentHFR, 0, 'f', 2);
    HFROut->setText(HFRText);
    starsOut->setText(QString("%1").arg(m_ImageData->getDetectedStars()));
    iterOut->setText(QString("%1").arg(absIterations + 1));

    // Display message in case _last_ HFR was negative
    if (lastHFR == FocusAlgorithmInterface::IGNORED_HFR)
        appendLogText(i18n("FITS received. No stars detected."));

    // If we have a valid HFR value
    if (currentHFR > 0)
    {
        // Check if we're done from polynomial fitting algorithm
        if (m_FocusAlgorithm == FOCUS_POLYNOMIAL && isVShapeSolution)
        {
            completeFocusProcedure(Ekos::FOCUS_COMPLETE);
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

            // Record the star information (X, Y, currentHFR)
            QVector3D oneStar = starCenter;
            oneStar.setZ(currentHFR);
            starsHFR.append(oneStar);
        }
        else
        {
            // Record the star information (X, Y, currentHFR)
            QVector3D oneStar(starCenter.x(), starCenter.y(), currentHFR);
            starsHFR.append(oneStar);
        }

        if (currentHFR > maxHFR)
            maxHFR = currentHFR;

        // Append point to the #Iterations vs #HFR chart in case of looping or in case in autofocus with a focus
        // that does not support position feedback.

        // If inAutoFocus is true without canAbsMove and without canRelMove, canTimerMove must be true.
        // We'd only want to execute this if the focus linear algorithm is not being used, as that
        // algorithm simulates a position-based system even for timer-based focusers.
        if (inFocusLoop || (inAutoFocus && ! isPositionBased()))
        {
            int pos = hfr_position.empty() ? 1 : hfr_position.last() + 1;
            addPlotPosition(pos, currentHFR);
        }
    }
    else
    {
        // Let's record an invalid star result - IGNORED_HFR must be suitable as invalid star HFR!
        QVector3D oneStar(starCenter.x(), starCenter.y(), FocusAlgorithmInterface::IGNORED_HFR);
        starsHFR.append(oneStar);
    }

    // First check that we haven't already search for stars
    // Since star-searching algorithm are time-consuming, we should only search when necessary
    m_FocusView->updateFrame();

    setHFRComplete();
}

void Focus::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    // If we have a box, sync the bounding box to its position.
    syncTrackingBoxPosition();

    // Notify user if we're not looping
    if (inFocusLoop == false)
        appendLogText(i18n("Image received."));

    if (captureInProgress && inFocusLoop == false && inAutoFocus == false)
        m_Camera->setUploadMode(rememberUploadMode);

    if (m_RememberCameraFastExposure && inFocusLoop == false && inAutoFocus == false)
    {
        m_RememberCameraFastExposure = false;
        m_Camera->setFastExposureEnabled(true);
    }

    captureInProgress = false;


    // Emit the whole image
    emit newImage(m_FocusView);
    // Emit the tracking (bounding) box view. Used in Summary View
    emit newStarPixmap(m_FocusView->getTrackingBoxPixmap(10));

    // If we are not looping; OR
    // If we are looping but we already have tracking box enabled; OR
    // If we are asked to analyze _all_ the stars within the field
    // THEN let's find stars in the image and get current HFR
    if (inFocusLoop == false || (inFocusLoop && (m_FocusView->isTrackingBoxEnabled() || focusUseFullField->isChecked())))
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

    // If star is NOT yet selected in a non-full-frame situation
    // then let's now try to find the star. This step is skipped for full frames
    // since there isn't a single star to select as we are only interested in the overall average HFR.
    // We need to check if we can find the star right away, or if we need to _subframe_ around the
    // selected star.
    if (focusUseFullField->isChecked() == false && starCenter.isNull())
    {
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
        if (focusAutoStarEnabled->isChecked())
        {
            // Do we have a valid star detected?
            const Edge selectedHFRStar = m_ImageData->getSelectedHFRStar();

            if (selectedHFRStar.x == -1)
            {
                appendLogText(i18n("Failed to automatically select a star. Please select a star manually."));

                // Center the tracking box in the frame and display it
                m_FocusView->setTrackingBox(QRect(w - focusBoxSize->value() / (subBinX * 2),
                                                  h - focusBoxSize->value() / (subBinY * 2),
                                                  focusBoxSize->value() / subBinX, focusBoxSize->value() / subBinY));
                m_FocusView->setTrackingBoxEnabled(true);

                // Use can now move it to select the desired star
                state = Ekos::FOCUS_WAITING;
                qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
                emit newStatus(state);

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
            if (subFramed == false && focusSubFrame->isEnabled() && focusSubFrame->isChecked())
            {
                int offset = (static_cast<double>(focusBoxSize->value()) / subBinX) * 1.5;
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

            m_FocusView->setTrackingBox(QRect((w - focusBoxSize->value()) / (subBinX * 2),
                                              (h - focusBoxSize->value()) / (2 * subBinY),
                                              focusBoxSize->value() / subBinX, focusBoxSize->value() / subBinY));
            m_FocusView->setTrackingBoxEnabled(true);

            // Now we wait
            state = Ekos::FOCUS_WAITING;
            qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
            emit newStatus(state);

            // If the user does not select for a timeout period, we abort.
            waitStarSelectTimer.start();
            return;
        }
    }

    // Check if the focus module is requested to verify if the minimum HFR value is met.
    if (minimumRequiredHFR >= 0)
    {
        // In case we failed to detected, we capture again.
        if (currentHFR == -1)
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
                completeFocusProcedure(Ekos::FOCUS_ABORTED);
            }
        }
        // If the detect current HFR is more than the minimum required HFR
        // then we should start the autofocus process now to bring it down.
        else if (currentHFR > minimumRequiredHFR)
        {
            qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR:" << currentHFR << "is above required minimum HFR:" << minimumRequiredHFR <<
                                       ". Starting AutoFocus...";
            minimumRequiredHFR = -1;
            start();
        }
        // Otherwise, the current HFR is fine and lower than the required minimum HFR so we announce success.
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR:" << currentHFR << "is below required minimum HFR:" << minimumRequiredHFR <<
                                       ". Autofocus successful.";
            completeFocusProcedure(Ekos::FOCUS_COMPLETE);
        }

        // Nothing more for now
        return;
    }

    // If focus logging is enabled, let's save the frame.
    if (Options::focusLogging() && Options::saveFocusImages())
    {
        QDir dir;
        QDateTime now = KStarsData::Instance()->lt();
        QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("autofocus/" +
                       now.toString("yyyy-MM-dd"));
        dir.mkpath(path);
        // IS8601 contains colons but they are illegal under Windows OS, so replacing them with '-'
        // The timestamp is no longer ISO8601 but it should solve interoperality issues between different OS hosts
        QString name     = "autofocus_frame_" + now.toString("HH-mm-ss") + ".fits.gz";
        QString filename = path + QDir::separator() + name + QStringLiteral("[compress R 100,100]");
        m_ImageData->saveImage(filename);
    }

    // If we are not in autofocus process, we're done.
    if (inAutoFocus == false)
    {
        // If we are done and there is no further autofocus,
        // we reset state to IDLE
        if (state != Ekos::FOCUS_IDLE)
        {
            state = Ekos::FOCUS_IDLE;
            qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
            emit newStatus(state);
        }

        resetButtons();
        return;
    }

    // Set state to progress
    if (state != Ekos::FOCUS_PROGRESS)
    {
        state = Ekos::FOCUS_PROGRESS;
        qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
        emit newStatus(state);
    }

    // Now let's kick in the algorithms

    if (m_FocusAlgorithm == FOCUS_LINEAR || m_FocusAlgorithm == FOCUS_LINEAR1PASS)
        autoFocusLinear();
    else if (canAbsMove || canRelMove)
        // Position-based algorithms
        autoFocusAbs();
    else
        // Time open-looped algorithms
        autoFocusRel();
}

void Focus::clearDataPoints()
{
    maxHFR = 1;
    polynomialFit.reset();
    hfr_position.clear();
    hfr_value.clear();
    isVShapeSolution = false;
    emit initHFRPlot(inFocusLoop == false && isPositionBased());
}

bool Focus::autoFocusChecks()
{
    if (++absIterations > MAXIMUM_ABS_ITERATIONS)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try increasing tolerance value."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
        return false;
    }

    // No stars detected, try to capture again
    if (currentHFR == FocusAlgorithmInterface::IGNORED_HFR)
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
            // JEE TODO: Linear currently continues if there are no stars
            // For L1P I think its better to Abort and give control back either to the user or the scheduler
            // This needs to be revisited to find the best way of dealing with this scenario.
            appendLogText(i18n("Failed to detect any stars at position %1. Continuing...", currentPosition));
            noStarCount = 0;
        }
        else
        {
            appendLogText(i18n("Failed to detect any stars. Reset frame and try again."));
            completeFocusProcedure(Ekos::FOCUS_ABORTED);
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
    QVector<double> HFRs;
    QVector<double> sigmas;
    QVector<int> positions;
    linearFocuser->getMeasurements(&positions, &HFRs, &sigmas);
    const FocusAlgorithmInterface::FocusParams &params = linearFocuser->getParams();

    // As an optimization for slower machines, e.g. RPi4s, if the points are the same except for
    // the last point, just emit the last point instead of redrawing everything.
    static QVector<double> lastHFRs;
    static QVector<int> lastPositions;
    bool incrementalChange = false;
    if (positions.size() > 1 && positions.size() == lastPositions.size() + 1)
    {
        bool ok = true;
        for (int i = 0; i < positions.size() - 1; ++i)
            if (positions[i] != lastPositions[i] || HFRs[i] != lastHFRs[i])
            {
                ok = false;
                break;
            }
        incrementalChange = ok;
    }
    lastPositions = positions;
    lastHFRs = HFRs;

    if (params.useWeights)
    {
        if (incrementalChange)
            emit newHFRPlotPositionWithSigma(static_cast<double>(positions.last()), HFRs.last(), sigmas.last(), params.initialStepSize,
                                             plt);
        else
        {
            emit initHFRPlot(true);
            for (int i = 0; i < positions.size(); ++i)
                emit newHFRPlotPositionWithSigma(static_cast<double>(positions[i]), HFRs[i], sigmas[i], params.initialStepSize, plt);
        }
    }
    else
    {
        if (incrementalChange)
            emit newHFRPlotPosition(static_cast<double>(positions.last()), HFRs.last(), params.initialStepSize, plt);
        else
        {
            emit initHFRPlot(true);
            for (int i = 0; i < positions.size(); ++i)
                emit newHFRPlotPosition(static_cast<double>(positions[i]), HFRs[i], params.initialStepSize, plt);
        }
    }

    // Plot the polynomial, if there are enough points.
    if (HFRs.size() > 3)
    {
        // The polynomial should only reflect 1st-pass samples.
        QVector<double> pass1HFRs, pass1Sigmas;
        QVector<int> pass1Positions;
        double minPosition, minValue;
        double searchMin = std::max(params.minPositionAllowed, params.startPosition - params.maxTravel);
        double searchMax = std::min(params.maxPositionAllowed, params.startPosition + params.maxTravel);

        linearFocuser->getPass1Measurements(&pass1Positions, &pass1HFRs, &pass1Sigmas);
        if (m_FocusAlgorithm == FOCUS_LINEAR)
        {
            // TODO: Need to determine whether to change LINEAR over to the LM solver in CurveFitting
            // This will be determined after L1P's first release has been deemed successful.
            polynomialFit.reset(new PolynomialFit(2, pass1Positions, pass1HFRs));

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
            curveFitting->fitCurve(pass1Positions, pass1HFRs, pass1Sigmas, params.curveFit, params.useWeights);

            if (curveFitting->findMin(params.startPosition, searchMin, searchMax, &minPosition, &minValue, params.curveFit))
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
    HFROut->setText(QString("%1").arg(linearFocuser->latestHFR(), 0, 'f', 2));

    emit setTitle(linearFocuser->getTextStatus(R2));

    if (!plt) HFRPlot->replot();
}

// Called after the first pass is complete and we're moving to the final focus position
// Calculate R2 for the curve and update the graph.
void Focus::plotLinearFinalUpdates()
{
    emit updateTitle(linearFocuser->getTextStatus(R2), true);
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

    addPlotPosition(currentPosition, currentHFR, false);

    // Only use the relativeHFR algorithm if full field is enabled with one capture/measurement.
    bool useFocusStarsHFR = focusUseFullField->isChecked() && focusFramesCount->value() == 1;
    auto focusStars = useFocusStarsHFR || (m_FocusAlgorithm == FOCUS_LINEAR1PASS) ? &(m_ImageData->getStarCenters()) : nullptr;
    int nextPosition;

    linearRequestedPosition = linearFocuser->newMeasurement(currentPosition, currentHFR, focusStars);
    if (m_FocusAlgorithm == FOCUS_LINEAR1PASS && linearFocuser->isDone())
        // Linear 1 Pass is done, graph is drawn, so just move to the focus position, and update the graph title.
        plotLinearFinalUpdates();
    else
        // Update the graph with the next datapoint, draw the curve, etc.
        plotLinearFocus();

    nextPosition = adjustLinearPosition(currentPosition, linearRequestedPosition, focusAFOverscan->value());

    if (linearFocuser->isDone())
    {
        if (linearFocuser->solution() != -1)
        {
            // Now test that the curve fit was acceptable. If not retry the focus process using standard retry process
            // R2 check is only available for Linear 1 Pass for Hyperbola and Parabola
            if (m_CurveFit == CurveFitting::FOCUS_QUADRATIC)
                // Linear only uses Quadratic so need to do the R2 check, just complete
                completeFocusProcedure(Ekos::FOCUS_COMPLETE, false);
            else if (R2 >= focusR2Limit->value())
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear Curve Fit check passed R2=%1 focusR2Limit=%2").arg(R2).arg(
                                               focusR2Limit->value());
                completeFocusProcedure(Ekos::FOCUS_COMPLETE, false);
                R2Retries = 0;
            }
            else if (R2Retries == 0)
            {
                // Failed the R2 check for the first time so retry...
                appendLogText(i18n("Curve Fit check failed R2=%1 focusR2Limit=%2 retrying...", R2, focusR2Limit->value()));
                completeFocusProcedure(Ekos::FOCUS_ABORTED, false);
                R2Retries++;
            }
            else
            {
                // Retried after an R2 check fail but failed again so... log msg and continue
                appendLogText(i18n("Curve Fit check failed again R2=%1 focusR2Limit=%2 but continuing...", R2, focusR2Limit->value()));
                completeFocusProcedure(Ekos::FOCUS_COMPLETE, false);
                R2Retries = 0;
            }
        }
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << linearFocuser->doneReason();
            appendLogText("Linear autofocus algorithm aborted.");
            completeFocusProcedure(Ekos::FOCUS_ABORTED, false);
        }
        return;
    }
    else
    {
        const int delta = nextPosition - currentPosition;

        if (!changeFocus(delta))
            completeFocusProcedure(Ekos::FOCUS_ABORTED, false);

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

    QString deltaTxt = QString("%1").arg(fabs(currentHFR - minHFR) * 100.0, 0, 'g', 3);
    QString HFRText  = QString("%1").arg(currentHFR, 0, 'g', 3);

    qCDebug(KSTARS_EKOS_FOCUS) << "========================================";
    qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR: " << currentHFR << " Current Position: " << currentPosition;
    qCDebug(KSTARS_EKOS_FOCUS) << "Last minHFR: " << minHFR << " Last MinHFR Pos: " << minHFRPos;
    qCDebug(KSTARS_EKOS_FOCUS) << "Delta: " << deltaTxt << "%";
    qCDebug(KSTARS_EKOS_FOCUS) << "========================================";

    if (minHFR)
        appendLogText(i18n("FITS received. HFR %1 @ %2. Delta (%3%)", HFRText, currentPosition, deltaTxt));
    else
        appendLogText(i18n("FITS received. HFR %1 @ %2.", HFRText, currentPosition));

    if (!autoFocusChecks())
        return;

    addPlotPosition(currentPosition, currentHFR);

    switch (m_LastFocusDirection)
    {
        case FOCUS_NONE:
            lastHFR                   = currentHFR;
            initialFocuserAbsPosition = currentPosition;
            minHFR                    = currentHFR;
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

            if (!changeFocus(pulseDuration))
                completeFocusProcedure(Ekos::FOCUS_ABORTED);

            break;

        case FOCUS_IN:
        case FOCUS_OUT:
            if (reverseDir && focusInLimit && focusOutLimit &&
                    fabs(currentHFR - minHFR) < (focusTolerance->value() / 100.0) && HFRInc == 0)
            {
                if (absIterations <= 2)
                {
                    QString message = i18n("Change in HFR is too small. Try increasing the step size or decreasing the tolerance.");
                    appendLogText(message);
                    KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                    completeFocusProcedure(Ekos::FOCUS_ABORTED);
                }
                else if (noStarCount > 0)
                {
                    QString message = i18n("Failed to detect focus star in frame. Capture and select a focus star.");
                    appendLogText(message);
                    KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                    completeFocusProcedure(Ekos::FOCUS_ABORTED);
                }
                else
                {
                    completeFocusProcedure(Ekos::FOCUS_COMPLETE);
                }
                break;
            }
            else if (currentHFR < lastHFR)
            {
                // Let's now limit the travel distance of the focuser
                if (HFRInc >= 1 && m_LastFocusDirection == FOCUS_OUT && lastHFRPos < focusInLimit && fabs(currentHFR - lastHFR) > 0.1)
                {
                    focusInLimit = lastHFRPos;
                    qCDebug(KSTARS_EKOS_FOCUS) << "New FocusInLimit " << focusInLimit;
                }
                else if (HFRInc >= 1 && m_LastFocusDirection == FOCUS_IN && lastHFRPos > focusOutLimit &&
                         fabs(currentHFR - lastHFR) > 0.1)
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

                lastHFR = currentHFR;

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
                    if (hfr_position.count() >= 2)
                    {
                        hfr_position.remove(hfr_position.count() - 2);
                        hfr_value.remove(hfr_value.count() - 2);
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

                lastHFR      = currentHFR;
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

                    if (m_FocusAlgorithm == FOCUS_POLYNOMIAL && hfr_position.count() > 4)
                    {
                        polynomialFit.reset(new PolynomialFit(2, 5, hfr_position, hfr_value));
                        double a = *std::min_element(hfr_position.constBegin(), hfr_position.constEnd());
                        double b = *std::max_element(hfr_position.constBegin(), hfr_position.constEnd());
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
                    completeFocusProcedure(Ekos::FOCUS_COMPLETE);
                }
                else
                {
                    QString message = i18n("Focuser cannot move further, device limits reached. Autofocus aborted.");
                    appendLogText(message);
                    KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                    completeFocusProcedure(Ekos::FOCUS_ABORTED);
                }
                return;
            }

            // Too many fluctuatoins
            if (fluctuations >= MAXIMUM_FLUCTUATIONS)
            {
                QString message = i18n("Unstable fluctuations. Try increasing initial step size or exposure time.");
                appendLogText(message);
                KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                completeFocusProcedure(Ekos::FOCUS_ABORTED);
                return;
            }

            // Ops, deadlock
            if (focusOutLimit && focusOutLimit == focusInLimit)
            {
                QString message = i18n("Deadlock reached. Please try again with different settings.");
                appendLogText(message);
                KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                completeFocusProcedure(Ekos::FOCUS_ABORTED);
                return;
            }

            // Restrict the target position even more with the maximum travel option
            if (fabs(targetPosition - initialFocuserAbsPosition) > focusMaxTravel->value())
            {
                int minTravelLimit = qMax(0.0, initialFocuserAbsPosition - focusMaxTravel->value());
                int maxTravelLimit = qMin(absMotionMax, initialFocuserAbsPosition + focusMaxTravel->value());

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
                                               << initialFocuserAbsPosition << ") exceeds maxTravel distance of " << focusMaxTravel->value();

                    QString message = i18n("Maximum travel limit reached. Autofocus aborted.");
                    appendLogText(message);
                    KSNotification::event(QLatin1String("FocusFailed"), message, KSNotification::Focus, KSNotification::Alert);
                    completeFocusProcedure(Ekos::FOCUS_ABORTED);
                    break;
                }
            }

            // Get delta for next move
            lastDelta = (targetPosition - currentPosition);

            qCDebug(KSTARS_EKOS_FOCUS) << "delta (targetPosition - currentPosition) " << lastDelta;

            // Limit to Maximum permitted delta (Max Single Step Size)
            if (ignoreLimitedDelta == false)
            {
                double limitedDelta = qMax(-1.0 * focusMaxSingleStep->value(), qMin(1.0 * focusMaxSingleStep->value(), lastDelta));
                if (std::fabs(limitedDelta - lastDelta) > 0)
                {
                    qCDebug(KSTARS_EKOS_FOCUS) << "Limited delta to maximum permitted single step " << focusMaxSingleStep->value();
                    lastDelta = limitedDelta;
                }
            }

            // Now cross your fingers and wait
            if (!changeFocus(lastDelta))
                completeFocusProcedure(Ekos::FOCUS_ABORTED);

            break;
    }
}

void Focus::addPlotPosition(int pos, double hfr, bool plot)
{
    hfr_position.append(pos);
    hfr_value.append(hfr);
    if (plot)
        emit newHFRPlotPosition(pos, hfr, pulseDuration);
}

void Focus::autoFocusRel()
{
    static int noStarCount = 0;
    static double minHFR   = 1e6;
    QString deltaTxt       = QString("%1").arg(fabs(currentHFR - minHFR) * 100.0, 0, 'g', 2);
    QString minHFRText     = QString("%1").arg(minHFR, 0, 'g', 3);
    QString HFRText        = QString("%1").arg(currentHFR, 0, 'g', 3);

    appendLogText(i18n("FITS received. HFR %1. Delta (%2%) Min HFR (%3)", HFRText, deltaTxt, minHFRText));

    if (pulseDuration <= MINIMUM_PULSE_TIMER)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try adjusting the tolerance value."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == FocusAlgorithmInterface::IGNORED_HFR)
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
        else
        {
            appendLogText(i18n("Failed to detect any stars. Reset frame and try again."));
            completeFocusProcedure(Ekos::FOCUS_ABORTED);
            return;
        }
    }
    else
        noStarCount = 0;

    switch (m_LastFocusDirection)
    {
        case FOCUS_NONE:
            lastHFR = currentHFR;
            minHFR  = 1e6;
            changeFocus(-pulseDuration);
            break;

        case FOCUS_IN:
        case FOCUS_OUT:
            if (fabs(currentHFR - minHFR) < (focusTolerance->value() / 100.0) && HFRInc == 0)
            {
                completeFocusProcedure(Ekos::FOCUS_COMPLETE);
            }
            else if (currentHFR < lastHFR)
            {
                if (currentHFR < minHFR)
                    minHFR = currentHFR;

                lastHFR = currentHFR;
                changeFocus(m_LastFocusDirection == FOCUS_IN ? -pulseDuration : pulseDuration);
                HFRInc = 0;
            }
            else
            {
                //HFRInc++;

                lastHFR = currentHFR;

                HFRInc = 0;

                pulseDuration *= 0.75;

                if (!changeFocus(m_LastFocusDirection == FOCUS_IN ? pulseDuration : -pulseDuration))
                    completeFocusProcedure(Ekos::FOCUS_ABORTED);
            }
            break;
    }
}

void Focus::autoFocusProcessPositionChange(IPState state)
{
    if (state == IPS_OK && captureInProgress == false)
    {
        // Normally, if we are auto-focusing, after we move the focuser we capture an image.
        // However, the Linear algorithm, at the start of its passes, requires two
        // consecutive focuser moves--the first out further than we want, and a second
        // move back in, so that we eliminate backlash and are always moving in before a capture.
        if (focuserAdditionalMovement > 0)
        {
            int temp = focuserAdditionalMovement;
            focuserAdditionalMovement = 0;
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Undoing backlash extension. Moving back in by %1").arg(temp);

            if (!focusIn(temp))
            {
                appendLogText(i18n("Focuser error, check INDI panel."));
                completeFocusProcedure(Ekos::FOCUS_ABORTED);
            }
        }
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Focus position reached at %1, starting capture in %2 seconds.").arg(
                                           currentPosition).arg(focusSettleTime->value());
            capture(focusSettleTime->value());
        }
    }
    else if (state == IPS_ALERT)
    {
        appendLogText(i18n("Focuser error, check INDI panel."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
    }
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
        focusBacklash->setValue(nvp->np[0].value);
        return;
    }

    if (nvp->isNameMatch("ABS_FOCUS_POSITION"))
    {
        if (m_DebugFocuser)
        {
            // Simulate focuser comms issues every 5 moves
            if (m_DebugFocuserCounter++ >= 5 && m_DebugFocuserCounter <= 1000)
            {
                if (m_DebugFocuserCounter == 1000)
                    m_DebugFocuserCounter = 0;
                appendLogText(i18n("Simulate focuser comms failure..."));
                return;
            }
        }

        m_FocusMotionTimer.stop();
        INumber *pos = IUFindNumber(nvp, "FOCUS_ABSOLUTE_POSITION");

        // FIXME: We should check state validity, but some focusers do not care - make ignore an option!
        if (pos)
        {
            int newPosition = static_cast<int>(pos->value);

            // Some absolute focuser constantly report the position without a state change.
            // Therefore we ignore it if both value and state are the same as last time.
            // HACK: This would shortcut the autofocus procedure reset, see completeFocusProcedure for the small hack
            if (currentPosition == newPosition && currentPositionState == nvp->s)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << "Abs Focuser still at " << currentPosition << ". State: " << pstateStr(
                                               currentPositionState);
                return;
            }

            currentPositionState = nvp->s;

            if (currentPosition != newPosition)
            {
                currentPosition = newPosition;
                qCDebug(KSTARS_EKOS_FOCUS) << "Abs Focuser position changed to " << currentPosition << ". State: " << pstateStr(
                                               currentPositionState);
                absTicksLabel->setText(QString::number(currentPosition));
                emit absolutePositionChanged(currentPosition);
            }
            else
                qCDebug(KSTARS_EKOS_FOCUS) << "Abs Focuser still at " << currentPosition << ". State: " << pstateStr(
                                               currentPositionState);
        }
        else
            qCDebug(KSTARS_EKOS_FOCUS) << "Abs Focuser unable to get focuser position. Last position " << currentPosition << ". State: "
                                       << pstateStr(nvp->s);

        if (nvp->s != IPS_OK)
        {
            if (inAutoFocus)
            {
                // We had something back from the focuser but we're not done yet, so
                // restart motion timer in case focuser gets stuck.
                qCDebug(KSTARS_EKOS_FOCUS) << "Restarting focus motion timer. State: " << pstateStr(nvp->s);
                m_FocusMotionTimer.start();
            }
        }
        else
        {
            // Systematically reset UI when focuser finishes moving
            resetButtons();

            if (adjustFocus)
            {
                adjustFocus = false;
                m_LastFocusDirection = FOCUS_NONE;
                emit focusPositionAdjusted();
                return;
            }

            if (m_RestartState == RESTART_NOW && status() != Ekos::FOCUS_ABORTED)
            {
                m_RestartState = RESTART_NONE;
                inAutoFocus = false;
                appendLogText(i18n("Restarting autofocus process..."));
                start();
            }
            else if (m_RestartState == RESTART_ABORT)
            {
                // We are trying to abort an autofocus run
                // This event means that the focuser has been reset and arrived at its starting point
                // so we can finish processing the abort. Set inAutoFocus to avoid repeating
                // processing already done in completeFocusProcedure
                m_RestartState = RESTART_NONE;
                inAutoFocus = false;
                completeFocusProcedure(Ekos::FOCUS_ABORTED);
            }
        }

        if (canAbsMove && inAutoFocus)
        {
            autoFocusProcessPositionChange(nvp->s);
        }
        else if (nvp->s == IPS_ALERT)
            appendLogText(i18n("Focuser error, check INDI panel."));
        return;
    }

    if (canAbsMove)
        return;

    if (nvp->isNameMatch("manualfocusdrive"))
    {
        m_FocusMotionTimer.stop();

        INumber *pos = IUFindNumber(nvp, "manualfocusdrive");
        if (pos && nvp->s == IPS_OK)
        {
            currentPosition += pos->value;
            absTicksLabel->setText(QString::number(static_cast<int>(currentPosition)));
            emit absolutePositionChanged(currentPosition);
        }

        if (adjustFocus && nvp->s == IPS_OK)
        {
            adjustFocus = false;
            m_LastFocusDirection = FOCUS_NONE;
            emit focusPositionAdjusted();
            return;
        }

        // restart if focus movement has finished
        if (m_RestartState == RESTART_NOW && nvp->s == IPS_OK && status() != Ekos::FOCUS_ABORTED)
        {
            m_RestartState = RESTART_NONE;
            inAutoFocus = false;
            appendLogText(i18n("Restarting autofocus process..."));
            start();
        }
        else if (m_RestartState == RESTART_ABORT && nvp->s == IPS_OK)
        {
            // Abort the autofocus run now the focuser has finished moving to its start position
            m_RestartState = RESTART_NONE;
            inAutoFocus = false;
            completeFocusProcedure(Ekos::FOCUS_ABORTED);
        }

        if (canRelMove && inAutoFocus)
        {
            autoFocusProcessPositionChange(nvp->s);
        }
        else if (nvp->s == IPS_ALERT)
            appendLogText(i18n("Focuser error, check INDI panel."));

        return;
    }

    if (nvp->isNameMatch("REL_FOCUS_POSITION"))
    {
        m_FocusMotionTimer.stop();

        INumber *pos = IUFindNumber(nvp, "FOCUS_RELATIVE_POSITION");
        if (pos && nvp->s == IPS_OK)
        {
            currentPosition += pos->value * (m_LastFocusDirection == FOCUS_IN ? -1 : 1);
            qCDebug(KSTARS_EKOS_FOCUS)
                    << QString("Rel Focuser position changed by %1 to %2")
                    .arg(pos->value).arg(currentPosition);
            absTicksLabel->setText(QString::number(static_cast<int>(currentPosition)));
            emit absolutePositionChanged(currentPosition);
        }

        if (adjustFocus && nvp->s == IPS_OK)
        {
            adjustFocus = false;
            m_LastFocusDirection = FOCUS_NONE;
            emit focusPositionAdjusted();
            return;
        }

        // restart if focus movement has finished
        if (m_RestartState == RESTART_NOW && nvp->s == IPS_OK && status() != Ekos::FOCUS_ABORTED)
        {
            m_RestartState = RESTART_NONE;
            inAutoFocus = false;
            appendLogText(i18n("Restarting autofocus process..."));
            start();
        }
        else if (m_RestartState == RESTART_ABORT && nvp->s == IPS_OK)
        {
            // Abort the autofocus run now the focuser has finished moving to its start position
            m_RestartState = RESTART_NONE;
            inAutoFocus = false;
            completeFocusProcedure(Ekos::FOCUS_ABORTED);
        }

        if (canRelMove && inAutoFocus)
        {
            autoFocusProcessPositionChange(nvp->s);
        }
        else if (nvp->s == IPS_ALERT)
            appendLogText(i18n("Focuser error, check INDI panel."));

        return;
    }

    if (canRelMove)
        return;

    if (nvp->isNameMatch("FOCUS_TIMER"))
    {
        m_FocusMotionTimer.stop();
        // restart if focus movement has finished
        if (m_RestartState == RESTART_NOW && nvp->s == IPS_OK && status() != Ekos::FOCUS_ABORTED)
        {
            m_RestartState = RESTART_NONE;
            inAutoFocus = false;
            appendLogText(i18n("Restarting autofocus process..."));
            start();
        }
        else if (m_RestartState == RESTART_ABORT && nvp->s == IPS_OK)
        {
            // Abort the autofocus run now the focuser has finished moving to its start position
            m_RestartState = RESTART_NONE;
            inAutoFocus = false;
            completeFocusProcedure(Ekos::FOCUS_ABORTED);
        }

        if (canAbsMove == false && canRelMove == false && inAutoFocus)
        {
            // Used by the linear focus algorithm. Ignored if that's not in use for the timer-focuser.
            INumber *pos = IUFindNumber(nvp, "FOCUS_TIMER_VALUE");
            if (pos)
            {
                currentPosition += pos->value * (m_LastFocusDirection == FOCUS_IN ? -1 : 1);
                qCDebug(KSTARS_EKOS_FOCUS)
                        << QString("Timer Focuser position changed by %1 to %2")
                        .arg(pos->value).arg(currentPosition);
            }
            autoFocusProcessPositionChange(nvp->s);
        }
        else if (nvp->s == IPS_ALERT)
            appendLogText(i18n("Focuser error, check INDI panel."));

        return;
    }
}

void Focus::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_FOCUS) << text;

    emit newLog(text);
}

void Focus::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

void Focus::appendFocusLogText(const QString &lines)
{
    if (Options::focusLogging())
    {

        if (!m_FocusLogFile.exists())
        {
            // Create focus-specific log file and write the header record
            QDir dir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
            dir.mkpath("focuslogs");
            m_FocusLogEnabled = m_FocusLogFile.open(QIODevice::WriteOnly | QIODevice::Text);
            if (m_FocusLogEnabled)
            {
                QTextStream header(&m_FocusLogFile);
                header << "date, time, position, temperature, filter, HFR, altitude\n";
                header.flush();
            }
            else
                qCWarning(KSTARS_EKOS_FOCUS) << "Failed to open focus log file: " << m_FocusLogFileName;
        }

        if (m_FocusLogEnabled)
        {
            QTextStream out(&m_FocusLogFile);
            out << QDateTime::currentDateTime().toString("yyyy-MM-dd, hh:mm:ss, ") << lines;
            out.flush();
        }
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
    HFRFrames.clear();

    clearDataPoints();

    //emit statusUpdated(true);
    state = Ekos::FOCUS_FRAMING;
    qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
    emit newStatus(state);

    resetButtons();

    appendLogText(i18n("Starting continuous exposure..."));

    capture();
}

void Focus::resetButtons()
{
    if (inFocusLoop)
    {
        startFocusB->setEnabled(false);
        startLoopB->setEnabled(false);
        stopFocusB->setEnabled(true);
        captureB->setEnabled(false);
        opticalTrainCombo->setEnabled(false);
        trainB->setEnabled(false);
        return;
    }

    if (inAutoFocus)
    {
        stopFocusB->setEnabled(true);
        startFocusB->setEnabled(false);
        startLoopB->setEnabled(false);
        captureB->setEnabled(false);
        focusOutB->setEnabled(false);
        focusInB->setEnabled(false);
        startGotoB->setEnabled(false);
        stopGotoB->setEnabled(false);
        resetFrameB->setEnabled(false);
        opticalTrainCombo->setEnabled(false);
        trainB->setEnabled(false);
        return;
    }

    auto enableCaptureButtons = (captureInProgress == false && hfrInProgress == false);

    opticalTrainCombo->setEnabled(true);
    trainB->setEnabled(true);

    captureB->setEnabled(enableCaptureButtons);
    resetFrameB->setEnabled(enableCaptureButtons);
    startLoopB->setEnabled(enableCaptureButtons);

    if (m_Focuser && m_Focuser->isConnected())
    {
        focusOutB->setEnabled(true);
        focusInB->setEnabled(true);

        startFocusB->setEnabled(m_FocusType == FOCUS_AUTO);
        stopFocusB->setEnabled(!enableCaptureButtons);
        startGotoB->setEnabled(canAbsMove);
        stopGotoB->setEnabled(true);
    }
    else
    {
        focusOutB->setEnabled(false);
        focusInB->setEnabled(false);

        startFocusB->setEnabled(false);
        stopFocusB->setEnabled(false);
        startGotoB->setEnabled(false);
        stopGotoB->setEnabled(false);
    }
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
    if (state == Ekos::FOCUS_PROGRESS)
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

    int offset = (static_cast<double>(focusBoxSize->value()) / subBinX) * 1.5;

    QRect starRect;

    bool squareMovedOutside = false;

    if (subFramed == false && focusSubFrame->isChecked() && targetChip->canSubframe())
    {
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

        //starRect = QRect((w-focusBoxSize->value())/(subBinX*2), (h-focusBoxSize->value())/(subBinY*2), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY);
        starCenter.setX(w / (2 * subBinX));
        starCenter.setY(h / (2 * subBinY));
    }
    else
    {
        //starRect = QRect(x-focusBoxSize->value()/(subBinX*2), y-focusBoxSize->value()/(subBinY*2), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY);
        double dist = sqrt((starCenter.x() - x) * (starCenter.x() - x) + (starCenter.y() - y) * (starCenter.y() - y));

        squareMovedOutside = (dist > (static_cast<double>(focusBoxSize->value()) / subBinX));
        starCenter.setX(x);
        starCenter.setY(y);
        //starRect = QRect( starCenter.x()-focusBoxSize->value()/(2*subBinX), starCenter.y()-focusBoxSize->value()/(2*subBinY), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY);
        starRect = QRect(starCenter.x() - focusBoxSize->value() / (2 * subBinX),
                         starCenter.y() - focusBoxSize->value() / (2 * subBinY), focusBoxSize->value() / subBinX,
                         focusBoxSize->value() / subBinY);
        m_FocusView->setTrackingBox(starRect);
    }

    starsHFR.clear();

    starCenter.setZ(subBinX);

    if (squareMovedOutside && inAutoFocus == false && focusAutoStarEnabled->isChecked())
    {
        focusAutoStarEnabled->blockSignals(true);
        focusAutoStarEnabled->setChecked(false);
        focusAutoStarEnabled->blockSignals(false);
        appendLogText(i18n("Disabling Auto Star Selection as star selection box was moved manually."));
        starSelected = false;
    }
    else if (starSelected == false)
    {
        appendLogText(i18n("Focus star is selected."));
        starSelected = true;
        capture();
    }

    waitStarSelectTimer.stop();
    FocusState nextState = inAutoFocus ? FOCUS_PROGRESS : FOCUS_IDLE;
    if (nextState != state)
    {
        state = nextState;
        qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
        emit newStatus(state);
    }
}

void Focus::checkFocus(double requiredHFR)
{
    if (inAutoFocus || inFocusLoop)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "Check Focus rejected, focus procedure is already running.";
    }
    else
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "Check Focus requested with minimum required HFR" << requiredHFR;
        minimumRequiredHFR = requiredHFR;

        appendLogText("Capturing to check HFR...");
        capture();
    }
}

void Focus::toggleSubframe(bool enable)
{
    if (enable == false)
        resetFrame();

    starSelected = false;
    starCenter   = QVector3D();

    if (focusUseFullField->isChecked())
        focusUseFullField->setChecked(!enable);

    if (focusUseFullField->isChecked() && (m_CurveFit == CurveFitting::FOCUS_HYPERBOLA
                                           || m_CurveFit == CurveFitting::FOCUS_PARABOLA))
        // Allow focusUseWeights for fullframe and Hyperbola or Parabola
        focusUseWeights->setEnabled(true);
    else if (m_CurveFit == CurveFitting::FOCUS_HYPERBOLA || m_CurveFit == CurveFitting::FOCUS_PARABOLA)
    {
        focusUseWeights->setEnabled(false);
        focusUseWeights->setChecked(false);
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
    focusAutoStarEnabled->setChecked(enable);
}

void Focus::setAutoSubFrameEnabled(bool enable)
{
    focusSubFrame->setChecked(enable);
}

void Focus::setAutoFocusParameters(int boxSize, int stepSize, int maxTravel, double tolerance)
{
    focusBoxSize->setValue(boxSize);
    focusTicks->setValue(stepSize);
    focusMaxTravel->setValue(maxTravel);
    focusTolerance->setValue(tolerance);
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
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
    }
    else if (state == FOCUS_WAITING)
    {
        state = FOCUS_IDLE;
        qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
        emit newStatus(state);
    }
}

void Focus::setAbsoluteFocusTicks()
{
    if (m_Focuser == nullptr)
    {
        appendLogText(i18n("Error: No Focuser detected."));
        checkStopFocus(true);
        return;
    }

    if (m_Focuser->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to Focuser."));
        checkStopFocus(true);
        return;
    }

    qCDebug(KSTARS_EKOS_FOCUS) << "Setting focus ticks to " << absTicksSpin->value();

    m_Focuser->moveAbs(absTicksSpin->value());
}

//void Focus::setActiveBinning(int bin)
//{
//    activeBin = bin + 1;
//    Options::setFocusXBin(activeBin);
//}

// TODO remove from kstars.kcfg
/*void Focus::setFrames(int value)
{
    Options::setFocusFrames(value);
}*/

void Focus::syncTrackingBoxPosition()
{
    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    Q_ASSERT(targetChip);

    int subBinX = 1, subBinY = 1;
    targetChip->getBinning(&subBinX, &subBinY);

    if (starCenter.isNull() == false)
    {
        double boxSize = focusBoxSize->value();
        int x, y, w, h;
        targetChip->getFrame(&x, &y, &w, &h);
        // If box size is larger than image size, set it to lower index
        if (boxSize / subBinX >= w || boxSize / subBinY >= h)
        {
            focusBoxSize->setValue((boxSize / subBinX >= w) ? w : h);
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

        QRect starRect = QRect(starCenter.x() - boxSize / (2 * subBinX), starCenter.y() - boxSize / (2 * subBinY),
                               boxSize / subBinX, boxSize / subBinY);
        m_FocusView->setTrackingBoxEnabled(true);
        m_FocusView->setTrackingBox(starRect);
    }
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
        }
        else if (fv->updateData(m_ImageData, url, lastFVTabID, &lastFVTabID) == false)
            fv->loadData(m_ImageData, url, &lastFVTabID);

        fv->show();
    }
}

void Focus::adjustFocusOffset(int value, bool useAbsoluteOffset)
{
    adjustFocus = true;

    int relativeOffset = 0;

    if (useAbsoluteOffset == false)
        relativeOffset = value;
    else
        relativeOffset = value - currentPosition;

    changeFocus(relativeOffset);
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

    ////////////////////////////////////////////////////////////////////////////////////////
    /// Focus Module ----> Filter Manager connections
    ////////////////////////////////////////////////////////////////////////////////////////

    // Update focuser absolute position.
    connect(this, &Focus::absolutePositionChanged, m_FilterManager.get(), &FilterManager::setFocusAbsolutePosition);

    // Update Filter Manager state
    connect(this, &Focus::newStatus, this, [this](Ekos::FocusState state)
    {
        if (m_FilterManager)
        {
            m_FilterManager->setFocusStatus(state);
            if (focusFilter->currentIndex() != -1 && canAbsMove && state == Ekos::FOCUS_COMPLETE)
            {
                m_FilterManager->setFilterAbsoluteFocusPosition(focusFilter->currentIndex(), currentPosition);
            }
        }
    });

    ////////////////////////////////////////////////////////////////////////////////////////
    /// Filter Manager ----> Focus Module connections
    ////////////////////////////////////////////////////////////////////////////////////////

    // Suspend guiding if filter offset is change with OAG
    connect(m_FilterManager.get(), &FilterManager::newStatus, this, [this](Ekos::FilterState filterState)
    {
        // If we are changing filter offset while idle, then check if we need to suspend guiding.
        if (filterState == FILTER_OFFSET && state != Ekos::FOCUS_PROGRESS)
        {
            if (m_GuidingSuspended == false && focusSuspendGuiding->isChecked())
            {
                m_GuidingSuspended = true;
                emit suspendGuiding();
            }
        }
    });

    // Take action once filter manager completes filter position
    connect(m_FilterManager.get(), &FilterManager::ready, this, [this]()
    {
        if (filterPositionPending)
        {
            filterPositionPending = false;
            capture();
        }
        else if (fallbackFilterPending)
        {
            fallbackFilterPending = false;
            emit newStatus(state);
        }
    });

    // Take action when filter operation fails
    connect(m_FilterManager.get(), &FilterManager::failed, this, [this]()
    {
        appendLogText(i18n("Filter operation failed."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
    });

    // Check focus if required by filter manager
    connect(m_FilterManager.get(), &FilterManager::checkFocus, this, &Focus::checkFocus);

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
        if (m_GuidingSuspended && state != Ekos::FOCUS_PROGRESS)
        {
            QTimer::singleShot(focusSettleTime->value() * 1000, this, [this]()
            {
                m_GuidingSuspended = false;
                emit resumeGuiding();
            });
        }
    });

    // Save focus exposure for a particular filter
    connect(focusExposure, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (m_FilterManager)
            m_FilterManager->setFilterExposure(focusFilter->currentIndex(), focusExposure->value());
    });

    // Load exposure if filter is changed.
    connect(focusFilter, &QComboBox::currentTextChanged, this, [this](const QString & text)
    {
        if (m_FilterManager)
            focusExposure->setValue(m_FilterManager->getFilterExposure(text));
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
    captureTimeoutCounter++;

    if (captureTimeoutCounter >= 3)
    {
        captureTimeoutCounter = 0;
        captureTimeout.stop();
        appendLogText(i18n("Exposure timeout. Aborting..."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
    }
    else
    {
        appendLogText(i18n("Exposure timeout. Restarting exposure..."));
        ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
        targetChip->abortExposure();

        prepareCapture(targetChip);

        if (targetChip->capture(focusExposure->value()))
        {
            // Timeout is exposure duration + timeout threshold in seconds
            //long const timeout = lround(ceil(focusExposure->value() * 1000)) + FOCUS_TIMEOUT_THRESHOLD;
            captureTimeout.start(Options::focusCaptureTimeout() * 1000);

            if (inFocusLoop == false)
                appendLogText(i18n("Capturing image again..."));

            resetButtons();
        }
        else if (inAutoFocus)
        {
            completeFocusProcedure(Ekos::FOCUS_ABORTED);
        }
    }
}

void Focus::processCaptureError(ISD::Camera::ErrorType type)
{
    if (type == ISD::Camera::ERROR_SAVE)
    {
        appendLogText(i18n("Failed to save image. Aborting..."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
        return;
    }

    captureFailureCounter++;

    if (captureFailureCounter >= 3)
    {
        captureFailureCounter = 0;
        appendLogText(i18n("Exposure failure. Aborting..."));
        completeFocusProcedure(Ekos::FOCUS_ABORTED);
        return;
    }

    appendLogText(i18n("Exposure failure. Restarting exposure..."));
    ISD::CameraChip *targetChip = m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    targetChip->abortExposure();
    targetChip->capture(focusExposure->value());
}

void Focus::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QComboBox *cbox = nullptr;

    QString key;
    QVariant value;

    if ( (dsb = qobject_cast<QDoubleSpinBox*>(sender())))
    {
        key = dsb->objectName();
        value = dsb->value();

    }
    else if ( (sb = qobject_cast<QSpinBox*>(sender())))
    {
        key = sb->objectName();
        value = sb->value();
    }
    else if ( (cb = qobject_cast<QCheckBox*>(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (cbox = qobject_cast<QComboBox*>(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }

    // Save immediately
    Options::self()->setProperty(key.toLatin1(), value);
    Options::self()->save();

    m_Settings[key] = value;
    m_GlobalSettings[key] = value;

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
    for (auto &oneWidget : findChildren<QComboBox*>())
    {
        if (oneWidget->objectName() == "opticalTrainCombo")
            continue;

        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
    }

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
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
    for (auto &oneWidget : findChildren<QSpinBox*>())
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
    for (auto &oneWidget : findChildren<QCheckBox*>())
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

void Focus::connectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Focus::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        connect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Focus::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        connect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Focus::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::Focus::syncSettings);

    // Train combo box should NOT be synced.
    disconnect(opticalTrainCombo, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Focus::syncSettings);
}

void Focus::disconnectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Focus::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        disconnect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Focus::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        disconnect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Focus::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::Focus::syncSettings);

}


void Focus::initPlots()
{
    connect(clearDataB, &QPushButton::clicked, this, &Ekos::Focus::clearDataPoints);

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
    connect(startFocusB, &QPushButton::clicked, this, &Ekos::Focus::start);
    connect(stopFocusB, &QPushButton::clicked, this, &Ekos::Focus::abort);

    // Focus IN/OUT
    connect(focusOutB, &QPushButton::clicked, this, &Ekos::Focus::focusOut);
    connect(focusInB, &QPushButton::clicked, this, &Ekos::Focus::focusIn);

    // Capture a single frame
    connect(captureB, &QPushButton::clicked, this, &Ekos::Focus::capture);
    // Start continuous capture
    connect(startLoopB, &QPushButton::clicked, this, &Ekos::Focus::startFraming);
    // Use a subframe when capturing
    connect(focusSubFrame, &QCheckBox::toggled, this, &Ekos::Focus::toggleSubframe);
    // Reset frame dimensions to default
    connect(resetFrameB, &QPushButton::clicked, this, &Ekos::Focus::resetFrame);
    // Sync setting if full field setting is toggled.
    connect(focusUseFullField, &QCheckBox::toggled, this, [&](bool toggled)
    {
        focusFullFieldInnerRadius->setEnabled(toggled);
        focusFullFieldOuterRadius->setEnabled(toggled);
        if (toggled)
        {
            focusSubFrame->setChecked(false);
            focusAutoStarEnabled->setChecked(false);
            if (m_CurveFit == CurveFitting::FOCUS_HYPERBOLA || m_CurveFit == CurveFitting::FOCUS_PARABOLA)
                // If we are using parabola or hyperbola enable setWeights
                focusUseWeights->setEnabled(true);
        }
        else
        {
            // Disable the overlay
            m_FocusView->setStarFilterRange(0, 1);
            if (m_CurveFit == CurveFitting::FOCUS_HYPERBOLA || m_CurveFit == CurveFitting::FOCUS_PARABOLA)
            {
                focusUseWeights->setEnabled(false);
                focusUseWeights->setChecked(false);
            }
        }
    });

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
    connect(focusBoxSize, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Ekos::Focus::updateBoxSize);

    // Update the focuser star detection if the detection algorithm selection changes.
    connect(focusDetection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&](int index)
    {
        m_FocusDetection = static_cast<StarAlgorithm>(index);
        focusThreshold->setEnabled(m_FocusDetection == ALGORITHM_THRESHOLD);
        focusMultiRowAverage->setEnabled(m_FocusDetection == ALGORITHM_BAHTINOV);
        if (m_FocusDetection == ALGORITHM_BAHTINOV)
        {
            // In case of Bahtinov mask uncheck auto select star
            focusAutoStarEnabled->setChecked(false);
            focusBoxSize->setMaximum(512);
        }
        else
        {
            // When not using Bathinov mask, limit box size to 256 and make sure value stays within range.
            if (focusBoxSize->value() > 256)
            {
                // Focus box size changed, update control
                focusBoxSize->setValue(focusBoxSize->value());
            }
            focusBoxSize->setMaximum(256);
        }
        focusAutoStarEnabled->setEnabled(m_FocusDetection != ALGORITHM_BAHTINOV);
    });

    // Update the focuser solution algorithm if the selection changes.
    connect(focusAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&](int index)
    {
        setFocusAlgorithm(static_cast<Algorithm>(index));
    });

    // Update the curve fit if the selection changes. Use the currentIndexChanged method rather than
    // activated as the former fires when the index is changed by the user AND if changed programmatically
    connect(focusCurveFit, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&](int index)
    {
        setCurveFit(static_cast<CurveFitting::CurveFit>(index));
    });

    // Reset star center on auto star check toggle
    connect(focusAutoStarEnabled, &QCheckBox::toggled, this, [&](bool enabled)
    {
        if (enabled)
        {
            starCenter   = QVector3D();
            starSelected = false;
            m_FocusView->setTrackingBox(QRect());
        }
    });
}

void Focus::setFocusAlgorithm(Algorithm algorithm)
{
    m_FocusAlgorithm = algorithm;
    switch(algorithm)
    {
        case FOCUS_ITERATIVE:
            focusOutSteps->setEnabled(false);             // Out step multiple
            focusMaxTravel->setEnabled(true);             // Max Travel
            focusTicks->setEnabled(true);                 // Initial Step Size
            focusMaxSingleStep->setEnabled(true);         // Max Step Size
            focusTolerance->setEnabled(true);             // Solution tolerance
            focusCurveFit->setEnabled(false);             // Curve fit can only be QUADRATIC
            focusCurveFit->setCurrentIndex(CurveFitting::FOCUS_QUADRATIC);
            focusAFOverscan->setEnabled(false);           // Overscan off
            break;

        case FOCUS_POLYNOMIAL:
            focusOutSteps->setEnabled(false);             // Out step multiple
            focusMaxTravel->setEnabled(true);             // Max Travel
            focusTicks->setEnabled(true);                 // Initial Step Size
            focusMaxSingleStep->setEnabled(true);         // Max Step Size
            focusTolerance->setEnabled(true);             // Solution tolerance
            focusCurveFit->setEnabled(false);             // Curve fit can only be QUADRATIC
            focusCurveFit->setCurrentIndex(CurveFitting::FOCUS_QUADRATIC);
            focusAFOverscan->setEnabled(false);           // Overscan off
            break;

        case FOCUS_LINEAR:
            focusOutSteps->setEnabled(true);              // Out step multiple
            focusMaxTravel->setEnabled(true);             // Max Travel
            focusTicks->setEnabled(true);                 // Initial Step Size
            focusMaxSingleStep->setEnabled(false);        // Max Step Size
            focusTolerance->setEnabled(true);             // Solution tolerance
            focusCurveFit->setEnabled(false);             // Curve fit can only be QUADRATIC
            focusCurveFit->setCurrentIndex(CurveFitting::FOCUS_QUADRATIC);
            focusAFOverscan->setEnabled(true);            // Overscan on
            break;

        case FOCUS_LINEAR1PASS:
            focusOutSteps->setEnabled(true);              // Out step multiple
            focusMaxTravel->setEnabled(true);             // Max Travel
            focusTicks->setEnabled(true);                 // Initial Step Size
            focusMaxSingleStep->setEnabled(false);        // Max Step Size
            focusTolerance->setEnabled(false);            // Solution tolerance
            focusCurveFit->setEnabled(true);              // Curve fit
            focusAFOverscan->setEnabled(true);            // Overscan on
            break;
    }
}

void Focus::setCurveFit(CurveFitting::CurveFit curve)
{
    m_CurveFit = curve;
    switch(m_CurveFit)
    {
        case CurveFitting::FOCUS_QUADRATIC:
            focusUseWeights->setEnabled(false);             // Use weights not allowed
            focusUseWeights->setChecked(false);
            focusR2Limit->setEnabled(false);                // focusR2Limit not allowed
            break;

        case CurveFitting::FOCUS_HYPERBOLA:
            focusUseWeights->setEnabled(
                focusUseFullField->isChecked());    // Only use weights on multi-stars with analysis (=FullField)
            focusR2Limit->setEnabled(true);                            // focusR2Limit allowed
            break;

        case CurveFitting::FOCUS_PARABOLA:
            focusUseWeights->setEnabled(
                focusUseFullField->isChecked());    // Only use weights on multi-stars with analysis (=FullField)
            focusR2Limit->setEnabled(true);                            // focusR2Limit allowed
            break;
    }
}

void Focus::initView()
{
    m_FocusView.reset(new FITSView(focusingWidget, FITS_FOCUS));
    m_FocusView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_FocusView->setBaseSize(focusingWidget->size());
    m_FocusView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(m_FocusView.get());
    focusingWidget->setLayout(vlayout);
    connect(m_FocusView.get(), &FITSView::trackingStarSelected, this, &Ekos::Focus::focusStarSelected, Qt::UniqueConnection);
    m_FocusView->setStarsEnabled(true);
    m_FocusView->setStarsHFREnabled(true);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
QVariantMap Focus::getAllSettings() const
{
    QVariantMap settings;

    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->currentText());

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    return settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Focus::setAllSettings(const QVariantMap &settings)
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
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Focus, m_Settings);

    // Restablish connections
    connectSettings();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool Focus::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
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

    return false;
};

void Focus::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &Focus::refreshOpticalTrain);
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        ProfileSettings::Instance()->setOneSetting(ProfileSettings::FocusOpticalTrain,
                OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index)));
        refreshOpticalTrain();
        emit trainChanged();
    });
    refreshOpticalTrain();
}

void Focus::refreshOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(opticalTrainCombo->count() > 0);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::FocusOpticalTrain);

    if (trainID.isValid())
    {
        auto id = trainID.toUInt();

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::FocusOpticalTrain, id);
        }

        auto name = OpticalTrainManager::Instance()->name(id);

        opticalTrainCombo->setCurrentText(name);

        auto focuser = OpticalTrainManager::Instance()->getFocuser(name);
        setFocuser(focuser);

        auto camera = OpticalTrainManager::Instance()->getCamera(name);
        if (camera)
        {
            auto scope = OpticalTrainManager::Instance()->getScope(name);
            opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(camera->getDeviceName(), scope["name"].toString()));
        }
        setCamera(camera);

        auto filterWheel = OpticalTrainManager::Instance()->getFilterWheel(name);
        setFilterWheel(filterWheel);

        // Load train settings
        OpticalTrainSettings::Instance()->setOpticalTrainID(id);
        auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Focus);
        if (settings.isValid())
            setAllSettings(settings.toJsonObject().toVariantMap());
        else
            m_Settings = m_GlobalSettings;
    }

    opticalTrainCombo->blockSignals(false);
}

}

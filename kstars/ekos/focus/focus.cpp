/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "focus.h"

#include "focusadaptor.h"
#include "focusalgorithms.h"
#include "polynomialfit.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "auxiliary/kspaths.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"
#include "indi/indifilter.h"
#include "ksnotification.h"

#include <basedevice.h>

#include <gsl/gsl_fit.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_min.h>

#include <ekos_focus_debug.h>

#define FOCUS_TIMEOUT_THRESHOLD  120000
#define MAXIMUM_ABS_ITERATIONS   30
#define MAXIMUM_RESET_ITERATIONS 2
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

    // #7 Image Effects
    for (auto &filter : FITSViewer::filterTypes)
        filterCombo->addItem(filter);
    filterCombo->setCurrentIndex(Options::focusEffect());
    defaultScale = static_cast<FITSScale>(Options::focusEffect());
    connect(filterCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Focus::filterChangeWarning);

    // #8 Load All settings
    loadSettings();

    // #9 Init Setting Connection now
    initSettingsConnections();

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    appendLogText(i18n("Idle."));

    // Create an autofocus CSV file, dated at startup time
    QString  dir = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "focuslogs/";
    m_FocusLogFileName = dir + "autofocus-" + QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss") + ".txt";
    m_FocusLogFile.setFileName(m_FocusLogFileName);

}

Focus::~Focus()
{
    if (focusingWidget->parent() == nullptr)
        toggleFocusingWidgetFullScreen();

    m_FocusLogFile.close();
}

void Focus::resetFrame()
{
    if (currentCCD)
    {
        ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

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

            focusView->setTrackingBox(QRect());
        }
    }
}

bool Focus::setCamera(const QString &device)
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

QString Focus::camera()
{
    if (currentCCD)
        return currentCCD->getDeviceName();

    return QString();
}

void Focus::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
    {
        ccdNum = CCDCaptureCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum >= 0 && ccdNum < CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
        if (targetChip && targetChip->isCapturing())
            return;

        for (ISD::CCD *oneCCD : CCDs)
        {
            if (oneCCD == currentCCD)
                continue;
            if (captureInProgress == false)
                oneCCD->disconnect(this);
        }

        if (targetChip)
        {
            targetChip->setImageView(focusView, FITS_FOCUS);

            binningCombo->setEnabled(targetChip->canBin());
            useSubFrame->setEnabled(targetChip->canSubframe());
            if (targetChip->canBin())
            {
                int subBinX = 1, subBinY = 1;
                binningCombo->clear();
                targetChip->getMaxBin(&subBinX, &subBinY);
                for (int i = 1; i <= subBinX; i++)
                    binningCombo->addItem(QString("%1x%2").arg(i).arg(i));

                activeBin = Options::focusXBin();
                binningCombo->setCurrentIndex(activeBin - 1);
            }
            else
                activeBin = 1;

            QStringList isoList = targetChip->getISOList();
            ISOCombo->clear();

            if (isoList.isEmpty())
            {
                ISOCombo->setEnabled(false);
                ISOLabel->setEnabled(false);
            }
            else
            {
                ISOCombo->setEnabled(true);
                ISOLabel->setEnabled(true);
                ISOCombo->addItems(isoList);
                ISOCombo->setCurrentIndex(targetChip->getISOIndex());
            }

            connect(currentCCD, &ISD::CCD::videoStreamToggled, this, &Ekos::Focus::setVideoStreamEnabled, Qt::UniqueConnection);

            liveVideoB->setEnabled(currentCCD->hasVideoStream());
            if (currentCCD->hasVideoStream())
                setVideoStreamEnabled(currentCCD->isStreamingEnabled());
            else
                liveVideoB->setIcon(QIcon::fromTheme("camera-off"));


            bool hasGain = currentCCD->hasGain();
            gainLabel->setEnabled(hasGain);
            gainIN->setEnabled(hasGain && currentCCD->getGainPermission() != IP_RO);
            if (hasGain)
            {
                double gain = 0, min = 0, max = 0, step = 1;
                currentCCD->getGainMinMaxStep(&min, &max, &step);
                if (currentCCD->getGain(&gain))
                {
                    gainIN->setMinimum(min);
                    gainIN->setMaximum(max);
                    if (step > 0)
                        gainIN->setSingleStep(step);

                    double defaultGain = Options::focusGain();
                    if (defaultGain > 0)
                        gainIN->setValue(defaultGain);
                    else
                        gainIN->setValue(gain);
                }
            }
            else
                gainIN->clear();
        }
    }

    syncCCDInfo();
}

void Focus::syncCCDInfo()
{
    if (currentCCD == nullptr)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    useSubFrame->setEnabled(targetChip->canSubframe());

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

                settings["x"]    = useSubFrame->isChecked() ? x : minX;
                settings["y"]    = useSubFrame->isChecked() ? y : minY;
                settings["w"]    = useSubFrame->isChecked() ? w : maxW;
                settings["h"]    = useSubFrame->isChecked() ? h : maxH;
                settings["binx"] = binx;
                settings["biny"] = biny;

                frameSettings[targetChip] = settings;
            }
        }
    }
}

void Focus::addFilter(ISD::GDInterface *newFilter)
{
    for (auto &oneFilter : Filters)
    {
        if (oneFilter->getDeviceName() == newFilter->getDeviceName())
            return;
    }

    FilterCaptureLabel->setEnabled(true);
    FilterDevicesCombo->setEnabled(true);
    FilterPosLabel->setEnabled(true);
    FilterPosCombo->setEnabled(true);
    filterManagerB->setEnabled(true);

    FilterDevicesCombo->addItem(newFilter->getDeviceName());

    Filters.append(static_cast<ISD::Filter *>(newFilter));

    int filterWheelIndex = 1;
    if (Options::defaultFocusFilterWheel().isEmpty() == false)
        filterWheelIndex = FilterDevicesCombo->findText(Options::defaultFocusFilterWheel());

    if (filterWheelIndex < 1)
        filterWheelIndex = 1;

    checkFilter(filterWheelIndex);
    FilterDevicesCombo->setCurrentIndex(filterWheelIndex);
}

bool Focus::setFilterWheel(const QString &device)
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

QString Focus::filterWheel()
{
    if (FilterDevicesCombo->currentIndex() >= 1)
        return FilterDevicesCombo->currentText();

    return QString();
}

bool Focus::setFilter(const QString &filter)
{
    if (FilterDevicesCombo->currentIndex() >= 1)
    {
        FilterPosCombo->setCurrentText(filter);
        return true;
    }

    return false;
}

QString Focus::filter()
{
    return FilterPosCombo->currentText();
}

void Focus::checkFilter(int filterNum)
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

    //Options::setDefaultFocusFilterWheel(currentFilter->getDeviceName());

    filterManager->setCurrentFilterWheel(currentFilter);

    FilterPosCombo->clear();

    FilterPosCombo->addItems(filterManager->getFilterLabels());

    currentFilterPosition = filterManager->getFilterPosition();

    FilterPosCombo->setCurrentIndex(currentFilterPosition - 1);

    //Options::setDefaultFocusFilterWheelFilter(FilterPosCombo->currentText());

    exposureIN->setValue(filterManager->getFilterExposure());
}

void Focus::addFocuser(ISD::GDInterface *newFocuser)
{
    ISD::Focuser *oneFocuser = static_cast<ISD::Focuser *>(newFocuser);

    if (Focusers.contains(oneFocuser))
        return;

    focuserCombo->addItem(oneFocuser->getDeviceName());

    Focusers.append(oneFocuser);

    currentFocuser = oneFocuser;

    checkFocuser();
}

bool Focus::setFocuser(const QString &device)
{
    for (int i = 0; i < focuserCombo->count(); i++)
        if (device == focuserCombo->itemText(i))
        {
            focuserCombo->setCurrentIndex(i);
            checkFocuser(i);
            return true;
        }

    return false;
}

QString Focus::focuser()
{
    if (currentFocuser)
        return currentFocuser->getDeviceName();

    return QString();
}

void Focus::checkFocuser(int FocuserNum)
{
    if (FocuserNum == -1)
        FocuserNum = focuserCombo->currentIndex();

    if (FocuserNum == -1)
    {
        currentFocuser = nullptr;
        return;
    }

    if (FocuserNum < Focusers.count())
        currentFocuser = Focusers.at(FocuserNum);

    filterManager->setFocusReady(currentFocuser->isConnected());

    // Disconnect all focusers
    for (auto &oneFocuser : Focusers)
    {
        disconnect(oneFocuser, &ISD::GDInterface::numberUpdated, this, &Ekos::Focus::processFocusNumber);
    }

    hasDeviation = currentFocuser->hasDeviation();

    canAbsMove = currentFocuser->canAbsMove();

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

    canRelMove = currentFocuser->canRelMove();

    // In case we have a purely relative focuser, we pretend
    // it is an absolute focuser with initial point set at 50,000.
    // This is done we can use the same algorithm used for absolute focuser.
    if (canAbsMove == false && canRelMove == true)
    {
        currentPosition = 50000;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }

    canTimerMove = currentFocuser->canTimerMove();

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

    focusType = (canRelMove || canAbsMove || canTimerMove) ? FOCUS_AUTO : FOCUS_MANUAL;

    bool hasBacklash = currentFocuser->hasBacklash();
    focusBacklashSpin->setEnabled(hasBacklash);
    focusBacklashSpin->disconnect(this);
    if (hasBacklash)
    {
        double min = 0, max = 0, step = 0;
        currentFocuser->getMinMaxStep("FOCUS_BACKLASH_STEPS", "FOCUS_BACKLASH_VALUE", &min, &max, &step);
        focusBacklashSpin->setMinimum(min);
        focusBacklashSpin->setMaximum(max);
        focusBacklashSpin->setSingleStep(step);
        focusBacklashSpin->setValue(currentFocuser->getBacklash());
        connect(focusBacklashSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value)
        {
            if (currentFocuser)
                currentFocuser->setBacklash(value);
        });
    }
    else
    {
        focusBacklashSpin->setValue(0);
    }

    initializeFocuserTemperature();

    connect(currentFocuser, &ISD::GDInterface::numberUpdated, this, &Ekos::Focus::processFocusNumber, Qt::UniqueConnection);
    //connect(currentFocuser, SIGNAL(propertyDefined(INDI::Property*)), this, &Ekos::Focus::(registerFocusProperty(INDI::Property*)), Qt::UniqueConnection);

    resetButtons();

    //if (!inAutoFocus && !inFocusLoop && !captureInProgress && !inSequenceFocus)
    //  emit autoFocusFinished(true, -1);
}

void Focus::addCCD(ISD::GDInterface *newCCD)
{
    if (CCDs.contains(static_cast<ISD::CCD *>(newCCD)))
        return;

    CCDs.append(static_cast<ISD::CCD *>(newCCD));

    CCDCaptureCombo->addItem(newCCD->getDeviceName());

    checkCCD();
}

void Focus::getAbsFocusPosition()
{
    if (!canAbsMove)
        return;

    INumberVectorProperty *absMove = currentFocuser->getBaseDevice()->getNumber("ABS_FOCUS_POSITION");

    if (absMove)
    {
        currentPosition = static_cast<int>(absMove->np[0].value);
        absMotionMax    = absMove->np[0].max;
        absMotionMin    = absMove->np[0].min;

        absTicksSpin->setMinimum(absMove->np[0].min);
        absTicksSpin->setMaximum(absMove->np[0].max);
        absTicksSpin->setSingleStep(absMove->np[0].step);

        maxTravelIN->setMinimum(absMove->np[0].min);
        maxTravelIN->setMaximum(absMove->np[0].max);

        absTicksLabel->setText(QString::number(currentPosition));

        stepIN->setMaximum(absMove->np[0].max / 2);
        //absTicksSpin->setValue(currentPosition);
    }
}

void Focus::initializeFocuserTemperature()
{
    INumberVectorProperty *temperatureProperty = currentFocuser->getBaseDevice()->getNumber("FOCUS_TEMPERATURE");

    if (temperatureProperty && temperatureProperty->s != IPS_ALERT)
    {
        focuserTemperature = temperatureProperty->np[0].value;
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

    emit newFocusTemperatureDelta(0);
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
        emit newFocusTemperatureDelta(abs(newTemperature - lastFocusTemperature));
    }
    else
    {
        emit newFocusTemperatureDelta(0);
    }
}

void Focus::start()
{
    if (currentCCD == nullptr)
    {
        appendLogText(i18n("No CCD connected."));
        return;
    }

    lastFocusDirection = FOCUS_NONE;

    polySolutionFound = 0;

    waitStarSelectTimer.stop();

    starsHFR.clear();

    lastHFR = 0;

    // Keep the  last focus temperature, it can still be useful in case the autofocus fails
    // lastFocusTemperature

    if (canAbsMove)
    {
        absIterations = 0;
        getAbsFocusPosition();
        pulseDuration = stepIN->value();
    }
    else if (canRelMove)
    {
        //appendLogText(i18n("Setting dummy central position to 50000"));
        absIterations   = 0;
        pulseDuration   = stepIN->value();
        //currentPosition = 50000;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }
    else
    {
        pulseDuration = stepIN->value();

        absIterations   = 0;
        absMotionMax    = 100000;
        absMotionMin    = 0;

        if (pulseDuration <= MINIMUM_PULSE_TIMER)
        {
            appendLogText(i18n("Starting pulse step is too low. Increase the step size to %1 or higher...",
                               MINIMUM_PULSE_TIMER * 5));
            return;
        }
    }

    inAutoFocus = true;
    focuserAdditionalMovement = 0;
    HFRFrames.clear();

    resetButtons();

    reverseDir = false;

    /*if (fw > 0 && fh > 0)
        starSelected= true;
    else
        starSelected= false;*/

    clearDataPoints();

    if (firstGaus)
    {
        profilePlot->removeGraph(firstGaus);
        firstGaus = nullptr;
    }

    //    Options::setFocusTicks(stepIN->value());
    //    Options::setFocusTolerance(toleranceIN->value());
    //    Options::setFocusExposure(exposureIN->value());
    //    Options::setFocusMaxTravel(maxTravelIN->value());
    //    Options::setFocusBoxSize(focusBoxSize->value());
    //    Options::setFocusSubFrame(useSubFrame->isChecked());
    //    Options::setFocusAutoStarEnabled(useAutoStar->isChecked());
    //    Options::setSuspendGuiding(suspendGuideCheck->isChecked());
    //    Options::setUseFocusDarkFrame(darkFrameCheck->isChecked());
    //    Options::setFocusFramesCount(focusFramesSpin->value());
    //    Options::setFocusUseFullField(useFullField->isChecked());

    qCDebug(KSTARS_EKOS_FOCUS)  << "Starting focus with box size: " << focusBoxSize->value()
                                << " Subframe: " << ( useSubFrame->isChecked() ? "yes" : "no" )
                                << " Autostar: " << ( useAutoStar->isChecked() ? "yes" : "no" )
                                << " Full frame: " << ( useFullField->isChecked() ? "yes" : "no " )
                                << " [" << fullFieldInnerRing->value() << "%," << fullFieldOuterRing->value() << "%]"
                                << " Step Size: " << stepIN->value() << " Threshold: " << thresholdSpin->value()
                                << " Gaussian Sigma: " << gaussianSigmaSpin->value()
                                << " Gaussian Kernel size: " << gaussianKernelSizeSpin->value()
                                << " Multi row average: " << multiRowAverageSpin->value()
                                << " Tolerance: " << toleranceIN->value()
                                << " Frames: " << 1 /*focusFramesSpin->value()*/ << " Maximum Travel: " << maxTravelIN->value();

    emit autofocusStarting(focuserTemperature != INVALID_VALUE
                           ? focuserTemperature : observatoryTemperature, filter());

    if (useAutoStar->isChecked())
        appendLogText(i18n("Autofocus in progress..."));
    else
        appendLogText(i18n("Please wait until image capture is complete..."));

    if (suspendGuideCheck->isChecked())
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

    KSNotification::event(QLatin1String("FocusStarted"), i18n("Autofocus operation started"));

    // Used for all the focuser types.
    if (focusAlgorithm == FOCUS_LINEAR)
    {
        const int position = static_cast<int>(currentPosition);
        FocusAlgorithmInterface::FocusParams params(
            maxTravelIN->value(), stepIN->value(), position, absMotionMin, absMotionMax,
            MAXIMUM_ABS_ITERATIONS, toleranceIN->value() / 100.0, filter(),
            focuserTemperature != INVALID_VALUE ? focuserTemperature : observatoryTemperature,
            Options::initialFocusOutSteps());
        linearFocuser.reset(MakeLinearFocuser(params));
        linearRequestedPosition = linearFocuser->initialPosition();
        const int newPosition = adjustLinearPosition(position, linearRequestedPosition);
        if (newPosition != position)
        {
            if (!changeFocus(newPosition - position))
            {
                abort();
                setAutoFocusResult(false);
            }
            // Avoid the capture below.
            return;
        }
    }
    capture();
}

int Focus::adjustLinearPosition(int position, int newPosition)
{
    if (newPosition > position)
    {
        constexpr int extraMotionSteps = 5;
        int adjustment = extraMotionSteps * stepIN->value();
        if (newPosition + adjustment > absMotionMax)
            adjustment = static_cast<int>(absMotionMax) - newPosition;

        focuserAdditionalMovement = adjustment;
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: extending outward movement by %1").arg(adjustment);

        return newPosition + adjustment;
    }
    return newPosition;
}

void Focus::checkStopFocus()
{
    if (inSequenceFocus == true)
    {
        inSequenceFocus = false;
        setAutoFocusResult(false);
    }

    if (captureInProgress && inAutoFocus == false && inFocusLoop == false)
    {
        captureB->setEnabled(true);
        stopFocusB->setEnabled(false);

        appendLogText(i18n("Capture aborted."));
    }

    abort();
}

void Focus::abort()
{
    QString str = "";
    const int size = hfr_position.size();
    for (int i = 0; i < size; ++i)
    {
        str.append(QString("%1%2|%3")
                   .arg(i == 0 ? "" : "|" )
                   .arg(QString::number(hfr_position[i], 'f', 0))
                   .arg(QString::number(hfr_value[i], 'f', 3)));
    }

    emit autofocusAborted(filter(), str);
    stop(true);
}

void Focus::stop(bool aborted)
{
    qCDebug(KSTARS_EKOS_FOCUS) << "Stopping Focus";

    captureTimeout.stop();

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    inAutoFocus        = false;
    focuserAdditionalMovement = 0;
    inFocusLoop        = false;
    // Why starSelected is set to false below? We should retain star selection status under:
    // 1. Autostar is off, or
    // 2. Toggle subframe, or
    // 3. Reset frame
    // 4. Manual motion?

    //starSelected       = false;
    polySolutionFound  = 0;
    captureInProgress  = false;
    captureFailureCounter = 0;
    minimumRequiredHFR = -1;
    noStarCount        = 0;
    HFRFrames.clear();
    //maxHFR=1;

    disconnect(currentCCD, &ISD::CCD::BLOBUpdated, this, &Ekos::Focus::newFITS);
    disconnect(currentCCD, &ISD::CCD::captureFailed, this, &Ekos::Focus::processCaptureFailure);

    if (rememberUploadMode != currentCCD->getUploadMode())
        currentCCD->setUploadMode(rememberUploadMode);

    if (rememberCCDExposureLooping)
        currentCCD->setExposureLoopingEnabled(true);

    targetChip->abortExposure();

    resetButtons();

    absIterations = 0;
    HFRInc        = 0;
    reverseDir    = false;

    //emit statusUpdated(false);
    if (aborted)
    {
        state = Ekos::FOCUS_ABORTED;
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

    if (currentCCD == nullptr)
    {
        appendLogText(i18n("Error: No camera detected."));
        return;
    }

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to camera."));
        return;
    }

    // reset timeout for receiving an image
    captureTimeout.stop();
    // reset timeout for focus star selection
    waitStarSelectTimer.stop();

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    double seqExpose = exposureIN->value();

    if (currentCCD->isBLOBEnabled() == false)
    {
        currentCCD->setBLOBEnabled(true);
    }

    if (currentFilter != nullptr && FilterPosCombo->currentIndex() != -1)
    {
        if (currentFilter->isConnected() == false)
        {
            appendLogText(i18n("Error: Lost connection to filter wheel."));
            return;
        }

        int targetPosition = FilterPosCombo->currentIndex() + 1;
        QString lockedFilter = filterManager->getFilterLock(FilterPosCombo->currentText());

        // We change filter if:
        // 1. Target position is not equal to current position.
        // 2. Locked filter of CURRENT filter is a different filter.
        if (lockedFilter != "--" && lockedFilter != FilterPosCombo->currentText())
        {
            int lockedFilterIndex = FilterPosCombo->findText(lockedFilter);
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
            filterManager->setFilterPosition(targetPosition,
                                             static_cast<FilterManager::FilterPolicy>(FilterManager::CHANGE_POLICY | FilterManager::OFFSET_POLICY));
            return;
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

    currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);

    targetChip->setBinning(activeBin, activeBin);

    targetChip->setCaptureMode(FITS_FOCUS);

    // Always disable filtering if using a dark frame and then re-apply after subtraction. TODO: Implement this in capture and guide and align
    if (darkFrameCheck->isChecked())
        targetChip->setCaptureFilter(FITS_NONE);
    else
        targetChip->setCaptureFilter(defaultScale);

    if (ISOCombo->isEnabled() && ISOCombo->currentIndex() != -1 &&
            targetChip->getISOIndex() != ISOCombo->currentIndex())
        targetChip->setISOIndex(ISOCombo->currentIndex());

    if (gainIN->isEnabled())
        currentCCD->setGain(gainIN->value());

    connect(currentCCD, &ISD::CCD::BLOBUpdated, this, &Ekos::Focus::newFITS);
    connect(currentCCD, &ISD::CCD::captureFailed, this, &Ekos::Focus::processCaptureFailure);

    targetChip->setFrameType(FRAME_LIGHT);

    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        targetChip->setFrame(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(),
                             settings["h"].toInt());
        settings["binx"]          = activeBin;
        settings["biny"]          = activeBin;
        frameSettings[targetChip] = settings;
    }

    captureInProgress = true;
    if (state != FOCUS_PROGRESS)
    {
        state = FOCUS_PROGRESS;
        emit newStatus(state);
    }

    focusView->setBaseSize(focusingWidget->size());

    // Timeout is exposure duration + timeout threshold in seconds
    captureTimeout.start(seqExpose * 1000 + FOCUS_TIMEOUT_THRESHOLD);

    targetChip->capture(seqExpose);

    if (inFocusLoop == false)
    {
        appendLogText(i18n("Capturing image..."));

        if (inAutoFocus == false)
        {
            captureB->setEnabled(false);
            stopFocusB->setEnabled(true);
        }
    }
}

bool Focus::focusIn(int ms)
{
    if (ms == -1)
        ms = stepIN->value();
    return changeFocus(-ms);
}

bool Focus::focusOut(int ms)
{
    if (ms == -1)
        ms = stepIN->value();
    return changeFocus(ms);
}

// If amount > 0 we focus out, otherwise in.
bool Focus::changeFocus(int amount)
{
    if (currentFocuser == nullptr)
        return false;

    // This needs to be re-thought. Just returning does not set the timer
    // and the algorithm ends in limbo.
    // Ignore zero
    // if (amount == 0)
    //    return true;

    if (currentFocuser->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to Focuser."));
        return false;
    }

    const int absAmount = abs(amount);
    const bool focusingOut = amount > 0;
    const QString dirStr = focusingOut ? i18n("outward") : i18n("inward");
    lastFocusDirection = focusingOut ? FOCUS_OUT : FOCUS_IN;

    qCDebug(KSTARS_EKOS_FOCUS) << "Focus " << dirStr << " (" << absAmount << ")";

    if (focusingOut)
        currentFocuser->focusOut();
    else
        currentFocuser->focusIn();

    if (canAbsMove)
    {
        currentFocuser->moveAbs(currentPosition + amount);
        appendLogText(i18n("Focusing %2 by %1 steps...", absAmount, dirStr));
    }
    else if (canRelMove)
    {
        currentFocuser->moveRel(absAmount);
        appendLogText(i18np("Focusing %2 by %1 step...", "Focusing %2 by %1 steps...", absAmount, dirStr));
    }
    else
    {
        currentFocuser->moveByTimer(absAmount);
        appendLogText(i18n("Focusing %2 by %1 ms...", absAmount, dirStr));
    }

    return true;
}

void Focus::newFITS(IBLOB *bp)
{
    if (bp == nullptr)
    {
        capture();
        return;
    }

    // Ignore guide head if there is any.
    if (!strcmp(bp->name, "CCD2"))
        return;

    captureTimeout.stop();
    captureTimeoutCounter = 0;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    disconnect(currentCCD, &ISD::CCD::BLOBUpdated, this, &Ekos::Focus::newFITS);
    disconnect(currentCCD, &ISD::CCD::captureFailed, this, &Ekos::Focus::processCaptureFailure);

    if (darkFrameCheck->isChecked())
    {
        FITSData *darkData   = DarkLibrary::Instance()->getDarkFrame(targetChip, exposureIN->value());
        QVariantMap settings = frameSettings[targetChip];
        uint16_t offsetX     = settings["x"].toInt() / settings["binx"].toInt();
        uint16_t offsetY     = settings["y"].toInt() / settings["biny"].toInt();

        connect(DarkLibrary::Instance(), &DarkLibrary::darkFrameCompleted, this, [&](bool completed)
        {
            DarkLibrary::Instance()->disconnect(this);
            darkFrameCheck->setChecked(completed);
            if (completed)
                setCaptureComplete();
            else
                abort();
            resetButtons();
        });
        connect(DarkLibrary::Instance(), &DarkLibrary::newLog, this, &Ekos::Focus::appendLogText);

        targetChip->setCaptureFilter(defaultScale);

        if (darkData)
            DarkLibrary::Instance()->subtract(darkData, focusView, defaultScale, offsetX, offsetY);
        else
        {
            DarkLibrary::Instance()->captureAndSubtract(targetChip, focusView, exposureIN->value(), offsetX, offsetY);
        }

        return;
    }

    setCaptureComplete();
    resetButtons();
}

double Focus::analyzeSources(FITSData *image_data)
{
    // When we're using FULL field view, we always use either CENTROID algorithm which is the default
    // standard algorithm in KStars, or SEP. The other algorithms are too inefficient to run on full frames and require
    // a bounding box for them to be effective in near real-time application.
    if (Options::focusUseFullField())
    {
        focusView->setTrackingBoxEnabled(false);

        if (focusDetection != ALGORITHM_CENTROID && focusDetection != ALGORITHM_SEP)
            focusView->findStars(ALGORITHM_CENTROID);
        else
            focusView->findStars(focusDetection);

        focusView->setStarFilterRange(static_cast <float> (fullFieldInnerRing->value() / 100.0),
                                      static_cast <float> (fullFieldOuterRing->value() / 100.0));
        focusView->filterStars();

        // Get the average HFR of the whole frame
        return image_data->getHFR(HFR_AVERAGE);
    }
    else
    {
        // If star is already selected then use whatever algorithm currently selected.
        if (starSelected)
        {
            focusView->findStars(focusDetection);
            return image_data->getHFR(HFR_MAX);
        }
        else
        {
            // Disable tracking box
            focusView->setTrackingBoxEnabled(false);

            // If algorithm is set something other than Centeroid or SEP, then force Centroid
            // Since it is the most reliable detector when nothing was selected before.
            if (focusDetection != ALGORITHM_CENTROID && focusDetection != ALGORITHM_SEP)
                focusView->findStars(ALGORITHM_CENTROID);
            else
                // Otherwise, continue to find use using the selected algorithm
                focusView->findStars(focusDetection);

            // Reenable tracking box
            focusView->setTrackingBoxEnabled(true);

            // Get maximum HFR in the frame
            return image_data->getHFR(HFR_MAX);
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
        return HFR == -1;
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
    return HFRFrames.count() < focusFramesSpin->value();
}

void Focus::emitComplete()
{
    QString str = "";
    const int size = hfr_position.size();
    for (int i = 0; i < size; ++i)
    {
        str.append(QString("%1%2|%3")
                   .arg(i == 0 ? "" : "|" )
                   .arg(QString::number(hfr_position[i], 'f', 0))
                   .arg(QString::number(hfr_value[i], 'f', 3)));
    }
    emit autofocusComplete(filter(), str);
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
        currentCCD->setUploadMode(rememberUploadMode);

    if (rememberCCDExposureLooping)
        currentCCD->setExposureLoopingEnabled(true);

    captureInProgress = false;

    // Get handle to the image data
    FITSData *image_data = focusView->getImageData();

    // Emit the tracking (bounding) box view
    emit newStarPixmap(focusView->getTrackingBoxPixmap(10));

    // If we are not looping; OR
    // If we are looping but we already have tracking box enabled; OR
    // If we are asked to analyze _all_ the stars within the field
    // THEN let's find stars in the image and get current HFR
    if (inFocusLoop == false || (inFocusLoop && (focusView->isTrackingBoxEnabled() || Options::focusUseFullField())))
    {
        // First check that we haven't already search for stars
        // Since star-searching algorithm are time-consuming, we should only search when necessary
        if (image_data->areStarsSearched() == false)
        {
            currentHFR = analyzeSources(image_data);
            focusView->updateFrame();
        }

        // Let's now report the current HFR
        qCDebug(KSTARS_EKOS_FOCUS) << "Focus newFITS #" << HFRFrames.count() + 1 << ": Current HFR " << currentHFR << " Num stars "
                                   << (starSelected ? 1 : image_data->getDetectedStars());

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
        starsOut->setText(QString("%1").arg(image_data->getDetectedStars()));

        // Display message in case _last_ HFR was negative
        if (lastHFR == -1)
            appendLogText(i18n("FITS received. No stars detected."));

        // If we have a valid HFR value
        if (currentHFR > 0)
        {
            // Check if we're done from polynomial fitting algorithm
            if (focusAlgorithm == FOCUS_POLYNOMIAL && polySolutionFound == MINIMUM_POLY_SOLUTIONS)
            {
                polySolutionFound = 0;
                emitComplete();
                appendLogText(i18n("Autofocus complete after %1 iterations.", hfr_position.count()));
                stop();
                setAutoFocusResult(true);
                graphPolynomialFunction();
                return;
            }
            Edge *maxStarHFR = nullptr;

            // Center tracking box around selected star (if it valid) either in:
            // 1. Autofocus
            // 2. CheckFocus (minimumHFRCheck)
            // The starCenter _must_ already be defined, otherwise, we proceed until
            // the latter half of the function searches for a star and define it.
            if (starCenter.isNull() == false && (inAutoFocus || minimumRequiredHFR >= 0) &&
                    (maxStarHFR = image_data->getMaxHFRStar()) != nullptr)
            {
                // Now we have star selected in the frame
                starSelected = true;
                starCenter.setX(qMax(0, static_cast<int>(maxStarHFR->x)));
                starCenter.setY(qMax(0, static_cast<int>(maxStarHFR->y)));

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
            if (inFocusLoop || (inAutoFocus && canAbsMove == false && canRelMove == false &&
                                focusAlgorithm != FOCUS_LINEAR))
            {
                if (hfr_position.empty())
                    hfr_position.append(1);
                else
                    hfr_position.append(hfr_position.last() + 1);
                hfr_value.append(currentHFR);

                drawHFRPlot();
            }
        }
        else
        {
            // Let's record an invalid star result
            QVector3D oneStar(starCenter.x(), starCenter.y(), -1);
            starsHFR.append(oneStar);
        }

        // Try to average values and find if we have bogus results
        if (inAutoFocus && starsHFR.count() > 3)
        {
            float mean = 0, sum = 0, stddev = 0, noHFR = 0;

            for (int i = 0; i < starsHFR.count(); i++)
            {
                sum += starsHFR[i].x();
                if (starsHFR[i].z() == -1)
                    noHFR++;
            }

            mean = sum / starsHFR.count();

            // Calculate standard deviation
            for (int i = 0; i < starsHFR.count(); i++)
                stddev += pow(starsHFR[i].x() - mean, 2);

            stddev = sqrt(stddev / starsHFR.count());

            if (currentHFR == -1 && (stddev > focusBoxSize->value() / 10.0 || noHFR / starsHFR.count() > 0.75))
            {
                appendLogText(i18n("No reliable star is detected. Aborting..."));
                abort();
                setAutoFocusResult(false);
                return;
            }
        }
    }

    // If we are just framing, let's capture again
    if (inFocusLoop)
    {
        capture();
        return;
    }

    // Get target chip
    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    // Get target chip binning
    int subBinX = 1, subBinY = 1;
    if (!targetChip->getBinning(&subBinX, &subBinY))
        qCDebug(KSTARS_EKOS_FOCUS) << "Warning: target chip is reporting no binning property, using 1x1.";

    // If star is NOT yet selected in a non-full-frame situation
    // then let's now try to find the star. This step is skipped for full frames
    // since there isn't a single star to select as we are only interested in the overall average HFR.
    // We need to check if we can find the star right away, or if we need to _subframe_ around the
    // selected star.
    if (Options::focusUseFullField() == false && starCenter.isNull())
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
        if (useAutoStar->isChecked())
        {
            // Do we have a valid star detected?
            Edge *maxStar = image_data->getMaxHFRStar();

            if (maxStar == nullptr)
            {
                appendLogText(i18n("Failed to automatically select a star. Please select a star manually."));

                // Center the tracking box in the frame and display it
                focusView->setTrackingBox(QRect(w - focusBoxSize->value() / (subBinX * 2),
                                                h - focusBoxSize->value() / (subBinY * 2),
                                                focusBoxSize->value() / subBinX, focusBoxSize->value() / subBinY));
                focusView->setTrackingBoxEnabled(true);

                // Use can now move it to select the desired star
                state = Ekos::FOCUS_WAITING;
                qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
                emit newStatus(state);

                // Start the wait timer so we abort after a timeout if the user does not make a choice
                waitStarSelectTimer.start();

                return;
            }

            // set the tracking box on maxStar
            starCenter.setX(maxStar->x);
            starCenter.setY(maxStar->y);
            starCenter.setZ(subBinX);
            syncTrackingBoxPosition();

            defaultScale = static_cast<FITSScale>(filterCombo->currentIndex());

            // Do we need to subframe?
            if (subFramed == false && useSubFrame->isEnabled() && useSubFrame->isChecked())
            {
                int offset = (static_cast<double>(focusBoxSize->value()) / subBinX) * 1.5;
                int subX   = (maxStar->x - offset) * subBinX;
                int subY   = (maxStar->y - offset) * subBinY;
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

                focusView->setFirstLoad(true);

                // Now let's capture again for the actual requested subframed image.
                capture();
                return;
            }
            // If we're subframed or don't need subframe, let's record the max star coordinates
            else
            {
                starCenter.setX(maxStar->x);
                starCenter.setY(maxStar->y);
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

            focusView->setTrackingBox(QRect((w - focusBoxSize->value()) / (subBinX * 2),
                                            (h - focusBoxSize->value()) / (2 * subBinY),
                                            focusBoxSize->value() / subBinX, focusBoxSize->value() / subBinY));
            focusView->setTrackingBoxEnabled(true);

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
                appendLogText(i18n("No stars detected, capturing again..."));
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
                setAutoFocusResult(false);
            }
        }
        // If the detect current HFR is more than the minimum required HFR
        // then we should start the autofocus process now to bring it down.
        else if (currentHFR > minimumRequiredHFR)
        {
            qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR:" << currentHFR << "is above required minimum HFR:" << minimumRequiredHFR <<
                                       ". Starting AutoFocus...";
            inSequenceFocus = true;
            start();
        }
        // Otherwise, the current HFR is fine and lower than the required minimum HFR so we announce success.
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR:" << currentHFR << "is below required minimum HFR:" << minimumRequiredHFR <<
                                       ". Autofocus successful.";
            setAutoFocusResult(true);
            drawProfilePlot();
        }

        // We reset minimum required HFR and call it a day.
        minimumRequiredHFR = -1;

        return;
    }

    // Let's draw the HFR Plot
    drawProfilePlot();

    // If focus logging is enabled, let's save the frame.
    if (Options::focusLogging() && Options::saveFocusImages())
    {
        QDir dir;
        QDateTime now = KStarsData::Instance()->lt();
        QString path = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "autofocus/" +
                       now.toString("yyyy-MM-dd");
        dir.mkpath(path);
        // IS8601 contains colons but they are illegal under Windows OS, so replacing them with '-'
        // The timestamp is no longer ISO8601 but it should solve interoperality issues between different OS hosts
        QString name     = "autofocus_frame_" + now.toString("HH-mm-ss") + ".fits";
        QString filename = path + QStringLiteral("/") + name;
        focusView->getImageData()->saveImage(filename);
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

    if (focusAlgorithm == FOCUS_LINEAR)
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
    hfr_position.clear();
    hfr_value.clear();
    polynomialGraph->data()->clear();
    focusPoint->data()->clear();
    polynomialGraphIsShown = false;
    HFRPlot->clearItems();
    polynomialFit.reset();

    drawHFRPlot();
}

void Focus::drawHFRIndeces()
{
    // Put the sample number inside the plot point's circle.
    for (int i = 0; i < hfr_position.size(); ++i)
    {
        QCPItemText *textLabel = new QCPItemText(HFRPlot);
        textLabel->setPositionAlignment(Qt::AlignCenter | Qt::AlignHCenter);
        textLabel->position->setType(QCPItemPosition::ptPlotCoords);
        textLabel->position->setCoords(hfr_position[i], hfr_value[i]);
        textLabel->setText(QString::number(i + 1));
        textLabel->setFont(QFont(font().family(), 12));
        textLabel->setPen(Qt::NoPen);
        textLabel->setColor(Qt::red);
    }
}

void Focus::drawHFRPlot()
{
    // DrawHFRPlot is the base on which other things are built upon.
    // Clear any previous annotations.
    HFRPlot->clearItems();

    v_graph->setData(hfr_position, hfr_value);

    drawHFRIndeces();

    double minHFRVal = currentHFR / 2.5;
    if (hfr_value.size() > 0)
        minHFRVal = std::max(0, static_cast<int>(0.9 * *std::min_element(hfr_value.begin(), hfr_value.end())));

    // True for the position-based algorithms and those that simulate position.
    if (inFocusLoop == false && (canAbsMove || canRelMove || (focusAlgorithm == FOCUS_LINEAR)))
    {
        const double minPosition = hfr_position.empty() ?
                                   0 : *std::min_element(hfr_position.constBegin(), hfr_position.constEnd());
        const double maxPosition = hfr_position.empty() ?
                                   1e6 : *std::max_element(hfr_position.constBegin(), hfr_position.constEnd());
        HFRPlot->xAxis->setRange(minPosition - pulseDuration, maxPosition + pulseDuration);
        HFRPlot->yAxis->setRange(minHFRVal, maxHFR);
    }
    else
    {
        //HFRPlot->xAxis->setLabel(i18n("Iteration"));
        HFRPlot->xAxis->setRange(1, hfr_value.count() + 1);
        HFRPlot->yAxis->setRange(currentHFR / 2.5, maxHFR * 1.25);
    }

    HFRPlot->replot();
}

void Focus::drawProfilePlot()
{
    QVector<double> currentIndexes;
    QVector<double> currentFrequencies;

    // HFR = 50% * 1.36 = 68% aka one standard deviation
    double stdDev = currentHFR * 1.36;
    float start   = -stdDev * 4;
    float end     = stdDev * 4;
    float step    = stdDev * 4 / 20.0;
    for (double x = start; x < end; x += step)
    {
        currentIndexes.append(x);
        currentFrequencies.append((1 / (stdDev * sqrt(2 * M_PI))) * exp(-1 * (x * x) / (2 * (stdDev * stdDev))));
    }

    currentGaus->setData(currentIndexes, currentFrequencies);

    if (lastGausIndexes.count() > 0)
        lastGaus->setData(lastGausIndexes, lastGausFrequencies);

    if (focusType == FOCUS_AUTO && firstGaus == nullptr)
    {
        firstGaus = profilePlot->addGraph();
        QPen pen;
        pen.setStyle(Qt::DashDotLine);
        pen.setWidth(2);
        pen.setColor(Qt::darkMagenta);
        firstGaus->setPen(pen);

        firstGaus->setData(currentIndexes, currentFrequencies);
    }
    else if (firstGaus)
    {
        profilePlot->removeGraph(firstGaus);
        firstGaus = nullptr;
    }

    profilePlot->rescaleAxes();
    profilePlot->replot();

    lastGausIndexes     = currentIndexes;
    lastGausFrequencies = currentFrequencies;

    profilePixmap = profilePlot->grab(); //.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    emit newProfilePixmap(profilePixmap);
}

bool Focus::autoFocusChecks()
{
    if (++absIterations > MAXIMUM_ABS_ITERATIONS)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try increasing tolerance value."));
        abort();
        setAutoFocusResult(false);
        return false;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        if (noStarCount < MAX_RECAPTURE_RETRIES)
        {
            appendLogText(i18n("No stars detected, capturing again..."));
            capture();
            noStarCount++;
            return false;
        }
        else if (noStarCount == MAX_RECAPTURE_RETRIES)
        {
            currentHFR = 20;
            noStarCount++;
        }
        else
        {
            appendLogText(i18n("Failed to detect any stars. Reset frame and try again."));
            abort();
            setAutoFocusResult(false);
            return false;
        }
    }
    else
        noStarCount = 0;
    return true;
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

    hfr_position.append(currentPosition);
    hfr_value.append(currentHFR);

    drawHFRPlot();

    if (hfr_position.size() > 3)
    {
        polynomialFit.reset(new PolynomialFit(2, hfr_position, hfr_value));
        double min_position, min_value;
        const FocusAlgorithmInterface::FocusParams &params = linearFocuser->getParams();
        double searchMin = std::max(params.minPositionAllowed, params.startPosition - params.maxTravel);
        double searchMax = std::min(params.maxPositionAllowed, params.startPosition + params.maxTravel);
        if (polynomialFit->findMinimum(linearFocuser->getParams().startPosition,
                                       searchMin, searchMax, &min_position, &min_value))
        {
            QPen pen;
            pen.setWidth(1);
            pen.setColor(QColor(180, 180, 180));
            polynomialGraph->setPen(pen);

            polynomialFit->drawPolynomial(HFRPlot, polynomialGraph);
            polynomialFit->drawMinimum(HFRPlot, focusPoint, min_position, min_value, font());
        }
        else
        {
            // During development of this algorithm, we show the polynomial graph in red if
            // no minimum was found. That happens when the order-2 polynomial is an inverted U
            // instead of a U shape (i.e. it has a maximum, but no minimum).
            QPen pen;
            pen.setWidth(1);
            pen.setColor(QColor(254, 0, 0));
            polynomialGraph->setPen(pen);
            polynomialFit->drawPolynomial(HFRPlot, polynomialGraph);

            polynomialGraph->data()->clear();
            focusPoint->data()->clear();
        }
    }

    linearRequestedPosition = linearFocuser->newMeasurement(currentPosition, currentHFR);
    const int nextPosition = adjustLinearPosition(currentPosition, linearRequestedPosition);
    if (linearRequestedPosition == -1)
    {
        if (linearFocuser->isDone() && linearFocuser->solution() != -1)
        {
            emitComplete();
            appendLogText(i18np("Autofocus complete after %1 iteration.",
                                "Autofocus complete after %1 iterations.", hfr_position.count()));
            stop();
            setAutoFocusResult(true);
        }
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << linearFocuser->doneReason();
            appendLogText("Linear autofocus algorithm aborted.");
            abort();
            setAutoFocusResult(false);
        }
        return;
    }
    else
    {
        const int delta = nextPosition - currentPosition;
        if (!changeFocus(delta))
        {
            abort();
            setAutoFocusResult(false);
        }
        return;
    }
}

void Focus::autoFocusAbs()
{
    static int minHFRPos = 0, focusOutLimit = 0, focusInLimit = 0;
    static double minHFR = 0;
    double targetPosition = 0, delta = 0;

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

    hfr_position.append(currentPosition);
    hfr_value.append(currentHFR);

    drawHFRPlot();

    switch (lastFocusDirection)
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
            if (!changeFocus(pulseDuration))
            {
                abort();
                setAutoFocusResult(false);
            }
            break;

        case FOCUS_IN:
        case FOCUS_OUT:
            static int lastHFRPos = 0, initSlopePos = 0;
            static double initSlopeHFR = 0;

            if (reverseDir && focusInLimit && focusOutLimit &&
                    fabs(currentHFR - minHFR) < (toleranceIN->value() / 100.0) && HFRInc == 0)
            {
                if (absIterations <= 2)
                {
                    appendLogText(
                        i18n("Change in HFR is too small. Try increasing the step size or decreasing the tolerance."));
                    abort();
                    setAutoFocusResult(false);
                }
                else if (noStarCount > 0)
                {
                    appendLogText(i18n("Failed to detect focus star in frame. Capture and select a focus star."));
                    abort();
                    setAutoFocusResult(false);
                }
                else
                {
                    emitComplete();
                    appendLogText(i18n("Autofocus complete after %1 iterations.", hfr_position.count()));
                    stop();
                    setAutoFocusResult(true);

                    if (focusAlgorithm == FOCUS_POLYNOMIAL)
                        graphPolynomialFunction();
                }
                break;
            }
            else if (currentHFR < lastHFR)
            {
                double slope = 0;

                // Let's try to calculate slope of the V curve.
                if (initSlopeHFR == 0 && HFRInc == 0 && HFRDec >= 1)
                {
                    initSlopeHFR = lastHFR;
                    initSlopePos = lastHFRPos;

                    qCDebug(KSTARS_EKOS_FOCUS) << "Setting initial slop to " << initSlopePos << " @ HFR " << initSlopeHFR;
                }

                // Let's now limit the travel distance of the focuser
                if (lastFocusDirection == FOCUS_OUT && lastHFRPos < focusInLimit && fabs(currentHFR - lastHFR) > 0.1)
                {
                    focusInLimit = lastHFRPos;
                    qCDebug(KSTARS_EKOS_FOCUS) << "New FocusInLimit " << focusInLimit;
                }
                else if (lastFocusDirection == FOCUS_IN && lastHFRPos > focusOutLimit &&
                         fabs(currentHFR - lastHFR) > 0.1)
                {
                    focusOutLimit = lastHFRPos;
                    qCDebug(KSTARS_EKOS_FOCUS) << "New FocusOutLimit " << focusOutLimit;
                }

                // If we have slope, get next target position
                if (initSlopeHFR && absMotionMax > 50)
                {
                    double factor = 0.5;
                    slope         = (currentHFR - initSlopeHFR) / (currentPosition - initSlopePos);
                    if (fabs(currentHFR - minHFR) * 100.0 < 0.5)
                        factor = 1 - fabs(currentHFR - minHFR) * 10;
                    targetPosition = currentPosition + (currentHFR * factor - currentHFR) / slope;
                    if (targetPosition < 0)
                    {
                        factor = 1;
                        while (targetPosition < 0 && factor > 0)
                        {
                            factor -= 0.005;
                            targetPosition = currentPosition + (currentHFR * factor - currentHFR) / slope;
                        }
                    }
                    qCDebug(KSTARS_EKOS_FOCUS) << "Using slope to calculate target pulse...";
                }
                // Otherwise proceed iteratively
                else
                {
                    if (lastFocusDirection == FOCUS_IN)
                        targetPosition = currentPosition - pulseDuration;
                    else
                        targetPosition = currentPosition + pulseDuration;

                    qCDebug(KSTARS_EKOS_FOCUS) << "Proceeding iteratively to next target pulse ...";
                }

                qCDebug(KSTARS_EKOS_FOCUS) << "V-Curve Slope " << slope << " current Position " << currentPosition
                                           << " targetPosition " << targetPosition;

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
                HFRInc = 0;
            }
            else

            {
                // HFR increased, let's deal with it.
                //HFRInc++;
                HFRDec = 0;

                // Reality Check: If it's first time, let's capture again and see if it changes.
                /*if (HFRInc <= 1 && reverseDir == false)
                {
                    capture();
                    return;
                }
                // Looks like we're going away from optimal HFR
                else
                {*/
                reverseDir   = true;
                lastHFR      = currentHFR;
                lastHFRPos   = currentPosition;
                initSlopeHFR = 0;
                HFRInc       = 0;

                qCDebug(KSTARS_EKOS_FOCUS) << "Focus is moving away from optimal HFR.";

                // Let's set new limits
                if (lastFocusDirection == FOCUS_IN)
                {
                    focusInLimit = currentPosition;
                    qCDebug(KSTARS_EKOS_FOCUS) << "Setting focus IN limit to " << focusInLimit;

                    if (hfr_position.count() > 3)
                    {
                        focusOutLimit = hfr_position[hfr_position.count() - 3];
                        qCDebug(KSTARS_EKOS_FOCUS) << "Setting focus OUT limit to " << focusOutLimit;
                    }
                }
                else
                {
                    focusOutLimit = currentPosition;
                    qCDebug(KSTARS_EKOS_FOCUS) << "Setting focus OUT limit to " << focusOutLimit;

                    if (hfr_position.count() > 3)
                    {
                        focusInLimit = hfr_position[hfr_position.count() - 3];
                        qCDebug(KSTARS_EKOS_FOCUS) << "Setting focus IN limit to " << focusInLimit;
                    }
                }

                bool polyMinimumFound = false;
                if (focusAlgorithm == FOCUS_POLYNOMIAL && hfr_position.count() > 5)
                {
                    polynomialFit.reset(new PolynomialFit(3, hfr_position, hfr_value));
                    double a = *std::min_element(hfr_position.constBegin(), hfr_position.constEnd());
                    double b = *std::max_element(hfr_position.constBegin(), hfr_position.constEnd());
                    double min_position = 0, min_hfr = 0;
                    polyMinimumFound = polynomialFit->findMinimum(minHFRPos, a, b, &min_position, &min_hfr);
                    qCDebug(KSTARS_EKOS_FOCUS) << "Found Minimum?" << (polyMinimumFound ? "Yes" : "No");
                    if (polyMinimumFound)
                    {
                        qCDebug(KSTARS_EKOS_FOCUS) << "Minimum Solution:" << min_hfr << "@" << min_position;
                        polySolutionFound++;
                        targetPosition = floor(min_position);
                        appendLogText(i18n("Found polynomial solution @ %1", QString::number(min_position, 'f', 0)));

                        polynomialFit->drawPolynomial(HFRPlot, polynomialGraph);
                        polynomialFit->drawMinimum(HFRPlot, focusPoint, min_position, min_hfr, font());
                    }
                }

                if (polyMinimumFound == false)
                {
                    // Decrease pulse
                    pulseDuration = pulseDuration * 0.75;

                    // Let's get close to the minimum HFR position so far detected
                    if (lastFocusDirection == FOCUS_OUT)
                        targetPosition = minHFRPos - pulseDuration / 2;
                    else
                        targetPosition = minHFRPos + pulseDuration / 2;
                }

                qCDebug(KSTARS_EKOS_FOCUS) << "new targetPosition " << targetPosition;
            }

            // Limit target Pulse to algorithm limits
            if (focusInLimit != 0 && lastFocusDirection == FOCUS_IN && targetPosition < focusInLimit)
            {
                targetPosition = focusInLimit;
                qCDebug(KSTARS_EKOS_FOCUS) << "Limiting target pulse to focus in limit " << targetPosition;
            }
            else if (focusOutLimit != 0 && lastFocusDirection == FOCUS_OUT && targetPosition > focusOutLimit)
            {
                targetPosition = focusOutLimit;
                qCDebug(KSTARS_EKOS_FOCUS) << "Limiting target pulse to focus out limit " << targetPosition;
            }

            // Limit target pulse to focuser limits
            if (targetPosition < absMotionMin)
                targetPosition = absMotionMin;
            else if (targetPosition > absMotionMax)
                targetPosition = absMotionMax;

            // Ops, we can't go any further, we're done.
            if (targetPosition == currentPosition)
            {
                emitComplete();
                appendLogText(i18n("Autofocus complete after %1 iterations.", hfr_position.count()));
                stop();
                setAutoFocusResult(true);
                if (focusAlgorithm == FOCUS_POLYNOMIAL)
                    graphPolynomialFunction();
                return;
            }

            // Ops, deadlock
            if (focusOutLimit && focusOutLimit == focusInLimit)
            {
                appendLogText(i18n("Deadlock reached. Please try again with different settings."));
                abort();
                setAutoFocusResult(false);
                return;
            }

            if (fabs(targetPosition - initialFocuserAbsPosition) > maxTravelIN->value())
            {
                int minTravelLimit = qMax(0.0, initialFocuserAbsPosition - maxTravelIN->value());
                int maxTravelLimit = qMin(absMotionMax, initialFocuserAbsPosition + maxTravelIN->value());

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
                                               << initialFocuserAbsPosition << ") exceeds maxTravel distance of " << maxTravelIN->value();

                    appendLogText("Maximum travel limit reached. Autofocus aborted.");
                    abort();
                    setAutoFocusResult(false);
                    break;
                }
            }

            // Get delta for next move
            delta = (targetPosition - currentPosition);

            qCDebug(KSTARS_EKOS_FOCUS) << "delta (targetPosition - currentPosition) " << delta;

            // Limit to Maximum permitted delta (Max Single Step Size)
            double limitedDelta = qMax(-1.0 * maxSingleStepIN->value(), qMin(1.0 * maxSingleStepIN->value(), delta));
            if (std::fabs(limitedDelta - delta) > 0)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << "Limited delta to maximum permitted single step " << maxSingleStepIN->value();
                delta = limitedDelta;
            }

            // Now cross your fingers and wait
            if (!changeFocus(delta))
            {
                abort();
                setAutoFocusResult(false);
            }
            break;
    }
}

void Focus::graphPolynomialFunction()
{
    if (polynomialGraph && polynomialFit)
    {
        polynomialGraphIsShown = true;
        polynomialFit->drawPolynomial(HFRPlot, polynomialGraph);
    }
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
        abort();
        setAutoFocusResult(false);
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        if (noStarCount++ < MAX_RECAPTURE_RETRIES)
        {
            appendLogText(i18n("No stars detected, capturing again..."));
            capture();
            return;
        }
        else
            currentHFR = 20;
    }
    else
        noStarCount = 0;

    switch (lastFocusDirection)
    {
        case FOCUS_NONE:
            lastHFR = currentHFR;
            minHFR  = 1e6;
            changeFocus(-pulseDuration);
            break;

        case FOCUS_IN:
        case FOCUS_OUT:
            if (fabs(currentHFR - minHFR) < (toleranceIN->value() / 100.0) && HFRInc == 0)
            {
                emitComplete();
                appendLogText(i18n("Autofocus complete after %1 iterations.", hfr_position.count()));
                stop();
                setAutoFocusResult(true);
                if (focusAlgorithm == FOCUS_POLYNOMIAL)
                    graphPolynomialFunction();
                break;
            }
            else if (currentHFR < lastHFR)
            {
                if (currentHFR < minHFR)
                    minHFR = currentHFR;

                lastHFR = currentHFR;
                changeFocus(lastFocusDirection == FOCUS_IN ? -pulseDuration : pulseDuration);
                HFRInc = 0;
            }
            else
            {
                //HFRInc++;

                lastHFR = currentHFR;

                HFRInc = 0;

                pulseDuration *= 0.75;

                if (!changeFocus(lastFocusDirection == FOCUS_IN ? pulseDuration : -pulseDuration))
                {
                    abort();
                    setAutoFocusResult(false);
                }
            }
            break;
    }
}

/*void Focus::registerFocusProperty(INDI::Property *prop)
{
    // Return if it is not our current focuser
    if (strcmp(prop->getDeviceName(), currentFocuser->getDeviceName()))
        return;

    // Do not make unnecessary function call
    // Check if current focuser supports absolute mode
    if (canAbsMove == false && currentFocuser->canAbsMove())
    {
        canAbsMove = true;
        getAbsFocusPosition();

        absTicksSpin->setEnabled(true);
        absTicksLabel->setEnabled(true);
        startGotoB->setEnabled(true);
    }

    // Do not make unnecessary function call
    // Check if current focuser supports relative mode
    if (canRelMove == false && currentFocuser->canRelMove())
        canRelMove = true;

    if (canTimerMove == false && currentFocuser->canTimerMove())
    {
        canTimerMove = true;
        resetButtons();
    }
}*/

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
            qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: un-doing extension. Moving back in by %1").arg(temp);

            if (!focusIn(temp))
            {
                appendLogText(i18n("Focuser error, check INDI panel."));
                abort();
                setAutoFocusResult(false);
            }
        }
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Focus position reached at %1, starting capture in %2 seconds.").arg(
                                           currentPosition).arg(FocusSettleTime->value());
            capture(FocusSettleTime->value());
        }
    }
    else if (state == IPS_ALERT)
    {
        appendLogText(i18n("Focuser error, check INDI panel."));
        abort();
        setAutoFocusResult(false);
    }
}

void Focus::processFocusNumber(INumberVectorProperty *nvp)
{
    // Return if it is not our current focuser
    if (nvp->device != currentFocuser->getDeviceName())
        return;

    // Only process focus properties
    if (QString(nvp->name).contains("focus", Qt::CaseInsensitive) == false)
        return;

    //    qCDebug(KSTARS_EKOS_FOCUS) << QString("processFocusNumber %1 state: %2")
    //                               .arg(nvp->name).arg(nvp->s);

    if (!strcmp(nvp->name, "FOCUS_BACKLASH_STEPS"))
    {
        focusBacklashSpin->setValue(nvp->np[0].value);
        return;
    }

    if (!strcmp(nvp->name, "FOCUS_TEMPERATURE"))
    {
        updateTemperature(FOCUSER_TEMPERATURE, nvp->np[0].value);
        return;
    }

    if (!strcmp(nvp->name, "ABS_FOCUS_POSITION"))
    {
        INumber *pos = IUFindNumber(nvp, "FOCUS_ABSOLUTE_POSITION");
        if (pos)
        {
            int newPosition = static_cast<int>(pos->value);
            // Some absolute focuser constantly report the position without a state change.
            // Therefore we ignore it if both value and state are the same as last time.
            if (currentPosition == newPosition && currentPositionState == nvp->s)
                return;

            currentPositionState = nvp->s;

            if (currentPosition != newPosition)
            {
                currentPosition = newPosition;
                qCDebug(KSTARS_EKOS_FOCUS) << "Abs Focuser position changed to " << currentPosition << " (state = " << currentPositionState
                                           << ")";
                absTicksLabel->setText(QString::number(currentPosition));
                emit absolutePositionChanged(currentPosition);
            }
        }

        if (adjustFocus && nvp->s == IPS_OK)
        {
            adjustFocus = false;
            lastFocusDirection = FOCUS_NONE;
            emit focusPositionAdjusted();
            return;
        }

        if (resetFocus && nvp->s == IPS_OK)
        {
            resetFocus = false;
            appendLogText(i18n("Restarting autofocus process..."));
            start();
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

    if (!strcmp(nvp->name, "manualfocusdrive"))
    {
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
            lastFocusDirection = FOCUS_NONE;
            emit focusPositionAdjusted();
            return;
        }

        if (resetFocus && nvp->s == IPS_OK)
        {
            resetFocus = false;
            appendLogText(i18n("Restarting autofocus process..."));
            start();
        }

        if (canRelMove && inAutoFocus)
        {
            autoFocusProcessPositionChange(nvp->s);
        }
        else if (nvp->s == IPS_ALERT)
            appendLogText(i18n("Focuser error, check INDI panel."));

        return;
    }

    if (!strcmp(nvp->name, "REL_FOCUS_POSITION"))
    {
        INumber *pos = IUFindNumber(nvp, "FOCUS_RELATIVE_POSITION");
        if (pos && nvp->s == IPS_OK)
        {
            currentPosition += pos->value * (lastFocusDirection == FOCUS_IN ? -1 : 1);
            qCDebug(KSTARS_EKOS_FOCUS)
                    << QString("Rel Focuser position changed by %1 to %2")
                    .arg(pos->value).arg(currentPosition);
            absTicksLabel->setText(QString::number(static_cast<int>(currentPosition)));
            emit absolutePositionChanged(currentPosition);
        }

        if (adjustFocus && nvp->s == IPS_OK)
        {
            adjustFocus = false;
            lastFocusDirection = FOCUS_NONE;
            emit focusPositionAdjusted();
            return;
        }

        if (resetFocus && nvp->s == IPS_OK)
        {
            resetFocus = false;
            appendLogText(i18n("Restarting autofocus process..."));
            start();
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

    if (!strcmp(nvp->name, "FOCUS_TIMER"))
    {
        if (resetFocus && nvp->s == IPS_OK)
        {
            resetFocus = false;
            appendLogText(i18n("Restarting autofocus process..."));
            start();
        }

        if (canAbsMove == false && canRelMove == false && inAutoFocus)
        {
            // Used by the linear focus algorithm. Ignored if that's not in use for the timer-focuser.
            INumber *pos = IUFindNumber(nvp, "FOCUS_TIMER_VALUE");
            if (pos)
            {
                currentPosition += pos->value * (lastFocusDirection == FOCUS_IN ? -1 : 1);
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
            QString  dir = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "focuslogs/";
            if (QDir(dir).exists() == false)
                QDir().mkpath(dir);
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
    if (currentCCD == nullptr)
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

        return;
    }

    if (currentFocuser)
    {
        focusOutB->setEnabled(true);
        focusInB->setEnabled(true);

        startFocusB->setEnabled(focusType == FOCUS_AUTO);
        startGotoB->setEnabled(canAbsMove);
        stopGotoB->setEnabled(true);
    }
    else
    {
        focusOutB->setEnabled(false);
        focusInB->setEnabled(false);

        startFocusB->setEnabled(false);
        startGotoB->setEnabled(false);
        stopGotoB->setEnabled(false);
    }

    stopFocusB->setEnabled(false);
    startLoopB->setEnabled(true);

    if (captureInProgress == false)
    {
        captureB->setEnabled(true);
        resetFrameB->setEnabled(true);
    }
}

void Focus::updateBoxSize(int value)
{
    if (currentCCD == nullptr)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    if (targetChip == nullptr)
        return;

    int subBinX, subBinY;
    targetChip->getBinning(&subBinX, &subBinY);

    QRect trackBox = focusView->getTrackingBox();
    QPoint center(trackBox.x() + (trackBox.width() / 2), trackBox.y() + (trackBox.height() / 2));

    trackBox =
        QRect(center.x() - value / (2 * subBinX), center.y() - value / (2 * subBinY), value / subBinX, value / subBinY);

    focusView->setTrackingBox(trackBox);
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

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    int subBinX, subBinY;
    targetChip->getBinning(&subBinX, &subBinY);

    // If binning was changed outside of the focus module, recapture
    if (subBinX != activeBin)
    {
        capture();
        return;
    }

    int offset = (static_cast<double>(focusBoxSize->value()) / subBinX) * 1.5;

    QRect starRect;

    bool squareMovedOutside = false;

    if (subFramed == false && useSubFrame->isChecked() && targetChip->canSubframe())
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

        focusView->setFirstLoad(true);

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
        focusView->setTrackingBox(starRect);
    }

    starsHFR.clear();

    starCenter.setZ(subBinX);

    //starSelected=true;

    defaultScale = static_cast<FITSScale>(filterCombo->currentIndex());

    if (squareMovedOutside && inAutoFocus == false && useAutoStar->isChecked())
    {
        useAutoStar->blockSignals(true);
        useAutoStar->setChecked(false);
        useAutoStar->blockSignals(false);
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
    qCDebug(KSTARS_EKOS_FOCUS) << "Check Focus requested with minimum required HFR" << requiredHFR;
    minimumRequiredHFR = requiredHFR;

    capture();
}

void Focus::toggleSubframe(bool enable)
{
    if (enable == false)
        resetFrame();

    starSelected = false;
    starCenter   = QVector3D();

    if (useFullField->isChecked())
        useFullField->setChecked(!enable);
}

void Focus::filterChangeWarning(int index)
{
    // index = 4 is MEDIAN filter which helps reduce noise
    if (index != 0 && index != FITS_MEDIAN)
        appendLogText(i18n("Warning: Only use filters for preview as they may interface with autofocus operation."));

    Options::setFocusEffect(index);

    defaultScale = static_cast<FITSScale>(index);
}

void Focus::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Focus::setBinning(int subBinX, int subBinY)
{
    INDI_UNUSED(subBinY);
    binningCombo->setCurrentIndex(subBinX - 1);
}

void Focus::setImageFilter(const QString &value)
{
    for (int i = 0; i < filterCombo->count(); i++)
        if (filterCombo->itemText(i) == value)
        {
            filterCombo->setCurrentIndex(i);
            break;
        }
}

void Focus::setAutoStarEnabled(bool enable)
{
    useAutoStar->setChecked(enable);
    Options::setFocusAutoStarEnabled(enable);
}

void Focus::setAutoSubFrameEnabled(bool enable)
{
    useSubFrame->setChecked(enable);
    Options::setFocusSubFrame(enable);
}

void Focus::setAutoFocusParameters(int boxSize, int stepSize, int maxTravel, double tolerance)
{
    focusBoxSize->setValue(boxSize);
    stepIN->setValue(stepSize);
    maxTravelIN->setValue(maxTravel);
    toleranceIN->setValue(tolerance);
}

void Focus::setAutoFocusResult(bool status)
{
    qCDebug(KSTARS_EKOS_FOCUS) << "AutoFocus result:" << status;

    if (status)
    {
        setLastFocusTemperature();

        // CR add auto focus position, temperature and filter to log in CSV format
        // this will help with setting up focus offsets and temperature compensation
        qCInfo(KSTARS_EKOS_FOCUS) << "Autofocus values: position, " << currentPosition << ", temperature, "
                                  << lastFocusTemperature << ", filter, " << filter()
                                  << ", HFR, " << currentHFR << ", altitude, " << mountAlt;


        appendFocusLogText(QString("%1, %2, %3, %4, %5\n")
                           .arg(QString::number(currentPosition))
                           .arg(QString::number(lastFocusTemperature, 'f', 1))
                           .arg(filter())
                           .arg(QString::number(currentHFR, 'f', 3))
                           .arg(QString::number(mountAlt, 'f', 1)));
    }

    // In case of failure, go back to last position if the focuser is absolute
    if (status == false && canAbsMove && currentFocuser && currentFocuser->isConnected() &&
            initialFocuserAbsPosition >= 0)
    {
        currentFocuser->moveAbs(initialFocuserAbsPosition);
        appendLogText(i18n("Autofocus failed, moving back to initial focus position %1.", initialFocuserAbsPosition));

        // If we're doing in sequence focusing using an absolute focuser, let's retry focusing starting from last known good position before we give up
        if (inSequenceFocus && resetFocusIteration++ < MAXIMUM_RESET_ITERATIONS && resetFocus == false)
        {
            resetFocus = true;
            // Reset focus frame in case the star in subframe was lost
            resetFrame();
            return;
        }
    }

    int settleTime = m_GuidingSuspended ? GuideSettleTime->value() : 0;

    // Always resume guiding if we suspended it before
    if (m_GuidingSuspended)
    {
        emit resumeGuiding();
        m_GuidingSuspended = false;
    }

    resetFocusIteration = 0;

    if (settleTime > 0)
        appendLogText(i18n("Settling..."));

    QTimer::singleShot(settleTime * 1000, this, [ &, status, settleTime]()
    {
        if (settleTime > 0)
            appendLogText(i18n("Settling complete."));

        if (status)
        {
            KSNotification::event(QLatin1String("FocusSuccessful"), i18n("Autofocus operation completed successfully"));
            state = Ekos::FOCUS_COMPLETE;
        }
        else
        {
            KSNotification::event(QLatin1String("FocusFailed"), i18n("Autofocus operation failed with errors"),
                                  KSNotification::EVENT_ALERT);
            state = Ekos::FOCUS_FAILED;
        }

        qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);

        // Do not emit result back yet if we have a locked filter pending return to original filter
        if (fallbackFilterPending)
        {
            filterManager->setFilterPosition(fallbackFilterPosition,
                                             static_cast<FilterManager::FilterPolicy>(FilterManager::CHANGE_POLICY | FilterManager::OFFSET_POLICY));
            return;
        }
        emit newStatus(state);

    });
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

        appendLogText(i18n("No star was selected. Aborting..."));
        initialFocuserAbsPosition = -1;
        abort();
        setAutoFocusResult(false);
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
    if (currentFocuser == nullptr)
        return;

    if (currentFocuser->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to Focuser."));
        return;
    }

    qCDebug(KSTARS_EKOS_FOCUS) << "Setting focus ticks to " << absTicksSpin->value();

    currentFocuser->moveAbs(absTicksSpin->value());
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
    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
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
        focusView->setTrackingBoxEnabled(true);
        focusView->setTrackingBox(starRect);
    }
}

void Focus::showFITSViewer()
{
    FITSData *data = focusView->getImageData();
    if (data)
    {
        QUrl url = QUrl::fromLocalFile(data->filename());

        if (fv.isNull())
        {
            if (Options::singleWindowCapturedFITS())
                fv = KStars::Instance()->genericFITSViewer();
            else
            {
                fv = new FITSViewer(Options::independentWindowFITS() ? nullptr : KStars::Instance());
                KStars::Instance()->addFITSViewer(fv);
            }

            fv->addFITS(url);
            FITSView *currentView = fv->getCurrentView();
            if (currentView)
                currentView->getImageData()->setAutoRemoveTemporaryFITS(false);
        }
        else
            fv->updateFITS(url, 0);

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
        focusingWidget->setWindowTitle(i18n("Focus Frame"));
        focusingWidget->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
        focusingWidget->showMaximized();
        focusingWidget->show();
    }
}

void Focus::setMountStatus(ISD::Telescope::Status newState)
{
    switch (newState)
    {
        case ISD::Telescope::MOUNT_PARKING:
        case ISD::Telescope::MOUNT_SLEWING:
        case ISD::Telescope::MOUNT_MOVING:
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

void Focus::setMountCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt,
                           int pierSide, const QString &ha)
{
    Q_UNUSED(ra);
    Q_UNUSED(dec);
    Q_UNUSED(az);
    Q_UNUSED(pierSide);
    Q_UNUSED(ha);
    mountAlt = dms(alt, true).Degrees();
}

void Focus::removeDevice(ISD::GDInterface *deviceRemoved)
{
    // Check in Focusers
    for (ISD::GDInterface *focuser : Focusers)
    {
        if (focuser->getDeviceName() == deviceRemoved->getDeviceName())
        {
            Focusers.removeAll(dynamic_cast<ISD::Focuser*>(focuser));
            focuserCombo->removeItem(focuserCombo->findText(focuser->getDeviceName()));
            QTimer::singleShot(1000, this, [this]()
            {
                checkFocuser();
                resetButtons();
            });
        }
    }

    // Check in CCDs
    for (ISD::GDInterface *ccd : CCDs)
    {
        if (ccd->getDeviceName() == deviceRemoved->getDeviceName())
        {
            CCDs.removeAll(dynamic_cast<ISD::CCD*>(ccd));
            CCDCaptureCombo->removeItem(CCDCaptureCombo->findText(ccd->getDeviceName()));
            CCDCaptureCombo->removeItem(CCDCaptureCombo->findText(ccd->getDeviceName() + QString(" Guider")));

            if (CCDs.empty())
            {
                currentCCD = nullptr;
                CCDCaptureCombo->setCurrentIndex(-1);
            }
            else
                CCDCaptureCombo->setCurrentIndex(0);

            QTimer::singleShot(1000, this, [this]()
            {
                checkCCD();
                resetButtons();
            });
        }
    }

    // Check in Filters
    for (ISD::GDInterface *filter : Filters)
    {
        if (filter->getDeviceName() == deviceRemoved->getDeviceName())
        {
            Filters.removeAll(filter);
            FilterDevicesCombo->removeItem(FilterDevicesCombo->findText(filter->getDeviceName()));
            if (Filters.empty())
            {
                currentFilter = nullptr;
                FilterDevicesCombo->setCurrentIndex(-1);
            }
            else
                FilterDevicesCombo->setCurrentIndex(0);

            QTimer::singleShot(1000, this, [this]()
            {
                checkFilter();
                resetButtons();
            });
        }
    }
}

void Focus::setFilterManager(const QSharedPointer<FilterManager> &manager)
{
    filterManager = manager;
    connect(filterManagerB, &QPushButton::clicked, [this]()
    {
        filterManager->show();
        filterManager->raise();
    });

    connect(filterManager.data(), &FilterManager::ready, [this]()
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
    }
           );

    connect(filterManager.data(), &FilterManager::failed, [this]()
    {
        appendLogText(i18n("Filter operation failed."));
        abort();
    }
           );

    connect(this, &Focus::newStatus, [this](Ekos::FocusState state)
    {
        if (FilterPosCombo->currentIndex() != -1 && canAbsMove && state == Ekos::FOCUS_COMPLETE)
        {
            filterManager->setFilterAbsoluteFocusPosition(FilterPosCombo->currentIndex(), currentPosition);
        }
    });

    connect(exposureIN, &QDoubleSpinBox::editingFinished, [this]()
    {
        if (currentFilter)
            filterManager->setFilterExposure(FilterPosCombo->currentIndex(), exposureIN->value());
        else
            Options::setFocusExposure(exposureIN->value());
    });

    connect(filterManager.data(), &FilterManager::labelsChanged, this, [this]()
    {
        FilterPosCombo->clear();
        FilterPosCombo->addItems(filterManager->getFilterLabels());
        currentFilterPosition = filterManager->getFilterPosition();
        FilterPosCombo->setCurrentIndex(currentFilterPosition - 1);
        //Options::setDefaultFocusFilterWheelFilter(FilterPosCombo->currentText());
    });
    connect(filterManager.data(), &FilterManager::positionChanged, this, [this]()
    {
        currentFilterPosition = filterManager->getFilterPosition();
        FilterPosCombo->setCurrentIndex(currentFilterPosition - 1);
        //Options::setDefaultFocusFilterWheelFilter(FilterPosCombo->currentText());
    });
    connect(filterManager.data(), &FilterManager::exposureChanged, this, [this]()
    {
        exposureIN->setValue(filterManager->getFilterExposure());
    });

    connect(FilterPosCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
            [ = ](const QString & text)
    {
        exposureIN->setValue(filterManager->getFilterExposure(text));
        //Options::setDefaultFocusFilterWheelFilter(text);
    });
}

void Focus::toggleVideo(bool enabled)
{
    if (currentCCD == nullptr)
        return;

    if (currentCCD->isBLOBEnabled() == false)
    {

        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL)
            currentCCD->setBLOBEnabled(true);
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, enabled]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                currentCCD->setVideoStreamEnabled(enabled);
            });
            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"));
        }
    }
    else
        currentCCD->setVideoStreamEnabled(enabled);
}

void Focus::setWeatherData(const std::vector<ISD::Weather::WeatherData> &data)
{
    auto pos = std::find_if(data.begin(), data.end(), [](ISD::Weather::WeatherData oneEntry)
    {
        return (oneEntry.name == "WEATHER_TEMPERATURE");
    });

    if (pos != data.end())
    {
        updateTemperature(OBSERVATORY_TEMPERATURE, pos->value);
    }
}

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
        appendLogText(i18n("Exposure timeout. Aborting..."));
        abort();
        if (inAutoFocus)
            setAutoFocusResult(false);
        else if (m_GuidingSuspended)
        {
            emit resumeGuiding();
            m_GuidingSuspended = false;
        }
        return;
    }

    appendLogText(i18n("Exposure timeout. Restarting exposure..."));
    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    targetChip->abortExposure();
    targetChip->capture(exposureIN->value());
    captureTimeout.start(exposureIN->value() * 1000 + FOCUS_TIMEOUT_THRESHOLD);
}

void Focus::processCaptureFailure()
{
    captureFailureCounter++;

    if (captureFailureCounter >= 3)
    {
        captureFailureCounter = 0;
        appendLogText(i18n("Exposure failure. Aborting..."));
        abort();
        if (inAutoFocus)
            setAutoFocusResult(false);
        else if (m_GuidingSuspended)
        {
            emit resumeGuiding();
            m_GuidingSuspended = false;
        }
        return;
    }

    appendLogText(i18n("Exposure failure. Restarting exposure..."));
    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    targetChip->abortExposure();
    targetChip->capture(exposureIN->value());
}

void Focus::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QComboBox *cbox = nullptr;

    if ( (dsb = qobject_cast<QDoubleSpinBox*>(sender())))
    {
        ///////////////////////////////////////////////////////////////////////////
        /// Focuser Group
        ///////////////////////////////////////////////////////////////////////////
        if (dsb == FocusSettleTime)
            Options::setFocusSettleTime(dsb->value());

        ///////////////////////////////////////////////////////////////////////////
        /// CCD & Filter Wheel Group
        ///////////////////////////////////////////////////////////////////////////
        else if (dsb == gainIN)
            Options::setFocusGain(dsb->value());

        ///////////////////////////////////////////////////////////////////////////
        /// Settings Group
        ///////////////////////////////////////////////////////////////////////////
        else if (dsb == fullFieldInnerRing)
            Options::setFocusFullFieldInnerRadius(dsb->value());
        else if (dsb == fullFieldOuterRing)
            Options::setFocusFullFieldOuterRadius(dsb->value());
        else if (dsb == GuideSettleTime)
            Options::setGuideSettleTime(dsb->value());
        else if (dsb == maxTravelIN)
            Options::setFocusMaxTravel(dsb->value());
        else if (dsb == toleranceIN)
            Options::setFocusTolerance(dsb->value());
        else if (dsb == thresholdSpin)
            Options::setFocusThreshold(dsb->value());
        else if (dsb == gaussianSigmaSpin)
            Options::setFocusGaussianSigma(dsb->value());
        else if (dsb == initialFocusOutStepsIN)
            Options::setInitialFocusOutSteps(dsb->value());
    }
    else if ( (sb = qobject_cast<QSpinBox*>(sender())))
    {
        ///////////////////////////////////////////////////////////////////////////
        /// Settings Group
        ///////////////////////////////////////////////////////////////////////////
        if (sb == focusBoxSize)
            Options::setFocusBoxSize(sb->value());
        else if (sb == stepIN)
            Options::setFocusTicks(sb->value());
        else if (sb == maxSingleStepIN)
            Options::setFocusMaxSingleStep(sb->value());
        else if (sb == focusFramesSpin)
            Options::setFocusFramesCount(sb->value());
        else if (sb == gaussianKernelSizeSpin)
            Options::setFocusGaussianKernelSize(sb->value());
        else if (sb == multiRowAverageSpin)
            Options::setFocusMultiRowAverage(sb->value());
    }
    else if ( (cb = qobject_cast<QCheckBox*>(sender())))
    {
        ///////////////////////////////////////////////////////////////////////////
        /// Settings Group
        ///////////////////////////////////////////////////////////////////////////
        if (cb == useAutoStar)
            Options::setFocusAutoStarEnabled(cb->isChecked());
        else if (cb == useSubFrame)
            Options::setFocusSubFrame(cb->isChecked());
        else if (cb == darkFrameCheck)
            Options::setUseFocusDarkFrame(cb->isChecked());
        else if (cb == useFullField)
            Options::setFocusUseFullField(cb->isChecked());
        else if (cb == suspendGuideCheck)
            Options::setSuspendGuiding(cb->isChecked());
    }
    else if ( (cbox = qobject_cast<QComboBox*>(sender())))
    {
        ///////////////////////////////////////////////////////////////////////////
        /// CCD & Filter Wheel Group
        ///////////////////////////////////////////////////////////////////////////
        if (cbox == focuserCombo)
            Options::setDefaultFocusFocuser(cbox->currentText());
        else if (cbox == CCDCaptureCombo)
            Options::setDefaultFocusCCD(cbox->currentText());
        else if (cbox == binningCombo)
        {
            activeBin = cbox->currentIndex() + 1;
            Options::setFocusXBin(activeBin);
        }
        else if (cbox == FilterDevicesCombo)
            Options::setDefaultFocusFilterWheel(cbox->currentText());
        // Filter Effects already taken care of in filterChangeWarning

        ///////////////////////////////////////////////////////////////////////////
        /// Settings Group
        ///////////////////////////////////////////////////////////////////////////
        else if (cbox == focusAlgorithmCombo)
            Options::setFocusAlgorithm(cbox->currentIndex());
        else if (cbox == focusDetectionCombo)
            Options::setFocusDetection(cbox->currentIndex());
    }
}

void Focus::loadSettings()
{
    ///////////////////////////////////////////////////////////////////////////
    /// Focuser Group
    ///////////////////////////////////////////////////////////////////////////
    // Focus settle time
    FocusSettleTime->setValue(Options::focusSettleTime());

    ///////////////////////////////////////////////////////////////////////////
    /// CCD & Filter Wheel Group
    ///////////////////////////////////////////////////////////////////////////
    // Default Exposure
    exposureIN->setValue(Options::focusExposure());
    // Binning
    activeBin = Options::focusXBin();
    binningCombo->setCurrentIndex(activeBin - 1);
    // Gain
    gainIN->setValue(Options::focusGain());

    ///////////////////////////////////////////////////////////////////////////
    /// Settings Group
    ///////////////////////////////////////////////////////////////////////////
    // Subframe?
    useSubFrame->setChecked(Options::focusSubFrame());
    // Dark frame?
    darkFrameCheck->setChecked(Options::useFocusDarkFrame());
    // Use full field?
    useFullField->setChecked(Options::focusUseFullField());
    // full field inner ring
    fullFieldInnerRing->setValue(Options::focusFullFieldInnerRadius());
    // full field outer ring
    fullFieldOuterRing->setValue(Options::focusFullFieldOuterRadius());
    // Suspend guiding?
    suspendGuideCheck->setChecked(Options::suspendGuiding());
    // Guide Setting time
    GuideSettleTime->setValue(Options::guideSettleTime());

    // Box Size
    focusBoxSize->setValue(Options::focusBoxSize());
    // Max Travel
    if (Options::focusMaxTravel() > maxTravelIN->maximum())
        maxTravelIN->setMaximum(Options::focusMaxTravel());
    maxTravelIN->setValue(Options::focusMaxTravel());
    // Step
    stepIN->setValue(Options::focusTicks());
    // Single Max Step
    maxSingleStepIN->setValue(Options::focusMaxSingleStep());
    // LinearFocus initial outward steps
    initialFocusOutStepsIN->setValue(Options::initialFocusOutSteps());
    // Tolerance
    toleranceIN->setValue(Options::focusTolerance());
    // Threshold spin
    thresholdSpin->setValue(Options::focusThreshold());
    // Focus Algorithm
    setFocusAlgorithm(static_cast<FocusAlgorithm>(Options::focusAlgorithm()));
    // This must go below the above line (which sets focusAlgorithm from options).
    focusAlgorithmCombo->setCurrentIndex(focusAlgorithm);
    // Frames Count
    focusFramesSpin->setValue(Options::focusFramesCount());
    // Focus Detection
    focusDetection = static_cast<StarAlgorithm>(Options::focusDetection());
    thresholdSpin->setEnabled(focusDetection == ALGORITHM_THRESHOLD);
    focusDetectionCombo->setCurrentIndex(focusDetection);
    // Gaussian blur
    gaussianSigmaSpin->setValue(Options::focusGaussianSigma());
    gaussianKernelSizeSpin->setValue(Options::focusGaussianKernelSize());
    // Hough algorithm multi row average
    multiRowAverageSpin->setValue(Options::focusMultiRowAverage());
    multiRowAverageSpin->setEnabled(focusDetection == ALGORITHM_BAHTINOV);

    // Increase focus box size in case of Bahtinov mask focus
    // Disable auto star in case of Bahtinov mask focus
    if (focusDetection == ALGORITHM_BAHTINOV)
    {
        Options::setFocusAutoStarEnabled(false);
        focusBoxSize->setMaximum(512);
    }
    else
    {
        // When not using Bathinov mask, limit box size to 256 and make sure value stays within range.
        if (Options::focusBoxSize() > 256)
        {
            Options::setFocusBoxSize(32);
        }
        focusBoxSize->setMaximum(256);
    }
    // Box Size
    focusBoxSize->setValue(Options::focusBoxSize());
    // Auto Star?
    useAutoStar->setChecked(Options::focusAutoStarEnabled());
    useAutoStar->setEnabled(focusDetection != ALGORITHM_BAHTINOV);
}

void Focus::initSettingsConnections()
{
    ///////////////////////////////////////////////////////////////////////////
    /// Focuser Group
    ///////////////////////////////////////////////////////////////////////////
    connect(focuserCombo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Focus::syncSettings);
    connect(FocusSettleTime, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);

    ///////////////////////////////////////////////////////////////////////////
    /// CCD & Filter Wheel Group
    ///////////////////////////////////////////////////////////////////////////
    connect(CCDCaptureCombo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Focus::syncSettings);
    connect(binningCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Focus::syncSettings);
    connect(gainIN, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(FilterDevicesCombo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Focus::syncSettings);
    connect(FilterPosCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Focus::syncSettings);

    ///////////////////////////////////////////////////////////////////////////
    /// Settings Group
    ///////////////////////////////////////////////////////////////////////////
    connect(useAutoStar, &QCheckBox::toggled, this, &Ekos::Focus::syncSettings);
    connect(useSubFrame, &QCheckBox::toggled, this, &Ekos::Focus::syncSettings);
    connect(darkFrameCheck, &QCheckBox::toggled, this, &Ekos::Focus::syncSettings);
    connect(useFullField, &QCheckBox::toggled, this, &Ekos::Focus::syncSettings);
    connect(fullFieldInnerRing, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(fullFieldOuterRing, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(suspendGuideCheck, &QCheckBox::toggled, this, &Ekos::Focus::syncSettings);
    connect(GuideSettleTime, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);

    connect(focusBoxSize, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Focus::syncSettings);
    connect(maxTravelIN, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(stepIN, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(maxSingleStepIN, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(initialFocusOutStepsIN, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(toleranceIN, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(thresholdSpin, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(gaussianSigmaSpin, &QDoubleSpinBox::editingFinished, this, &Focus::syncSettings);
    connect(gaussianKernelSizeSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Focus::syncSettings);
    connect(multiRowAverageSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Focus::syncSettings);

    connect(focusAlgorithmCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Focus::syncSettings);
    connect(focusFramesSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Focus::syncSettings);
    connect(focusDetectionCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Focus::syncSettings);
}

void Focus::initPlots()
{
    connect(clearDataB, &QPushButton::clicked, this, &Ekos::Focus::clearDataPoints);

    profileDialog = new QDialog(this);
    profileDialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    QVBoxLayout *profileLayout = new QVBoxLayout(profileDialog);
    profileDialog->setWindowTitle(i18n("Relative Profile"));
    profilePlot = new QCustomPlot(profileDialog);
    profilePlot->setBackground(QBrush(Qt::black));
    profilePlot->xAxis->setBasePen(QPen(Qt::white, 1));
    profilePlot->yAxis->setBasePen(QPen(Qt::white, 1));
    profilePlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    profilePlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    profilePlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    profilePlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    profilePlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    profilePlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    profilePlot->xAxis->setBasePen(QPen(Qt::white, 1));
    profilePlot->yAxis->setBasePen(QPen(Qt::white, 1));
    profilePlot->xAxis->setTickPen(QPen(Qt::white, 1));
    profilePlot->yAxis->setTickPen(QPen(Qt::white, 1));
    profilePlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    profilePlot->yAxis->setSubTickPen(QPen(Qt::white, 1));
    profilePlot->xAxis->setTickLabelColor(Qt::white);
    profilePlot->yAxis->setTickLabelColor(Qt::white);
    profilePlot->xAxis->setLabelColor(Qt::white);
    profilePlot->yAxis->setLabelColor(Qt::white);

    profileLayout->addWidget(profilePlot);
    profileDialog->setLayout(profileLayout);
    profileDialog->resize(400, 300);

    connect(relativeProfileB, &QPushButton::clicked, profileDialog, &QDialog::show);

    currentGaus = profilePlot->addGraph();
    currentGaus->setLineStyle(QCPGraph::lsLine);
    currentGaus->setPen(QPen(Qt::red, 2));

    lastGaus = profilePlot->addGraph();
    lastGaus->setLineStyle(QCPGraph::lsLine);
    QPen pen(Qt::darkGreen);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(2);
    lastGaus->setPen(pen);

    HFRPlot->setBackground(QBrush(Qt::black));

    HFRPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    HFRPlot->yAxis->setBasePen(QPen(Qt::white, 1));

    HFRPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    HFRPlot->yAxis->setTickPen(QPen(Qt::white, 1));

    HFRPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    HFRPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));

    HFRPlot->xAxis->setTickLabelColor(Qt::white);
    HFRPlot->yAxis->setTickLabelColor(Qt::white);

    HFRPlot->xAxis->setLabelColor(Qt::white);
    HFRPlot->yAxis->setLabelColor(Qt::white);

    HFRPlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    HFRPlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    HFRPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    HFRPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    HFRPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    HFRPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);

    HFRPlot->yAxis->setLabel(i18n("HFR"));

    HFRPlot->setInteractions(QCP::iRangeZoom);
    HFRPlot->setInteraction(QCP::iRangeDrag, true);

    polynomialGraph = HFRPlot->addGraph();
    polynomialGraph->setLineStyle(QCPGraph::lsLine);
    polynomialGraph->setPen(QPen(QColor(140, 140, 140), 2, Qt::DotLine));
    polynomialGraph->setScatterStyle(QCPScatterStyle::ssNone);

    connect(HFRPlot->xAxis, static_cast<void(QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged), this, [this]()
    {
        drawHFRIndeces();
        if (polynomialGraphIsShown)
        {
            if (focusAlgorithm == FOCUS_POLYNOMIAL)
                graphPolynomialFunction();
        }
    });

    connect(HFRPlot, &QCustomPlot::mouseMove, this, [this](QMouseEvent * event)
    {
        double key = HFRPlot->xAxis->pixelToCoord(event->localPos().x());
        if (HFRPlot->xAxis->range().contains(key))
        {
            QCPGraph *graph = qobject_cast<QCPGraph *>(HFRPlot->plottableAt(event->pos(), false));

            if (graph)
            {
                if(graph == v_graph)
                {
                    int positionKey = v_graph->findBegin(key);
                    double focusPosition = v_graph->dataMainKey(positionKey);
                    double halfFluxRadius = v_graph->dataMainValue(positionKey);
                    QToolTip::showText(
                        event->globalPos(),
                        i18nc("HFR graphics tooltip; %1 is the Focus Position; %2 is the Half Flux Radius;",
                              "<table>"
                              "<tr><td>POS:   </td><td>%1</td></tr>"
                              "<tr><td>HFR:   </td><td>%2</td></tr>"
                              "</table>",
                              QString::number(focusPosition, 'f', 0),
                              QString::number(halfFluxRadius, 'f', 2)));
                }
            }
        }
    });

    focusPoint = HFRPlot->addGraph();
    focusPoint->setLineStyle(QCPGraph::lsImpulse);
    focusPoint->setPen(QPen(QColor(140, 140, 140), 2, Qt::SolidLine));
    focusPoint->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::white, Qt::yellow, 10));

    v_graph = HFRPlot->addGraph();
    v_graph->setLineStyle(QCPGraph::lsNone);
    v_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::white, Qt::white, 14));

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
    connect(&captureTimer, &QTimer::timeout, this, [&]()
    {
        capture();
    });

    // How long do we wait until an exposure times out and needs a retry?
    captureTimeout.setSingleShot(true);
    connect(&captureTimeout, &QTimer::timeout, this, &Ekos::Focus::processCaptureTimeout);

    // Start/Stop focus
    connect(startFocusB, &QPushButton::clicked, this, &Ekos::Focus::start);
    connect(stopFocusB, &QPushButton::clicked, this, &Ekos::Focus::checkStopFocus);

    // Focus IN/OUT
    connect(focusOutB, &QPushButton::clicked, [&]()
    {
        focusOut();
    });
    connect(focusInB, &QPushButton::clicked, [&]()
    {
        focusIn();
    });

    // Capture a single frame
    connect(captureB, &QPushButton::clicked, this, &Ekos::Focus::capture);
    // Start continuous capture
    connect(startLoopB, &QPushButton::clicked, this, &Ekos::Focus::startFraming);
    // Use a subframe when capturing
    connect(useSubFrame, &QCheckBox::toggled, this, &Ekos::Focus::toggleSubframe);
    // Reset frame dimensions to default
    connect(resetFrameB, &QPushButton::clicked, this, &Ekos::Focus::resetFrame);
    // Sync setting if full field setting is toggled.
    connect(useFullField, &QCheckBox::toggled, [&](bool toggled)
    {
        fullFieldInnerRing->setEnabled(toggled);
        fullFieldOuterRing->setEnabled(toggled);
        if (toggled)
        {
            useSubFrame->setChecked(false);
            useAutoStar->setChecked(false);
        }
        else
        {
            // Disable the overlay
            focusView->setStarFilterRange(0, 1);
        }
    });


    // Sync settings if the CCD selection is updated.
    connect(CCDCaptureCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Focus::checkCCD);
    // Sync settings if the Focuser selection is updated.
    connect(focuserCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Focus::checkFocuser);
    // Sync settings if the filter selection is updated.
    connect(FilterDevicesCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Focus::checkFilter);

    // Set focuser absolute position
    connect(startGotoB, &QPushButton::clicked, this, &Ekos::Focus::setAbsoluteFocusTicks);
    connect(stopGotoB, &QPushButton::clicked, [this]()
    {
        if (currentFocuser)
            currentFocuser->stop();
    });
    // Update the focuser box size used to enclose a star
    connect(focusBoxSize, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Ekos::Focus::updateBoxSize);

    // Update the focuser star detection if the detection algorithm selection changes.
    connect(focusDetectionCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [&](int index)
    {
        focusDetection = static_cast<StarAlgorithm>(index);
        thresholdSpin->setEnabled(focusDetection == ALGORITHM_THRESHOLD);
        multiRowAverageSpin->setEnabled(focusDetection == ALGORITHM_BAHTINOV);
        if (focusDetection == ALGORITHM_BAHTINOV)
        {
            // In case of Bahtinov mask uncheck auto select star
            useAutoStar->setChecked(false);
            focusBoxSize->setMaximum(512);
        }
        else
        {
            // When not using Bathinov mask, limit box size to 256 and make sure value stays within range.
            if (Options::focusBoxSize() > 256)
            {
                Options::setFocusBoxSize(32);
                // Focus box size changed, update control
                focusBoxSize->setValue(Options::focusBoxSize());
            }
            focusBoxSize->setMaximum(256);
        }
        useAutoStar->setEnabled(focusDetection != ALGORITHM_BAHTINOV);
    });

    // Update the focuser solution algorithm if the selection changes.
    connect(focusAlgorithmCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [&](int index)
    {
        setFocusAlgorithm(static_cast<FocusAlgorithm>(index));
    });

    // Reset star center on auto star check toggle
    connect(useAutoStar, &QCheckBox::toggled, this, [&](bool enabled)
    {
        if (enabled)
        {
            starCenter   = QVector3D();
            starSelected = false;
            focusView->setTrackingBox(QRect());
        }
    });
}

void Focus::setFocusAlgorithm(FocusAlgorithm algorithm)
{
    focusAlgorithm = algorithm;
    switch(algorithm)
    {
        case FOCUS_ITERATIVE:
            initialFocusOutStepsIN->setEnabled(false); // Out step multiple
            maxTravelIN->setEnabled(true);             // Max Travel
            stepIN->setEnabled(true);                  // Initial Step Size
            maxSingleStepIN->setEnabled(true);         // Max Step Size
            break;

        case FOCUS_POLYNOMIAL:
            initialFocusOutStepsIN->setEnabled(false); // Out step multiple
            maxTravelIN->setEnabled(true);             // Max Travel
            stepIN->setEnabled(true);                  // Initial Step Size
            maxSingleStepIN->setEnabled(true);         // Max Step Size
            break;

        case FOCUS_LINEAR:
            initialFocusOutStepsIN->setEnabled(true);  // Out step multiple
            maxTravelIN->setEnabled(true);             // Max Travel
            stepIN->setEnabled(true);                  // Initial Step Size
            maxSingleStepIN->setEnabled(false);        // Max Step Size
            break;
    }
}

void Focus::initView()
{
    focusView = new FITSView(focusingWidget, FITS_FOCUS);
    focusView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    focusView->setBaseSize(focusingWidget->size());
    focusView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(focusView);
    focusingWidget->setLayout(vlayout);
    connect(focusView, &FITSView::trackingStarSelected, this, &Ekos::Focus::focusStarSelected, Qt::UniqueConnection);
    focusView->setStarsEnabled(true);
    focusView->setStarsHFREnabled(true);
}

}

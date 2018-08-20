/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "focus.h"

#include "focusadaptor.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "auxiliary/kspaths.h"
#include "ekos/ekosmanager.h"
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
    setupUi(this);

    new FocusAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Focus", this);

    //frameModified     = false;

    waitStarSelectTimer.setInterval(AUTO_STAR_TIMEOUT);
    connect(&waitStarSelectTimer, SIGNAL(timeout()), this, SLOT(checkAutoStarTimeout()));

    //fy=fw=fh=0;
    HFRFrames.clear();

    FilterDevicesCombo->addItem("--");

    showFITSViewerB->setIcon(
        QIcon::fromTheme("kstars_fitsviewer"));
    showFITSViewerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(showFITSViewerB, SIGNAL(clicked()), this, SLOT(showFITSViewer()));

    toggleFullScreenB->setIcon(
        QIcon::fromTheme("view-fullscreen"));
    toggleFullScreenB->setShortcut(Qt::Key_F4);
    toggleFullScreenB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(toggleFullScreenB, SIGNAL(clicked()), this, SLOT(toggleFocusingWidgetFullScreen()));

    connect(startFocusB, SIGNAL(clicked()), this, SLOT(start()));
    connect(stopFocusB, SIGNAL(clicked()), this, SLOT(checkStopFocus()));

    connect(focusOutB, SIGNAL(clicked()), this, SLOT(focusOut()));
    connect(focusInB, SIGNAL(clicked()), this, SLOT(focusIn()));

    connect(captureB, SIGNAL(clicked()), this, SLOT(capture()));

    connect(startLoopB, SIGNAL(clicked()), this, SLOT(startFraming()));

    connect(useSubFrame, SIGNAL(toggled(bool)), this, SLOT(toggleSubframe(bool)));

    connect(resetFrameB, SIGNAL(clicked()), this, SLOT(resetFrame()));

    connect(CCDCaptureCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultCCD(QString)));
    connect(CCDCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkCCD(int)));

    connect(useFullField, &QCheckBox::toggled, [&](bool toggled)
    {
        Options::setFocusUseFullField(toggled);
        if (toggled)
        {
            useSubFrame->setChecked(false);
            useAutoStar->setChecked(false);
        }
    });

    FocusSettleTime->setValue(Options::focusSettleTime());
    connect(FocusSettleTime, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
         [=](double d) { Options::setFocusSettleTime(d); });

    connect(focuserCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultFocuser(QString)));
    connect(focuserCombo, SIGNAL(activated(int)), this, SLOT(checkFocuser(int)));

    connect(FilterDevicesCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));
    connect(setAbsTicksB, SIGNAL(clicked()), this, SLOT(setAbsoluteFocusTicks()));
    connect(binningCombo, SIGNAL(activated(int)), this, SLOT(setActiveBinning(int)));
    connect(focusBoxSize, SIGNAL(valueChanged(int)), this, SLOT(updateBoxSize(int)));

    focusDetection = static_cast<StarAlgorithm>(Options::focusDetection());
    focusDetectionCombo->setCurrentIndex(focusDetection);

    connect(focusDetectionCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [&](int index) {
        focusDetection = static_cast<StarAlgorithm>(index);
        thresholdSpin->setEnabled(focusDetection == ALGORITHM_THRESHOLD);
        Options::setFocusDetection(index);
    });

    focusAlgorithm = static_cast<FocusAlgorithm>(Options::focusAlgorithm());
    focusAlgorithmCombo->setCurrentIndex(focusAlgorithm);
    connect(focusAlgorithmCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [&](int index) {
        focusAlgorithm = static_cast<FocusAlgorithm>(index);
        //toleranceIN->setEnabled(focusAlgorithm == FOCUS_ITERATIVE);
        Options::setFocusAlgorithm(index);
    });

    activeBin = Options::focusXBin();
    binningCombo->setCurrentIndex(activeBin - 1);

    focusFramesSpin->setValue(Options::focusFramesCount());

    connect(clearDataB, SIGNAL(clicked()), this, SLOT(clearDataPoints()));

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

    connect(relativeProfileB, SIGNAL(clicked()), profileDialog, SLOT(show()));

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

    v_graph = HFRPlot->addGraph();
    v_graph->setLineStyle(QCPGraph::lsNone);
    v_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::white, Qt::red, 3));

    resetButtons();

    appendLogText(i18n("Idle."));

    for (auto &filter : FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    filterCombo->setCurrentIndex(Options::focusEffect());
    defaultScale = static_cast<FITSScale>(Options::focusEffect());
    connect(filterCombo, SIGNAL(activated(int)), this, SLOT(filterChangeWarning(int)));

    exposureIN->setValue(Options::focusExposure());
    toleranceIN->setValue(Options::focusTolerance());
    stepIN->setValue(Options::focusTicks());
    useAutoStar->setChecked(Options::focusAutoStarEnabled());
    focusBoxSize->setValue(Options::focusBoxSize());
    if (Options::focusMaxTravel() > maxTravelIN->maximum())
        maxTravelIN->setMaximum(Options::focusMaxTravel());
    maxTravelIN->setValue(Options::focusMaxTravel());
    useSubFrame->setChecked(Options::focusSubFrame());
    suspendGuideCheck->setChecked(Options::suspendGuiding());    
    darkFrameCheck->setChecked(Options::useFocusDarkFrame());
    thresholdSpin->setValue(Options::focusThreshold());
    useFullField->setChecked(Options::focusUseFullField());
    //focusFramesSpin->setValue(Options::focusFrames());

    connect(thresholdSpin, SIGNAL(valueChanged(double)), this, SLOT(setThreshold(double)));
    //connect(focusFramesSpin, SIGNAL(valueChanged(int)), this, SLOT(setFrames(int)));

    focusView = new FITSView(focusingWidget, FITS_FOCUS);
    focusView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    focusView->setBaseSize(focusingWidget->size());
    focusView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(focusView);
    focusingWidget->setLayout(vlayout);
    connect(focusView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(focusStarSelected(int,int)),
            Qt::UniqueConnection);

    focusView->setStarsEnabled(true);

    // Reset star center on auto star check toggle
    connect(useAutoStar, &QCheckBox::toggled, this, [&](bool enabled) {
        if (enabled)
        {
            starCenter   = QVector3D();
            starSelected = false;
            focusView->setTrackingBox(QRect());
        }
    });

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);
}

Focus::~Focus()
{
    if (focusingWidget->parent() == nullptr)
        toggleFocusingWidgetFullScreen();
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

            qCDebug(KSTARS_EKOS_FOCUS) << "Frame is reset. X:" << x << "Y:" << y << "W:" << w << "H:" << h << "binX:" << 1 << "binY:" << 1;

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

bool Focus::setCCD(const QString &device)
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

void Focus::setDefaultCCD(QString ccd)
{
    Options::setDefaultFocusCCD(ccd);
}

void Focus::setDefaultFocuser(QString focuser)
{
    Options::setDefaultFocusFocuser(focuser);
}

void Focus::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
    {
        ccdNum = CCDCaptureCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum >= 0 && ccdNum <= CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
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

                binningCombo->setCurrentIndex(activeBin - 1);
            }

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
    foreach (ISD::GDInterface *filter, Filters)
    {
        if (!strcmp(filter->getDeviceName(), newFilter->getDeviceName()))
            return;
    }

    FilterCaptureLabel->setEnabled(true);
    FilterDevicesCombo->setEnabled(true);
    FilterPosLabel->setEnabled(true);
    FilterPosCombo->setEnabled(true);
    filterManagerB->setEnabled(true);

    FilterDevicesCombo->addItem(newFilter->getDeviceName());

    Filters.append(static_cast<ISD::Filter *>(newFilter));

    checkFilter(1);

    FilterDevicesCombo->setCurrentIndex(1);
}

bool Focus::setFilter(QString device, int filterSlot)
{
    bool deviceFound = false;

    for (int i = 0; i < FilterDevicesCombo->count(); i++)
        if (device == FilterDevicesCombo->itemText(i))
        {
            checkFilter(i);
            deviceFound = true;
            break;
        }

    if (deviceFound == false)
        return false;

    if (filterSlot < FilterDevicesCombo->count())
        FilterDevicesCombo->setCurrentIndex(filterSlot);

    return true;
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
        currentFilterPosition=-1;
        FilterPosCombo->clear();
        return;
    }

    if (filterNum <= Filters.count())
        currentFilter = Filters.at(filterNum-1);

    filterManager->setCurrentFilterWheel(currentFilter);

    FilterPosCombo->clear();

    FilterPosCombo->addItems(filterManager->getFilterLabels());

    currentFilterPosition = filterManager->getFilterPosition();

    FilterPosCombo->setCurrentIndex(currentFilterPosition-1);

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

bool Focus::setFocuser(const QString & device)
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

void Focus::checkFocuser(int FocuserNum)
{
    if (FocuserNum == -1)
        FocuserNum = focuserCombo->currentIndex();

    if (FocuserNum == -1)
    {
        currentFocuser = nullptr;
        return;
    }

    if (FocuserNum <= Focusers.count())
        currentFocuser = Focusers.at(FocuserNum);

    filterManager->setFocusReady(currentFocuser->isConnected());

    // Disconnect all focusers
    foreach (ISD::Focuser *oneFocuser, Focusers)
    {
        disconnect(oneFocuser, SIGNAL(numberUpdated(INumberVectorProperty*)), this,
                   SLOT(processFocusNumber(INumberVectorProperty*)));
        //disconnect(oneFocuser, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(registerFocusProperty(INDI::Property*)));
    }

    canAbsMove = currentFocuser->canAbsMove();

    if (canAbsMove)
    {
        getAbsFocusPosition();

        absTicksSpin->setEnabled(true);
        absTicksLabel->setEnabled(true);
        setAbsTicksB->setEnabled(true);

        absTicksSpin->setValue(currentPosition);
    }
    else
    {
        absTicksSpin->setEnabled(false);
        absTicksLabel->setEnabled(false);
        setAbsTicksB->setEnabled(false);
    }

    canRelMove = currentFocuser->canRelMove();

    // In case we have a purely relative focuser, we pretend
    // it is an absolute focuser with initial point set at 50,000
    // This is done we can use the same algorithm used for absolute focuser
    if (canAbsMove == false && canRelMove == true)
    {
        currentPosition = 50000;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }

    canTimerMove = currentFocuser->canTimerMove();

    focusType = (canRelMove || canAbsMove || canTimerMove) ? FOCUS_AUTO : FOCUS_MANUAL;

    connect(currentFocuser, SIGNAL(numberUpdated(INumberVectorProperty*)), this,
            SLOT(processFocusNumber(INumberVectorProperty*)), Qt::UniqueConnection);
    //connect(currentFocuser, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(registerFocusProperty(INDI::Property*)), Qt::UniqueConnection);

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
        currentPosition = absMove->np[0].value;
        absMotionMax    = absMove->np[0].max;
        absMotionMin    = absMove->np[0].min;

        absTicksSpin->setMinimum(absMove->np[0].min);
        absTicksSpin->setMaximum(absMove->np[0].max);
        absTicksSpin->setSingleStep(absMove->np[0].step);

        maxTravelIN->setMinimum(absMove->np[0].min);
        maxTravelIN->setMaximum(absMove->np[0].max);

        absTicksLabel->setText(QString::number(static_cast<int>(currentPosition)));
        //absTicksSpin->setValue(currentPosition);
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

    if (canAbsMove)
    {
        absIterations = 0;
        getAbsFocusPosition();
        pulseDuration = stepIN->value();
    }
    else if (canRelMove)
    {
        appendLogText(i18n("Setting dummy central position to 50000"));
        absIterations   = 0;
        pulseDuration   = stepIN->value();
        currentPosition = 50000;
        absMotionMax    = 100000;
        absMotionMin    = 0;
    }
    else
    {
        pulseDuration = stepIN->value();

        if (pulseDuration <= MINIMUM_PULSE_TIMER)
        {
            appendLogText(i18n("Starting pulse step is too low. Increase the step size to %1 or higher...",
                               MINIMUM_PULSE_TIMER * 5));
            return;
        }
    }

    inAutoFocus = true;
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

    Options::setFocusTicks(stepIN->value());
    Options::setFocusTolerance(toleranceIN->value());
    //Options::setFocusExposure(exposureIN->value());
    Options::setFocusMaxTravel(maxTravelIN->value());
    Options::setFocusBoxSize(focusBoxSize->value());
    Options::setFocusSubFrame(useSubFrame->isChecked());
    Options::setFocusAutoStarEnabled(useAutoStar->isChecked());
    Options::setSuspendGuiding(suspendGuideCheck->isChecked());    
    Options::setUseFocusDarkFrame(darkFrameCheck->isChecked());
    Options::setFocusFramesCount(focusFramesSpin->value());

    qCDebug(KSTARS_EKOS_FOCUS)  << "Starting focus with box size: " << focusBoxSize->value()
                 << " Step Size: " << stepIN->value() << " Threshold: " << thresholdSpin->value()
                 << " Tolerance: " << toleranceIN->value()
                 << " Frames: " << 1 /*focusFramesSpin->value()*/ << " Maximum Travel: " << maxTravelIN->value();

    if (useAutoStar->isChecked())
        appendLogText(i18n("Autofocus in progress..."));
    else
        appendLogText(i18n("Please wait until image capture is complete..."));

    if (suspendGuideCheck->isChecked())
        emit suspendGuiding();

    //emit statusUpdated(true);
    state = Ekos::FOCUS_PROGRESS;
    qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
    emit newStatus(state);

    // Denoise with median filter
    //defaultScale = FITS_MEDIAN;

    KSNotification::event(QLatin1String("FocusStarted"), i18n("Autofocus operation started"));

    capture();
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
    stop(true);
}

void Focus::stop(bool aborted)
{
    qCDebug(KSTARS_EKOS_FOCUS) << "Stopppig Focus";

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    inAutoFocus        = false;
    inFocusLoop        = false;
    // Why starSelected is set to false below? We should retain star selection status under:
    // 1. Autostar is off, or
    // 2. Toggle subframe, or
    // 3. Reset frame
    // 4. Manual motion?

    //starSelected       = false;
    polySolutionFound  = 0;
    captureInProgress  = false;
    minimumRequiredHFR = -1;
    noStarCount        = 0;
    HFRFrames.clear();
    //maxHFR=1;

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    if (rememberUploadMode != currentCCD->getUploadMode())
        currentCCD->setUploadMode(rememberUploadMode);

    if (rememberCCDExposureLooping)
        currentCCD->setExposureLoopingEnabled(true);

    targetChip->abortExposure();

    //resetFrame();

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

void Focus::capture()
{
    if (captureInProgress)
    {
        qCWarning(KSTARS_EKOS_FOCUS) << "Capture called while already in progress. Capture is ignored.";
        return;
    }

    if (currentCCD == nullptr)
    {
        appendLogText(i18n("No CCD connected."));
        return;
    }

    waitStarSelectTimer.stop();

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    double seqExpose = exposureIN->value();

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        return;
    }

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
            filterManager->setFilterPosition(targetPosition, static_cast<FilterManager::FilterPolicy>(FilterManager::CHANGE_POLICY|FilterManager::OFFSET_POLICY));
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

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

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

    focusView->setBaseSize(focusingWidget->size());

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
    if (currentFocuser == nullptr)
        return false;

    if (currentFocuser->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to Focuser."));
        return false;
    }

    if (ms == -1)
        ms = stepIN->value();

    qCDebug(KSTARS_EKOS_FOCUS) << "Focus in (" << ms << ")";

    lastFocusDirection = FOCUS_IN;

    currentFocuser->focusIn();

    if (canAbsMove)
    {
        currentFocuser->moveAbs(currentPosition - ms);
        appendLogText(i18n("Focusing inward by %1 steps...", ms));
    }
    else if (canRelMove)
    {
        currentFocuser->moveRel(ms);
        appendLogText(i18n("Focusing inward by %1 steps...", ms));
    }
    else
    {
        currentFocuser->moveByTimer(ms);
        appendLogText(i18n("Focusing inward by %1 ms...", ms));
    }

    return true;
}

bool Focus::focusOut(int ms)
{
    if (currentFocuser == nullptr)
        return false;

    if (currentFocuser->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to Focuser."));
        return false;
    }

    lastFocusDirection = FOCUS_OUT;

    if (ms == -1)
        ms = stepIN->value();

    qCDebug(KSTARS_EKOS_FOCUS) << "Focus out (" << ms << ")";

    currentFocuser->focusOut();

    if (canAbsMove)
    {
        currentFocuser->moveAbs(currentPosition + ms);
        appendLogText(i18n("Focusing outward by %1 steps...", ms));
    }
    else if (canRelMove)
    {
        currentFocuser->moveRel(ms);
        appendLogText(i18n("Focusing outward by %1 steps...", ms));
    }
    else
    {
        currentFocuser->moveByTimer(ms);
        appendLogText(i18n("Focusing outward by %1 ms...", ms));
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

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    if (darkFrameCheck->isChecked())
    {
        FITSData *darkData   = DarkLibrary::Instance()->getDarkFrame(targetChip, exposureIN->value());;
        QVariantMap settings = frameSettings[targetChip];
        uint16_t offsetX     = settings["x"].toInt() / settings["binx"].toInt();
        uint16_t offsetY     = settings["y"].toInt() / settings["biny"].toInt();

        connect(DarkLibrary::Instance(), SIGNAL(darkFrameCompleted(bool)), this, SLOT(setCaptureComplete()));
        connect(DarkLibrary::Instance(), SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));

        targetChip->setCaptureFilter(defaultScale);

        if (darkData)
            DarkLibrary::Instance()->subtract(darkData, focusView, defaultScale, offsetX, offsetY);
        else
        {
            bool rc = DarkLibrary::Instance()->captureAndSubtract(targetChip, focusView, exposureIN->value(), offsetX,
                                                                  offsetY);
            darkFrameCheck->setChecked(rc);
        }

        return;
    }

    setCaptureComplete();
}

void Focus::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    int subBinX = 1, subBinY = 1;
    targetChip->getBinning(&subBinX, &subBinY);

    // Always reset capture mode to NORMAL
    // JM 2016-09-28: Disable setting back to FITS_NORMAL as it might be causing issues. Each module should set capture module separately.
    //targetChip->setCaptureMode(FITS_NORMAL);

    syncTrackingBoxPosition();

    //connect(targetImage, SIGNAL(trackingStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)), Qt::UniqueConnection);

    if (inFocusLoop == false)
        appendLogText(i18n("Image received."));

    if (captureInProgress && inFocusLoop == false && inAutoFocus == false)
    {
        captureB->setEnabled(true);
        stopFocusB->setEnabled(false);
        currentCCD->setUploadMode(rememberUploadMode);
    }

    if (rememberCCDExposureLooping)
        currentCCD->setExposureLoopingEnabled(true);

    captureInProgress = false;

    FITSData *image_data = focusView->getImageData();

    emit newStarPixmap(focusView->getTrackingBoxPixmap(10));

    // If we're not framing, let's try to detect stars
    //if (inFocusLoop == false || (inFocusLoop && focusView->isTrackingBoxEnabled()))
    if (inFocusLoop == false || (inFocusLoop && (focusView->isTrackingBoxEnabled() || Options::focusUseFullField())))
    {
        if (image_data->areStarsSearched() == false)
        {
            //if (starSelected == false && autoStarCheck->isChecked() && subFramed == false)
            //if (autoStarCheck->isChecked() && subFramed == false)
            //focusView->findStars(ALGORITHM_CENTROID);

            currentHFR = -1;

            if (Options::focusUseFullField())
            {                
                if (focusDetection != ALGORITHM_CENTROID && focusDetection != ALGORITHM_SEP)
                    focusView->findStars(ALGORITHM_CENTROID);
                else
                    focusView->findStars(focusDetection);
                focusView->updateFrame();
                currentHFR = image_data->getHFR(HFR_AVERAGE);
            }
            else
            {
                if (starSelected)
                {
                    focusView->findStars(focusDetection);
                    focusView->updateFrame();
                    currentHFR = image_data->getHFR(HFR_MAX);
                }
                else
                {
                   focusView->setTrackingBoxEnabled(false);

                    if (focusDetection != ALGORITHM_CENTROID && focusDetection != ALGORITHM_SEP)
                        focusView->findStars(ALGORITHM_CENTROID);
                    else
                        focusView->findStars(focusDetection);

                   focusView->setTrackingBoxEnabled(true);

                    focusView->updateFrame();
                    currentHFR = image_data->getHFR(HFR_MAX);
                }
            }
        }

        qCDebug(KSTARS_EKOS_FOCUS) << "Focus newFITS #" << HFRFrames.count() + 1 << ": Current HFR " << currentHFR;

        HFRFrames.append(currentHFR);

        // Check if we need to average more than a single frame
        if (HFRFrames.count() >= focusFramesSpin->value())
        {
            currentHFR = 0;

            // Remove all -1
            QMutableVectorIterator<double> i(HFRFrames);
            while (i.hasNext())
            {
                if (i.next() == -1)
                    i.remove();
            }

            if (HFRFrames.isEmpty())
                currentHFR = -1;
            else
            {
                // Perform simple sigma clipping if frames count > 3
                if (HFRFrames.count() > 3)
                {
                    // Sort all HFRs
                    std::sort(HFRFrames.begin(), HFRFrames.end());
                    const auto median =
                        ((HFRFrames.size() % 2) ?
                            HFRFrames[HFRFrames.size() / 2] :
                            ((double)HFRFrames[HFRFrames.size() / 2 - 1] + HFRFrames[HFRFrames.size() / 2]) * .5);
                    const auto mean = std::accumulate(HFRFrames.begin(), HFRFrames.end(), .0) / HFRFrames.size();
                    double variance = 0;
                    foreach (auto val, HFRFrames)
                        variance += (val - mean) * (val - mean);
                    const double stddev = sqrt(variance / HFRFrames.size());

                    // Reject those 2 sigma away from median
                    const double sigmaHigh = median + stddev * 2;
                    const double sigmaLow  = median - stddev * 2;

                    QMutableVectorIterator<double> i(HFRFrames);
                    while (i.hasNext())
                    {
                        auto val = i.next();
                        if (val > sigmaHigh || val < sigmaLow)
                            i.remove();
                    }
                }

                // Find average HFR
                currentHFR = std::accumulate(HFRFrames.begin(), HFRFrames.end(), .0) / HFRFrames.size();

                HFRFrames.clear();
            }
        }
        else
        {
            capture();
            return;
        }


        if (canAbsMove)
            emit newHFR(currentHFR, static_cast<int>(currentPosition));
        else
            emit newHFR(currentHFR, -1);

        QString HFRText = QString("%1").arg(currentHFR, 0, 'f', 2);

        if (/*focusType == FOCUS_MANUAL && */ lastHFR == -1)
            appendLogText(i18n("FITS received. No stars detected."));

        HFROut->setText(HFRText);

        if (currentHFR > 0)
        {
            // Check if we're done
            if (focusAlgorithm == FOCUS_POLYNOMIAL && polySolutionFound == MINIMUM_POLY_SOLUTIONS)
            {
                polySolutionFound = 0;
                appendLogText(i18n("Autofocus complete after %1 iterations.", hfr_position.count()));
                stop();
                emit resumeGuiding();
                setAutoFocusResult(true);
                return;
            }
            Edge *maxStarHFR = nullptr;

            // Center tracking box around selected star
            //if (starSelected && inAutoFocus)
            if (starCenter.isNull() == false && (inAutoFocus || minimumRequiredHFR >= 0) &&
                (maxStarHFR = image_data->getMaxHFRStar()) != nullptr)
            {
                starSelected = true;
                starCenter.setX(qMax(0, static_cast<int>(maxStarHFR->x)));
                starCenter.setY(qMax(0, static_cast<int>(maxStarHFR->y)));

                syncTrackingBoxPosition();

                // Record information to know if we have bogus results
                QVector3D oneStar = starCenter;
                oneStar.setZ(currentHFR);
                starsHFR.append(oneStar);
            }
            else
            {
                // Record information to know if we have bogus results
                QVector3D oneStar(starCenter.x(), starCenter.y(), currentHFR);
                starsHFR.append(oneStar);
            }

            if (currentHFR > maxHFR)
                maxHFR = currentHFR;

            if (inFocusLoop || (inAutoFocus && canAbsMove == false && canRelMove == false))
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

    // If just framing, let's capture again
    if (inFocusLoop)
    {
        capture();
        return;
    }

    if (Options::focusUseFullField() == false && starCenter.isNull())
    {
        int x = 0, y = 0, w = 0, h = 0;

        if (frameSettings.contains(targetChip))
        {
            QVariantMap settings = frameSettings[targetChip];
            x                    = settings["x"].toInt();
            y                    = settings["y"].toInt();
            w                    = settings["w"].toInt();
            h                    = settings["h"].toInt();
        }
        else
            targetChip->getFrame(&x, &y, &w, &h);

        if (useAutoStar->isChecked())
        {
            Edge *maxStar = image_data->getMaxHFRStar();

            if (maxStar == nullptr)
            {
                appendLogText(i18n("Failed to automatically select a star. Please select a star manually."));

                //if (fw == 0 || fh == 0)
                //targetChip->getFrame(&fx, &fy, &fw, &fh);

                //targetImage->setTrackingBox(QRect((fw-focusBoxSize->value())/2, (fh-focusBoxSize->value())/2, focusBoxSize->value(), focusBoxSize->value()));
                focusView->setTrackingBox(QRect(w - focusBoxSize->value() / (subBinX * 2),
                                                h - focusBoxSize->value() / (subBinY * 2),
                                                focusBoxSize->value() / subBinX, focusBoxSize->value() / subBinY));
                focusView->setTrackingBoxEnabled(true);

                state = Ekos::FOCUS_WAITING;
                qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
                emit newStatus(state);

                waitStarSelectTimer.start();

                return;
            }

            if (subFramed == false && useSubFrame->isEnabled() && useSubFrame->isChecked())
            {
                int offset = (static_cast<double>(focusBoxSize->value()) / subBinX) * 1.5;
                int subX   = (maxStar->x - offset) * subBinX;
                int subY   = (maxStar->y - offset) * subBinY;
                int subW   = offset * 2 * subBinX;
                int subH   = offset * 2 * subBinY;

                int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
                targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

                if (subX < minX)
                    subX = minX;
                if (subY < minY)
                    subY = minY;
                if ((subW + subX) > maxW)
                    subW = maxW - subX;
                if ((subH + subY) > maxH)
                    subH = maxH - subY;

                //targetChip->setFocusFrame(subX, subY, subW, subH);

                //fx += subX;
                //fy += subY;
                //fw = subW;
                //fh = subH;
                //frameModified = true;

                //x += subX;
                //y += subY;
                //w = subW;
                //h = subH;                

                QVariantMap settings = frameSettings[targetChip];
                settings["x"]        = subX;
                settings["y"]        = subY;
                settings["w"]        = subW;
                settings["h"]        = subH;
                settings["binx"]     = subBinX;
                settings["biny"]     = subBinY;

                qCDebug(KSTARS_EKOS_FOCUS) << "Frame is subframed. X:" << subX << "Y:" << subY << "W:" << subW << "H:" << subH << "binX:" << subBinX << "binY:" << subBinY;

                starsHFR.clear();

                frameSettings[targetChip] = settings;

                starCenter.setX(subW / (2 * subBinX));
                starCenter.setY(subH / (2 * subBinY));
                starCenter.setZ(subBinX);

                subFramed = true;                

                focusView->setFirstLoad(true);                

                capture();
            }
            else
            {
                starCenter.setX(maxStar->x);
                starCenter.setY(maxStar->y);
                starCenter.setZ(subBinX);

                if (inAutoFocus)
                    capture();
            }

            syncTrackingBoxPosition();
            defaultScale = static_cast<FITSScale>(filterCombo->currentIndex());
            return;
        }
        else
        {
            appendLogText(i18n("Capture complete. Select a star to focus."));

            starSelected = false;
            //if (fw == 0 || fh == 0)
            //targetChip->getFrame(&fx, &fy, &fw, &fh);

            int subBinX = 1, subBinY = 1;
            targetChip->getBinning(&subBinX, &subBinY);

            focusView->setTrackingBox(QRect((w - focusBoxSize->value()) / (subBinX * 2),
                                            (h - focusBoxSize->value()) / (2 * subBinY),
                                            focusBoxSize->value() / subBinX, focusBoxSize->value() / subBinY));
            focusView->setTrackingBoxEnabled(true);

            state = Ekos::FOCUS_WAITING;
            qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
            emit newStatus(state);

            waitStarSelectTimer.start();
            //connect(targetImage, SIGNAL(trackingStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)), Qt::UniqueConnection);
            return;
        }
    }

    if (minimumRequiredHFR >= 0)
    {

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
            else
            {
                noStarCount = 0;
                setAutoFocusResult(false);
            }
        }
        else if (currentHFR > minimumRequiredHFR)
        {
            qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR:" << currentHFR << "is above required minimum HFR:" << minimumRequiredHFR << ". Starting AutoFocus...";
            inSequenceFocus = true;
            start();
        }
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << "Current HFR:" << currentHFR << "is below required minimum HFR:" << minimumRequiredHFR << ". Autofocus successful.";
            setAutoFocusResult(true);
            drawProfilePlot();
        }

        minimumRequiredHFR = -1;

        return;
    }

    drawProfilePlot();

    if (Options::focusLogging())
    {
        QDir dir;
        QString path = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "autofocus/" +
                       QDateTime::currentDateTime().toString("yyyy-MM-dd");
        dir.mkpath(path);
        // IS8601 contains colons but they are illegal under Windows OS, so replacing them with '-'
        // The timestamp is no longer ISO8601 but it should solve interoperality issues between different OS hosts
        QString name     = "autofocus_frame_" + QDateTime::currentDateTime().toString("HH-mm-ss") + ".fits";
        QString filename = path + QStringLiteral("/") + name;
        focusView->getImageData()->saveFITS(filename);
    }

    if (inAutoFocus == false)
        return;

    if (state != Ekos::FOCUS_PROGRESS)
    {
        state = Ekos::FOCUS_PROGRESS;
        qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);
        emit newStatus(state);
    }    

    if (canAbsMove || canRelMove)
        autoFocusAbs();
    else
        autoFocusRel();
}

void Focus::clearDataPoints()
{
    maxHFR = 1;
    hfr_position.clear();
    hfr_value.clear();

    drawHFRPlot();
}

void Focus::drawHFRPlot()
{
    v_graph->setData(hfr_position, hfr_value);

    if (inFocusLoop == false && (canAbsMove || canRelMove))
    {
        //HFRPlot->xAxis->setLabel(i18n("Position"));
        HFRPlot->xAxis->setRange(minPos - pulseDuration, maxPos + pulseDuration);
        HFRPlot->yAxis->setRange(currentHFR / 1.5, maxHFR);
    }
    else
    {
        //HFRPlot->xAxis->setLabel(i18n("Iteration"));
        HFRPlot->xAxis->setRange(1, hfr_value.count() + 1);
        HFRPlot->yAxis->setRange(currentHFR / 1.5, maxHFR * 1.25);
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

    if (++absIterations > MAXIMUM_ABS_ITERATIONS)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try increasing tolerance value."));
        abort();
        setAutoFocusResult(false);
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        if (noStarCount < MAX_RECAPTURE_RETRIES)
        {
            appendLogText(i18n("No stars detected, capturing again..."));
            capture();
            noStarCount++;
            return;
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
            return;
        }
    }
    else
        noStarCount = 0;

    if (hfr_position.empty())
    {
        maxPos = 1;
        minPos = 1e6;
    }

    if (currentPosition > maxPos)
        maxPos = currentPosition;
    if (currentPosition < minPos)
        minPos = currentPosition;

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
            if (focusOut(pulseDuration) == false)
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
                    appendLogText(i18n("Autofocus complete after %1 iterations.", hfr_position.count()));
                    stop();
                    emit resumeGuiding();
                    setAutoFocusResult(true);
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
                    qCDebug(KSTARS_EKOS_FOCUS) << "new minHFR " << minHFR << " @ positioin " << minHFRPos;
                }

                lastHFRPos = currentPosition;

                // HFR is decreasing, we are on the right direction
                HFRDec++;
                HFRInc = 0;
            }
            else

            {
                // HFR increased, let's deal with it.
                HFRInc++;
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
                    double chisq = 0, min_position = 0, min_hfr = 0;
                    coeff = gsl_polynomial_fit(hfr_position.data(), hfr_value.data(), hfr_position.count(), 3, chisq);

                    polyMinimumFound = findMinimum(minHFRPos, &min_position, &min_hfr);

                    qCDebug(KSTARS_EKOS_FOCUS) << "Polynomial Coefficients c0:" << coeff[0] << "c1:" << coeff[1] << "c2:" << coeff[2]
                                 << "c3:" << coeff[3];
                     qCDebug(KSTARS_EKOS_FOCUS) << "Found Minimum?" << (polyMinimumFound ? "Yes" : "No");

                    if (polyMinimumFound)
                    {
                        qCDebug(KSTARS_EKOS_FOCUS) << "Minimum Solution:" << min_hfr << "@" << min_position;
                        polySolutionFound++;
                        targetPosition = floor(min_position);
                        appendLogText(i18n("Found polynomial solution @ %1", QString::number(min_position, 'f', 0)));
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
                appendLogText(i18n("Autofocus complete after %1 iterations.", hfr_position.count()));
                stop();
                emit resumeGuiding();
                setAutoFocusResult(true);
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
                qCDebug(KSTARS_EKOS_FOCUS) << "targetPosition (" << targetPosition << ") - initHFRAbsPos ("
                             << initialFocuserAbsPosition << ") exceeds maxTravel distance of " << maxTravelIN->value();

                appendLogText("Maximum travel limit reached. Autofocus aborted.");
                abort();
                setAutoFocusResult(false);
                break;
            }

            // Get delta for next move
            delta = (targetPosition - currentPosition);

            qCDebug(KSTARS_EKOS_FOCUS) << "delta (targetPosition - currentPosition) " << delta;
            qCDebug(KSTARS_EKOS_FOCUS) << "Focusing " << ((delta < 0) ? "IN" : "OUT");

            // Now cross your fingers and wait
            bool rc = false;
            if (delta > 0)
                rc = focusOut(delta);
            else
                rc = focusIn(fabs(delta));

            if (rc == false)
            {
                abort();
                setAutoFocusResult(false);
            }
            break;
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
            focusIn(pulseDuration);
            break;

        case FOCUS_IN:
        case FOCUS_OUT:
            if (fabs(currentHFR - minHFR) < (toleranceIN->value() / 100.0) && HFRInc == 0)
            {
                appendLogText(i18n("Autofocus complete after %1 iterations.", hfr_position.count()));
                stop();
                emit resumeGuiding();
                setAutoFocusResult(true);
                break;
            }
            else if (currentHFR < lastHFR)
            {
                if (currentHFR < minHFR)
                    minHFR = currentHFR;

                lastHFR = currentHFR;
                if (lastFocusDirection == FOCUS_IN)
                    focusIn(pulseDuration);
                else
                    focusOut(pulseDuration);
                HFRInc = 0;
            }
            else
            {
                HFRInc++;

                lastHFR = currentHFR;

                HFRInc = 0;

                pulseDuration *= 0.75;

                bool rc = false;

                if (lastFocusDirection == FOCUS_IN)
                    rc = focusOut(pulseDuration);
                else
                    rc = focusIn(pulseDuration);

                if (rc == false)
                {
                    abort();
                    setAutoFocusResult(false);
                }
            }
            break;

        default:
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
        setAbsTicksB->setEnabled(true);
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

void Focus::processFocusNumber(INumberVectorProperty *nvp)
{
    // Return if it is not our current focuser
    if (strcmp(nvp->device, currentFocuser->getDeviceName()))
        return;

    if (!strcmp(nvp->name, "ABS_FOCUS_POSITION"))
    {
        INumber *pos = IUFindNumber(nvp, "FOCUS_ABSOLUTE_POSITION");
        if (pos)
        {
            currentPosition = pos->value;
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

        if (canAbsMove && inAutoFocus)
        {
            if (nvp->s == IPS_OK && captureInProgress == false)
                QTimer::singleShot(FocusSettleTime->value()*1000, this, SLOT(capture()));
                //capture();
            else if (nvp->s == IPS_ALERT)
            {
                appendLogText(i18n("Focuser error, check INDI panel."));
                abort();
                setAutoFocusResult(false);
            }
        }
        return;
    }

    if (canAbsMove)
        return;

    if (!strcmp(nvp->name, "REL_FOCUS_POSITION"))
    {
        INumber *pos = IUFindNumber(nvp, "FOCUS_RELATIVE_POSITION");
        if (pos && nvp->s == IPS_OK)
        {
            currentPosition += pos->value * (lastFocusDirection == FOCUS_IN ? -1 : 1);
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
            if (nvp->s == IPS_OK && captureInProgress == false)
                QTimer::singleShot(FocusSettleTime->value()*1000, this, SLOT(capture()));
            else if (nvp->s == IPS_ALERT)
            {
                appendLogText(i18n("Focuser error, check INDI panel."));
                abort();
                setAutoFocusResult(false);
            }
        }

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
            if (nvp->s == IPS_OK && captureInProgress == false)
                QTimer::singleShot(FocusSettleTime->value()*1000, this, SLOT(capture()));
            else if (nvp->s == IPS_ALERT)
            {
                appendLogText(i18n("Focuser error, check INDI panel."));
                abort();
                setAutoFocusResult(false);
            }
        }

        return;
    }
}

void Focus::appendLogText(const QString &text)
{
    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                            QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_FOCUS) << text;

    emit newLog();
}

void Focus::clearLog()
{
    logText.clear();
    emit newLog();
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
        setAbsTicksB->setEnabled(false);

        resetFrameB->setEnabled(false);

        return;
    }

    if (currentFocuser)
    {
        focusOutB->setEnabled(true);
        focusInB->setEnabled(true);

        startFocusB->setEnabled(focusType == FOCUS_AUTO);
        setAbsTicksB->setEnabled(canAbsMove);
    }
    else
    {
        focusOutB->setEnabled(false);
        focusInB->setEnabled(false);

        startFocusB->setEnabled(false);
        setAbsTicksB->setEnabled(false);
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

        qCDebug(KSTARS_EKOS_FOCUS) << "Frame is subframed. X:" << x << "Y:" << y << "W:" << w << "H:" << h << "binX:" << subBinX << "binY:" << subBinY;

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

        squareMovedOutside = (dist > ((double)focusBoxSize->value() / subBinX));
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

    if (starSelected == false)
    {
        appendLogText(i18n("Focus star is selected."));
        starSelected = true;
    }

    if (squareMovedOutside && inAutoFocus == false && useAutoStar->isChecked())
    {
        useAutoStar->blockSignals(true);
        useAutoStar->setChecked(false);
        useAutoStar->blockSignals(false);
        appendLogText(i18n("Disabling Auto Star Selection as star selection box was moved manually."));
        starSelected = false;
    }

    waitStarSelectTimer.stop();
    state = inAutoFocus ? FOCUS_PROGRESS : FOCUS_IDLE;
    qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);

    emit newStatus(state);
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

    resetFocusIteration = 0;

    if (status)
    {
        KSNotification::event(QLatin1String("FocusSuccessful"), i18n("Autofocus operation completed successfully"));
        state = Ekos::FOCUS_COMPLETE;
    }
    else
    {
        KSNotification::event(QLatin1String("FocusFailed"), i18n("Autofocus operation failed with errors"), KSNotification::EVENT_ALERT);
        state = Ekos::FOCUS_FAILED;
    }

    qCDebug(KSTARS_EKOS_FOCUS) << "State:" << Ekos::getFocusStatusString(state);

    // Do not emit result back yet if we have a locked filter pending return to original filter
    if (fallbackFilterPending)
    {
        filterManager->setFilterPosition(fallbackFilterPosition, static_cast<FilterManager::FilterPolicy>(FilterManager::CHANGE_POLICY|FilterManager::OFFSET_POLICY));
        return;
    }
    emit newStatus(state);
}

void Focus::checkAutoStarTimeout()
{
    //if (starSelected == false && inAutoFocus)
    if (starCenter.isNull() && inAutoFocus)
    {
        if (rememberStarCenter.isNull() == false)
        {
            focusStarSelected(rememberStarCenter.x(), rememberStarCenter.y());
            appendLogText(i18n("No star was selected. Using last known position..."));
            return;
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

void Focus::setActiveBinning(int bin)
{
    activeBin = bin + 1;
    Options::setFocusXBin(activeBin);
}

void Focus::setThreshold(double value)
{
    Options::setFocusThreshold(value);
}

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
        QUrl url = QUrl::fromLocalFile(data->getFilename());

        if (fv.isNull())
        {
            if (Options::singleWindowCapturedFITS())
                fv = KStars::Instance()->genericFITSViewer();
            else
            {
                fv = new FITSViewer(Options::independentWindowFITS() ? nullptr : KStars::Instance());
                KStars::Instance()->getFITSViewersList().append(fv.data());
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

void Focus::adjustFocusOffset(int value, bool useAbsoluteOffset)
{
    adjustFocus = true;

    int relativeOffset = 0;

    if (useAbsoluteOffset == false)
        relativeOffset = value;
    else
        relativeOffset = value - currentPosition;

    if (relativeOffset > 0)
        focusOut(relativeOffset);
    else
        focusIn(abs(relativeOffset));
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
        focusingWidget->setParent(0);
        focusingWidget->setWindowTitle(i18n("Focus Frame"));
        focusingWidget->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
        focusingWidget->showMaximized();
        focusingWidget->show();
    }
}

void Focus::setMountStatus(ISD::Telescope::TelescopeStatus newState)
{
    switch (newState)
    {
        case ISD::Telescope::MOUNT_PARKING:
        case ISD::Telescope::MOUNT_SLEWING:
        case ISD::Telescope::MOUNT_MOVING:
            captureB->setEnabled(false);
            startFocusB->setEnabled(false);
            startLoopB->setEnabled(false);
            break;

        default:
            resetButtons();
            break;
    }
}

double Focus::fn1(double x, void *params)
{
    Focus *module = static_cast<Focus *>(params);

    return (module->coeff[0] + module->coeff[1] * x + module->coeff[2] * pow(x, 2) + module->coeff[3] * pow(x, 3));
}

bool Focus::findMinimum(double expected, double *position, double *hfr)
{
    int status;
    int iter = 0, max_iter = 100;
    const gsl_min_fminimizer_type *T;
    gsl_min_fminimizer *s;
    double m = expected;
    double a = *std::min_element(hfr_position.constBegin(), hfr_position.constEnd());
    double b = *std::max_element(hfr_position.constBegin(), hfr_position.constEnd());
    ;
    gsl_function F;

    F.function = &Focus::fn1;
    F.params   = this;

    // Must turn off error handler or it aborts on error
    gsl_set_error_handler_off();

    T      = gsl_min_fminimizer_brent;
    s      = gsl_min_fminimizer_alloc(T);
    status = gsl_min_fminimizer_set(s, &F, m, a, b);

    if (status != GSL_SUCCESS)
    {
        qCWarning(KSTARS_EKOS_FOCUS) << "Focus GSL error:" << gsl_strerror(status);
        return false;
    }

    do
    {
        iter++;
        status = gsl_min_fminimizer_iterate(s);

        m = gsl_min_fminimizer_x_minimum(s);
        a = gsl_min_fminimizer_x_lower(s);
        b = gsl_min_fminimizer_x_upper(s);

        status = gsl_min_test_interval(a, b, 0.01, 0.0);

        if (status == GSL_SUCCESS)
        {
            *position = m;
            *hfr      = fn1(m, this);
        }
    } while (status == GSL_CONTINUE && iter < max_iter);

    gsl_min_fminimizer_free(s);

    return (status == GSL_SUCCESS);
}

void Focus::removeDevice(ISD::GDInterface *deviceRemoved)
{
    // Check in Focusers
    for (ISD::GDInterface *focuser : Focusers)
    {
        if (focuser == deviceRemoved)
        {
            Focusers.removeOne(dynamic_cast<ISD::Focuser*>(focuser));
            focuserCombo->removeItem(focuserCombo->findText(focuser->getDeviceName()));
            checkFocuser();
            resetButtons();
            return;
        }
    }

    // Check in CCDs
    for (ISD::GDInterface *ccd : CCDs)
    {
        if (ccd == deviceRemoved)
        {
            CCDs.removeOne(dynamic_cast<ISD::CCD*>(ccd));
            CCDCaptureCombo->removeItem(CCDCaptureCombo->findText(ccd->getDeviceName()));
            checkCCD();
            resetButtons();
            return;
        }
    }

    // Check in Filters
    for (ISD::GDInterface *filter : Filters)
    {
        if (filter == deviceRemoved)
        {
            Filters.removeOne(filter);
            filterCombo->removeItem(filterCombo->findText(filter->getDeviceName()));
            checkFilter();
            resetButtons();
            return;
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
        FilterPosCombo->setCurrentIndex(currentFilterPosition-1);
    });
    connect(filterManager.data(), &FilterManager::positionChanged, this, [this]()
    {
        currentFilterPosition = filterManager->getFilterPosition();
        FilterPosCombo->setCurrentIndex(currentFilterPosition-1);
    });
    connect(filterManager.data(), &FilterManager::exposureChanged, this, [this]()
    {
        exposureIN->setValue(filterManager->getFilterExposure());
    ;});

    connect(FilterPosCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
            [=](const QString &text)
    {
        exposureIN->setValue(filterManager->getFilterExposure(text));
    }
    );
}

}

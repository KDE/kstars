/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QFileDialog>
#include <QDirIterator>
#include <QStandardPaths>

#include <KMessageBox>
#include <KDirWatch>
#include <KLocalizedString>

#include <basedevice.h>
#include <lilxml.h>

#include "Options.h"

#include "ksnotify.h"
#include "kstars.h"
#include "kstarsdata.h"

#include "capture.h"

#include "indi/driverinfo.h"
#include "indi/indifilter.h"
#include "indi/clientmanager.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitsview.h"

#include "ekosmanager.h"
#include "captureadaptor.h"

#include "QProgressIndicator.h"

#define INVALID_TEMPERATURE 10000
#define INVALID_HA          10000
#define MF_TIMER_TIMEOUT    90000
#define MF_RA_DIFF_LIMIT    4
#define MAX_TEMP_DIFF       0.1
#define MAX_CAPTURE_RETRIES 3

namespace Ekos
{

SequenceJob::SequenceJob()
{
    statusStrings = QStringList() << xi18n("Idle") << xi18n("In Progress") << xi18n("Error") << xi18n("Aborted") << xi18n("Complete");
    status = JOB_IDLE;
    exposure=count=delay=frameType=targetFilter=isoIndex=-1;
    currentTemperature=targetTemperature=INVALID_TEMPERATURE;
    captureFilter=FITS_NONE;
    preview=false;
    showFITS=false;
    filterReady=temperatureReady=true;
    activeChip=NULL;
    activeCCD=NULL;
    activeFilter= NULL;
    statusCell = NULL;
    completed=0;
    captureRetires=0;
}

void SequenceJob::reset()
{
    // Reset to default values
    activeChip->setBatchMode(false);
    activeChip->setShowFITS(Options::showFITS());
}

void SequenceJob::resetStatus()
{
    status = JOB_IDLE;
    completed=0;
    exposeLeft=0;
    captureRetires=0;
    if (preview == false && statusCell)
        statusCell->setText(statusStrings[status]);
}

void SequenceJob::abort()
{
    status = JOB_ABORTED;
    if (preview == false && statusCell)
        statusCell->setText(statusStrings[status]);
    if (activeChip->canAbort())
        activeChip->abortExposure();
    activeChip->setBatchMode(false);
}

void SequenceJob::done()
{
    status = JOB_DONE;

    if (statusCell)
        statusCell->setText(statusStrings[status]);

}

void SequenceJob::prepareCapture()
{
    activeChip->setBatchMode(!preview);

    activeChip->setShowFITS(showFITS);

    activeCCD->setFITSDir(fitsDir);

    activeCCD->setISOMode(isoMode);

    activeCCD->setSeqPrefix(prefix);

    if (activeChip->isBatchMode())
        activeCCD->updateUploadSettings();

    if (isoIndex != -1)
    {
        if (isoIndex != activeChip->getISOIndex())
             activeChip->setISOIndex(isoIndex);
    }

    if (targetFilter != -1 && activeFilter != NULL)
    {
        if (targetFilter == currentFilter)
            //emit prepareComplete();
            filterReady = true;
        else
        {
            filterReady = false;
            activeFilter->runCommand(INDI_SET_FILTER, &targetFilter);
        }
    }


    if (Options::enforceTemperatureControl() && targetTemperature != currentTemperature)
    {
        temperatureReady = false;
        activeCCD->setTemperature(targetTemperature);
    }

    if (temperatureReady && filterReady)
        emit prepareComplete();

}

SequenceJob::CAPTUREResult SequenceJob::capture(bool isDark)
{
    if (targetFilter != -1 && activeFilter != NULL)
    {
        if (targetFilter != currentFilter)
            activeFilter->runCommand(INDI_SET_FILTER, &targetFilter);
    }

   if (isoIndex != -1)
   {
       if (isoIndex != activeChip->getISOIndex())
            activeChip->setISOIndex(isoIndex);
   }

   if ((w > 0 && h > 0) && activeChip->canSubframe() && activeChip->setFrame(x, y, w, h) == false)
   {
        status = JOB_ERROR;

        if (preview == false && statusCell)
            statusCell->setText(statusStrings[status]);

        return CAPTURE_FRAME_ERROR;

   }

    if (activeChip->canBin() && activeChip->setBinning(binX, binY) == false)
    {
        status = JOB_ERROR;

        if (preview == false && statusCell)
            statusCell->setText(statusStrings[status]);

        return CAPTURE_BIN_ERROR;
    }

    if (isDark)
    {
        activeChip->setFrameType(FRAME_DARK);
        activeChip->setCaptureMode(FITS_CALIBRATE);
    }
    else
    {
        activeChip->setFrameType(frameTypeName);
        activeChip->setCaptureMode(FITS_NORMAL);
        activeChip->setCaptureFilter(captureFilter);
    }

    // If filter is different that CCD, send the filter info
    if (activeFilter && activeFilter != activeCCD)
        activeCCD->setFilter(filter);

    status = JOB_BUSY;

    if (preview == false && statusCell)
        statusCell->setText(statusStrings[status]);

    exposeLeft = exposure;

    activeChip->capture(exposure);

    return CAPTURE_OK;

}

void SequenceJob::setTargetFilter(int pos, const QString & name)
{
    targetFilter = pos;
    filter    = name;
}

void SequenceJob::setFrameType(int type, const QString & name)
{
    frameType = type;
    frameTypeName = name;
}

double SequenceJob::getExposeLeft() const
{
    return exposeLeft;
}

void SequenceJob::setExposeLeft(double value)
{
    exposeLeft = value;
}

void SequenceJob::setPrefixSettings(const QString &prefix, bool typeEnabled, bool filterEnabled, bool exposureEnabled)
{
    rawPrefix               = prefix;
    typePrefixEnabled       = typeEnabled;
    filterPrefixEnabled     = filterEnabled;
    expPrefixEnabled        = exposureEnabled;
}

void SequenceJob::getPrefixSettings(QString &prefix, bool &typeEnabled, bool &filterEnabled, bool &exposureEnabled)
{
    prefix          = rawPrefix;
    typeEnabled     = typePrefixEnabled;
    filterEnabled   = filterPrefixEnabled;
    exposureEnabled = expPrefixEnabled;
}
double SequenceJob::getCurrentTemperature() const
{
    return currentTemperature;
}

void SequenceJob::setCurrentTemperature(double value)
{
    currentTemperature = value;

    if (Options::enforceTemperatureControl() == false || fabs(targetTemperature - currentTemperature) <= MAX_TEMP_DIFF)
        temperatureReady = true;

    if (filterReady && temperatureReady && (status == JOB_IDLE || status == JOB_ABORTED))
        emit prepareComplete();
}

double SequenceJob::getTargetTemperature() const
{
    return targetTemperature;
}

void SequenceJob::setTargetTemperature(double value)
{
    targetTemperature = value;
}

double SequenceJob::getTargetADU() const
{
    return targetADU;
}

void SequenceJob::setTargetADU(double value)
{
    targetADU = value;
}
int SequenceJob::getCaptureRetires() const
{
    return captureRetires;
}

void SequenceJob::setCaptureRetires(int value)
{
    captureRetires = value;
}


int SequenceJob::getISOIndex() const
{
    return isoIndex;
}

void SequenceJob::setISOIndex(int value)
{
    isoIndex = value;
}

int SequenceJob::getCurrentFilter() const
{
    return currentFilter;
}

void SequenceJob::setCurrentFilter(int value)
{
    currentFilter = value;

    if (currentFilter == targetFilter)
        filterReady = true;

    if (filterReady && temperatureReady && (status == JOB_IDLE || status == JOB_ABORTED))
        emit prepareComplete();
}

Capture::Capture()
{
    setupUi(this);

    new CaptureAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Capture",  this);

    currentCCD = NULL;
    currentTelescope = NULL;
    currentFilter = NULL;

    filterSlot = NULL;
    filterName = NULL;
    activeJob  = NULL;

    targetChip = NULL;
    guideChip  = NULL;

    deviationDetected = false;
    spikeDetected     = false;

    isAutoGuiding   = false;
    guideDither     = false;
    isAutoFocus     = false;
    autoFocusStatus = false;
    resumeAlignmentAfterFlip= false;

    mDirty          = false;
    jobUnderEdit    = false;
    currentFilterPosition   = -1;

    calibrationState  = CALIBRATE_NONE;
    meridianFlipStage = MF_NONE;
    resumeGuidingAfterFlip = false;

    ADURaw1 = ADURaw2 = ExpRaw1 = ExpRaw2 = -1;
    ADUSlope = 0;

    pi = new QProgressIndicator(this);

    progressLayout->addWidget(pi, 0, 4, 1, 1);

    seqWatcher		= new KDirWatch();
    seqTimer = new QTimer(this);
    connect(startB, SIGNAL(clicked()), this, SLOT(startSequence()));
    connect(stopB, SIGNAL(clicked()), this, SLOT(stopSequence()));
    connect(seqTimer, SIGNAL(timeout()), this, SLOT(captureImage()));

    connect(CCDCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkCCD(int)));

    connect(FilterCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));

    connect(displayCheck, SIGNAL(toggled(bool)), this, SLOT(checkPreview(bool)));

    connect(previewB, SIGNAL(clicked()), this, SLOT(captureOne()));

    connect( seqWatcher, SIGNAL(dirty(QString)), this, SLOT(checkSeqBoundary(QString)));

    connect(addToQueueB, SIGNAL(clicked()), this, SLOT(addJob()));
    connect(removeFromQueueB, SIGNAL(clicked()), this, SLOT(removeJob()));
    connect(queueUpB, SIGNAL(clicked()), this, SLOT(moveJobUp()));
    connect(queueDownB, SIGNAL(clicked()), this, SLOT(moveJobDown()));
    connect(selectFITSDirB, SIGNAL(clicked()), this, SLOT(saveFITSDirectory()));
    connect(queueSaveB, SIGNAL(clicked()), this, SLOT(saveSequenceQueue()));
    connect(queueSaveAsB, SIGNAL(clicked()), this, SLOT(saveSequenceQueueAs()));
    connect(queueLoadB, SIGNAL(clicked()), this, SLOT(loadSequenceQueue()));
    connect(resetB, SIGNAL(clicked()), this, SLOT(resetJobs()));
    connect(queueTable, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(editJob(QModelIndex)));
    connect(queueTable, SIGNAL(itemSelectionChanged()), this, SLOT(resetJobEdit()));
    connect(setTemperatureB, SIGNAL(clicked()), this, SLOT(setTemperature()));
    connect(temperatureIN, SIGNAL(editingFinished()), setTemperatureB, SLOT(setFocus()));
    connect(frameTypeCombo, SIGNAL(activated(int)), this, SLOT(checkFrameType(int)));
    connect(resetFrameB, SIGNAL(clicked()), this, SLOT(resetFrame()));

    addToQueueB->setIcon(QIcon::fromTheme("list-add"));
    removeFromQueueB->setIcon(QIcon::fromTheme("list-remove"));
    queueUpB->setIcon(QIcon::fromTheme("go-up"));
    queueDownB->setIcon(QIcon::fromTheme("go-down"));
    selectFITSDirB->setIcon(QIcon::fromTheme("document-open-folder"));
    queueLoadB->setIcon(QIcon::fromTheme("document-open"));
    queueSaveB->setIcon(QIcon::fromTheme("document-save"));
    queueSaveAsB->setIcon(QIcon::fromTheme("document-save-as"));
    resetB->setIcon(QIcon::fromTheme("system-reboot"));
    resetFrameB->setIcon(QIcon::fromTheme("view-refresh"));

    fitsDir->setText(Options::fitsDir());

    seqExpose = 0;
    seqTotalCount = 0;
    seqCurrentCount = 0;
    seqDelay = 0;
    useGuideHead = false;
    guideDither = false;
    firstAutoFocus = true;

    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    // Hide until required
    ADUSeparator->setVisible(false);
    ADULabel->setVisible(false);
    ADUValue->setVisible(false);
    ADUPercentageLabel->setVisible(false);

    displayCheck->setEnabled(Options::showFITS());
    displayCheck->setChecked(Options::showFITS());
    guideDeviationCheck->setChecked(Options::enforceGuideDeviation());
    guideDeviation->setValue(Options::guideDeviation());
    autofocusCheck->setChecked(Options::enforceAutofocus());
    parkCheck->setChecked(Options::autoParkTelescope());
    meridianCheck->setChecked(Options::autoMeridianFlip());
    meridianHours->setValue(Options::autoMeridianHours());
    temperatureCheck->setChecked(Options::enforceTemperatureControl());
    ADUValue->setValue(Options::aDUValue());

    connect(autofocusCheck, SIGNAL(toggled(bool)), this, SLOT(setDirty()));
    connect(HFRPixels, SIGNAL(valueChanged(double)), this, SLOT(setDirty()));
    connect(guideDeviationCheck, SIGNAL(toggled(bool)), this, SLOT(setDirty()));
    connect(guideDeviation, SIGNAL(valueChanged(double)), this, SLOT(setDirty()));
    connect(meridianCheck, SIGNAL(toggled(bool)), this, SLOT(setDirty()));
    connect(meridianHours, SIGNAL(valueChanged(double)), this, SLOT(setDirty()));
    connect(parkCheck, SIGNAL(toggled(bool)), this, SLOT(setDirty()));

}

Capture::~Capture()
{
    qDeleteAll(jobs);
}

void Capture::addCCD(ISD::GDInterface *newCCD, bool isPrimaryCCD)
{
    ISD::CCD *ccd = static_cast<ISD::CCD *> (newCCD);

    CCDCaptureCombo->addItem(ccd->getDeviceName());

    CCDs.append(ccd);

    if (isPrimaryCCD)
    {
        if (Filters.count() > 0)
            syncFilterInfo();
        checkCCD(CCDs.count()-1);
        CCDCaptureCombo->setCurrentIndex(CCDs.count()-1);
    }
    else
    {
        checkCCD(0);
        CCDCaptureCombo->setCurrentIndex(0);
    }
}

void Capture::addGuideHead(ISD::GDInterface *newCCD)
{
    QString guiderName = newCCD->getDeviceName() + QString(" Guider");

    if (CCDCaptureCombo->findText(guiderName) == -1)
    {
        CCDCaptureCombo->addItem(guiderName);
        CCDs.append(static_cast<ISD::CCD *> (newCCD));
    }
}

void Capture::addFilter(ISD::GDInterface *newFilter)
{
    foreach(ISD::GDInterface *filter, Filters)
    {
        if (!strcmp(filter->getDeviceName(), newFilter->getDeviceName()))
            return;
    }

    FilterCaptureCombo->addItem(newFilter->getDeviceName());

    Filters.append(static_cast<ISD::Filter *>(newFilter));

    checkFilter(0);

    FilterCaptureCombo->setCurrentIndex(0);

}

void Capture::startSequence()
{
    if (darkSubCheck->isChecked())
    {
        KMessageBox::error(this, xi18n("Auto dark subtract is not supported in batch mode."));
        return;
    }

    Options::setGuideDeviation(guideDeviation->value());
    Options::setEnforceGuideDeviation(guideDeviationCheck->isChecked());
    Options::setEnforceAutofocus(autofocusCheck->isChecked());
    Options::setAutoMeridianFlip(meridianCheck->isChecked());
    Options::setAutoMeridianHours(meridianHours->value());
    Options::setAutoParkTelescope(parkCheck->isChecked());
    Options::setEnforceTemperatureControl(temperatureCheck->isChecked());
    Options::setADUValue(ADUValue->value());

    if (queueTable->rowCount() ==0)
        addJob();

    SequenceJob *first_job = NULL;

    foreach(SequenceJob *job, jobs)
    {
        if (job->getStatus() == SequenceJob::JOB_IDLE || job->getStatus() == SequenceJob::JOB_ABORTED)
        {
            first_job = job;
            break;
        }
    }

    if (first_job == NULL)
    {
        foreach(SequenceJob *job, jobs)
        {
            if (job->getStatus() != SequenceJob::JOB_DONE)
            {
                appendLogText(xi18n("No pending jobs found. Please add a job to the sequence queue."));
                return;
            }
        }

        if (KMessageBox::warningContinueCancel(NULL, xi18n("All jobs are complete. Do you want to reset the status of all jobs and restart capturing?"),
                                               xi18n("Reset job status"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                                               "reset_job_complete_status_warning") !=KMessageBox::Continue)
            return;

        foreach(SequenceJob *job, jobs)
            job->resetStatus();

        first_job = jobs.first();
    }

    deviationDetected = false;
    spikeDetected     = false;

    initialHA = getCurrentHA();
    meridianFlipStage = MF_NONE;

    prepareJob(first_job);

}

void Capture::stopSequence()
{

    retries              = 0;
    seqTotalCount        = 0;
    seqCurrentCount      = 0;
    ADURaw1 = ADURaw2 = ExpRaw1 = ExpRaw2 = -1;
    ADUSlope = 0;

    if (activeJob)
    {
        if (activeJob->getStatus() == SequenceJob::JOB_BUSY)
        {
            if (Options::playCCDAlarm())
                KSNotify::play(KSNotify::NOTIFY_ERROR);
            activeJob->abort();
        }

        activeJob->reset();
    }

    secondsLabel->clear();
    //currentCCD->disconnect(this);
    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double, IPState)), this, SLOT(updateCaptureProgress(ISD::CCDChip*,double,IPState)));

    currentCCD->setFITSDir("");

    imgProgress->reset();
    imgProgress->setEnabled(false);

    fullImgCountOUT->setText(QString());
    currentImgCountOUT->setText(QString());
    exposeOUT->setText(QString());

    startB->setEnabled(true);
    stopB->setEnabled(false);

    if (displayCheck->isChecked())
        previewB->setEnabled(true);
    else
        previewB->setEnabled(false);

    pi->stopAnimation();
    seqTimer->stop();

}

bool Capture::setCCD(QString device)
{
    for (int i=0; i < CCDCaptureCombo->count(); i++)
        if (device == CCDCaptureCombo->itemText(i))
        {
            checkCCD(i);
            return true;
        }

    return false;
}

void Capture::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
        ccdNum = CCDCaptureCombo->currentIndex();

    foreach(ISD::CCD *ccd, CCDs)
    {
        disconnect(ccd, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processCCDNumber(INumberVectorProperty*)));
        disconnect(ccd, SIGNAL(newTemperatureValue(double)), this, SLOT(updateCCDTemperature(double)));
    }

    if (ccdNum <= CCDs.count())
    {                
        // Check whether main camera or guide head only
        currentCCD = CCDs.at(ccdNum);

        if (CCDCaptureCombo->itemText(ccdNum).right(6) == QString("Guider"))
        {
            useGuideHead = true;
            targetChip = currentCCD->getChip(ISD::CCDChip::GUIDE_CCD);
        }
        else
        {
            currentCCD = CCDs.at(ccdNum);
            targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
            useGuideHead = false;
        }

        if (currentCCD->hasCooler())
        {

            temperatureCheck->setEnabled(true);
            temperatureIN->setEnabled(true);

            if (currentCCD->getBaseDevice()->getPropertyPermission("CCD_TEMPERATURE") != IP_RO)
            {
                double min,max,step;
                setTemperatureB->setEnabled(true);
                temperatureIN->setReadOnly(false);
                currentCCD->getMinMaxStep("CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", &min, &max, &step);
                temperatureIN->setMinimum(min);
                temperatureIN->setMaximum(max);
                temperatureIN->setSingleStep(1);
            }
            else
            {
                setTemperatureB->setEnabled(false);
                temperatureIN->setReadOnly(true);
            }

            double temperature=0;
            if (currentCCD->getTemperature(&temperature))
            {
                temperatureOUT->setText(QString::number(temperature, 'f', 2));
                if (temperatureIN->cleanText().isEmpty())
                    temperatureIN->setValue(temperature);
            }
        }
        else
        {
            temperatureCheck->setEnabled(false);
            temperatureIN->setEnabled(false);
            temperatureIN->clear();
            setTemperatureB->setEnabled(false);
        }
        updateFrameProperties();

        QStringList frameTypes = targetChip->getFrameTypes();

        frameTypeCombo->clear();

        if (frameTypes.isEmpty())
            frameTypeCombo->setEnabled(false);
        else
        {
            frameTypeCombo->setEnabled(true);
            frameTypeCombo->addItems(frameTypes);
            frameTypeCombo->setCurrentIndex(targetChip->getFrameType());
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

        connect(currentCCD, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processCCDNumber(INumberVectorProperty*)), Qt::UniqueConnection);
        connect(currentCCD, SIGNAL(newTemperatureValue(double)), this, SLOT(updateCCDTemperature(double)), Qt::UniqueConnection);
    }
}

void Capture::updateFrameProperties()
{
    int x,y,w,h;
    double min,max,step;
    int xstep=0, ystep=0;

    QString frameProp = useGuideHead ? QString("GUIDER_FRAME") : QString("CCD_FRAME");
    QString exposureProp = useGuideHead ? QString("GUIDER_EXPOSURE") : QString("CCD_EXPOSURE");
    QString exposureElem = useGuideHead ? QString("GUIDER_EXPOSURE_VALUE") : QString("CCD_EXPOSURE_VALUE");
    targetChip = useGuideHead ? currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) : currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    frameWIN->setEnabled(targetChip->canSubframe());
    frameHIN->setEnabled(targetChip->canSubframe());
    frameXIN->setEnabled(targetChip->canSubframe());
    frameYIN->setEnabled(targetChip->canSubframe());

    binXIN->setEnabled(targetChip->canBin());
    binYIN->setEnabled(targetChip->canBin());

    if (currentCCD->getMinMaxStep(exposureProp, exposureElem, &min, &max, &step))
    {
        exposureIN->setMinimum(min);
        exposureIN->setMaximum(max);
        exposureIN->setSingleStep(step);
    }

    if (targetChip->canBin())
    {
        int binx=1,biny=1;
        targetChip->getMaxBin(&binx, &biny);
        binXIN->setMaximum(binx);
        binYIN->setMaximum(biny);
        targetChip->getBinning(&binx, &biny);
        binXIN->setValue(binx);
        binYIN->setValue(biny);
    }
    else
    {
        binXIN->setValue(1);
        binYIN->setValue(1);
    }

    if (currentCCD->getMinMaxStep(frameProp, "WIDTH", &min, &max, &step))
    {
        if (step == 0)
            xstep = (int) max * 0.05;
        else
            xstep = step;

        if (min >= 0 && max > 0)
        {
            frameWIN->setMinimum(min);
            frameWIN->setMaximum(max);
            frameWIN->setSingleStep(xstep);
        }
    }

    if (currentCCD->getMinMaxStep(frameProp, "HEIGHT", &min, &max, &step))
    {
        if (step == 0)
            ystep = (int) max * 0.05;
        else
            ystep = step;

        if (min >= 0 && max > 0)
        {
            frameHIN->setMinimum(min);
            frameHIN->setMaximum(max);
            frameHIN->setSingleStep(ystep);
        }
    }

    if (currentCCD->getMinMaxStep(frameProp, "X", &min, &max, &step))
    {
        if (step == 0)
            step = xstep;

        if (min >= 0 && max > 0)
        {
            frameXIN->setMinimum(min);
            frameXIN->setMaximum(max);
            frameXIN->setSingleStep(step);
        }
    }

    if (currentCCD->getMinMaxStep(frameProp, "Y", &min, &max, &step))
    {
        if (step == 0)
            step = ystep;

        if (min >= 0 && max > 0)
        {
            frameYIN->setMinimum(min);
            frameYIN->setMaximum(max);
            frameYIN->setSingleStep(step);
        }
    }

    if (targetChip->getFrame(&x,&y,&w,&h))
    {
        if (x >= 0)
            frameXIN->setValue(x);
        if (y >= 0)
            frameYIN->setValue(y);
        if (w > 0)
            frameWIN->setValue(w);
        if (h > 0)
            frameHIN->setValue(h);
    }

}

void Capture::processCCDNumber(INumberVectorProperty *nvp)
{
    if (currentCCD && ( (!strcmp(nvp->name, "CCD_FRAME") && useGuideHead == false) || (!strcmp(nvp->name, "GUIDER_FRAME") && useGuideHead)))
        updateFrameProperties();    
}

void Capture::resetFrame()
{
    targetChip = useGuideHead ? currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) : currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    targetChip->resetFrame();
    updateFrameProperties();
}

void Capture::syncFrameType(ISD::GDInterface *ccd)
{
    if (strcmp(ccd->getDeviceName(), CCDCaptureCombo->currentText().toLatin1()))
        return;

    ISD::CCDChip *tChip = NULL;
    tChip = (static_cast<ISD::CCD *> (ccd) )->getChip(ISD::CCDChip::PRIMARY_CCD);

    QStringList frameTypes = tChip->getFrameTypes();

    frameTypeCombo->clear();

    if (frameTypes.isEmpty())
        frameTypeCombo->setEnabled(false);
    else
    {
        frameTypeCombo->setEnabled(true);
        frameTypeCombo->addItems(frameTypes);
        frameTypeCombo->setCurrentIndex(tChip->getFrameType());
    }


}

bool Capture::setFilter(QString device, int filterSlot)
{
    bool deviceFound=false;

    for (int i=0; i < FilterCaptureCombo->count(); i++)
        if (device == FilterCaptureCombo->itemText(i))
        {
            checkFilter(i);
            deviceFound = true;
            break;
        }

    if (deviceFound == false)
        return false;

    if (filterSlot < FilterCaptureCombo->count())
        FilterCaptureCombo->setCurrentIndex(filterSlot);

    return true;
}

void Capture::checkFilter(int filterNum)
{

    if (filterNum == -1)
    {
        filterNum = FilterCaptureCombo->currentIndex();
        if (filterNum == -1)
            return;
    }


    QStringList filterAlias = Options::filterAlias();

    if (filterNum <= Filters.count())
        currentFilter = Filters.at(filterNum);

    syncFilterInfo();

    FilterPosCombo->clear();

    filterName   = currentFilter->getBaseDevice()->getText("FILTER_NAME");
    filterSlot = currentFilter->getBaseDevice()->getNumber("FILTER_SLOT");

    if (filterSlot == NULL)
    {
        KMessageBox::error(0, xi18n("Unable to find FILTER_SLOT property in driver %1", currentFilter->getBaseDevice()->getDeviceName()));
        return;
    }

    for (int i=0; i < filterSlot->np[0].max; i++)
    {
        QString item;

        if (filterName != NULL && (i < filterName->ntp))
            item = filterName->tp[i].text;
        else if (i < filterAlias.count() && filterAlias[i].isEmpty() == false)
            item = filterAlias.at(i);
        else
            item = QString("Filter_%1").arg(i+1);

        FilterPosCombo->addItem(item);

    }

    FilterPosCombo->setCurrentIndex( (int) filterSlot->np[0].value-1);

    currentFilterPosition = (int) filterSlot->np[0].value;

    if (activeJob && (activeJob->getStatus() == SequenceJob::JOB_ABORTED || activeJob->getStatus() == SequenceJob::JOB_IDLE))
        activeJob->setCurrentFilter(currentFilterPosition);

}

void Capture::syncFilterInfo()
{
    if (currentCCD && currentFilter)
    {
        ITextVectorProperty *activeDevices = currentCCD->getBaseDevice()->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            IText *activeFilter = IUFindText(activeDevices, "ACTIVE_FILTER");
            if (activeFilter && strcmp(activeFilter->text, currentFilter->getDeviceName()))
            {
                IUSaveText(activeFilter, currentFilter->getDeviceName());
                currentCCD->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
            }
        }
    }
}

void Capture::startNextExposure()
{
    if (seqDelay > 0)
        secondsLabel->setText(xi18n("Waiting..."));
    seqTimer->start(seqDelay);
}

void Capture::newFITS(IBLOB *bp)
{
    ISD::CCDChip *tChip = NULL;

    // If there is no active job, ignore
    if (activeJob == NULL || meridianFlipStage >= MF_ALIGNING)
        return;

    if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
    {
        if (bp == NULL)
        {
            stopSequence();
            return;
        }

        if (!strcmp(bp->name, "CCD2"))
            tChip = currentCCD->getChip(ISD::CCDChip::GUIDE_CCD);
        else
            tChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

        if (tChip != targetChip)
            return;

        if (targetChip->getCaptureMode() == FITS_FOCUS || targetChip->getCaptureMode() == FITS_GUIDE)
            return;

        // If the FITS is not for our device, simply ignore
        if (QString(bp->bvp->device)  != currentCCD->getDeviceName() || (startB->isEnabled() && previewB->isEnabled()))
            return;

        if (calibrationState == CALIBRATE_START)
        {
            calibrationState = CALIBRATE_DONE;
            startNextExposure();
            return;
        }

        if (darkSubCheck->isChecked() && calibrationState == CALIBRATE_DONE)
        {
            calibrationState = CALIBRATE_NONE;

            FITSView *calibrateImage = targetChip->getImage(FITS_CALIBRATE);
            FITSView *currentImage   = targetChip->getImage(FITS_NORMAL);

            FITSData *image_data = NULL;

            if (currentImage)
                image_data = currentImage->getImageData();

            if (image_data && calibrateImage && currentImage)
                image_data->subtract(calibrateImage->getImageData()->getImageBuffer());
        }
    }

    secondsLabel->setText(xi18n("Complete."));

    if (seqTotalCount <= 0)
    {
       jobs.removeOne(activeJob);
       delete (activeJob);
       activeJob = NULL;
       stopSequence();
       return;
    }

    // Check if we need to do flat field slope calculation if the user specified a desired ADU value
    if (activeJob->getTargetADU() > 0 && activeJob->getFrameType() == FRAME_FLAT)
    {
        FITSData *image_data = NULL;
        FITSView *currentImage   = targetChip->getImage(FITS_NORMAL);
        if (currentImage)
        {
            image_data = currentImage->getImageData();
            double currentADU = image_data->getADUPercentage();
            double currentSlope = ADUSlope;

            double nextExposure = setCurrentADU(currentADU);

            if (nextExposure <= 0)
            {
                appendLogText(xi18n("Unable to calculate optimal exposure settings, please take the flats manually."));
                activeJob->setTargetADU(0);
                ADUValue->setValue(0);
                stopSequence();
                return;
            }

            appendLogText(xi18n("Current ADU is %1% Next exposure is %2 seconds.", QString::number(currentADU, 'g', 2), QString::number(nextExposure, 'g', 2)));

            activeJob->setExposure(nextExposure);

            // Start next exposure in case ADU Slope is not calculated yet
            if (currentSlope == 0)
            {
                startNextExposure();
                return;
            }
        }
        else
        {
            appendLogText(xi18n("An empty image is received, aborting..."));
            stopSequence();
            return;
        }
    }

    seqCurrentCount++;
    activeJob->setCompleted(seqCurrentCount);
    imgProgress->setValue(seqCurrentCount);

    appendLogText(xi18n("Received image %1 out of %2.", seqCurrentCount, seqTotalCount));

    currentImgCountOUT->setText( QString::number(seqCurrentCount));

    // if we're done
    if (seqCurrentCount == seqTotalCount)
    {
        activeJob->done();

        stopSequence();

        // Check if meridian condition is met
        if (checkMeridianFlip())
            return;

        // Check if there are more pending jobs and execute them
        if (resumeSequence())
            return;
        // Otherwise, we're done. We park if required and resume guiding if no parking is done and autoguiding was engaged before.
        else
        {
            if (Options::playCCDAlarm())
                    KSNotify::play(KSNotify::NOTIFY_OK);

            if (parkCheck->isChecked() && currentTelescope && currentTelescope->canPark())
            {
                appendLogText(xi18n("Parking telescope..."));
                emit telescopeParking();
                currentTelescope->Park();
                return;
            }

            //Resume guiding if it was suspended before
            if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
                emit suspendGuiding(false);
        }

        return;
    }

    // Check if meridian condition is met
    if (checkMeridianFlip())
        return;

    resumeSequence();
}

bool Capture::resumeSequence()
{
    // If seqTotalCount is zero, we have to find if there are more pending jobs in the queue
    if (seqTotalCount == 0)
    {
        SequenceJob *next_job = NULL;

        foreach(SequenceJob *job, jobs)
        {
            if (job->getStatus() == SequenceJob::JOB_IDLE || job->getStatus() == SequenceJob::JOB_ABORTED)
            {
                next_job = job;
                break;
            }
        }

        if (next_job)
        {
            prepareJob(next_job);

            //Resume guiding if it was suspended before
            if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
                emit suspendGuiding(false);

            return true;
        }
        else
            return false;
    }
    // Otherwise, let's prepare for next exposure after making sure in-sequence focus and dithering are complete if applicable.
    else
    {
        isAutoFocus = (autofocusCheck->isEnabled() && autofocusCheck->isChecked() && HFRPixels->value() > 0);
        if (isAutoFocus)
            autoFocusStatus = false;

        if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
            emit suspendGuiding(false);

        if (isAutoGuiding && guideDither)
        {
                secondsLabel->setText(i18n("Dithering..."));
                emit exposureComplete();
        }
        else if (isAutoFocus && activeJob->getFrameType() == FRAME_LIGHT)
        {
            secondsLabel->setText(xi18n("Focusing..."));
            emit checkFocus(HFRPixels->value());
        }
        else
            startNextExposure();

        return true;
    }
}

void Capture::captureOne()
{
    if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
    {
        appendLogText(xi18n("Cannot take preview image while CCD upload mode is set to local. Please change upload mode to client and try again."));
        return;
    }

    addJob(true);

    prepareJob(jobs.last());
}

void Capture::captureImage()
{
    seqTimer->stop();
    bool isDark=false;
    SequenceJob::CAPTUREResult rc=SequenceJob::CAPTURE_OK;

    if (useGuideHead == false && darkSubCheck->isChecked() && calibrationState == CALIBRATE_NONE)
        isDark = true;


    if (filterSlot != NULL)
    {
        currentFilterPosition = (int) filterSlot->np[0].value;
        activeJob->setCurrentFilter(currentFilterPosition);
    }

    if (currentCCD->hasCooler())
    {
        double temperature=0;
        currentCCD->getTemperature(&temperature);
        activeJob->setCurrentTemperature(temperature);
    }

     rc = activeJob->capture(isDark);

     switch (rc)
     {
        case SequenceJob::CAPTURE_OK:
         if (isDark)
         {
            calibrationState = CALIBRATE_START;
            appendLogText(xi18n("Capturing dark frame..."));
         }
         else
           appendLogText(xi18n("Capturing image..."));
         break;

        case SequenceJob::CAPTURE_FRAME_ERROR:
            appendLogText(xi18n("Failed to set sub frame."));
            stopSequence();
            break;

        case SequenceJob::CAPTURE_BIN_ERROR:
            appendLogText(xi18n("Failed to set binning."));
            stopSequence();
            break;
     }


}

void Capture::resumeCapture()
{
    appendLogText(xi18n("Dither complete."));

    if (isAutoFocus && autoFocusStatus == false)
    {
        secondsLabel->setText(xi18n("Focusing..."));
        emit checkFocus(HFRPixels->value());
        return;
    }

    startNextExposure();
}

/*******************************************************************************/
/* Update the prefix for the sequence of images to be captured                 */
/*******************************************************************************/
void Capture::updateSequencePrefix( const QString &newPrefix, const QString &dir)
{
    seqPrefix = newPrefix;

    //seqLister->setNameFilter(QString("*.fits"));
    seqWatcher->addDir(dir, KDirWatch::WatchFiles);

    seqCount = 1;

    //seqLister->openUrl(dir);

    checkSeqBoundary(dir);

}

/*******************************************************************************/
/* Determine the next file number sequence. That is, if we have file1.png      */
/* and file2.png, then the next sequence should be file3.png		       */
/*******************************************************************************/
void Capture::checkSeqBoundary(const QString &path)
{
    int newFileIndex=-1;
    QString tempName;

    // No updates during meridian flip
    if (meridianFlipStage >= MF_ALIGNING)
        return;

    //KFileItemList::const_iterator it = items.begin();
    //const KFileItemList::const_iterator end = items.end();
    QDirIterator it(path, QDir::Files);
    while (it.hasNext())
        {
            tempName = it.next();
            tempName.remove(path + "/");

            // find the prefix first
            //if (tempName.startsWith(seqPrefix) == false || tempName.endsWith(".fits") == false)
            if (seqPrefix.isEmpty() || tempName.startsWith(seqPrefix) == false)
                continue;

            tempName = tempName.remove(seqPrefix);
            if (tempName.startsWith("_"))
                tempName = tempName.remove(0, 1);

            bool indexOK = false;
            newFileIndex = tempName.mid(0, 3).toInt(&indexOK);
            if (indexOK && newFileIndex >= seqCount)
                seqCount = newFileIndex + 1;
        }

    currentCCD->setSeqCount(seqCount);

}

void Capture::checkPreview(bool enable)
{
    if (enable && currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
    {
        appendLogText(i18n("Cannot enable preview while CCD upload mode is set to local. Change upload mode to client or both and try again."));
        displayCheck->disconnect(this);
        displayCheck->setChecked(false);
        connect(displayCheck, SIGNAL(toggled(bool)), this, SLOT(checkPreview(bool)));
        return;
    }

    previewB->setEnabled(enable);
}

void Capture::appendLogText(const QString &text)
{

    logText.insert(0, xi18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    emit newLog();
}

void Capture::clearLog()
{
    logText.clear();
    emit newLog();
}

void Capture::updateCaptureProgress(ISD::CCDChip * tChip, double value, IPState state)
{

    if (targetChip != tChip || targetChip->getCaptureMode() != FITS_NORMAL || meridianFlipStage >= MF_ALIGNING)
        return;

    exposeOUT->setText(QString::number(value, 'd', 2));

    if (activeJob)
        activeJob->setExposeLeft(value);

    if (activeJob && state == IPS_ALERT)
    {
        int retries = activeJob->getCaptureRetires()+1;

        activeJob->setCaptureRetires(retries);

        appendLogText(xi18n("Capture failed."));
        if (retries == 3)
        {
            stopSequence();
            return;
        }

        appendLogText(xi18n("Restarting capture attempt #%1", retries));
        captureImage();
        return;
    }

    if (value == 0)
    {
        activeJob->setCaptureRetires(0);

        if (currentCCD && currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
        {
            if (activeJob && activeJob->getStatus() == SequenceJob::JOB_BUSY && activeJob->getExposeLeft() == 0 && state == IPS_OK)
            {
               newFITS(0);
               return;
            }
        }

        if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
            emit suspendGuiding(true);

            secondsLabel->setText(i18n("Downloading..."));
    }
    // JM: Don't change to i18np, value is DOUBLE, not Integer.
    else if (value <= 1)
        secondsLabel->setText(xi18n("second left"));
    else
        secondsLabel->setText(xi18n("seconds left"));
}

void Capture::updateCCDTemperature(double value)
{    
    if (temperatureCheck->isEnabled() == false)
        checkCCD();

    temperatureOUT->setText(QString::number(value, 'f', 2));

    if (activeJob && (activeJob->getStatus() == SequenceJob::JOB_ABORTED || activeJob->getStatus() == SequenceJob::JOB_IDLE))
        activeJob->setCurrentTemperature(value);
}

void Capture::addJob(bool preview)
{
    SequenceJob *job = NULL;
    QString imagePrefix;

    Options::setEnforceTemperatureControl(temperatureCheck->isChecked());

    if (preview == false && darkSubCheck->isChecked())
    {
        KMessageBox::error(this, xi18n("Auto dark subtract is not supported in batch mode."));
        return;
    }

    if (jobUnderEdit)
        job = jobs.at(queueTable->currentRow());
    else
        job = new SequenceJob();

    if (job == NULL)
    {
        qWarning() << "Job is NULL!" << endl;
        return;
    }

    if (ISOCheck->isChecked())
        job->setISOMode(true);
    else
        job->setISOMode(false);

    if (ISOCombo->isEnabled())
        job->setISOIndex(ISOCombo->currentIndex());

    job->setPreview(preview);

    if (temperatureIN->isEnabled())
    {
        double currentTemperature;
        currentCCD->getTemperature(&currentTemperature);
        job->setTargetTemperature(temperatureIN->value());
        job->setCurrentTemperature(currentTemperature);
    }

    job->setCaptureFilter((FITSScale)  filterCombo->currentIndex());

    job->setFITSDir(fitsDir->text());

    job->setShowFITS(displayCheck->isChecked());

    job->setTargetADU(ADUValue->value());

    imagePrefix = prefixIN->text();        

    constructPrefix(imagePrefix);

    job->setPrefixSettings(prefixIN->text(), frameTypeCheck->isChecked(), filterCheck->isChecked(), expDurationCheck->isChecked());
    job->setFrameType(frameTypeCombo->currentIndex(), frameTypeCombo->currentText());
    job->setPrefix(imagePrefix);

    if (filterSlot != NULL && currentFilter != NULL)
       job->setTargetFilter(FilterPosCombo->currentIndex()+1, FilterPosCombo->currentText());

    job->setExposure(exposureIN->value());

    job->setCount(countIN->value());

    job->setBin(binXIN->value(), binYIN->value());

    job->setDelay(delayIN->value() * 1000);		/* in ms */

    job->setActiveChip(targetChip);
    job->setActiveCCD(currentCCD);
    job->setActiveFilter(currentFilter);

    job->setFrame(frameXIN->value(), frameYIN->value(), frameWIN->value(), frameHIN->value());

    if (jobUnderEdit == false)
        jobs.append(job);

    // Nothing more to do if preview
    if (preview)
        return;

    int currentRow = 0;
    if (jobUnderEdit == false)
    {

        currentRow = queueTable->rowCount();
        queueTable->insertRow(currentRow);
    }
    else
        currentRow = queueTable->currentRow();

    QTableWidgetItem *status = jobUnderEdit ? queueTable->item(currentRow, 0) : new QTableWidgetItem();
    status->setText(job->getStatusString());
    status->setTextAlignment(Qt::AlignHCenter);
    status->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    job->setStatusCell(status);

    QTableWidgetItem *filter = jobUnderEdit ? queueTable->item(currentRow, 1) : new QTableWidgetItem();
    filter->setText("--");
    if (frameTypeCombo->currentText().compare("Bias", Qt::CaseInsensitive) &&
            frameTypeCombo->currentText().compare("Dark", Qt::CaseInsensitive) &&
            FilterPosCombo->count() > 0)
        filter->setText(FilterPosCombo->currentText());

    filter->setTextAlignment(Qt::AlignHCenter);
    filter->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *type = jobUnderEdit ? queueTable->item(currentRow, 2) : new QTableWidgetItem();
    type->setText(frameTypeCombo->currentText());
    type->setTextAlignment(Qt::AlignHCenter);
    type->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *bin = jobUnderEdit ? queueTable->item(currentRow, 3) : new QTableWidgetItem();
    bin->setText(QString("%1x%2").arg(binXIN->value()).arg(binYIN->value()));
    bin->setTextAlignment(Qt::AlignHCenter);
    bin->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *exp = jobUnderEdit ? queueTable->item(currentRow, 4) : new QTableWidgetItem();
    exp->setText(QString::number(exposureIN->value()));
    exp->setTextAlignment(Qt::AlignHCenter);
    exp->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *iso = jobUnderEdit ? queueTable->item(currentRow, 5) : new QTableWidgetItem();
    if (ISOCombo->currentIndex() != -1)
        iso->setText(ISOCombo->currentText());
    else
        iso->setText("--");
    iso->setTextAlignment(Qt::AlignHCenter);
    iso->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);


    QTableWidgetItem *count = jobUnderEdit ? queueTable->item(currentRow, 6) : new QTableWidgetItem();
    count->setText(QString::number(countIN->value()));
    count->setTextAlignment(Qt::AlignHCenter);
    count->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    if (jobUnderEdit == false)
    {
        queueTable->setItem(currentRow, 0, status);
        queueTable->setItem(currentRow, 1, filter);
        queueTable->setItem(currentRow, 2, type);
        queueTable->setItem(currentRow, 3, bin);
        queueTable->setItem(currentRow, 4, exp);
        queueTable->setItem(currentRow, 5, iso);
        queueTable->setItem(currentRow, 6, count);
    }

    removeFromQueueB->setEnabled(true);

    if (queueTable->rowCount() > 0)
    {
        queueSaveAsB->setEnabled(true);
        queueSaveB->setEnabled(true);
        resetB->setEnabled(true);
        mDirty = true;
    }

    if (queueTable->rowCount() > 1)
    {
        queueUpB->setEnabled(true);
        queueDownB->setEnabled(true);
    }

    if (jobUnderEdit)
    {
        jobUnderEdit = false;
        resetJobEdit();
        appendLogText(xi18n("Job #%1 changes applied.", currentRow+1));
    }

}

void Capture::removeJob()
{
    int currentRow = queueTable->currentRow();

    if (currentRow < 0)
    {
        currentRow = queueTable->rowCount()-1;
        if (currentRow < 0)
            return;
    }

    queueTable->removeRow(currentRow);

    SequenceJob *job = jobs.at(currentRow);
    jobs.removeOne(job);
    delete (job);

    if (queueTable->rowCount() == 0)
        removeFromQueueB->setEnabled(false);

    if (queueTable->rowCount() == 1)
    {
        queueUpB->setEnabled(false);
        queueDownB->setEnabled(false);
    }

    for (int i=0; i < jobs.count(); i++)
        jobs.at(i)->setStatusCell(queueTable->item(i, 0));

    queueTable->selectRow(queueTable->currentRow());

    if (queueTable->rowCount() == 0)
    {
        queueSaveAsB->setEnabled(false);
        queueSaveB->setEnabled(false);
        resetB->setEnabled(false);
    }

    mDirty = true;


}

void Capture::moveJobUp()
{
    int currentRow = queueTable->currentRow();

    int columnCount = queueTable->columnCount();

    if (currentRow <= 0 || queueTable->rowCount() == 1)
        return;

    int destinationRow = currentRow - 1;

    for (int i=0; i < columnCount; i++)
    {
        QTableWidgetItem *downItem = queueTable->takeItem(currentRow, i);
        QTableWidgetItem *upItem   = queueTable->takeItem(destinationRow, i);

        queueTable->setItem(destinationRow, i, downItem);
        queueTable->setItem(currentRow, i, upItem);
    }

    SequenceJob *job = jobs.takeAt(currentRow);

    jobs.removeOne(job);
    jobs.insert(destinationRow, job);

    queueTable->selectRow(destinationRow);

    for (int i=0; i < jobs.count(); i++)
      jobs.at(i)->setStatusCell(queueTable->item(i, 0));


    mDirty = true;

}

void Capture::moveJobDown()
{
    int currentRow = queueTable->currentRow();

    int columnCount = queueTable->columnCount();

    if (currentRow < 0 || queueTable->rowCount() == 1 || (currentRow+1) == queueTable->rowCount() )
        return;

    int destinationRow = currentRow + 1;

    for (int i=0; i < columnCount; i++)
    {
        QTableWidgetItem *downItem = queueTable->takeItem(currentRow, i);
        QTableWidgetItem *upItem   = queueTable->takeItem(destinationRow, i);

        queueTable->setItem(destinationRow, i, downItem);
        queueTable->setItem(currentRow, i, upItem);
    }

    SequenceJob *job = jobs.takeAt(currentRow);

    jobs.removeOne(job);
    jobs.insert(destinationRow, job);

    queueTable->selectRow(destinationRow);

    for (int i=0; i < jobs.count(); i++)
        jobs.at(i)->setStatusCell(queueTable->item(i, 0));

    mDirty = true;

}

void Capture::prepareJob(SequenceJob *job)
{
    activeJob = job;

    if (currentFilterPosition > 0)
    {
        activeJob->setCurrentFilter(currentFilterPosition);

        if (currentFilterPosition != activeJob->getTargetFilter())
        {
            appendLogText(xi18n("Changing filter to %1...", FilterPosCombo->itemText(activeJob->getTargetFilter()-1)));
            secondsLabel->setText(xi18n("Set filter..."));
            pi->startAnimation();
            previewB->setEnabled(false);
            startB->setEnabled(false);
            stopB->setEnabled(true);
        }
    }

    if (currentCCD->hasCooler() && temperatureCheck->isChecked())
    {
        if (activeJob->getCurrentTemperature() != INVALID_TEMPERATURE &&
                fabs(activeJob->getCurrentTemperature() - activeJob->getTargetTemperature()) > MAX_TEMP_DIFF)
        {
            appendLogText(xi18n("Setting temperature to %1 C...", activeJob->getTargetTemperature()));
            secondsLabel->setText(xi18n("Set %1 C...", activeJob->getTargetTemperature()));
            pi->startAnimation();
            previewB->setEnabled(false);
            startB->setEnabled(false);
            stopB->setEnabled(true);
        }
    }

    connect(activeJob, SIGNAL(prepareComplete()), this, SLOT(executeJob()));

    job->prepareCapture();
}

void Capture::executeJob()
{
    activeJob->disconnect(this);

    if (activeJob->isPreview())
        seqTotalCount = -1;
    else
        seqTotalCount = activeJob->getCount();

    seqDelay = activeJob->getDelay();

    seqCurrentCount = activeJob->getCompleted();

    if (activeJob->isPreview() == false)
    {
        fullImgCountOUT->setText( QString::number(seqTotalCount));
        currentImgCountOUT->setText(QString::number(seqCurrentCount));

        // set the progress info
        imgProgress->setEnabled(true);
        imgProgress->setMaximum(seqTotalCount);
        imgProgress->setValue(seqCurrentCount);

        if (currentCCD->getUploadMode() != ISD::CCD::UPLOAD_LOCAL)
            updateSequencePrefix(activeJob->getPrefix(), activeJob->getFITSDir());
    }

    // Update button status
    startB->setEnabled(false);
    stopB->setEnabled(true);
    previewB->setEnabled(false);

    pi->startAnimation();

    useGuideHead = (activeJob->getActiveChip()->getType() == ISD::CCDChip::PRIMARY_CCD) ? false : true;

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    connect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(updateCaptureProgress(ISD::CCDChip*,double,IPState)));

    captureImage();

}

void Capture::enableGuideLimits()
{
    guideDeviationCheck->setEnabled(true);
    guideDeviation->setEnabled(true);
}

void Capture::setGuideDeviation(double delta_ra, double delta_dec)
{
    // If guiding is started after a meridian flip we will start getting guide deviations again
    // if the guide deviations are within our limits, we resume the sequence
    if (meridianFlipStage == MF_GUIDING)
    {
        double deviation_rms = sqrt(delta_ra*delta_ra + delta_dec*delta_dec);
        if (deviation_rms < guideDeviation->value())
        {
                initialHA = getCurrentHA();
                meridianFlipStage = MF_NONE;
                appendLogText(xi18n("Post meridian flip calibration completed successfully."));
                resumeSequence();
        }
    }
    else if (guideDeviationCheck->isChecked() == false || activeJob == NULL)
        return;

    // We don't enforce limit on previews
    if (activeJob->isPreview() || activeJob->getExposeLeft() == 0)
        return;

    double deviation_rms = sqrt(delta_ra*delta_ra + delta_dec*delta_dec);

    QString deviationText = QString("%1").arg(deviation_rms, 0, 'g', 3);

    if (activeJob->getStatus() == SequenceJob::JOB_BUSY)
    {
        if (deviation_rms > guideDeviation->value())
        {
            // Ignore spikes ONCE
            if (spikeDetected == false)
            {
                spikeDetected = true;
                return;
            }

            spikeDetected = false;
            deviationDetected = true;
            appendLogText(xi18n("Guiding deviation %1 exceeded limit value of %2 arcsecs, aborting exposure.", deviationText, guideDeviation->value()));
            stopSequence();
        }
        return;
    }

    if (activeJob->getStatus() == SequenceJob::JOB_ABORTED && deviationDetected)
    {
        if (deviation_rms <= guideDeviation->value())
        {
            deviationDetected = false;
            appendLogText(xi18n("Guiding deviation %1 is now lower than limit value of %2 arcsecs, resuming exposure.", deviationText, guideDeviation->value()));
            startSequence();
            return;
        }
    }

}

void Capture::setGuideDither(bool enable)
{
    guideDither = enable;
}

void Capture::setAutoguiding(bool enable, bool isDithering)
{  
    isAutoGuiding = enable;
    guideDither   = isDithering;
}

void Capture::updateAutofocusStatus(bool status, double HFR)
{
    autoFocusStatus = status;

    if (status)
    {
        autofocusCheck->setEnabled(true);
        HFRPixels->setEnabled(true);
        if (HFR > 0 && firstAutoFocus && HFRPixels->value() == 0)
        {
           firstAutoFocus = false;
           HFRPixels->setValue(HFR);
        }
    }

    if (isAutoFocus && activeJob && activeJob->getStatus() == SequenceJob::JOB_BUSY)
    {
        if (status)
            startNextExposure();
        else
        {
            appendLogText(xi18n("Autofocus failed. Aborting exposure..."));
            secondsLabel->setText("");
            stopSequence();
        }
    }
}

void Capture::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = static_cast<ISD::Telescope*> (newTelescope);

    connect(currentTelescope, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processTelescopeNumber(INumberVectorProperty*)), Qt::UniqueConnection);

    meridianCheck->setEnabled(true);
    meridianHours->setEnabled(true);

    syncTelescopeInfo();
}

void Capture::syncTelescopeInfo()
{
    if (currentCCD && currentTelescope && currentTelescope->isConnected())
    {
        parkCheck->setEnabled(currentTelescope->canPark());

        ITextVectorProperty *activeDevices = currentCCD->getBaseDevice()->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            IText *activeTelescope = IUFindText(activeDevices, "ACTIVE_TELESCOPE");
            if (activeTelescope)
            {
                IUSaveText(activeTelescope, currentTelescope->getDeviceName());

                currentCCD->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
            }
        }
    }
}

void Capture::saveFITSDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(KStars::Instance(), xi18n("FITS Save Directory"), fitsDir->text());

    if (!dir.isEmpty())
        fitsDir->setText(dir);
}

void Capture::loadSequenceQueue()
{
    QUrl fileURL = QFileDialog::getOpenFileName(KStars::Instance(), xi18n("Open Ekos Sequence Queue"), "", "Ekos Sequence Queue (*.esq)");
    if (fileURL.isEmpty())
        return;

    if (fileURL.isValid() == false)
    {
       QString message = xi18n( "Invalid URL: %1", fileURL.path() );
       KMessageBox::sorry( 0, message, xi18n( "Invalid URL" ) );
       return;
    }

    loadSequenceQueue(fileURL);
}

bool Capture::loadSequenceQueue(const QUrl &fileURL)
{
    QFile sFile;
    sFile.setFileName(fileURL.path());

    if ( !sFile.open( QIODevice::ReadOnly))
    {
        QString message = xi18n( "Unable to open file %1",  fileURL.path());
        KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
        return false;
    }

    //QTextStream instream(&sFile);

    qDeleteAll(jobs);
    jobs.clear();
    for (int i=0; i < queueTable->rowCount(); i++)
        queueTable->removeRow(i);

    LilXML *xmlParser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = NULL;
    XMLEle *ep;
    char c;

    while ( sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
             for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
             {
                 if (!strcmp(tagXMLEle(ep), "GuideDeviation"))
                 {
                      if (!strcmp(findXMLAttValu(ep, "enabled"), "true"))
                      {
                         guideDeviationCheck->setChecked(true);
                         guideDeviation->setValue(atof(pcdataXMLEle(ep)));
                      }
                     else
                         guideDeviationCheck->setChecked(false);

                 }
                 else if (!strcmp(tagXMLEle(ep), "Autofocus"))
                 {
                      if (!strcmp(findXMLAttValu(ep, "enabled"), "true"))
                      {
                         autofocusCheck->setChecked(true);
                         HFRPixels->setValue(atof(pcdataXMLEle(ep)));
                      }
                     else
                         autofocusCheck->setChecked(false);

                 }
                 else if (!strcmp(tagXMLEle(ep), "MeridianFlip"))
                 {
                      if (!strcmp(findXMLAttValu(ep, "enabled"), "true"))
                      {
                         meridianCheck->setChecked(true);
                         meridianHours->setValue(atof(pcdataXMLEle(ep)));
                      }
                     else
                         meridianCheck->setChecked(false);

                 }
                 else if (!strcmp(tagXMLEle(ep), "Park"))
                 {
                      if (!strcmp(findXMLAttValu(ep, "enabled"), "true"))
                         parkCheck->setChecked(true);
                     else
                         parkCheck->setChecked(false);

                 }
                 else
                 {
                     processJobInfo(ep);
                 }

             }
             delXMLEle(root);
        }
        else if (errmsg[0])
        {
            appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            return false;
        }
    }

    sequenceURL = fileURL;
    mDirty = false;
    delLilXML(xmlParser);
    return true;

}

bool Capture::processJobInfo(XMLEle *root)
{

    XMLEle *ep;
    XMLEle *subEP;

    for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Exposure"))
            exposureIN->setValue(atof(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Binning"))
        {
            subEP = findXMLEle(ep, "X");
            if (subEP)
                binXIN->setValue(atoi(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "Y");
            if (subEP)
                binYIN->setValue(atoi(pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Frame"))
        {
            subEP = findXMLEle(ep, "X");
            if (subEP)
                frameXIN->setValue(atoi(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "Y");
            if (subEP)
                frameYIN->setValue(atoi(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "W");
            if (subEP)
                frameWIN->setValue(atoi(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "H");
            if (subEP)
                frameHIN->setValue(atoi(pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Temperature"))
        {
            if (temperatureIN->isEnabled())
                temperatureIN->setValue(atof(pcdataXMLEle(ep))-1);

            // If force attribute exist, we change temperatureCheck, otherwise do nothing.
            if (!strcmp(findXMLAttValu(ep, "force"), "true"))
                temperatureCheck->setChecked(true);
            else if (!strcmp(findXMLAttValu(ep, "force"), "false"))
                temperatureCheck->setChecked(false);

        }
        else if (!strcmp(tagXMLEle(ep), "Filter"))
        {
            FilterPosCombo->setCurrentIndex(atoi(pcdataXMLEle(ep))-1);
        }
        else if (!strcmp(tagXMLEle(ep), "Type"))
        {
            frameTypeCombo->setCurrentIndex(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Prefix"))
        {
            subEP = findXMLEle(ep, "RawPrefix");
            if (subEP)
                prefixIN->setText(pcdataXMLEle(subEP));
            subEP = findXMLEle(ep, "TypeEnabled");
            if (subEP)
                frameTypeCheck->setChecked( !strcmp("1", pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "FilterEnabled");
            if (subEP)
                filterCheck->setChecked( !strcmp("1", pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "ExpEnabled");
            if (subEP)
                expDurationCheck->setChecked( !strcmp("1", pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Count"))
        {
            countIN->setValue(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Delay"))
        {
            delayIN->setValue(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "ADU"))
        {
            ADUValue->setValue(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
        {
            fitsDir->setText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "ISOMode"))
        {
            ISOCheck->setChecked( !strcmp("1", pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "ISOIndex"))
        {
            if (ISOCombo->isEnabled())
                ISOCombo->setCurrentIndex(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "ShowFITS"))
        {
            displayCheck->setChecked( !strcmp("1", pcdataXMLEle(ep)));
        }

    }

    addJob(false);

    return true;
}

void Capture::saveSequenceQueue()
{
    QUrl backupCurrent = sequenceURL;

    if (sequenceURL.path().contains("/tmp/"))
        sequenceURL.clear();

    // If no changes made, return.
    if( mDirty == false && !sequenceURL.isEmpty())
        return;

    if (sequenceURL.isEmpty())
    {
        sequenceURL = QFileDialog::getSaveFileName(KStars::Instance(), xi18n("Save Ekos Sequence Queue"), "", "Ekos Sequence Queue (*.esq)");
        // if user presses cancel
        if (sequenceURL.isEmpty())
        {
            sequenceURL = backupCurrent;
            return;
        }

        if (sequenceURL.path().contains('.') == 0)
            sequenceURL.setPath(sequenceURL.path() + ".esq");

        if (QFile::exists(sequenceURL.path()))
        {
            int r = KMessageBox::warningContinueCancel(0,
                        xi18n( "A file named \"%1\" already exists. "
                              "Overwrite it?", sequenceURL.fileName() ),
                        xi18n( "Overwrite File?" ),
                        KGuiItem(xi18n( "&Overwrite" )) );
            if(r==KMessageBox::Cancel) return;
        }
    }

    if ( sequenceURL.isValid() )
    {
        if ( (saveSequenceQueue(sequenceURL.path())) == false)
        {
            KMessageBox::error(KStars::Instance(), xi18n("Failed to save sequence queue"), xi18n("FITS Save"));
            return;
        }

        mDirty = false;

    } else
    {
        QString message = xi18n( "Invalid URL: %1", sequenceURL.url() );
        KMessageBox::sorry(KStars::Instance(), message, xi18n( "Invalid URL" ) );
    }

}

void Capture::saveSequenceQueueAs()
{
    sequenceURL.clear();
    saveSequenceQueue();
}

bool Capture::saveSequenceQueue(const QString &path)
{
    QFile file;
    QString rawPrefix;
    bool typeEnabled, filterEnabled, expEnabled;

    file.setFileName(path);

    if ( !file.open( QIODevice::WriteOnly))
    {
        QString message = xi18n( "Unable to write to file %1",  path);
        KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
        return false;
    }

    QTextStream outstream(&file);

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    outstream << "<SequenceQueue version='1.1'>" << endl;
    outstream << "<GuideDeviation enabled='" << (guideDeviationCheck->isChecked() ? "true" : "false") << "'>" << guideDeviation->value() << "</GuideDeviation>" << endl;
    outstream << "<Autofocus enabled='" << (autofocusCheck->isChecked() ? "true" : "false") << "'>" << HFRPixels->value() << "</Autofocus>" << endl;
    outstream << "<MeridianFlip enabled='" << (meridianCheck->isChecked() ? "true" : "false") << "'>" << meridianHours->value() << "</MeridianFlip>" << endl;
    outstream << "<Park enabled='" << (parkCheck->isChecked() ? "true" : "false") << "'></Park>" << endl;
    foreach(SequenceJob *job, jobs)
    {
        job->getPrefixSettings(rawPrefix, typeEnabled, filterEnabled, expEnabled);

         outstream << "<Job>" << endl;

         outstream << "<Exposure>" << job->getExposure() << "</Exposure>" << endl;
         outstream << "<Binning>" << endl;
            outstream << "<X>"<< job->getXBin() << "</X>" << endl;
            outstream << "<Y>"<< job->getXBin() << "</Y>" << endl;
         outstream << "</Binning>" << endl;
         outstream << "<Frame>" << endl;
            outstream << "<X>" << job->getSubX() << "</X>" << endl;
            outstream << "<Y>" << job->getSubY() << "</Y>" << endl;
            outstream << "<W>" << job->getSubW() << "</W>" << endl;
            outstream << "<H>" << job->getSubH() << "</H>" << endl;
        outstream << "</Frame>" << endl;
        if (job->getTargetTemperature() != INVALID_TEMPERATURE)
            outstream << "<Temperature force='" << (temperatureCheck->isChecked() ? "true":"false") << "'>" << job->getTargetTemperature() << "</Temperature>" << endl;
        outstream << "<Filter>" << job->getTargetFilter() << "</Filter>" << endl;
        outstream << "<Type>" << job->getFrameType() << "</Type>" << endl;
        outstream << "<Prefix>" << endl;
            //outstream << "<CompletePrefix>" << job->getPrefix() << "</CompletePrefix>" << endl;
            outstream << "<RawPrefix>" << rawPrefix << "</RawPrefix>" << endl;
            outstream << "<TypeEnabled>" << (typeEnabled ? 1 : 0) << "</TypeEnabled>" << endl;
            outstream << "<FilterEnabled>" << (filterEnabled ? 1 : 0) << "</FilterEnabled>" << endl;
            outstream << "<ExpEnabled>" << (expEnabled ? 1 : 0) << "</ExpEnabled>" << endl;
        outstream << "</Prefix>" << endl;
        outstream << "<Count>" << job->getCount() << "</Count>" << endl;
        // ms to seconds
        outstream << "<Delay>" << job->getDelay()/1000 << "</Delay>" << endl;
        if (job->getTargetADU() > 0)
            outstream << "<ADU>" << job->getTargetADU() << "</ADU>" << endl;

        outstream << "<FITSDirectory>" << job->getFITSDir() << "</FITSDirectory>" << endl;
        outstream << "<ISOMode>" << (job->getISOMode() ? 1 : 0) << "</ISOMode>" << endl;
        if (job->getISOIndex() != -1)
            outstream << "<ISOIndex>" << (job->getISOIndex()) << "</ISOIndex>" << endl;
        outstream << "<ShowFITS>" << (job->isShowFITS() ? 1 : 0) << "</ShowFITS>" << endl;

        outstream << "</Job>" << endl;
    }

    outstream << "</SequenceQueue>" << endl;

    appendLogText(xi18n("Sequence queue saved to %1", path));
    file.close();
   return true;
}

void Capture::resetJobs()
{
    if (KMessageBox::warningContinueCancel(NULL, xi18n("Are you sure you want to reset status of all jobs?"),
                                           xi18n("Reset job status"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                                           "reset_job_status_warning") !=KMessageBox::Continue)
        return;

    foreach(SequenceJob *job, jobs)
        job->resetStatus();

    stopSequence();    
}

void Capture::editJob(QModelIndex i)
{
    SequenceJob *job = jobs.at(i.row());
    if (job == NULL)
        return;
    QString rawPrefix;
    bool typeEnabled, filterEnabled, expEnabled;

     job->getPrefixSettings(rawPrefix, typeEnabled, filterEnabled, expEnabled);

   exposureIN->setValue(job->getExposure());
   binXIN->setValue(job->getXBin());
   binYIN->setValue(job->getYBin());
   frameXIN->setValue(job->getSubX());
   frameYIN->setValue(job->getSubY());
   frameWIN->setValue(job->getSubW());
   frameHIN->setValue(job->getSubH());
   FilterPosCombo->setCurrentIndex(job->getTargetFilter()-1);
   frameTypeCombo->setCurrentIndex(job->getFrameType());
   prefixIN->setText(rawPrefix);
   frameTypeCheck->setChecked(typeEnabled);
   filterCheck->setChecked(filterEnabled);
   expDurationCheck->setChecked(expEnabled);
   countIN->setValue(job->getCount());
   delayIN->setValue(job->getDelay()/1000);
   ADUValue->setValue(job->getTargetADU());
   fitsDir->setText(job->getFITSDir());
   ISOCheck->setChecked(job->getISOMode());
   if (ISOCombo->isEnabled())
        ISOCombo->setCurrentIndex(job->getISOIndex());
   displayCheck->setChecked(job->isShowFITS());

   appendLogText(xi18n("Editing job #%1...", i.row()+1));

   addToQueueB->setIcon(QIcon::fromTheme("svn-update"));

   jobUnderEdit = true;

}

void Capture::resetJobEdit()
{
   if (jobUnderEdit)
       appendLogText(xi18n("Editing job canceled."));

   jobUnderEdit = false;
   addToQueueB->setIcon(QIcon::fromTheme("list-add"));
}

void Capture::constructPrefix(QString &imagePrefix)
{
    if (frameTypeCheck->isChecked())
    {
        if (imagePrefix.isEmpty() == false)
            imagePrefix += '_';

        imagePrefix += frameTypeCombo->currentText();
    }
    if (filterCheck->isChecked() && FilterPosCombo->currentText().isEmpty() == false &&
            frameTypeCombo->currentText().compare("Bias", Qt::CaseInsensitive) &&
                        frameTypeCombo->currentText().compare("Dark", Qt::CaseInsensitive))
    {
        if (imagePrefix.isEmpty() == false || frameTypeCheck->isChecked())
            imagePrefix += '_';

        imagePrefix += FilterPosCombo->currentText();
    }
    if (expDurationCheck->isChecked())
    {
        if (imagePrefix.isEmpty() == false || frameTypeCheck->isChecked())
            imagePrefix += '_';

        imagePrefix += QString::number(exposureIN->value(), 'd', 0) + QString("_secs");
    }
}

QString Capture::getJobState(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getStatusString();
    }

    return QString();
}

int Capture::getJobImageProgress(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getCompleted();
    }

    return -1;
}

int Capture::getJobImageCount(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getCount();
    }

    return -1;
}

double Capture::getJobExposureProgress(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getExposeLeft();
    }

    return -1;
}

double Capture::getJobExposureDuration(int id)
{
    if (id < jobs.count())
    {
        SequenceJob *job = jobs.at(id);
        return job->getExposure();
    }

    return -1;
}

void Capture::setMaximumGuidingDeviaiton(bool enable, double value)
{
    if (guideDeviationCheck->isEnabled())
    {
        guideDeviationCheck->setChecked(enable);
        if (enable)
            guideDeviation->setValue(value);
    }

}

void Capture::setInSequenceFocus(bool enable, double HFR)
{
    if (autofocusCheck->isEnabled())
    {
        autofocusCheck->setChecked(enable);
        if (enable)
            HFRPixels->setValue(HFR);
    }
}

void Capture::setParkOnComplete(bool enable)
{
    if (parkCheck->isEnabled())
        parkCheck->setChecked(enable);
}

void Capture::setTemperature()
{
    if (currentCCD)
        currentCCD->setTemperature(temperatureIN->value());
}

void Capture::clearSequenceQueue()
{
    stopSequence();
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);
    jobs.clear();
    qDeleteAll(jobs);
}

QString Capture::getSequenceQueueStatus()
{
    if (jobs.count() == 0)
        return "Invalid";

    int idle=0, error=0, complete=0, aborted=0,running=0;

    foreach(SequenceJob* job, jobs)
    {
        switch (job->getStatus())
        {
            case SequenceJob::JOB_ABORTED:
                aborted++;
                break;
            case SequenceJob::JOB_BUSY:
                running++;
                break;
            case SequenceJob::JOB_DONE:
                complete++;
                break;
            case SequenceJob::JOB_ERROR:
                error++;
                break;
            case SequenceJob::JOB_IDLE:
                idle++;
            break;
        }
    }

    if (error > 0)
        return "Error";

    if (aborted > 0)
        return "Aborted";

    if (running > 0)
        return "Running";

    if (idle == jobs.count())
        return "Idle";

    if (complete == jobs.count())
        return "Complete";

    return "Invalid";
}

void Capture::processTelescopeNumber(INumberVectorProperty *nvp)
{
    // If it is not ours, return.
    if (strcmp(nvp->device, currentTelescope->getDeviceName()) || strcmp(nvp->name, "EQUATORIAL_EOD_COORD"))
        return;

    switch (meridianFlipStage)
    {
        case MF_NONE:
            break;
        case MF_INITIATED:
        {
            if (nvp->s == IPS_BUSY)
                meridianFlipStage = MF_FLIPPING;
        }
        break;

        case MF_FLIPPING:
        {
            double ra, dec;
            currentTelescope->getEqCoords(&ra, &dec);
            double diffRA = initialRA - ra;
            if (fabs(diffRA) > MF_RA_DIFF_LIMIT || nvp->s == IPS_OK)
                meridianFlipStage = MF_SLEWING;
        }
        break;

        case MF_SLEWING:

            if (nvp->s != IPS_OK)
                break;

            // We are at a new initialHA
            initialHA= getCurrentHA();

            appendLogText(xi18n("Telescope completed the meridian flip."));

            if (resumeAlignmentAfterFlip == true)
            {
                appendLogText(xi18n("Performing post flip re-alignment..."));
                secondsLabel->setText(xi18n("Aligning..."));
                meridianFlipStage = MF_ALIGNING;
                emit meridialFlipTracked();
                return;
            }

            checkGuidingAfterFlip();
            break;

       default:
        break;
    }

}

void Capture::checkGuidingAfterFlip()
{
    // If we're not autoguiding then we're done
    if (resumeGuidingAfterFlip == false)
    {
        resumeSequence();
        meridianFlipStage = MF_NONE;
    }
    else
    {
        appendLogText(xi18n("Performing post flip re-calibration and guiding..."));
        secondsLabel->setText(xi18n("Calibrating..."));
        meridianFlipStage = MF_GUIDING;
        emit meridianFlipCompleted();
    }
}

double Capture::getCurrentHA()
{
    double currentRA, currentDEC;

    if (currentTelescope == NULL)
        return INVALID_HA;

    if (currentTelescope->getEqCoords(&currentRA, &currentDEC) == false)
    {
        appendLogText(xi18n("Failed to retrieve telescope coordinates. Unable to calculate telescope's hour angle."));
        return INVALID_HA;
    }

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());

    dms ha( lst.Degrees() - currentRA*15.0 );

    double HA = ha.Hours();

    if (HA > 12)
        HA -= 24;

    return HA;
}

bool Capture::checkMeridianFlip()
{

    if (meridianCheck->isEnabled() == false || meridianCheck->isChecked() == false || initialHA > 0)
        return false;


    double currentHA = getCurrentHA();

    //appendLogText(xi18n("Current hour angle %1", currentHA));

    if (currentHA == INVALID_HA)
        return false;

    if (currentHA > meridianHours->value())
    {
        //NOTE: DO NOT make the follow sentence PLURAL as the value is in double
        appendLogText(xi18n("Current hour angle %1 hours exceeds meridian flip limit of %2 hours. Auto meridian flip is initiated.", QString::number(currentHA, 'f', 2), meridianHours->value()));
        meridianFlipStage = MF_INITIATED;

        // Suspend guiding first before commanding a meridian flip
        //if (isAutoGuiding && currentCCD->getChip(ISD::CCDChip::GUIDE_CCD) == guideChip)
//            emit suspendGuiding(false);

        // If we are autoguiding, we should resume autoguiding after flip
        resumeGuidingAfterFlip = isAutoGuiding;

        if (isAutoGuiding || isAutoFocus)
            emit meridianFlipStarted();

        double dec;
        currentTelescope->getEqCoords(&initialRA, &dec);
        currentTelescope->Slew(initialRA,dec);
        secondsLabel->setText(xi18n("Meridian Flip..."));
        QTimer::singleShot(MF_TIMER_TIMEOUT, this, SLOT(checkMeridianFlipTimeout()));
        return true;
    }

    return false;
}

void Capture::checkMeridianFlipTimeout()
{
    static bool guidingEngaged = false, alignmentEngaged=false;

    if (meridianFlipStage == MF_NONE)
        return;

    if (meridianFlipStage < MF_ALIGNING)
    {
        appendLogText(xi18n("Telescope meridian flip timed out."));
        stopSequence();
    }
    else if (meridianFlipStage == MF_ALIGNING)
    {
        if (alignmentEngaged == false)
        {
            QTimer::singleShot(MF_TIMER_TIMEOUT*2, this, SLOT(checkMeridianFlipTimeout()));
            alignmentEngaged = true;
        }
        else
        {
            appendLogText(xi18n("Alignment timed out."));
            stopSequence();
        }

    }
    else if (meridianFlipStage == MF_GUIDING)
    {
        if (guidingEngaged == false)
        {
            QTimer::singleShot(MF_TIMER_TIMEOUT*2, this, SLOT(checkMeridianFlipTimeout()));
            guidingEngaged = true;
        }
        else
        {
            appendLogText(xi18n("Guiding timed out."));
            stopSequence();
        }
    }
}

void Capture::enableAlignmentFlag()
{
    resumeAlignmentAfterFlip = true;
}

void Capture::checkAlignmentSlewComplete()
{
    if (meridianFlipStage == MF_ALIGNING)
    {
        appendLogText(xi18n("Post flip re-alignment completed successfully."));
        checkGuidingAfterFlip();
    }
}

void Capture::checkFrameType(int index)
{
    if (frameTypeCombo->itemText(index) == xi18n("Flat"))
    {
        ADUSeparator->setVisible(true);
        ADULabel->setVisible(true);
        ADUValue->setVisible(true);
        ADUPercentageLabel->setVisible(true);
    }
    else
    {
        ADUSeparator->setVisible(false);
        ADULabel->setVisible(false);
        ADUValue->setVisible(false);
        ADUPercentageLabel->setVisible(false);
    }
}

double Capture::setCurrentADU(double value)
{
    double nextExposure = 0;

    if (ExpRaw1 == -1)
        ExpRaw1 = activeJob->getExposure();
    else if (ExpRaw2 == -1)
        ExpRaw2 = activeJob->getExposure();
    else
    {
        ExpRaw1 = ExpRaw2;
        ExpRaw2 = activeJob->getExposure();
    }

    if (ADURaw1 == -1)
        ADURaw1 = value;
    else if (ADURaw2 == -1)
        ADURaw2 = value;
    else
    {
        ADURaw1 = ADURaw2;
        ADURaw2 = value;
    }

    qDebug() << "Exposure #1 (" << ExpRaw1 << "," << ADURaw1 << ") Exspoure #2 (" << ExpRaw2 << "," << ADURaw2 << ")";

    // If we don't have the 2nd point, let's take another exposure with value relative to what we have now
    if (ADURaw2 == -1 || ExpRaw2 == -1 || (ADURaw1 == ADURaw2))
    {
        if (value < activeJob->getTargetADU())
            nextExposure = activeJob->getExposure()*1.5;
        else
            nextExposure = activeJob->getExposure()*.75;

        qDebug() << "Next Exposure: " << nextExposure;

        return nextExposure;
    }

    if (fabs(ADURaw2 - ADURaw1) < 0.01)
        ADUSlope=1e-6;
    else
        ADUSlope = (ExpRaw2 - ExpRaw1) / (ADURaw2 - ADURaw1);

    qDebug() << "ADU Slope: " << ADUSlope;

    nextExposure = ADUSlope * (activeJob->getTargetADU() - ADURaw2) + ExpRaw2;

    qDebug() << "Next Exposure: " << nextExposure;

    return nextExposure;

}

void Capture::setDirty()
{
    mDirty = true;
}


}



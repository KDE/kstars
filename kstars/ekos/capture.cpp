/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "capture.h"
#include "Options.h"

#include <KMessageBox>

#include "indi/driverinfo.h"
#include "indi/indifilter.h"
#include "../fitsviewer/fitsviewer.h"
#include "../fitsviewer/fitsimage.h"

#include <libindi/basedevice.h>

namespace Ekos
{

Capture::Capture()
{
    setupUi(this);

    currentCCD = NULL;
    currentFilter = NULL;

    filterSlot = NULL;
    filterName = NULL;

    calibrationState = CALIBRATE_NONE;


    seqLister		= new KDirLister();
    seqTimer = new QTimer(this);
    connect(startB, SIGNAL(clicked()), this, SLOT(startSequence()));
    connect(stopB, SIGNAL(clicked()), this, SLOT(stopSequence()));
    connect(seqTimer, SIGNAL(timeout()), this, SLOT(captureImage()));

    connect(CCDCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkCCD(int)));

    connect(FilterCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));

    connect( seqLister, SIGNAL(newItems (const KFileItemList & )), this, SLOT(checkSeqBoundary(const KFileItemList &)));

    seqExpose = 0;
    seqTotalCount = 0;
    seqCurrentCount = 0;
    seqDelay = 0;
    useGuideHead = false;

    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    displayCheck->setEnabled(Options::showFITS());

}

void Capture::addCCD(ISD::GDInterface *newCCD)
{
    CCDCaptureCombo->addItem(newCCD->getDeviceName());

    connect(newCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    CCDs.append(static_cast<ISD::CCD *> (newCCD));

    checkCCD(0);

    if ( (static_cast<ISD::CCD *> (newCCD))->hasGuideHead())
    {
        CCDCaptureCombo->addItem(newCCD->getDeviceName() + QString(" Guider"));
    }

}

void Capture::addGuideHead(ISD::GDInterface *newCCD)
{
    CCDCaptureCombo->addItem(newCCD->getDeviceName() + QString(" Guider"));
}

void Capture::addFilter(ISD::GDInterface *newFilter)
{
    FilterCaptureCombo->addItem(newFilter->getDeviceName());

    Filters.append(static_cast<ISD::Filter *>(newFilter));

    checkFilter(0);

    FilterCaptureCombo->setCurrentIndex(0);

}

void Capture::startSequence()
{

    ISD::CCDChip *targetChip = NULL;

    if (useGuideHead)
        targetChip = currentCCD->getChip(ISD::CCDChip::GUIDE_CCD);
    else
        targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);


    if (displayCheck->isChecked() == false && darkSubCheck->isChecked())
    {
        KMessageBox::error(this, i18n("Auto dark subtract is not supported in batch mode."));
        return;
    }

    if (ISOCheck->isChecked())
        currentCCD->setISOMode(true);
    else
        currentCCD->setISOMode(false);


    if (displayCheck->isChecked())
        targetChip->setBatchMode(false);
    else
        targetChip->setBatchMode(true);

    currentCCD->setSeqPrefix(prefixIN->text());

    if (filterSlot != NULL && currentFilter != NULL)
    {
        if (FilterPosCombo->currentIndex() != filterSlot->np[0].value)
        {
            int cindex = FilterPosCombo->currentIndex();
            currentFilter->runCommand(INDI_SET_FILTER, &cindex);
        }
    }

    seqExpose = exposureIN->value();
    seqTotalCount = countIN->value();
    seqCurrentCount = 0;
    seqDelay = delayIN->value() * 1000;		/* in ms */

    fullImgCountOUT->setText( QString::number(seqTotalCount));
    currentImgCountOUT->setText(QString::number(seqCurrentCount));

    // set the progress info
    imgProgress->setEnabled(true);
    imgProgress->setMaximum(seqTotalCount);
    imgProgress->setValue(seqCurrentCount);

    updateSequencePrefix(prefixIN->text());
\
    // Update button status
    startB->setEnabled(false);
    stopB->setEnabled(true);

    captureImage();
}

void Capture::stopSequence()
{
    retries              = 0;
    seqTotalCount        = 0;
    seqCurrentCount      = 0;

    imgProgress->reset();
    imgProgress->setEnabled(false);

    fullImgCountOUT->setText(QString());
    currentImgCountOUT->setText(QString());

    startB->setEnabled(true);
    stopB->setEnabled(false);
    seqTimer->stop();

}

void Capture::checkCCD(int ccdNum)
{
    if (ccdNum <= CCDs.count())
    {
        int x,y,w,h;
        int binx,biny;
        double min,max,step;
        QString frameProp = QString("CCD_FRAME");
        ISD::CCDChip *targetChip = NULL;

        // Check whether main camera or guide head only

        if (CCDCaptureCombo->itemText(ccdNum).right(6) == QString("Guider"))
        {
            frameProp = QString("GUIDE_FRAME");
            ccdNum--;
            useGuideHead = true;
            currentCCD = CCDs.at(ccdNum);
            targetChip = currentCCD->getChip(ISD::CCDChip::GUIDE_CCD);
        }
        else
        {
            currentCCD = CCDs.at(ccdNum);
            targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
            useGuideHead = false;
        }


        if (currentCCD->getMinMaxStep(frameProp, "X", &min, &max, &step))
        {
            frameXIN->setMinimum(min);
            frameXIN->setMaximum(max);
            frameXIN->setSingleStep(step);
        }

        if (currentCCD->getMinMaxStep(frameProp, "Y", &min, &max, &step))
        {
            frameYIN->setMinimum(min);
            frameYIN->setMaximum(max);
            frameYIN->setSingleStep(step);
        }

        if (currentCCD->getMinMaxStep(frameProp, "WIDTH", &min, &max, &step))
        {
            frameWIN->setMinimum(min);
            frameWIN->setMaximum(max);
            frameWIN->setSingleStep(step);
        }

        if (currentCCD->getMinMaxStep(frameProp, "HEIGHT", &min, &max, &step))
        {
            frameHIN->setMinimum(min);
            frameHIN->setMaximum(max);
            frameHIN->setSingleStep(step);
        }

        if (targetChip->getFrame(&x,&y,&w,&h))
        {

            frameXIN->setValue(x);
            frameYIN->setValue(y);
            frameWIN->setValue(w);
            frameHIN->setValue(h);
        }

        if (targetChip->getBinning(&binx, &biny))
        {
            binXCombo->setCurrentIndex(binx-1);
            binYCombo->setCurrentIndex(biny-1);
        }

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

    }
}

void Capture::syncFrameType(ISD::GDInterface *ccd)
{
    if (strcmp(ccd->getDeviceName(), CCDCaptureCombo->currentText().toLatin1()))
        return;

    ISD::CCDChip *targetChip = NULL;
    targetChip = (static_cast<ISD::CCD *> (ccd) )->getChip(ISD::CCDChip::PRIMARY_CCD);

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


}

void Capture::checkFilter(int filterNum)
{

    if (filterNum <= Filters.count())
        currentFilter = Filters.at(filterNum);

    FilterPosCombo->clear();

    filterName   = currentFilter->getDriverInfo()->getBaseDevice()->getText("FILTER_NAME");
    filterSlot = currentFilter->getDriverInfo()->getBaseDevice()->getNumber("FILTER_SLOT");

    if (filterSlot == NULL)
    {
        KMessageBox::error(0, i18n("Unable to find FILTER_SLOT property in driver %1", currentFilter->getDriverInfo()->getBaseDevice()->getDeviceName()));
        return;
    }

    for (int i=0; i < filterSlot->np[0].max; i++)
    {
        QString item;

        if (filterName != NULL && (i < filterName->ntp))
            item = filterName->tp[i].text;
        else
            item = QString("Filter %1").arg(i+1);

        FilterPosCombo->addItem(item);

    }

    FilterPosCombo->setCurrentIndex( (int) filterSlot->np[0].value-1);

}

void Capture::newFITS(IBLOB *bp)
{

    ISD::CCDChip *targetChip = NULL;

    if (!strcmp(bp->name, "CCD2"))
        targetChip = currentCCD->getChip(ISD::CCDChip::GUIDE_CCD);
    else
        targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);


    if (targetChip->getCaptureMode() == FITS_FOCUS || targetChip->getCaptureMode() == FITS_GUIDE)
        return;

    // If the FITS is not for our device, simply ignore
    if (QString(bp->bvp->device)  != currentCCD->getDeviceName() || startB->isEnabled())
        return;    

    if (calibrationState == CALIBRATE_START)
    {
        calibrationState = CALIBRATE_DONE;
        seqTimer->start(seqDelay);
        return;
    }

    if (darkSubCheck->isChecked() && calibrationState == CALIBRATE_DONE)
    {
        calibrationState = CALIBRATE_NONE;

        FITSImage *calibrateImage = targetChip->getImage(FITS_CALIBRATE);
        FITSImage *currentImage   = targetChip->getImage(FITS_NORMAL);

        if (calibrateImage && currentImage)
            currentImage->subtract(calibrateImage);
    }

    seqCurrentCount++;
    imgProgress->setValue(seqCurrentCount);

    appendLogText(i18n("Received image %1 out of %2.", seqCurrentCount, seqTotalCount));

    currentImgCountOUT->setText( QString::number(seqCurrentCount));

    // if we're done
    if (seqCurrentCount == seqTotalCount)
    {
        retries              = 0;
        seqTotalCount        = 0;
        seqCurrentCount      = 0;
        //active               = false;
        seqTimer->stop();

        startB->setEnabled(true);
        stopB->setEnabled(false);
    }
    else
        seqTimer->start(seqDelay);

}


void Capture::captureImage()
{
    if (currentCCD == NULL)
        return;


    ISD::CCDChip *targetChip = NULL;

    if (useGuideHead)
        targetChip = currentCCD->getChip(ISD::CCDChip::GUIDE_CCD);
    else
        targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);


    seqTimer->stop();

    if (targetChip->setFrame(frameXIN->value(), frameYIN->value(), frameWIN->value(), frameHIN->value()) == false)
    {
        appendLogText(i18n("Failed to set sub frame."));
        return;

    }

    if (useGuideHead == false && targetChip->setBinning(binXCombo->currentIndex()+1, binYCombo->currentIndex()+1) == false)
    {
        appendLogText(i18n("Failed to set binning."));
        return;
    }

    if (useGuideHead == false && darkSubCheck->isChecked() && calibrationState == CALIBRATE_NONE)
    {
        calibrationState = CALIBRATE_START;
        targetChip->setFrameType(FRAME_DARK);
        targetChip->setCaptureMode(FITS_CALIBRATE);
        appendLogText(i18n("Capturing dark frame..."));
    }
    else
    {

        targetChip->setFrameType(frameTypeCombo->currentText());

        targetChip->setCaptureMode(FITS_NORMAL);
        targetChip->setCaptureFilter( (FITSScale) filterCombo->currentIndex());
        appendLogText(i18n("Capturing image..."));
    }

    targetChip->capture(seqExpose);
}



/*******************************************************************************/
/* Update the prefix for the sequence of images to be captured                 */
/*******************************************************************************/
void Capture::updateSequencePrefix( const QString &newPrefix)
{
    seqPrefix = newPrefix;

    seqLister->setNameFilter(QString("*.fits"));

    seqCount = 1;

    seqLister->openUrl(Options::fitsDir());

    checkSeqBoundary(seqLister->items());

}

/*******************************************************************************/
/* Determine the next file number sequence. That is, if we have file1.png      */
/* and file2.png, then the next sequence should be file3.png		       */
/*******************************************************************************/
void Capture::checkSeqBoundary(const KFileItemList & items)
{
    int newFileIndex;
    QString tempName;

    KFileItemList::const_iterator it = items.begin();
    const KFileItemList::const_iterator end = items.end();
    for ( ; it != end; ++it )
    {
        tempName = (*it).name();

        // find the prefix first
        if (tempName.startsWith(seqPrefix) == false || tempName.endsWith(".fits") == false)
            continue;

        if (seqPrefix.isEmpty() == false)
           tempName.remove(seqPrefix + "_");


        int usIndex = tempName.indexOf('_');

        if (usIndex == -1)
            usIndex = tempName.indexOf('.');

        tempName.remove(usIndex, tempName.size() - usIndex);

        bool indexOK = false;

        newFileIndex = tempName.toInt(&indexOK);


        if (indexOK && newFileIndex >= seqCount)
            seqCount = newFileIndex + 1;

        //qDebug() << "Now the tempName is " << tempName << " conversion is " << (indexOK ? "OK" : "Failed") << " and valu is " << newFileIndex
          //          << " and seqCount is " << seqCount << endl;
    }

    currentCCD->setSeqCount(seqCount);

}

void Capture::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    emit newLog();
}

void Capture::clearLog()
{
    logText.clear();
    emit newLog();
}

}

#include "capture.moc"

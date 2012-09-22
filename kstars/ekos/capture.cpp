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

    filterType = FITS_NONE;

    seqLister		= new KDirLister();
    seqTimer = new QTimer(this);
    connect(startB, SIGNAL(clicked()), this, SLOT(startSequence()));
    connect(stopB, SIGNAL(clicked()), this, SLOT(stopSequence()));
    connect(seqTimer, SIGNAL(timeout()), this, SLOT(captureImage()));

    connect(CCDCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkCCD(int)));

    connect(FilterCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));

    connect( seqLister, SIGNAL(newItems (const KFileItemList & )), this, SLOT(checkSeqBoundary(const KFileItemList &)));
    connect( filterCombo, SIGNAL(activated(int)), this, SLOT(updateImageFilter(int)));

    seqExpose = 0;
    seqTotalCount = 0;
    seqCurrentCount = 0;
    seqDelay = 0;

    displayCheck->setEnabled(Options::showFITS());

}

void Capture::setCCD(ISD::GDInterface *newCCD)
{
    CCDCaptureCombo->addItem(newCCD->getDeviceName());

    currentCCD = (ISD::CCD *) newCCD;

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    CCDs.append(currentCCD);
}

void Capture::addFilter(ISD::GDInterface *newFilter)
{
    FilterCaptureCombo->addItem(newFilter->getDeviceName());

    currentFilter = (ISD::Filter *) newFilter;
    Filters.append(currentFilter);

    checkFilter(Filters.count() - 1);

}

void Capture::startSequence()
{

    if (ISOCheck->isChecked())
        currentCCD->setISOMode(true);
    else
        currentCCD->setISOMode(false);


    if (displayCheck->isChecked())
        currentCCD->setBatchMode(false);
    else
        currentCCD->setBatchMode(true);

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
        currentCCD = CCDs.at(ccdNum);
    /*
    INDI_D *idevice = NULL;
    QString targetCCD = CCDCombo->itemText(ccdNum);

    idevice = imenu->findDeviceByLabel(targetCCD);

    if (!idevice)
    {
        KMessageBox::error(this, i18n("INDI device %1 no longer exists.", targetCCD));
        CCDCombo->removeItem(ccdNum);
        lastCCD = CCDCombo->currentIndex();
        if (lastCCD != -1)
            checkCCD(lastCCD);
        return;
    }

    if (!idevice->isOn())
    {
        KMessageBox::error(this, i18n("%1 is disconnected. Establish a connection to the device using the INDI Control Panel.", targetCCD));

        CCDCombo->setCurrentIndex(lastCCD);
        return;
    }

    currentCCD = targetCCD;*/
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
        KMessageBox::error(0, i18n("Unable to find FILTER_SLOT property in driver %1").arg(currentFilter->getDriverInfo()->getBaseDevice()->getDeviceName()));
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

    FilterPosCombo->setCurrentIndex( (int) filterSlot->np[0].value);
}

void Capture::newFITS(IBLOB *bp)
{

    // If the FITS is not for our device, simply ignore
    if (QString(bp->bvp->device)  != currentCCD->getDeviceName() || startB->isEnabled())
        return;    

    seqCurrentCount++;
    imgProgress->setValue(seqCurrentCount);

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

    seqTimer->stop();

    currentCCD->setCaptureMode(FITS_NORMAL);
    currentCCD->setCaptureFilter(filterType);
    currentCCD->capture(seqExpose);
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

void Capture::updateImageFilter(int index)
{
    if (index == 0)
        filterType = FITS_NONE;
    else if (index == 1)
        filterType = FITS_LOW_PASS;
    else if (index == 2)
        filterType = FITS_EQUALIZE;

}

}

#include "capture.moc"

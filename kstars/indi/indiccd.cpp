/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <KMessageBox>
#include <KStatusBar>
#include <KTemporaryFile>

#include <basedevice.h>

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitscommon.h"
#include "clientmanager.h"
#include "streamwg.h"
#include "indiccd.h"
#include "kstars.h"

#include "Options.h"

const int MAX_FILENAME_LEN = 1024;

namespace ISD
{

CCDChip::CCDChip(INDI::BaseDevice *bDevice, ClientManager *cManager, ChipType cType)
{
    baseDevice    = bDevice;
    clientManager = cManager;
    type          = cType;
    batchMode     = false;
    displayFITS   = true;

    captureMode   = FITS_NORMAL;
    captureFilter = FITS_NONE;

    normalImage = focusImage = guideImage = calibrationImage = NULL;
}

FITSImage * CCDChip::getImage(FITSMode imageType)
{
    switch (imageType)
    {
        case FITS_NORMAL:
            return normalImage;
            break;

        case FITS_FOCUS:
            return focusImage;
            break;

        case FITS_GUIDE:
            return guideImage;
            break;

        case FITS_CALIBRATE:
            return calibrationImage;
            break;
    }

    return NULL;

}

void CCDChip::setImage(FITSImage *image, FITSMode imageType)
{

    switch (imageType)
    {
        case FITS_NORMAL:
            normalImage = image;
            break;

        case FITS_FOCUS:
            focusImage = image;
            break;

        case FITS_GUIDE:
            guideImage = image;
            break;

        case FITS_CALIBRATE:
            calibrationImage = image;
            break;
    }

}

bool CCDChip::getFrame(int *x, int *y, int *w, int *h)
{

    INumberVectorProperty *frameProp = NULL;

    switch (type)
    {
       case PRIMARY_CCD:
        frameProp = baseDevice->getNumber("CCD_FRAME");
        break;

      case GUIDE_CCD:
        frameProp = baseDevice->getNumber("GUIDE_FRAME");
        break;

    }

    if (frameProp == NULL)
        return false;

    INumber *arg = IUFindNumber(frameProp, "X");
    if (arg == NULL)
        return false;

    *x = arg->value;


    arg = IUFindNumber(frameProp, "Y");
    if (arg == NULL)
        return false;

    *y = arg->value;


    arg = IUFindNumber(frameProp, "WIDTH");
    if (arg == NULL)
        return false;

    *w = arg->value;

    arg = IUFindNumber(frameProp, "HEIGHT");
    if (arg == NULL)
        return false;

    *h = arg->value;

    return true;

}

bool CCDChip::setFrame(int x, int y, int w, int h)
{
    INumberVectorProperty *frameProp = NULL;

    switch (type)
    {
       case PRIMARY_CCD:
        frameProp = baseDevice->getNumber("CCD_FRAME");
        break;

      case GUIDE_CCD:
        frameProp = baseDevice->getNumber("GUIDE_FRAME");
        break;

    }

    if (frameProp == NULL)
        return false;

    INumber *xarg = IUFindNumber(frameProp, "X");
    INumber *yarg = IUFindNumber(frameProp, "Y");
    INumber *warg = IUFindNumber(frameProp, "WIDTH");
    INumber *harg = IUFindNumber(frameProp, "HEIGHT");

    if (xarg && yarg && warg && harg)
    {
        if (xarg->value == x && yarg->value == y && warg->value == w && harg->value == h)
            return true;

        xarg->value = x;
        yarg->value = y;
        warg->value = w;
        harg->value = h;

        clientManager->sendNewNumber(frameProp);
        return true;
    }

    return false;

}

bool CCDChip::capture(double exposure)
{
    INumberVectorProperty *expProp = NULL;

    switch (type)
    {
       case PRIMARY_CCD:
        expProp = baseDevice->getNumber("CCD_EXPOSURE");
        break;

      case GUIDE_CCD:
        expProp = baseDevice->getNumber("GUIDER_EXPOSURE");
        break;

    }

    if (expProp == NULL)
        return false;

    expProp->np[0].value = exposure;

    clientManager->sendNewNumber(expProp);

    return true;
}

bool CCDChip::abortExposure()
{
    ISwitchVectorProperty *abortProp = NULL;

    switch (type)
    {
       case PRIMARY_CCD:
        abortProp = baseDevice->getSwitch("CCD_ABORT_EXPOSURE");
        break;

      case GUIDE_CCD:
        abortProp = baseDevice->getSwitch("GUIDER_ABORT_EXPOSURE");
        break;

    }

    if (abortProp == NULL)
        return false;

    ISwitch *abort = IUFindSwitch(abortProp, "ABORT");

    if (abort == NULL)
        return false;

    abort->s = ISS_ON;

    clientManager->sendNewSwitch(abortProp);

    return true;
}

bool CCDChip::setFrameType(const QString & name)
{
    CCDFrameType fType = FRAME_LIGHT;

    if (name == "FRAME_LIGHT" || name == "Light")
        fType = FRAME_LIGHT;
    else if (name == "FRAME_DARK" || name == "Dark")
        fType = FRAME_DARK;
    else if (name == "FRAME_BIAS" || name == "Bias")
        fType = FRAME_BIAS;
    else if (name == "FRAME_FLAT" || name == "Flat")
        fType = FRAME_FLAT;
    else
    {
        qDebug() << name << " frame type is unknown." << endl;
        return false;
    }

    return setFrameType(fType);

}

bool CCDChip::setFrameType(CCDFrameType fType)
{
    if (type == GUIDE_CCD)
        return false;

    ISwitchVectorProperty *frameProp = baseDevice->getSwitch("CCD_FRAME_TYPE");
    if (frameProp == NULL)
        return false;

    ISwitch *ccdFrame = NULL;

    if (fType == FRAME_LIGHT)
        ccdFrame = IUFindSwitch(frameProp, "FRAME_LIGHT");
    else if (fType == FRAME_DARK)
        ccdFrame = IUFindSwitch(frameProp, "FRAME_DARK");
    else if (fType == FRAME_BIAS)
        ccdFrame = IUFindSwitch(frameProp, "FRAME_BIAS");
    else if (fType == FRAME_FLAT)
        ccdFrame = IUFindSwitch(frameProp, "FRAME_FLAT");

    if (ccdFrame == NULL)
        return false;

    if (ccdFrame->s == ISS_ON)
        return true;

    if (fType != FRAME_LIGHT)
        captureMode = FITS_CALIBRATE;

    IUResetSwitch(frameProp);
    ccdFrame->s = ISS_ON;

    clientManager->sendNewSwitch(frameProp);

    return true;
}

CCDFrameType CCDChip::getFrameType()
{

    CCDFrameType fType = FRAME_LIGHT;

    if (type == GUIDE_CCD)
        return fType;

    ISwitchVectorProperty *frameProp = baseDevice->getSwitch("CCD_FRAME_TYPE");
    if (frameProp == NULL)
        return fType;

    ISwitch *ccdFrame = NULL;

    ccdFrame = IUFindOnSwitch(frameProp);

    if (ccdFrame == NULL)
    {
        qDebug() << "Cannot find active frame in CCD!" << endl;
        return fType;
    }


    if (!strcmp(ccdFrame->name, "FRAME_LIGHT"))
        fType = FRAME_LIGHT;
    else if (!strcmp(ccdFrame->name, "FRAME_DARK"))
            fType = FRAME_DARK;
    else if (!strcmp(ccdFrame->name, "FRAME_FLAT"))
            fType = FRAME_FLAT;
    else if (!strcmp(ccdFrame->name, "FRAME_BIAS"))
            fType = FRAME_BIAS;

    return fType;

}

bool CCDChip::setBinning(CCDBinType binType)
{
    if (type == GUIDE_CCD)
        return false;

    switch (binType)
    {
        case SINGLE_BIN:
            return setBinning(1,1);
            break;
        case DOUBLE_BIN:
            return setBinning(2,2);
            break;
        case TRIPLE_BIN:
            return setBinning(3,3);
            break;
        case QUADRAPLE_BIN:
            return setBinning(4,4);
            break;
    }

    return false;
}

CCDBinType CCDChip::getBinning()
{
    CCDBinType binType = SINGLE_BIN;

    if (type == GUIDE_CCD)
        return binType;

    INumberVectorProperty *binProp = baseDevice->getNumber("CCD_BINNING");
    if (binProp == NULL)
        return binType;

    INumber *horBin = NULL, *verBin=NULL;

    horBin = IUFindNumber(binProp, "HOR_BIN");
    verBin = IUFindNumber(binProp, "VER_BIN");

    if (!horBin || !verBin)
        return binType;

    switch ( (int) horBin->value)
    {
        case 2:
          binType = DOUBLE_BIN;
          break;

        case 3:
           binType = TRIPLE_BIN;
           break;

         case 4:
            binType = QUADRAPLE_BIN;
            break;

         default:
        break;

    }

    return binType;

}

bool CCDChip::getBinning(int *bin_x, int *bin_y)
{
    if (type == GUIDE_CCD)
        return false;

    INumberVectorProperty *binProp = baseDevice->getNumber("CCD_BINNING");
    if (binProp == NULL)
        return false;

    INumber *horBin = NULL, *verBin=NULL;

    horBin = IUFindNumber(binProp, "HOR_BIN");
    verBin = IUFindNumber(binProp, "VER_BIN");

    if (!horBin || !verBin)
        return false;

    *bin_x = horBin->value;
    *bin_y = verBin->value;

    return true;
}

bool CCDChip::setBinning(int bin_x, int bin_y)
{
    if (type == GUIDE_CCD)
        return false;

    INumberVectorProperty *binProp = baseDevice->getNumber("CCD_BINNING");
    if (binProp == NULL)
        return false;

    INumber *horBin = NULL, *verBin=NULL;

    horBin = IUFindNumber(binProp, "HOR_BIN");
    verBin = IUFindNumber(binProp, "VER_BIN");

    if (!horBin || !verBin)
        return false;

    if (horBin->value == bin_x && verBin->value == bin_y)
        return true;

    if (bin_x > horBin->max || bin_y > verBin->max)
        return false;

    horBin->value = bin_x;
    verBin->value = bin_y;

    clientManager->sendNewNumber(binProp);

    return true;

}


CCD::CCD(GDInterface *iPtr) : DeviceDecorator(iPtr)
{
    dType = KSTARS_CCD;
    ISOMode   = true;
    HasGuideHead = false;
    fv          = NULL;
    streamWindow      = NULL;
    ST4Driver = NULL;
    seqCount  = 0 ;

    primaryChip = new CCDChip(baseDevice, clientManager, CCDChip::PRIMARY_CCD);

    normalTabID = calibrationTabID = focusTabID = guideTabID = -1;
    guideChip   = NULL;

}

CCD::~CCD()
{
    delete (fv);
    delete (streamWindow);
    delete (primaryChip);
    delete (guideChip);
}

void CCD::registerProperty(INDI::Property *prop)
{
    if (!strcmp(prop->getName(), "GUIDER_EXPOSURE"))
    {
        HasGuideHead = true;
        guideChip = new CCDChip(baseDevice, clientManager, CCDChip::GUIDE_CCD);
    }
    else if (!strcmp(prop->getName(), "CCD_FRAME_TYPE"))
    {
        ISwitchVectorProperty *ccdFrame = prop->getSwitch();

        for (int i=0; i < ccdFrame->nsp; i++)
            primaryChip->addFrameLabel(ccdFrame->sp[i].label);
    }

    DeviceDecorator::registerProperty(prop);
}

void CCD::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void CCD::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "CCD_FRAME"))
    {
        INumber *wd = IUFindNumber(nvp, "WIDTH");
        INumber *ht = IUFindNumber(nvp, "HEIGHT");

        if (wd && ht && streamWindow)
            streamWindow->setSize(wd->value, ht->value);

        emit numberUpdated(nvp);

        return;

    }

    if (!strcmp(nvp->name, "CCD_EXPOSURE"))
    {
        if (nvp->s == IPS_BUSY)
        {
            INumber *np = IUFindNumber(nvp, "CCD_EXPOSURE_VALUE");
            if (np)
                emit newExposureValue(primaryChip, np->value);
        }
    }

    if (!strcmp(nvp->name, "GUIDER_EXPOSURE"))
    {
        if (nvp->s == IPS_BUSY)
        {
            INumber *np = IUFindNumber(nvp, "GUIDER_EXPOSURE_VALUE");
            if (np)
                emit newExposureValue(guideChip, np->value);
        }
    }

    DeviceDecorator::processNumber(nvp);
}

void CCD::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "VIDEO_STREAM"))
    {
        if (streamWindow == NULL)
        {
            streamWindow = new StreamWG();

            INumberVectorProperty *ccdFrame = baseDevice->getNumber("CCD_FRAME");

            if (ccdFrame == NULL)
                return;

            INumber *wd = IUFindNumber(ccdFrame, "WIDTH");
            INumber *ht = IUFindNumber(ccdFrame, "HEIGHT");

            if (wd && ht)
                streamWindow->setSize(wd->value, ht->value);

            connect(streamWindow, SIGNAL(destroyed()), this, SLOT(StreamWindowDestroyed()));
        }

        if (svp->sp[0].s == ISS_ON)
            streamWindow->enableStream(true);
        else
            streamWindow->enableStream(false);


        emit switchUpdated(svp);

        return;
    }

    if (!strcmp(svp->name, "CONNECTION"))
    {
        ISwitch *dSwitch = IUFindSwitch(svp, "DISCONNECT");

        if (dSwitch && dSwitch->s == ISS_ON && streamWindow != NULL)
        {
            streamWindow->enableStream(false);
            streamWindow->close();
        }

        emit switchUpdated(svp);

    }

    DeviceDecorator::processSwitch(svp);

}

void CCD::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}


void CCD::processBLOB(IBLOB* bp)
{

    // If stream, process it first
    if (!strcmp(bp->format, ".stream") && streamWindow)
    {
        if (streamWindow->isStreamEnabled() == false)
            return;

        streamWindow->show();
        streamWindow->newFrame( (static_cast<unsigned char *> (bp->blob)), bp->size, streamWindow->getWidth(), streamWindow->getHeight());
        return;
    }

    // If it's not FITS, don't process it.
    if (strcmp(bp->format, ".fits"))
    {
        DeviceDecorator::processBLOB(bp);
        return;
    }

    CCDChip *targetChip = NULL;

    if (!strcmp(bp->name, "CCD2"))
        targetChip = guideChip;
    else
        targetChip = primaryChip;

     // It's either FITS or OTHER
    QString currentDir = Options::fitsDir();
    int nr, n=0;
    KTemporaryFile tmpFile;

    if (currentDir.endsWith('/'))
        currentDir.truncate(sizeof(currentDir)-1);

    QString filename(currentDir + '/');

    // Create file name for FITS to be shown in FITS Viewer
    //if (targetChip->isBatchMode() == false && Options::showFITS())
    if (targetChip->isBatchMode() == false)
    {

        tmpFile.setPrefix("fits");
        tmpFile.setAutoRemove(false);

         if (!tmpFile.open())
         {
                 qDebug() << "Error: Unable to open " << filename << endl;
                 return;
         }

         QDataStream out(&tmpFile);

         for (nr=0; nr < (int) bp->size; nr += n)
             n = out.writeRawData( static_cast<char *> (bp->blob) + nr, bp->size - nr);

         tmpFile.close();

         filename = tmpFile.fileName();

    }
    // Create file name for others
    else
    {
         QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss");

           // if (seqCount == 0)
            //{
                if (ISOMode == false)
                    filename += seqPrefix + (seqPrefix.isEmpty() ? "" : "_") +  QString("%1.fits").arg(QString().sprintf("%02d", seqCount));
                else
                    filename += seqPrefix + (seqPrefix.isEmpty() ? "" : "_") + QString("%1_%2.fits").arg(QString().sprintf("%02d", seqCount)).arg(ts);
           // }
            //else
              //  filename += QString("file_") + ts + ".fits";

            QFile fits_temp_file(filename);
            if (!fits_temp_file.open(QIODevice::WriteOnly))
            {
                    kDebug() << "Error: Unable to open " << fits_temp_file.fileName() << endl;
            return;
            }

            QDataStream out(&fits_temp_file);

            for (nr=0; nr < (int) bp->size; nr += n)
                n = out.writeRawData( static_cast<char *> (bp->blob) + nr, bp->size - nr);

            fits_temp_file.close();
    }

    if (targetChip->isBatchMode())
        KStars::Instance()->statusBar()->changeItem( i18n("FITS file saved to %1", filename ), 0);

    // Unless we have cfitsio, we're done.
    #ifdef HAVE_CFITSIO_H
    if (Options::showFITS() && targetChip->showFITS() == true)
    {
        KUrl fileURL(filename);

        if (fv == NULL)
        {
            fv = new FITSViewer(KStars::Instance());
            connect(fv, SIGNAL(destroyed()), this, SLOT(FITSViewerDestroyed()));
            connect(fv, SIGNAL(destroyed()), this, SIGNAL(FITSViewerClosed()));
        }

        FITSScale captureFilter = targetChip->getCaptureFilter();        

        switch (targetChip->getCaptureMode())
        {
            case FITS_NORMAL:
                normalTabID = fv->addFITS(&fileURL, FITS_NORMAL, captureFilter);
                targetChip->setImage(fv->getImage(normalTabID), FITS_NORMAL);
                break;

            case FITS_FOCUS:
                if (focusTabID == -1)
                    focusTabID = fv->addFITS(&fileURL, FITS_FOCUS, captureFilter);
                else if (fv->updateFITS(&fileURL, focusTabID, captureFilter) == false)
                    focusTabID = fv->addFITS(&fileURL, FITS_FOCUS, captureFilter);

                targetChip->setImage(fv->getImage(focusTabID), FITS_FOCUS);
                break;

        case FITS_GUIDE:
            if (guideTabID == -1)
                guideTabID = fv->addFITS(&fileURL, FITS_GUIDE, captureFilter);
            else if (fv->updateFITS(&fileURL, guideTabID, captureFilter) == false)
                guideTabID = fv->addFITS(&fileURL, FITS_GUIDE, captureFilter);

            targetChip->setImage(fv->getImage(guideTabID), FITS_GUIDE);
            break;

        case FITS_CALIBRATE:
            if (calibrationTabID == -1)
                calibrationTabID = fv->addFITS(&fileURL, FITS_CALIBRATE, captureFilter);
            else if (fv->updateFITS(&fileURL, calibrationTabID, captureFilter) == false)
                calibrationTabID = fv->addFITS(&fileURL, FITS_CALIBRATE, captureFilter);

            targetChip->setImage(fv->getImage(calibrationTabID), FITS_CALIBRATE);
            break;

        }

        fv->show();

    }
    #endif

    emit BLOBUpdated(bp);

}

void CCD::FITSViewerDestroyed()
{
    fv = NULL;
    normalTabID = calibrationTabID = focusTabID = guideTabID = -1;
}

void CCD::StreamWindowDestroyed()
{
    qDebug() << "Stream windows destroyed " << endl;
    delete(streamWindow);
    streamWindow = NULL;
}

bool CCD::hasGuideHead()
{
    return HasGuideHead;
}

CCDChip * CCD::getChip(CCDChip::ChipType cType)
{
    switch (cType)
    {
        case CCDChip::PRIMARY_CCD:
            return primaryChip;
            break;

        case CCDChip::GUIDE_CCD:
            return guideChip;
            break;
    }

    return NULL;
}



}

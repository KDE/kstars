/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <KMessageBox>
#include <KStatusBar>
#include <libindi/basedevice.h>

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


CCD::CCD(GDInterface *iPtr) : DeviceDecorator(iPtr)
{
    dType = KSTARS_CCD;
    batchMode = false;
    ISOMode   = true;
    captureMode = FITS_NORMAL;
    captureFilter     = FITS_NONE;
    fv                = NULL;
    streamWindow      = NULL;
    focusTabID        = guideTabID = -1;

}

CCD::~CCD()
{
    delete (fv);
    delete (streamWindow);
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

        return;

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

    }

    DeviceDecorator::processSwitch(svp);

}

void CCD::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}

bool CCD::getFrame(int *x, int *y, int *w, int *h)
{

    INumberVectorProperty *frameProp = baseDevice->getNumber("CCD_FRAME");

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

bool CCD::capture(double exposure)
{
    INumberVectorProperty *expProp = baseDevice->getNumber("CCD_EXPOSURE_REQUEST");
    if (expProp == NULL)
        return false;

    expProp->np[0].value = exposure;

    clientManager->sendNewNumber(expProp);

    return true;
}

bool CCD::setFrameType(CCDFrameType fType)
{
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

    IUResetSwitch(frameProp);
    ccdFrame->s = ISS_ON;

    clientManager->sendNewSwitch(frameProp);

    return true;
}

bool CCD::setBinning(CCDBinType binType)
{
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

bool CCD::setBinning(int bin_x, int bin_y)
{
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

     // It's either FITS or OTHER
    char file_template[MAX_FILENAME_LEN];
    QString currentDir = Options::fitsDir();
    int fd, nr, n=0;

    if (currentDir.endsWith('/'))
        currentDir.truncate(sizeof(currentDir)-1);

    QString filename(currentDir + '/');

    // Create file name for FITS to be shown in FITS Viewer
    if (batchMode == false && Options::showFITS())
    {
        strncpy(file_template, "/tmp/fitsXXXXXX", MAX_FILENAME_LEN);
        if ((fd = mkstemp(file_template)) < 0)
        {
            KMessageBox::error(NULL, i18n("Error making temporary filename."));
            return;
        }
        filename = QString(file_template);
        close(fd);
    }
    // Create file name for others
    else
    {
         QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss");

            if (batchMode)
            {
                if (ISOMode == false)
                    filename += seqPrefix + (seqPrefix.isEmpty() ? "" : "_") +  QString("%1.fits").arg(QString().sprintf("%02d", seqCount));
                else
                    filename += seqPrefix + (seqPrefix.isEmpty() ? "" : "_") + QString("%1_%2.fits").arg(QString().sprintf("%02d", seqCount)).arg(ts);
            }
            else
                filename += QString("file_") + ts + ".fits";

            //seqCount++;
    }

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

    if (batchMode)
        KStars::Instance()->statusBar()->changeItem( i18n("FITS file saved to %1", filename ), 0);

    // Unless we have cfitsio, we're done.
    #ifdef HAVE_CFITSIO_H
    if (batchMode == false && Options::showFITS())
    {
        KUrl fileURL(filename);

        if (fv == NULL)
        {
            fv = new FITSViewer(KStars::Instance());
            connect(fv, SIGNAL(destroyed()), this, SLOT(FITSViewerDestroyed()));
        }

        switch (captureMode)
        {
            case FITS_NORMAL:
                fv->addFITS(&fileURL, FITS_NORMAL, captureFilter);
                break;

            case FITS_FOCUS:
                if (focusTabID == -1)
                    focusTabID = fv->addFITS(&fileURL, FITS_FOCUS, captureFilter);
                else if (fv->updateFITS(&fileURL, focusTabID, captureFilter) == false)
                    focusTabID = fv->addFITS(&fileURL, FITS_FOCUS, captureFilter);
                break;

        case FITS_GUIDE:
            if (guideTabID == -1)
                guideTabID = fv->addFITS(&fileURL, FITS_GUIDE, captureFilter);
            else if (fv->updateFITS(&fileURL, guideTabID, captureFilter) == false)
                guideTabID = fv->addFITS(&fileURL, FITS_GUIDE, captureFilter);
            break;

        }

        captureMode = FITS_NORMAL;

        fv->show();

    }
    #endif

    emit BLOBUpdated(bp);

}

void CCD::FITSViewerDestroyed()
{
    fv = NULL;
    focusTabID = -1;
    guideTabID = -1;

}

void CCD::StreamWindowDestroyed()
{
    qDebug() << "Stream windows destroyed " << endl;
    delete(streamWindow);
    streamWindow = NULL;
}

bool CCD::canGuide()
{
    INumberVectorProperty *raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    INumberVectorProperty *decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");

    if (raPulse && decPulse)
        return true;
    else
        return false;
}

bool CCD::doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs )
{
    if (canGuide() == false)
        return false;

    bool raOK=false, decOK=false;
    raOK  = doPulse(ra_dir, ra_msecs);
    decOK = doPulse(dec_dir, dec_msecs);

    if (raOK && decOK)
        return true;
    else
        return false;
}

bool CCD::doPulse(GuideDirection dir, int msecs )
{
    INumberVectorProperty *raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    INumberVectorProperty *decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");
    INumberVectorProperty *npulse = NULL;
    INumber *dirPulse=NULL;

    if (raPulse == NULL || decPulse == NULL)
        return false;

    switch(dir)
    {
    case RA_INC_DIR:
    dirPulse = IUFindNumber(raPulse, "TIMED_GUIDE_W");
    if (dirPulse == NULL)
        return false;

    npulse = raPulse;
    break;

    case RA_DEC_DIR:
    dirPulse = IUFindNumber(raPulse, "TIMED_GUIDE_E");
    if (dirPulse == NULL)
        return false;

    npulse = raPulse;
    break;

    case DEC_INC_DIR:
    dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_N");
    if (dirPulse == NULL)
        return false;

    npulse = decPulse;
    break;

    case DEC_DEC_DIR:
    dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_S");
    if (dirPulse == NULL)
        return false;

    npulse = decPulse;
    break;

    default:
        return false;

    }

    if (dirPulse == NULL || npulse == NULL)
        return false;

    dirPulse->value = msecs;

    clientManager->sendNewNumber(npulse);

    qDebug() << "Sending pulse for " << npulse->name << " in direction " << dirPulse->name << " for " << msecs << " ms " << endl;

    return true;


}


}

/*  INDI STD
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This apppication is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    2004-01-18: This class handles INDI Standard properties.
 */

#include "indistd.h"
#include "Options.h"
#include "indielement.h"
#include "indiproperty.h"
#include "indigroup.h"
#include "indidevice.h"
#include "indidriver.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skyobject.h"
#include "simclock.h"
#include "devicemanager.h"
#include "dialogs/timedialog.h"
#include "streamwg.h"

#include <config-kstars.h>

#ifdef HAVE_CFITSIO_H
#include "fitsviewer/fitsviewer.h"
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <zlib.h>

#include <QTimer>
#include <QLabel>
#include <QFont>
#include <QEventLoop>
#include <QSocketNotifier>

#include <KTemporaryFile>

#include <klocale.h>
#include <kdebug.h>
#include <kpushbutton.h>
#include <klineedit.h>
#include <kstatusbar.h>
#include <kmessagebox.h>
#include <kprogressdialog.h>
#include <kurl.h>
#include <kdirlister.h>
#include <kaction.h>
#include <kactioncollection.h>

#define STD_BUFFER_SIZ		1024000
#define FRAME_ILEN		1024
#define MAX_FILENAME_LEN	128

INDIStdDevice::INDIStdDevice(INDI_D *associatedDevice, KStars * kswPtr)
{

    dp			  = associatedDevice;
    ksw  		  = kswPtr;
    seqCount		  = 0;
    batchMode 		  = false;
    ISOMode   		  = false;
    driverLocationUpdated = false;
    driverTimeUpdated     = false;
    asciiFileDirty        = true;

    currentObject  	= NULL;
    streamWindow   	= new StreamWG(this, NULL);

    devTimer 		= new QTimer(this);
    seqLister		= new KDirLister();
    ascii_data_file     = new QFile();

    telescopeSkyObject   = new SkyObject(0, 0, 0, 0, i18n("Telescope"));

    connect( devTimer, SIGNAL(timeout()), this, SLOT(timerDone()) );
    connect( seqLister, SIGNAL(newItems (const KFileItemList & )), this, SLOT(checkSeqBoundary(const KFileItemList &)));

    //downloadDialog = new KProgressDialog(NULL, i18n("INDI"), i18n("Downloading Data..."));
    //downloadDialog->reject();

    parser		= newLilXML();
}

INDIStdDevice::~INDIStdDevice()
{
    //ksw->data()->skyComposite()->removeTelescopeMarker(telescopeSkyObject);
    streamWindow->enableStream(false);
    streamWindow->close();
    streamDisabled();
    delete (telescopeSkyObject);
    delete (seqLister);
}

void INDIStdDevice::handleBLOB(unsigned char *buffer, int bufferSize, const QString &dataFormat, INDI_D::DTypes dataType)
{

    if (dataType == INDI_D::VIDEO_STREAM)
    {
        if (!streamWindow->processStream)
            return;

        streamWindow->show();
        streamWindow->streamFrame->newFrame( buffer, bufferSize, streamWindow->streamWidth, streamWindow->streamHeight);
        return;
    }

    // It's either FITS or OTHER
    char file_template[MAX_FILENAME_LEN];
    QString currentDir = Options::fitsDir();
    int fd, nr, n=0;

    if (currentDir.endsWith('/'))
		currentDir.truncate(sizeof(currentDir)-1);

    QString filename(currentDir + '/');

    streamWindow->close();

    if (dataType == INDI_D::DATA_FITS && !batchMode && Options::showFITS())
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
    // Save file to disk
    else
    {
         QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss");

        if (dataType == INDI_D::DATA_FITS)
        {
            if ( batchMode)
	    {
		if (!ISOMode)
			filename += seqPrefix + (seqPrefix.isEmpty() ? "" : "_") +  QString("%1.fits").arg(seqCount , 2);
		else
			filename += seqPrefix + (seqPrefix.isEmpty() ? "" : "_") + QString("%1_%2.fits").arg(seqCount, 2).arg(ts);
	    }
            else
		filename += QString("file_") + ts + ".fits";

            seqCount++;
        }
        else if (dataType == INDI_D::ASCII_DATA_STREAM)
            filename += QString("file_") + ts + dataFormat;
        else
	    filename += QString("file_") + ts + '.' + dataFormat;
    }

    //kDebug() << "Final file name is " << filename;

    if (dataType == INDI_D::ASCII_DATA_STREAM)
    {
        if (asciiFileDirty)
        {
            asciiFileDirty = false;
            ascii_data_file->close();
            ascii_data_file->setFileName(filename);
            if (!ascii_data_file->open(QIODevice::WriteOnly))
            {
                        kDebug() << "Error: Unable to open " << ascii_data_file->fileName() << endl;
                        return;
            }
        }

           QDataStream out(ascii_data_file);
           for (nr=0; nr < (int) bufferSize; nr += n)
               n = out.writeRawData( (const char *) (buffer+nr), bufferSize - nr);

           out.writeRawData( (const char *) "\n" , 1);
           ascii_data_file->flush();

     }
    else
    {
        QFile fits_temp_file(filename);
        if (!fits_temp_file.open(QIODevice::WriteOnly))
        {
                kDebug() << "Error: Unable to open " << fits_temp_file.fileName() << endl;
		return;
        }

        QDataStream out(&fits_temp_file);

        for (nr=0; nr < (int) bufferSize; nr += n)
            n = out.writeRawData( (const char *) (buffer+nr), bufferSize - nr);

        fits_temp_file.close();
    }


    // We're done if we have DATA_OTHER or DATA_STREAM or DATA_FITS if CFITSIO is not enabled.
    if (dataType == INDI_D::DATA_OTHER || dataType == INDI_D::ASCII_DATA_STREAM)
    {
        if (dataType == INDI_D::DATA_OTHER)
            ksw->statusBar()->changeItem( i18n("Data file saved to %1", filename ), 0);
        return;
    }

    if (dataType == INDI_D::DATA_FITS && (batchMode || !Options::showFITS()))
    {
        ksw->statusBar()->changeItem( i18n("FITS file saved to %1", filename ), 0);
        emit FITSReceived(dp->label);
        return;
    }


    // FIXME It appears that FITSViewer causes a possible stack corruption, needs to investigate

    // Unless we have cfitsio, we're done.
    #ifdef HAVE_CFITSIO_H
    KUrl fileURL(filename);

    FITSViewer * fv = new FITSViewer(&fileURL, ksw);
    fv->fitsChange();
    fv->show();
    #endif

}

/*******************************************************************************/
/* Process TEXT & NUMBER property updates received FROM the driver	       */
/*******************************************************************************/
void INDIStdDevice::setTextValue(INDI_P *pp)
{
    INDI_E *el;
    int wd, ht;
    int d, m, y, min, sec, hour;
    QDate indiDate;
    QTime indiTime;
    KStarsDateTime indiDateTime;

    switch (pp->stdID)
    {

    case TIME_UTC:
        if ( Options::useTimeUpdate() && !driverTimeUpdated)
	{
	    driverTimeUpdated = true;
            processDeviceInit();
	}

	// Update KStars time once we receive update from INDI, if the source is set to DEVICE
	if (Options::useDeviceSource())
	{
	        el = pp->findElement("UTC");
	        if (!el) return;

	        sscanf(el->text.toAscii().data(), "%d%*[^0-9]%d%*[^0-9]%dT%d%*[^0-9]%d%*[^0-9]%d", &y, &m, &d, &hour, &min, &sec);
	        indiDate.setYMD(y, m, d);
	        indiTime.setHMS(hour, min, sec);
	        indiDateTime.setDate(indiDate);
	        indiDateTime.setTime(indiTime);

        	ksw->data()->changeDateTime(indiDateTime);
        	ksw->data()->syncLST();
	}

        break;

    case TIME_LST:
        break;

    case GEOGRAPHIC_COORD:
        if ( Options::useGeographicUpdate() && !driverLocationUpdated)
	{
	    driverLocationUpdated = true;
            processDeviceInit();
	}

	// Update KStars Location once we receive update from INDI, if the source is set to DEVICE
	if (Options::useDeviceSource()) {
		dms lng, lat;

        el = pp->findElement("LONG");
        if (!el)
            return;
        sscanf(el->text.toAscii().data(), "%d%*[^0-9]%d%*[^0-9]%d", &d, &min, &sec);
		lng.setD(d,min,sec);

		el = pp->findElement("LAT");
        if (!el)
            return;
        sscanf(el->text.toAscii().data(), "%d%*[^0-9]%d%*[^0-9]%d", &d, &min, &sec);
		lat.setD(d,min,sec);

        ksw->data()->geo()->setLong(lng);
        ksw->data()->geo()->setLat(lat);
	}
        break;

    case CCD_EXPOSURE_REQUEST:
        if (pp->state == PS_IDLE || pp->state == PS_OK)
            pp->set_w->setText(i18n("Capture"));
        break;

    case CCD_FRAME:
        el = pp->findElement("WIDTH");
        if (!el) return;
        wd = (int) el->value;
        el = pp->findElement("HEIGHT");
        if (!el) return;
        ht = (int) el->value;

        streamWindow->setSize(wd, ht);
        //streamWindow->allocateStreamBuffer();
        break;

    case EQUATORIAL_COORD:
    case EQUATORIAL_EOD_COORD:
        if (!dp->isOn()) break;
        el = pp->findElement("RA");
        if (!el) return;
        telescopeSkyObject->setRA(el->value);
        el = pp->findElement("DEC");
        if (!el) return;
        telescopeSkyObject->setDec(el->value);
        telescopeSkyObject->EquatorialToHorizontal(ksw->data()->lst(), ksw->data()->geo()->lat());

        if (ksw->map()->focusObject() == telescopeSkyObject)
            ksw->map()->updateFocus();
        else
            ksw->map()->update();
        break;


    case HORIZONTAL_COORD:
        if (!dp->isOn()) break;
        el = pp->findElement("ALT");
        if (!el) return;
        telescopeSkyObject->setAlt(el->value);
        el = pp->findElement("AZ");
        if (!el) return;
        telescopeSkyObject->setAz(el->value);
        telescopeSkyObject->HorizontalToEquatorial(ksw->data()->lst(), ksw->data()->geo()->lat());
        // Force immediate update of skymap if the focus object is our telescope.
        if (ksw->map()->focusObject() == telescopeSkyObject)
            ksw->map()->updateFocus();
        else
            ksw->map()->update();
        break;

    default:
        break;

    }

}

/*******************************************************************************/
/* Process SWITCH property update received FROM the driver		       */
/*******************************************************************************/
void INDIStdDevice::setLabelState(INDI_P *pp)
{
    INDI_E *lp;
    INDI_P *imgProp;
    QAction *tmpAction;
    INDIDriver *drivers = ksw->indiDriver();
    QFont buttonFont;

    switch (pp->stdID)
    {
    case CONNECTION:
        lp = pp->findElement("CONNECT");
        if (!lp) return;

        if (lp->state == PS_ON)
        {
            createDeviceInit();
            emit linkAccepted();

            imgProp = dp->findProp("CCD_EXPOSURE_REQUEST");
            if (imgProp)
            {
                tmpAction = ksw->actionCollection()->action("capture_sequence");
                if (!tmpAction)
                    kDebug() << "Warning: capture_sequence action not found";
                else
                    tmpAction->setEnabled(true);
            }
        }
        else
        {
            if (streamWindow)
            {
                streamWindow->enableStream(false);
                streamWindow->close();
            }

            if (ksw->map()->focusObject() == telescopeSkyObject)
            {
                ksw->map()->stopTracking();
                ksw->map()->setFocusObject(NULL);
            }

            drivers->updateMenuActions();
            ksw->map()->forceUpdateNow();
            emit linkRejected();
        }
        break;

    case VIDEO_STREAM:
        lp = pp->findElement("ON");
        if (!lp) return;
        if (lp->state == PS_ON)
            streamWindow->enableStream(true);
        else
            streamWindow->enableStream(false);
        break;

    default:
        break;
    }

}

/*******************************************************************************/
/* Tell driver to stop sending stream			                       */
/*******************************************************************************/
void INDIStdDevice::streamDisabled()
{
    INDI_P *pp;
    INDI_E *el;

    pp = dp->findProp("VIDEO_STREAM");
    if (!pp) return;

    el = pp->findElement("OFF");
    if (!el) return;

    if (el->state == PS_ON)
        return;

    // Turn stream off
    pp->newSwitch(el);

}

/*******************************************************************************/
/* Update the prefix for the sequence of images to be captured                 */
/*******************************************************************************/
void INDIStdDevice::updateSequencePrefix( const QString &newPrefix)
{
    seqPrefix = newPrefix;

    seqLister->setNameFilter(QString("%1_*.fits").arg(seqPrefix));

    seqCount = 0;

    if (ISOMode) return;

    seqLister->openUrl(Options::fitsDir());

    checkSeqBoundary(seqLister->items());

}

/*******************************************************************************/
/* Determine the next file number sequence. That is, if we have file1.png      */
/* and file2.png, then the next sequence should be file3.png		       */
/*******************************************************************************/
void INDIStdDevice::checkSeqBoundary(const KFileItemList & items)
{
    int newFileIndex;
    QString tempName;

    // No need to check when in ISO mode
    if (ISOMode)
        return;

    char *tempPrefix = new char[64];

    KFileItemList::const_iterator it = items.begin();
    const KFileItemList::const_iterator end = items.end();
    for ( ; it != end; ++it )
    {
        tempName = (*it).name();

        // find the prefix first
        if (tempName.count(seqPrefix) == 0)
            continue;

        strncpy(tempPrefix, tempName.toAscii().data(), 64);
        tempPrefix[63] = '\0';

        char * t = tempPrefix;

        // skip chars
    while (*t) { if (isdigit(*t)) break; t++; }
        //tempPrefix = t;

        newFileIndex = strtol(t, NULL, 10);

        if (newFileIndex >= seqCount)
            seqCount = newFileIndex + 1;
    }

    delete [] (tempPrefix);

}

/*******************************************************************************/
/* Prompt user to end date and time settings, then send it over to INDI Driver */
/*******************************************************************************/
void INDIStdProperty::newTime()
{
    INDI_E * timeEle;
    INDI_P *SDProp;

    timeEle = pp->findElement("UTC");
    if (!timeEle) return;

    TimeDialog timedialog ( ksw->data()->ut(), ksw->data()->geo(), ksw, true );

    if ( timedialog.exec() == QDialog::Accepted )
    {
        QTime newTime( timedialog.selectedTime() );
        QDate newDate( timedialog.selectedDate() );

        timeEle->write_w->setText(QString("%1-%2-%3T%4:%5:%6")
                                  .arg(newDate.year()).arg(newDate.month())
                                  .arg(newDate.day()).arg(newTime.hour())
                                  .arg(newTime.minute()).arg(newTime.second()));
        pp->newText();
    }
    else return;

    SDProp  = pp->pg->dp->findProp("TIME_LST");
    if (!SDProp) return;
    timeEle = SDProp->findElement("LST");
    if (!timeEle) return;

    timeEle->write_w->setText(ksw->data()->lst()->toHMSString());
    SDProp->newText();
}

/*********************************************************************************/
/* Update the Driver's Time							 */
/*********************************************************************************/
void INDIStdDevice::updateTime()
{
    INDI_P *pp;
    INDI_E *lp;

    /* Update UTC */
    pp = dp->findProp("TIME_UTC_OFFSET");
    if (!pp) return;

    lp = pp->findElement("OFFSET");

    if (!lp) return;

    // Send DST corrected TZ
    lp->updateValue(ksw->data()->geo()->TZ());
    pp->newText();

    pp = dp->findProp("TIME_UTC");
    if (!pp) return;

    lp = pp->findElement("UTC");

    if (!lp) return;

    QTime newTime( ksw->data()->ut().time());
    QDate newDate( ksw->data()->ut().date());

    lp->write_w->setText(QString("%1-%2-%3T%4:%5:%6").arg(newDate.year()).arg(newDate.month())
                         .arg(newDate.day()).arg(newTime.hour())
                         .arg(newTime.minute()).arg(newTime.second()));
    pp->newText();

}

/*********************************************************************************/
/* Update the Driver's Geographical Location					 */
/*********************************************************************************/
void INDIStdDevice::updateLocation()
{
    INDI_P *pp;
    INDI_E * latEle, * longEle;
    GeoLocation *geo = ksw->data()->geo();

    pp = dp->findProp("GEOGRAPHIC_COORD");
    if (!pp) return;

    latEle  = pp->findElement("LAT");
    if (!latEle) return;
    longEle = pp->findElement("LONG");
    if (!longEle) return;

    if (geo->lng()->Degrees() >= 0)
        longEle->write_w->setText(geo->lng()->toDMSString());
    else
        longEle->write_w->setText( dms(geo->lng()->Degrees() + 360.0).toDMSString());

    latEle->write_w->setText(geo->lat()->toDMSString());

    pp->newText();
}

/*********************************************************************************/
/* If a new property is received, process it and trigger events when necessary   */
/*********************************************************************************/
void INDIStdDevice::registerProperty(INDI_P *pp)
{
    INDI_E * portEle;
    INDIDriver *drivers = ksw->indiDriver();
    QString str;

    switch (pp->stdID)
    {
    case DEVICE_PORT:
        portEle = pp->findElement("PORT");
        if (!portEle) return;

        if (drivers)
        {
            //for (unsigned int i=0; i < drivers->devices.size(); i++)
            foreach(IDevice *device, drivers->devices)
            {
                if (device->deviceManager == dp->deviceManager)
                {
                    if (device->deviceType == KSTARS_TELESCOPE)
                    {
                        portEle->read_w->setText( Options::telescopePort() );
                        portEle->write_w->setText( Options::telescopePort() );
                        portEle->text = Options::telescopePort();
                        break;
                    }
                    else if (device->deviceType == KSTARS_VIDEO)
                    {
                        portEle->read_w->setText( Options::videoPort() );
                        portEle->write_w->setText( Options::videoPort() );
                        portEle->text = Options::videoPort();
                        break;
                    }
                }
            }
        }
        break;

    case EQUATORIAL_EOD_COORD:
    case EQUATORIAL_EOD_COORD_REQUEST:
    case HORIZONTAL_COORD:
        emit newTelescope();
        break;

        // Update Device menu actions
        drivers->updateMenuActions();
    }
}

/*********************************************************************************/
/* Find out which properties KStars should listen to before declaring		 */
/* that a device is online.							 */
/*********************************************************************************/
void INDIStdDevice::createDeviceInit()
{

    INDI_P *prop;

    if ( Options::useTimeUpdate() && Options::useComputerSource())
    {
        prop = dp->findProp("TIME_UTC");
        if (prop)
        {
	    driverTimeUpdated = false;
            updateTime();
        }
    }

    if ( Options::useGeographicUpdate() && Options::useComputerSource())
    {
        prop = dp->findProp("GEOGRAPHIC_COORD");
        if (prop)
        {
	    driverLocationUpdated = false;
            updateLocation();
        }
    }

    if ( Options::showINDIMessages() )
        ksw->statusBar()->changeItem( i18n("%1 is online.", dp->name), 0);

    ksw->map()->forceUpdateNow();
}

/*********************************************************************************/
/* Declare a device is online only when the driver updates the necessary	 */
/* properties required by the user (e.g. time and location)			 */
/*********************************************************************************/
void INDIStdDevice::processDeviceInit()
{

    if ( driverTimeUpdated && driverLocationUpdated && Options::showINDIMessages() )
        ksw->statusBar()->changeItem( i18n("%1 is online and ready.", dp->name), 0);

}

/*********************************************************************************/
/* Process Non-Sidereal Tracking requests					 */
/*********************************************************************************/
bool INDIStdDevice::handleNonSidereal()
{
    if (!currentObject)
        return false;

    INDI_E *nameEle = NULL, *tracklp = NULL;

    if (currentObject == NULL || currentObject->isSolarSystem() == false)
        return false;

    kDebug() << "Object of type " << currentObject->typeName();

    // Only Meade Classic will offer an explicit SOLAR_SYSTEM property. If such a property exists
    // then we take advantage of it. Otherwise, we send RA/DEC to the telescope and start a timer
    // based on the object type. Objects with high proper motions will require faster updates.
    // handle Non Sideral is ONLY called when tracking an object, not slewing.
    INDI_P *prop = dp->findProp(QString("SOLAR_SYSTEM"));
    INDI_P *setMode = dp->findProp(QString("ON_COORD_SET"));

    // If the device support it
    if (prop && setMode)
    {
        tracklp = setMode->findElement( "TRACK" );
        if (tracklp == NULL) return false;

        kDebug() << "Device supports SOLAR_SYSTEM property";

        foreach(INDI_E *lp, prop->el)
        {
            if (currentObject->name().toLower() == lp->label.toLower())
            {
                prop->newSwitch(lp);
                setMode->newSwitch(tracklp);

                /* Send object name if available */
                nameEle = dp->findElem("OBJECT_NAME");
                if (nameEle && nameEle->pp->perm != PP_RO)
                {
                    nameEle->write_w->setText(currentObject->name());
                    nameEle->pp->newText();
                }

                return true;
            }
        }
    }

    kDebug() << "Device doesn't support SOLAR_SYSTEM property, issuing a timer";
    kDebug() << "Starting timer for object of type " << currentObject->typeName();


    switch (currentObject->type())
    {
        // Planet/Moon
    case 2:
    case 12:
        kDebug() << "Initiating pulse tracking for " << currentObject->name();
        devTimer->start(INDI_PULSE_TRACKING);
        break;
        // Comet/Asteroid
    case 9:
    case 10:
        kDebug() << "Initiating pulse tracking for " << currentObject->name();
        devTimer->start(INDI_PULSE_TRACKING);
        break;
    default:
        break;
    }

    return false;
}

/*********************************************************************************/
/* Issue new re-tracking command to the driver					 */
/*********************************************************************************/
void INDIStdDevice::timerDone()
{
    INDI_P *prop;
    INDI_E *RAEle, *DecEle;
    INDI_E *el;
    bool useJ2000 = false;

    if (!dp->isOn())
    {
        devTimer->stop();
        return;
    }

    prop = dp->findProp("ON_COORD_SET");
    if (prop == NULL || !currentObject)
        return;

    el   = prop->findElement("TRACK");
    if (!el) return;

    if (el->state != PS_ON)
    {
        devTimer->stop();
        return;
    }

    // We issue command to the REQUEST property
    prop = dp->findProp("EQUATORIAL_EOD_COORD_REQUEST");

    if (prop == NULL) {
        // Backward compatibility
        prop = dp->findProp("EQUATORIAL_EOD_COORD");
        if(prop == NULL) {
        	prop = dp->findProp("EQUATORIAL_COORD");
        	if(prop)
                useJ2000 = true;
        }
    }
    if (prop == NULL || !currentObject)
        return;

    // wait until slew is done
    if (prop->state == PS_BUSY)
        return;

    kDebug() << "Timer called, starting processing";

    SkyPoint sp = *currentObject;

    kDebug() << "RA: " << currentObject->ra().toHMSString() << " - DEC: " << currentObject->dec().toDMSString();
    kDebug() << "Az: " << currentObject->az().toHMSString() << " - Alt "  << currentObject->alt().toDMSString();

    if (useJ2000) {
        sp = *currentObject;
        sp.apparentCoord( ksw->data()->ut().djd() , (long double) J2000);
    }

    // We need to get from JNow (Skypoint) to J2000
    // The ra0() of a skyPoint is the same as its JNow ra() without this process

    // Use J2000 coordinate as required by INDI
    RAEle  = prop->findElement("RA");
    if (!RAEle) return;
    DecEle = prop->findElement("DEC");
    if (!DecEle) return;

    RAEle->write_w->setText(QString("%1:%2:%3").arg(sp.ra().hour())
                            .arg(sp.ra().minute())
                            .arg(sp.ra().second()));
    DecEle->write_w->setText(QString("%1:%2:%3").arg(sp.dec().degree())
                             .arg(sp.dec().arcmin())
                             .arg(sp.dec().arcsec()));
    prop->newText();

}

INDIStdProperty::INDIStdProperty(INDI_P *associatedProperty, KStars * kswPtr, INDIStdDevice *stdDevPtr)
{
    pp     = associatedProperty;
    ksw    = kswPtr;
    stdDev = stdDevPtr;
}

INDIStdProperty::~INDIStdProperty()
{

}

/*********************************************************************************/
/* Process TEXT & NUMBER properties about to be SENT TO INDI Driver 		 */
/*********************************************************************************/
void INDIStdProperty::newText()
{
    INDI_E *lp;
    INDIDriver *drivers = ksw->indiDriver();

    switch (pp->stdID)
    {
        /* Save Port name in KStars options */
    case DEVICE_PORT:
        lp = pp->findElement("PORT");

        if (lp && drivers)
        {
            //for (unsigned int i=0; i < drivers->devices.size(); i++)
            foreach( IDevice *device, drivers->devices)
            {
                if (device->deviceManager == stdDev->dp->deviceManager)
                {
                    if (device->deviceType == KSTARS_TELESCOPE)
                    {
                        Options::setTelescopePort( lp->text );
                    }
                    else if (device->deviceType == KSTARS_VIDEO)
                    {
                        Options::setVideoPort( lp->text );
                    }
                    break;
                }
            }
        }

        break;
    }

}

/*********************************************************************************/
/* Process SWITCH properties about to be SENT TO INDI Driver 		 	 */
/*********************************************************************************/
bool INDIStdProperty::newSwitch(INDI_E* el)
{
    INDI_P *prop;

    switch (pp->stdID)
    {
    case CONNECTION:
        if (el->name == QString("DISCONNECT"))
	{
	    /*********************************************************/
	    /* List of Actions to do when the device is DISCONNECTED */
	    /*********************************************************/

	    /* #1 Disable Stream, if enabled */
            stdDev->streamDisabled();

	    /* #2 set driverLocationUpdated and driverTimeUpdated to false*/
	    stdDev->driverLocationUpdated = false;
            stdDev->driverTimeUpdated = false;

	}
        else
        {
 	    /*********************************************************/
	    /* List of Actions to do when the device is CONNECTED    */
	    /*********************************************************/

	    /* #1 Set and update the device port */
            prop = pp->pg->dp->findProp("DEVICE_PORT");
            if (prop)
                prop->newText();
        }
        break;
    case TELESCOPE_ABORT_MOTION:
    case TELESCOPE_PARK:
    case TELESCOPE_MOTION_NS:
    case TELESCOPE_MOTION_WE:
        //TODO add text in the status bar "Slew aborted."
        stdDev->devTimer->stop();
        break;
    default:
        break;
    }

    return false;

}

/*******************************************************************************/
/* Move Telescope to targetted location			                       */
/*******************************************************************************/
bool INDIStdDevice::slew_scope(SkyPoint *scope_target, INDI_E *lp)
{

    INDI_E *RAEle(NULL), *DecEle(NULL), *AzEle(NULL), *AltEle(NULL), *trackEle(NULL);
    INDI_P *EqProp(NULL), *HorProp(NULL);
    bool useJ2000 (false);

    EqProp = dp->findProp("EQUATORIAL_EOD_COORD_REQUEST");
    if (EqProp == NULL)
    {
	// Backward compatibility
        EqProp = dp->findProp("EQUATORIAL_EOD_COORD");
        if (EqProp == NULL)
	{
	// J2000 Property
        EqProp = dp->findProp("EQUATORIAL_COORD_REQUEST");
        if (EqProp)
            useJ2000 = true;
        }
    }

    HorProp = dp->findProp("HORIZONTAL_COORD_REQUEST");

    if (EqProp && EqProp->perm == PP_RO)
		EqProp = NULL;

    if (HorProp && HorProp->perm == PP_RO)
          	HorProp = NULL;

    //kDebug() << "Skymap click - RA: " << scope_target->ra().toHMSString() << " DEC: " << scope_target->dec().toDMSString();

        if (EqProp)
        {
	            RAEle  = EqProp->findElement("RA");
	            if (!RAEle) return false;
	            DecEle = EqProp->findElement("DEC");
               	    if (!DecEle) return false;

           if (useJ2000)
                scope_target->apparentCoord(ksw->data()->ut().djd(), (long double) J2000);

              RAEle->write_w->setText(QString("%1:%2:%3").arg(scope_target->ra().hour()).arg(scope_target->ra().minute()).arg(scope_target->ra().second()));
              DecEle->write_w->setText(QString("%1:%2:%3").arg(scope_target->dec().degree()).arg(scope_target->dec().arcmin()).arg(scope_target->dec().arcsec()));

       }

        if (HorProp)
        {
	            AzEle = HorProp->findElement("AZ");
	            if (!AzEle) return false;
	            AltEle = HorProp->findElement("ALT");
	            if (!AltEle) return false;

            AzEle->write_w->setText(QString("%1:%2:%3").arg(scope_target->az().degree()).arg(scope_target->az().arcmin()).arg(scope_target->az().arcsec()));
            AltEle->write_w->setText(QString("%1:%2:%3").arg(scope_target->alt().degree()).arg(scope_target->alt().arcmin()).arg(scope_target->alt().arcsec()));

        }

        /* Could not find either properties! */
        if (EqProp == NULL && HorProp == NULL)
            return false;

	trackEle = lp;
        if (trackEle == NULL)
	{
	        trackEle = dp->findElem("TRACK");
	        if (trackEle == NULL)
	        {
		   trackEle = dp->findElem("SLEW");
		   if (trackEle == NULL)
			return false;
		}
         }

        trackEle->pp->newSwitch(trackEle);
        if (EqProp)
            EqProp->newText();
        if (HorProp)
            HorProp->newText();

	return true;
}

/*******************************************************************************/
/* INDI Standard Property is triggered from the context menu                   */
/*******************************************************************************/
bool INDIStdProperty::actionTriggered(INDI_E *lp)
{
    INDI_E * nameEle(NULL);

    switch (pp->stdID)
    {
        /* Handle Slew/Track/Sync */
    case ON_COORD_SET:
        // #1 set current object to NULL
        stdDev->currentObject = NULL;
        // #2 Deactivate timer if present
        if (stdDev->devTimer->isActive())
            stdDev->devTimer->stop();

        stdDev->currentObject = ksw->map()->clickedObject();

        // Track is similar to slew, except that for non-sidereal objects
        // it tracks the objects automatically via a timer.
        if ((lp->name == "TRACK"))
            if (stdDev->handleNonSidereal())
                return true;

           nameEle = stdDev->dp->findElem("OBJECT_NAME");
       	   if (nameEle && nameEle->pp->perm != PP_RO)
           {
               if (stdDev->currentObject == NULL)
			nameEle->write_w->setText("--");
		else
			nameEle->write_w->setText(stdDev->currentObject->name());
               nameEle->pp->newText();
           }

	        if (stdDev->currentObject == NULL)
		  return stdDev->slew_scope(ksw->map()->clickedPoint(), lp);
		else
		  return stdDev->slew_scope(static_cast<SkyPoint *> (stdDev->currentObject), lp);


        break;

        /* Handle Abort */
    case TELESCOPE_ABORT_MOTION:
        kDebug() << "Stopping timer.";
        stdDev->devTimer->stop();
        pp->newSwitch(lp);
        return true;
        break;

    case TELESCOPE_MOTION_NS:
        pp->newSwitch(lp);
        return true;
        break;

    case TELESCOPE_MOTION_WE:
        pp->newSwitch(lp);
        return true;
        break;

    default:
        break;
    }

    return false;

}


#include "indistd.moc"


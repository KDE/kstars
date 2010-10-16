/*  INDI frontend for KStars
    Copyright (C) 2003-2005 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

    Initial code based on Elwood Downey's Xephem

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    JM Changelog:
    2003-04-28 Used indimenu.c as a template. C --> C++, Xm --> KDE/Qt
    2003-05-01 Added tab for devices and a group feature
    2003-05-02 Added scrolling area. Most things are rewritten
    2003-05-05 Device/Group separation
    2003-05-29 Replaced raw INDI time with KStars's timedialog
    2003-08-02 Upgrading to INDI v 1.11
    2003-08-09 Initial support for non-sidereal tracking
    2004-01-15 redesigning the GUI to support INDI v1.2 and fix previous GUI bugs
               and problems. The new GUI can easily incoperate extensions to the INDI
	       protocol as required
    2005-10-30 Porting to KDE/QT 4
    2006-09-14 all char* to QString

 */

#include "indidevice.h"
#include "indiproperty.h"
#include "indigroup.h"
#include "devicemanager.h"
#include "indimenu.h"
#include "indidriver.h"
#include "indistd.h"
#include "kstars.h"
#include "skymap.h"
#include "skyobjects/skyobject.h"
#include "dialogs/timedialog.h"
#include "geolocation.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <zlib.h>
#include <indicom.h>
#include <base64.h>

#include <QFrame>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QLayout>
#include <QButtonGroup>
#include <QSocketNotifier>
#include <QDateTime>
#include <QSplitter>


#include <kled.h>
#include <klineedit.h>
#include <kpushbutton.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kcombobox.h>
#include <knuminput.h>
#include <kstatusbar.h>
#include <kmenu.h>
#include <kvbox.h>
#include <ktabwidget.h>
#include <ktextedit.h>

#define NINDI_STD	27

/* INDI standard property used across all clients to enable interoperability. */
const char * indi_std[NINDI_STD] =
    {"CONNECTION", "DEVICE_PORT", "TIME_UTC", "TIME_LST", "TIME_UTC_OFFSET" , "GEOGRAPHIC_COORD", "EQUATORIAL_COORD", "EQUATORIAL_EOD_COORD", "EQUATORIAL_EOD_COORD_REQUEST", "HORIZONTAL_COORD", "TELESCOPE_ABORT_MOTION", "ON_COORD_SET", "SOLAR_SYSTEM", "TELESCOPE_MOTION_NS", "TELESCOPE_MOTION_WE",  "TELESCOPE_PARK", "CCD_EXPOSURE", "CCD_TEMPERATURE", "CCD_FRAME", "CCD_FRAME_TYPE", "CCD_BINNING", "CCD_INFO", "VIDEO_STREAM", "FOCUS_SPEED", "FOCUS_MOTION", "FOCUS_TIMER", "FILTER_SLOT" };

/*******************************************************************
** INDI Device: The work-horse. Responsible for handling its
** child properties and managing signal and changes.
*******************************************************************/
INDI_D::INDI_D(INDIMenu *menuParent, DeviceManager *InParentManager, const QString &inName, const QString &inLabel) : KDialog( 0 )
  {
    name      		= inName;
    label     		= inLabel;
    parent		= menuParent;
    deviceManager 	= InParentManager;
  
    deviceVBox     	= new QSplitter();
    deviceVBox->setOrientation(Qt::Vertical);

    groupContainer 	= new KTabWidget();
  
    msgST_w        	= new KTextEdit();
    msgST_w->setReadOnly(true);

    enableBLOBC = new QCheckBox(i18n("Binary Transfer"));
    enableBLOBC->setChecked(true);
    enableBLOBC->setToolTip(i18n("Enable binary data transfer from driver to KStars and vice-versa."));

    connect(enableBLOBC, SIGNAL(stateChanged(int)), this, SLOT(setBLOBOption(int)));
  
    dataBuffer 		= (unsigned char *) malloc (1);
  
    stdDev 		= new INDIStdDevice(this, parent->ksw);
  
    curGroup      	= NULL;
  
    INDIStdSupport 	= false;

    deviceVBox->addWidget(groupContainer);
    deviceVBox->addWidget(msgST_w);
    deviceVBox->addWidget(enableBLOBC);

    parent->mainTabWidget->addTab(deviceVBox, label);
}

INDI_D::~INDI_D()
{
    while ( ! gl.isEmpty() ) delete gl.takeFirst();

    delete(deviceVBox);
    delete (stdDev);
    free (dataBuffer);
    dataBuffer = NULL;
    deviceVBox = NULL;
    stdDev     = NULL;
}

void INDI_D::registerProperty(INDI_P *pp)
{

    if (isINDIStd(pp))
        pp->pg->dp->INDIStdSupport = true;

    stdDev->registerProperty(pp);

}

bool INDI_D::isINDIStd(INDI_P *pp)
{
    for (uint i=0; i < NINDI_STD; i++)
        if (!strcmp(pp->name.toAscii(), indi_std[i]))
        {
            pp->stdID = i;
            return true;
        }

    return false;
}

/* Remove a property from a group, if there are no more properties
 * left in the group, then delete the group as well */
int INDI_D::removeProperty(INDI_P *pp)
{
    for (int i=0; i < gl.size(); i++)
        if (gl[i]->removeProperty(pp))
        {
            if (gl[i]->pl.count() == 0)
                delete gl.takeAt(i);
            return 0;
        }


    kDebug() << "INDI: Device " << name << " has no property named " << pp->name;
    return (-1);
}

/* implement any <set???> received from the device.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::setAnyCmd (XMLEle *root, QString & errmsg)
{
    XMLAtt *ap;
    INDI_P *pp;

    ap = findAtt (root, "name", errmsg);
    if (!ap)
        return (-1);

    pp = findProp (valuXMLAtt(ap));
    if (!pp)
    {
        errmsg = QString("INDI: <%1> device %2 has no property named %3").arg(tagXMLEle(root)).arg(name).arg(valuXMLAtt(ap));
        return (-1);
    }

    deviceManager->checkMsg (root, this);

    return (setValue (pp, root, errmsg));
}

/* set the given GUI property according to the XML command.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setValue (INDI_P *pp, XMLEle *root, QString & errmsg)
{
    XMLAtt *ap;

    /* set overall property state, if any */
    ap = findXMLAtt (root, "state");
    if (ap)
    {
        if (crackLightState (valuXMLAtt(ap), &pp->state) == 0)
            pp->drawLt (pp->state);
        else
        {
            errmsg = QString("INDI: <%1> bogus state %2 for %3 %4").arg(tagXMLEle(root)).arg(valuXMLAtt(ap)).arg(name).arg(pp->name);
            return (-1);
        }
    }

    /* allow changing the timeout */
    ap = findXMLAtt (root, "timeout");
    if (ap)
        pp->timeout = atof(valuXMLAtt(ap));

    /* process specific GUI features */
    switch (pp->guitype)
    {
    case PG_NONE:
        break;

    case PG_NUMERIC:	/* FALLTHRU */
    case PG_TEXT:
        return (setTextValue (pp, root, errmsg));
        break;

    case PG_BUTTONS:
    case PG_LIGHTS:
    case PG_RADIO:
    case PG_MENU:
        return (setLabelState (pp, root, errmsg));
        break;

    case PG_BLOB:
        return (setBLOB(pp, root, errmsg));
        break;

    default:
        break;
    }

    return (0);
}


/* set the given TEXT or NUMERIC property from the given element.
 * root should have <text> or <number> child.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setTextValue (INDI_P *pp, XMLEle *root, QString & errmsg)
{
    XMLEle *ep;
    XMLAtt *ap;
    INDI_E *lp;
    QString elementName;
    char iNumber[32];
    double min, max;

    for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0))
    {
        if (strcmp (tagXMLEle(ep), "oneText") && strcmp(tagXMLEle(ep), "oneNumber"))
            continue;

        ap = findXMLAtt(ep, "name");
        if (!ap)
        {
            kDebug() << "Error: unable to find attribute 'name' for property " << pp->name;
            return (-1);
        }

        elementName = valuXMLAtt(ap);

        lp = pp->findElement(elementName);

        if (!lp)
        {
            errmsg = QString("Error: unable to find element '%1' in property '%2'").arg(elementName).arg(pp->name);
            return (-1);
        }

        //fprintf(stderr, "tag okay, getting perm\n");
        switch (pp->perm)
        {
        case PP_RW:	// FALLTHRU
        case PP_RO:
            if (pp->guitype == PG_TEXT)
            {
                lp->text = QString(pcdataXMLEle(ep));
                lp->read_w->setText(lp->text);
            }
            else if (pp->guitype == PG_NUMERIC)
            {
                lp->value = atof(pcdataXMLEle(ep));
                numberFormat(iNumber, lp->format.toAscii(), lp->value);
                lp->text = iNumber;
                lp->read_w->setText(lp->text);

                ap = findXMLAtt (ep, "min");
                if (ap) { min = atof(valuXMLAtt(ap)); lp->setMin(min); }
                ap = findXMLAtt (ep, "max");
                if (ap) { max = atof(valuXMLAtt(ap)); lp->setMax(max); }

                /*if (lp->spin_w)
                {
                 lp->spin_w->setValue(lp->value);
                lp->spinChanged(lp->value);
                }*/

            }
            break;

        case PP_WO:
            // FIXME for WO properties, only min/max needs to be updated
            /* if (pp->guitype == PG_TEXT)
               lp->write_w->setText(QString(pcdataXMLEle(ep)));
             else*/ if (pp->guitype == PG_NUMERIC)
            {
                /*lp->value = atof(pcdataXMLEle(ep));
                numberFormat(iNumber, lp->format.toAscii(), lp->value);
                lp->text = iNumber;

                if (lp->spin_w)
                         lp->spin_w->setValue(lp->value);
                else
                   lp->write_w->setText(lp->text);*/

                ap = findXMLAtt (ep, "min");
                if (ap) { min = (int) atof(valuXMLAtt(ap)); lp->setMin(min); }
                ap = findXMLAtt (ep, "max");
                if (ap) { max = (int) atof(valuXMLAtt(ap)); lp->setMax(max); }
            }
            break;

        }
    }

    /* handle standard cases if needed */
    stdDev->setTextValue(pp);

    // suppress warning
    errmsg = errmsg;

    return (0);
}

/* set the given BUTTONS or LIGHTS property from the given element.
 * root should have some <switch> or <light> children.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setLabelState (INDI_P *pp, XMLEle *root, QString & errmsg)
{
    int menuChoice=0;
    unsigned i=0;
    XMLEle *ep;
    XMLAtt *ap;
    INDI_E *lp = NULL;
    int islight;
    PState state;

    /* for each child element */
    for (ep = nextXMLEle (root, 1), i=0; ep != NULL; ep = nextXMLEle (root, 0), i++)
    {

        /* only using light and switch */
        islight = !strcmp (tagXMLEle(ep), "oneLight");
        if (!islight && strcmp (tagXMLEle(ep), "oneSwitch"))
            continue;

        ap =  findXMLAtt (ep, "name");
        /* no name */
        if (!ap)
        {
            errmsg = QString("INDI: <%1> %2 %3 %4 requires name").arg(tagXMLEle(root)).arg(name,pp->name).arg(tagXMLEle(ep));
            return (-1);
        }

        if ((islight && crackLightState (pcdataXMLEle(ep), &state) < 0)
                || (!islight && crackSwitchState (pcdataXMLEle(ep), &state) < 0))
        {
            errmsg = QString("INDI: <%1> unknown state %2 for %3 %4 %5").arg(tagXMLEle(root)).arg(pcdataXMLEle(ep)).arg(name).arg(pp->name).arg(tagXMLEle(ep));
            return (-1);
        }

        /* find matching label */
        //fprintf(stderr, "Find matching label. Name from XML is %s\n", valuXMLAtt(ap));
        lp = pp->findElement(QString(valuXMLAtt(ap)));

        if (!lp)
        {
            errmsg = QString("INDI: <%1> %2 %3 has no choice named %4").arg(tagXMLEle(root)).arg(name).arg(pp->name).arg(valuXMLAtt(ap));
            return (-1);
        }

        QFont buttonFont;
        /* engage new state */
        lp->state = state;

        switch (pp->guitype)
        {
        case PG_BUTTONS:
            if (islight)
                break;

            lp->push_w->setDown(state == PS_ON ? true : false);
            buttonFont = lp->push_w->font();
            buttonFont.setBold(state == PS_ON ? true : false);
            lp->push_w->setFont(buttonFont);

            break;

        case PG_RADIO:
            lp->check_w->setChecked(state == PS_ON ? true : false);
            break;
        case PG_MENU:
            if (state == PS_ON)
            {
                if (menuChoice)
                {
                    errmsg = QString("INDI: <%1> %2 %3 has multiple ON states").arg(tagXMLEle(root)).arg(name).arg(pp->name);
                    return (-1);
                }
                menuChoice = 1;
                pp->om_w->setCurrentIndex(i);
            }
            break;

        case PG_LIGHTS:
            lp->drawLt();
            break;

        default:
            break;
        }

    }

    stdDev->setLabelState(pp);

    return (0);
}

/* Set BLOB vector. Process incoming data stream
 * Return 0 if okay, -1 if error
*/
int INDI_D::setBLOB(INDI_P *pp, XMLEle * root, QString & errmsg)
{
    XMLEle *ep;
    INDI_E *blobEL;

    for (ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0))
    {

        if (strcmp(tagXMLEle(ep), "oneBLOB") == 0)
        {

            blobEL = pp->findElement(QString(findXMLAttValu (ep, "name")));

            if (blobEL)
                return processBlob(blobEL, ep, errmsg);
            else
            {
                errmsg = QString("INDI: set %1.%2.%3 not found").arg(name).arg(pp->name).arg(findXMLAttValu(ep, "name"));
                return (-1);
            }
        }
    }

    return (0);

}

/* Process incoming data stream
 * Return 0 if okay, -1 if error
*/
int INDI_D::processBlob(INDI_E *blobEL, XMLEle *ep, QString & errmsg)
{
    XMLAtt *ap = NULL;
    int blobSize=0, r=0;
    DTypes dataType;
    uLongf dataSize=0;
    QString dataFormat;
    char *baseBuffer=NULL;
    unsigned char *blobBuffer(NULL);
    bool iscomp(false);

    ap = findXMLAtt(ep, "size");
    if (!ap)
    {
        errmsg = QString("INDI: set %1 size not found").arg(blobEL->name);
        return (-1);
    }

    dataSize = atoi(valuXMLAtt(ap));

    ap = findXMLAtt(ep, "format");
    if (!ap)
    {
        errmsg = QString("INDI: set %1 format not found").arg(blobEL->name);
        return (-1);
    }

    dataFormat = QString(valuXMLAtt(ap));

    baseBuffer = (char *) malloc ( (3*pcdatalenXMLEle(ep)/4) * sizeof (char));

    if (baseBuffer == NULL)
    {
	errmsg = QString("Unable to allocate memory for FITS base buffer");
	kDebug() << errmsg;
	return (-1);
    }
 
    blobSize   = from64tobits (baseBuffer, pcdataXMLEle(ep));
    blobBuffer = (unsigned char *) baseBuffer;

    /* Blob size = 0 when only state changes */
    if (dataSize == 0)
    {
        free (blobBuffer);
        return (0);
    }
    else if (blobSize < 0)
    {
        free (blobBuffer);
        errmsg = QString("INDI: %1.%2.%3 bad base64").arg(name).arg(blobEL->pp->name).arg(blobEL->name);
        return (-1);
    }

    iscomp = (dataFormat.indexOf(".z") != -1);

    dataFormat.remove(".z");

    if (dataFormat == ".fits") dataType = DATA_FITS;
    else if (dataFormat == ".stream") dataType = VIDEO_STREAM;
    else if (dataFormat == ".ccdpreview") dataType = DATA_CCDPREVIEW;
    else if (dataFormat.contains("ascii")) dataType = ASCII_DATA_STREAM;
    else dataType = DATA_OTHER;

    //kDebug() << "We're getting data with size " << dataSize;
    //kDebug() << "data format " << dataFormat;

    if (iscomp)
    {

        dataBuffer = (unsigned char *) realloc (dataBuffer,  (dataSize * sizeof(unsigned char)));
	if (baseBuffer == NULL)
        {
		free (blobBuffer);
		errmsg = QString("Unable to allocate memory for FITS data buffer");
		kDebug() << errmsg;
		return (-1);
    	}

        r = uncompress(dataBuffer, &dataSize, blobBuffer, (uLong) blobSize);
        if (r != Z_OK)
        {
            errmsg = QString("INDI: %1.%2.%3 compression error: %d").arg(name).arg(blobEL->pp->name).arg(r);
            free (blobBuffer);
            return -1;
        }

        //kDebug() << "compressed";
    }
    else
    {
        //kDebug() << "uncompressed!!";
        dataBuffer = (unsigned char *) realloc (dataBuffer,  (dataSize * sizeof(unsigned char)));
        memcpy(dataBuffer, blobBuffer, dataSize);
    }

    if (dataType == ASCII_DATA_STREAM && blobEL->pp->state != PS_BUSY)
    {
        stdDev->asciiFileDirty = true;

        if (blobEL->pp->state == PS_IDLE)
        {
            free (blobBuffer);
            return(0);
        }
    }

    stdDev->handleBLOB(dataBuffer, dataSize, dataFormat, dataType);

    free (blobBuffer);

    return (0);

}

bool INDI_D::isOn()
{

    INDI_P *prop;

    prop = findProp(QString("CONNECTION"));
    if (!prop)
        return false;

    return (prop->isOn(QString("CONNECT")));
}

INDI_P * INDI_D::addProperty (XMLEle *root, QString & errmsg)
{
    INDI_P *pp = NULL;
    INDI_G *pg = NULL;
    XMLAtt *ap = NULL;

    // Search for group tag
    ap = findAtt (root, "group", errmsg);
    if (!ap)
    {
        kDebug() << errmsg;
        return NULL;
    }
    // Find an existing group, if none found, create one
    pg = findGroup(QString(valuXMLAtt(ap)), 1, errmsg);

    if (!pg)
        return NULL;

    /* get property name and add new property to dp */
    ap = findAtt (root, "name", errmsg);
    if (ap == NULL)
        return NULL;

    if (findProp (valuXMLAtt(ap)))
    {
        errmsg = QString("INDI: <%1 %2 %3> already exists.").arg(tagXMLEle(root)).arg(name).arg(valuXMLAtt(ap));
        return NULL;
    }

    /* Remove Vertical spacer from group layout, this is done every time
      * a new property arrives. The spacer is then appended to the end of the
      * properties */
    pg->propertyLayout->removeItem(pg->VerticalSpacer);

    pp = new INDI_P(pg, QString(valuXMLAtt(ap)));

    /* init state */
    ap = findAtt (root, "state", errmsg);
    if (!ap)
    {
        delete(pp);
        return (NULL);
    }

    if (crackLightState (valuXMLAtt(ap), &pp->state) < 0)
    {
        errmsg = QString("INDI: <%1> bogus state %2 for %3 %4").arg(tagXMLEle(root)).arg(valuXMLAtt(ap)).arg(pp->pg->dp->name).arg(pp->name);
        delete(pp);
        return (NULL);
    }

    /* init timeout */
    ap = findAtt (root, "timeout", errmsg);
    /* default */
    pp->timeout = ap ? atof(valuXMLAtt(ap)) : 0;

    /* log any messages */
    deviceManager->checkMsg (root, this);

    pp->addGUI(root);

    /* ok! */
    return (pp);
}

INDI_P * INDI_D::findProp (const QString &name)
{
    for (int i = 0; i < gl.size(); i++)
        for (int j = 0; j < gl[i]->pl.size(); j++)
            if (name == gl[i]->pl[j]->name)
                return (gl[i]->pl[j]);

    return NULL;
}

INDI_G *  INDI_D::findGroup (const QString &grouptag,
                             int create, QString & errmsg)
{

    for ( int i=0; i < gl.size(); ++i ) {
        if (gl[i]->name == grouptag)
        {
            curGroup = gl[i];
            return gl[i];
        }
    }

    /* couldn't find an existing group, create a new one if create is 1*/
    if (create)
    {
        QString newgrouptag = grouptag;
        if (newgrouptag.isEmpty())
            newgrouptag = QString("Group%1").arg(gl.size() + 1);

        curGroup = new INDI_G(this, newgrouptag);
        gl.append(curGroup);
        return curGroup;
    }

    errmsg = QString("INDI: group %1 not found in %2").arg(grouptag).arg(name);
    return NULL;
}


/* find "perm" attribute in root, crack and set *pp.
 * return 0 if ok else -1 with excuse in errmsg[]
 */

int INDI_D::findPerm (INDI_P *pp, XMLEle *root, PPerm *permp, QString & errmsg)
{
    XMLAtt *ap;

    ap = findXMLAtt(root, "perm");
    if (!ap)
    {
        errmsg = QString("INDI: <%1 %2 %2> missing attribute 'perm'").arg(tagXMLEle(root)).arg(pp->pg->dp->name).arg(pp->name);
        return (-1);
    }
    if (!strcmp(valuXMLAtt(ap), "ro") || !strcmp(valuXMLAtt(ap), "r"))
        *permp = PP_RO;
    else if (!strcmp(valuXMLAtt(ap), "wo"))
        *permp = PP_WO;
    else if (!strcmp(valuXMLAtt(ap), "rw") || !strcmp(valuXMLAtt(ap), "w"))
        *permp = PP_RW;
    else {
        errmsg = QString("INDI: <%1> unknown perm %2 for %3 %4").arg(tagXMLEle(root)).arg(valuXMLAtt(ap)).arg(pp->pg->dp->name).arg(pp->name);
        return (-1);
    }

    return (0);
}

/* convert the given light/property state string to the PState at psp.
 * return 0 if successful, else -1 and leave *psp unchanged.
 */
int INDI_D::crackLightState (char *name, PState *psp)
{
    typedef struct
    {
        PState s;
        const char *name;
    } PSMap;

    PSMap psmap[] =
        {
            {PS_IDLE,  "Idle"},
            {PS_OK,    "Ok"},
            {PS_BUSY,  "Busy"},
            {PS_ALERT, "Alert"},
        };

    for (int i = 0; i < 4; i++)
        if (!strcmp (psmap[i].name, name)) {
            *psp = psmap[i].s;
            return (0);
        }

    return (-1);
}

/* convert the given switch state string to the PState at psp.
 * return 0 if successful, else -1 and leave *psp unchanged.
 */
int INDI_D::crackSwitchState (char *name, PState *psp)
{
    typedef struct
    {
        PState s;
        const char *name;
    } PSMap;

    PSMap psmap[] =
        {
            {PS_ON,  "On"},
            {PS_OFF, "Off"},
        };


    for (int i = 0; i < 2; i++)
        if (!strcmp (psmap[i].name, name))
        {
            *psp = psmap[i].s;
            return (0);
        }

    return (-1);
}

int INDI_D::buildTextGUI(XMLEle *root, QString & errmsg)
{
    INDI_P *pp = NULL;
    int err_code=0;
    PPerm p;
    bool isGroupVisible=false;

    /* build a new property */
    pp = addProperty (root, errmsg);

    if (pp == NULL)
        return DeviceManager::INDI_PROPERTY_DUPLICATED;

    /* get the permission, it will determine layout issues */
    if (findPerm (pp, root, &p, errmsg))
    {
        delete(pp);
        return DeviceManager::INDI_PROPERTY_INVALID;
    }

    /* we know it will be a general text GUI */
    pp->guitype = PG_TEXT;
    pp->perm = p;

    if (pp->pg->propertyContainer->isVisible())
    {
        isGroupVisible = true;
        pp->pg->dp->parent->mainTabWidget->hide();
    }

    if ( (err_code = pp->buildTextGUI(root, errmsg)) < 0)
    {
        delete (pp);
        return err_code;
    }

    pp->pg->addProperty(pp);

    if (isGroupVisible)
        pp->pg->dp->parent->mainTabWidget->show();

    return (0);
}

/* build GUI for a number property.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::buildNumberGUI (XMLEle *root, QString & errmsg)
{
    INDI_P *pp = NULL;
    int err_code=0;
    PPerm p;
    bool isGroupVisible=false;

    /* build a new property */
    pp = addProperty (root, errmsg);

    if (pp == NULL)
        return DeviceManager::INDI_PROPERTY_DUPLICATED;

    /* get the permission, it will determine layout issues */
    if (findPerm (pp, root, &p, errmsg))
    {
        delete(pp);
        return DeviceManager::INDI_PROPERTY_INVALID;
    }

    /* we know it will be a number GUI */
    pp->guitype = PG_NUMERIC;
    pp->perm = p;

    if (pp->pg->propertyContainer->isVisible())
    {
        isGroupVisible = true;
        pp->pg->dp->parent->mainTabWidget->hide();
    }

    if ( (err_code = pp->buildNumberGUI(root, errmsg)) < 0)
    {
        delete (pp);
        return err_code;
    }

    pp->pg->addProperty(pp);

    if (isGroupVisible)
        pp->pg->dp->parent->mainTabWidget->show();


    return (0);
}

/* build GUI for switches property.
 * rule and number of will determine exactly how the GUI is built.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::buildSwitchesGUI (XMLEle *root, QString & errmsg)
{
    INDI_P *pp;
    XMLAtt *ap;
    XMLEle *ep;
    int n, err;
    bool isGroupVisible=false;

    /* build a new property */
    pp = addProperty (root, errmsg);
    if (!pp)
        return DeviceManager::INDI_PROPERTY_DUPLICATED;

    ap = findAtt (root, "rule", errmsg);
    if (!ap)
    {
        delete(pp);
        return DeviceManager::INDI_PROPERTY_INVALID;
    }

    /* decide GUI. might use MENU if OneOf but too many for button array */
    if (!strcmp (valuXMLAtt(ap), "OneOfMany") || !strcmp (valuXMLAtt(ap), "AtMostOne"))
    {
        /* count number of switches -- make menu if too many */
        for ( ep = nextXMLEle(root, 1) , n = 0 ; ep != NULL; ep = nextXMLEle(root, 0))
            if (!strcmp (tagXMLEle(ep), "defSwitch"))
                n++;

        if (n > MAXRADIO)
        {
            pp->guitype = PG_MENU;
            if (pp->pg->propertyContainer->isVisible())
            {
                isGroupVisible = true;
                pp->pg->dp->parent->mainTabWidget->hide();
            }
            err = pp->buildMenuGUI (root, errmsg);
            if (err < 0)
            {
                delete(pp);
                pp = 0;
                return err;
            }

            pp->pg->addProperty(pp);
            if (isGroupVisible)
                pp->pg->dp->parent->mainTabWidget->show();
            return (err);
        }

        /* otherwise, build 1-4 button layout */
        pp->guitype = PG_BUTTONS;
        if (pp->pg->propertyContainer->isVisible())
        {
            isGroupVisible = true;
            pp->pg->dp->parent->mainTabWidget->hide();
        }
        err = pp->buildSwitchesGUI(root, errmsg);
        if (err < 0)
        {
            delete (pp);
            pp =0;
            return err;
        }

        pp->pg->addProperty(pp);
        if (isGroupVisible)
            pp->pg->dp->parent->mainTabWidget->show();
        return (err);

    }
    else if (!strcmp (valuXMLAtt(ap), "AnyOfMany"))
    {
        /* 1-4 checkboxes layout */
        pp->guitype = PG_RADIO;
        if (pp->pg->propertyContainer->isVisible())
        {
            isGroupVisible = true;
            pp->pg->dp->parent->mainTabWidget->hide();
        }
        err = pp->buildSwitchesGUI(root, errmsg);
        if (err < 0)
        {
            delete (pp);
            pp=0;
            return err;
        }

        if (isGroupVisible)
            pp->pg->dp->parent->mainTabWidget->show();
        pp->pg->addProperty(pp);
        return (err);
    }

    errmsg = QString("INDI: <%1> unknown rule %2 for %3 %4").arg(tagXMLEle(root)).arg(valuXMLAtt(ap)).arg(name).arg(pp->name);

    delete(pp);
    return DeviceManager::INDI_PROPERTY_INVALID;
}



/* build GUI for a lights GUI.
 * return 0 if ok, else -1 with reason in errmsg[] */
int INDI_D::buildLightsGUI (XMLEle *root, QString & errmsg)
{
    INDI_P *pp;
    int err_code=0;
    bool isGroupVisible=false;

    // build a new property
    pp = addProperty (root, errmsg);
    if (!pp)
       return DeviceManager::INDI_PROPERTY_DUPLICATED;

    pp->guitype = PG_LIGHTS;

    if (pp->pg->propertyContainer->isVisible())
    {
        isGroupVisible = true;
        pp->pg->dp->parent->mainTabWidget->hide();
    }

    if ( (err_code = pp->buildLightsGUI(root, errmsg)) < 0)
    {
        delete (pp);
        return err_code;
    }

    pp->pg->addProperty(pp);

    if (isGroupVisible)
        pp->pg->dp->parent->mainTabWidget->show();

    return (0);
}

/* build GUI for a BLOB GUI.
 * return 0 if ok, else -1 with reason in errmsg[] */
int INDI_D::buildBLOBGUI  (XMLEle *root, QString & errmsg)
{
    INDI_P *pp;
    int err_code=0;
    PPerm p;
    bool isGroupVisible=false;

    // build a new property
    pp = addProperty (root, errmsg);
    if (!pp)
       return DeviceManager::INDI_PROPERTY_DUPLICATED;

    /* get the permission, it will determine layout issues */
    if (findPerm (pp, root, &p, errmsg))
    {
        delete(pp);
        return DeviceManager::INDI_PROPERTY_INVALID;
    }

    /* we know it will be a number GUI */
    pp->perm = p;
    pp->guitype = PG_BLOB;

    if (pp->pg->propertyContainer->isVisible())
    {
        isGroupVisible = true;
        pp->pg->dp->parent->mainTabWidget->hide();
    }

    if ( (err_code = pp->buildBLOBGUI(root, errmsg)) < 0)
    {
        delete (pp);
        return err_code;
    }

    pp->pg->addProperty(pp);

    if (isGroupVisible)
        pp->pg->dp->parent->mainTabWidget->show();

    return (0);
}

INDI_E * INDI_D::findElem(const QString &name)
{
    INDI_G *grp;
    INDI_P *prop;
    INDI_E *el;

    for ( int i=0; i < gl.size(); ++i ) {
        grp = gl[i];
        for ( int j=0; j < grp->pl.size(); j++)
        {
            prop = grp->pl[j];
            el = prop->findElement(name);
            if (el != NULL) return el;
        }
    }

    return NULL;
}

void INDI_D::engageTracking()
{

    if (!isOn()) return;

    if (stdDev->telescopeSkyObject == NULL) return;

    stdDev->ksw->map()->setClickedObject(stdDev->telescopeSkyObject);
    stdDev->ksw->map()->setClickedPoint(stdDev->telescopeSkyObject);
    stdDev->ksw->map()->slotCenter();
    return;

}

void INDI_D::setBLOBOption(int state)
{
    if (deviceManager == NULL)
        return;

    if (state == Qt::Checked)
        deviceManager->enableBLOB(true, name);
    else
        deviceManager->enableBLOB(false, name);
}

#include "indidevice.moc"

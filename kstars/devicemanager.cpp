/*  Device Manager
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    JM Changelog
    2004-16-1:	Start
   
 */
 
#include "devicemanager.h"
#include "indimenu.h"
#include "indiproperty.h"
#include "indigroup.h"
#include "indidevice.h"
#include "kstars.h"
 
#include <qsocketnotifier.h>
#include <qtextedit.h>

#include <kdebug.h>
#include <kmessagebox.h>
#include <kstatusbar.h>
 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
 
 /*******************************************************************
** The device manager contain devices running from one indiserver
** This allow KStars to control multiple devices distributed acorss
** multiple servers seemingly in a way that is completely transparent
** to devices and drivers alike.
** The device Manager can be thought of as the 'networking' parent
** of devices, while indimenu is 'GUI' parent of devices
*******************************************************************/

DeviceManager::DeviceManager(INDIMenu *INDIparent, int inID) 
{

 parent = INDIparent;
 mgrID  = inID;

 indi_dev.setAutoDelete(true);
 
 serverFD  = -1;
 serverFP  = NULL;
 XMLParser = NULL;
 sNotifier = NULL;

}

DeviceManager::~DeviceManager()
{
  
   if (serverFP)
    	fclose(serverFP);
	
  if (serverFD >= 0)
    close(serverFD);
  
  if (XMLParser)
  {
   delLilXML(XMLParser);
   XMLParser = NULL;
  }
  
  indi_dev.clear();

}

bool DeviceManager::indiConnect(QString host, QString port)
{

	struct sockaddr_in pin;
	struct hostent *serverHostName = gethostbyname(host.ascii());
	QString errMsg;
	errMsg = QString("Connection to INDI host at %1 on port %2 failed.").arg(host).arg(port);


	memset(&pin, 0, sizeof(pin));
	pin.sin_family 		= AF_INET;
	pin.sin_addr.s_addr 	= ((struct in_addr *) (serverHostName->h_addr))->s_addr;
	pin.sin_port 		= htons(port.toInt());

	if ( (serverFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
	 KMessageBox::error(0, i18n("Can not create socket"));
	 return false;
	}

	if ( ::connect(serverFD, (struct sockaddr*) &pin, sizeof(pin)) == -1)
	{
	  KMessageBox::error(0, errMsg);
	  serverFD = -1;
	  return false;
	}

	// callback notified
	sNotifier = new QSocketNotifier( serverFD, QSocketNotifier::Read, this);
        QObject::connect( sNotifier, SIGNAL(activated(int)), this, SLOT(dataReceived()));

	if (XMLParser)
	   delLilXML(XMLParser);
	XMLParser = newLilXML();

	// ready for fprintf
	serverFP = fdopen(serverFD, "w");

	if (serverFP == NULL)
	{
	 KMessageBox::error(0, i18n("Can't read server file descriptor"));
	 serverFD = -1;
	 return false;
	}

	setbuf (serverFP, NULL);

	fprintf(serverFP, "<getProperties version='0.1'/>\n");

	// We made it!
	return true;
}


void DeviceManager::dataReceived()
{
	char ibuf[32];	/* not so much user input lags */
	char msg[1024];
	int i, nr;

	/* read INDI command */
	nr = read (serverFD, ibuf, sizeof(ibuf));
	if (nr <= 0)
	{
	    if (nr < 0)
		sprintf (msg, "INDI: input error.");
	    else
		sprintf (msg, "INDI: agent closed connection.");


            tcflush(serverFD, TCIFLUSH);
	    sNotifier->disconnect();
	    close(serverFD);
	    parent->removeDeviceMgr(mgrID);
	    KMessageBox::error(0, QString(msg));

            return;
	}

	/* process each char */
	for (i = 0; i < nr; i++)
	{
  	  if (!XMLParser)
	     	return;

	    XMLEle *root = readXMLEle (XMLParser, (int)ibuf[i], msg);
	    if (root)
	    {
                //prXMLEle (stdout, root, 0);
		if (dispatchCommand(root, msg) < 0)
		{
		    //kdDebug() << msg << endl;
		    fprintf(stderr, msg);
		    prXMLEle (stdout, root, 0);
		}

		delXMLEle (root);
	    }
	    else if (msg[0])
	    {
		kdDebug() << msg << endl;
	    }
	}
}

int DeviceManager::dispatchCommand(XMLEle *root, char errmsg[])
{

  if  (!strcmp (tagXMLEle(root), "message"))
    	 return messageCmd(root, errmsg);
  else if  (!strcmp (tagXMLEle(root), "delProperty"))
        return delPropertyCmd(root, errmsg);

  /* Get the device, if not available, create it */
  INDI_D *dp = findDev (root, 1, errmsg);
      if (dp == NULL)
	    return -1;

   if (!strcmp (tagXMLEle(root), "defTextVector"))
      return dp->buildTextGUI(root, errmsg);
   else if (!strcmp (tagXMLEle(root), "defNumberVector"))
      return dp->buildNumberGUI(root, errmsg);
   else if (!strcmp (tagXMLEle(root), "defSwitchVector"))
       return dp->buildSwitchesGUI(root, errmsg);
   else if (!strcmp (tagXMLEle(root), "defLightVector"))
        return dp->buildLightsGUI(root, errmsg);
   else if (!strcmp (tagXMLEle(root), "setTextVector") ||
   	    !strcmp (tagXMLEle(root), "setNumberVector") ||
	    !strcmp (tagXMLEle(root), "setSwitchVector") ||
	    !strcmp (tagXMLEle(root), "setLightVector"))
	return dp->setAnyCmd(root, errmsg);

   return (-1);
}

/* delete the property in the given device, including widgets and data structs.
 * when last property is deleted, delete the device too.
 * if no property name attribute at all, delete the whole device regardless.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int DeviceManager::delPropertyCmd (XMLEle *root, char errmsg[])
{

	XMLAtt *ap;
	INDI_D *dp;
	INDI_P *pp;

	/* dig out device and optional property name */
	dp = findDev (root, 0, errmsg);
	if (!dp)
	    return (-1);
	    
	checkMsg(root, dp);
	
	ap = findXMLAtt (root, "name");

	/* Delete property if it exists, otherwise, delete the whole device */
	if (ap)
	{
	  pp = dp->findProp(QString(valuXMLAtt(ap)));

	 if(pp)
	 return dp->removeProperty(pp);
	    else
	      return (-1);
	}
	// delete the whole device
	else
            return removeDevice(dp->name, errmsg);

}

int DeviceManager::removeDevice(QString devName, char errmsg[])
{

    // remove all devices if devName == NULL
    if (devName == NULL)
    {
        indi_dev.clear();
	return (0);
    }

    for (unsigned int i=0; i < indi_dev.count(); i++)
    {
         if (indi_dev.at(i)->name ==  devName)
	 {
	    kdDebug() << "Device Manager: Device found, deleting " << devName << endl;
	    indi_dev.remove(i);
            return (0);
	 }
    }

   sprintf(errmsg, "Device %s not found" , devName.ascii());
   return -1;
}

INDI_D * DeviceManager::findDev (QString devName, char errmsg[])
{
	/* search for existing */
	for (unsigned int i = 0; i < indi_dev.count(); i++)
	{
	    if (indi_dev.at(i)->name == devName)
		return (indi_dev.at(i));
	}

	sprintf (errmsg, "INDI: no such device %s", devName.ascii());
	kdDebug() << errmsg;

	return NULL;
}

/* add new device to mainrc_w using info in dep.
- * if trouble return NULL with reason in errmsg[]
- */
INDI_D * DeviceManager::addDevice (XMLEle *dep, char errmsg[])
{
	INDI_D *dp;
	XMLAtt *ap;

	/* allocate new INDI_D on indi_dev */
	ap = findAtt (dep, "device", errmsg);
	if (!ap)
	    return NULL;

	 // FIXME change the mechanism by which naming is assiged.
	 // Make a list (QStristList or something) that holds active devices
	 // And each new device should be checked against this list.
	if (parent->currentLabel.isEmpty())
	 parent->setCustomLabel(valuXMLAtt(ap));

	//fprintf(stderr, "\n\n\n ***************** Adding a device %s with label %s *************** \n\n\n", valuXMLAtt(ap), label);
	dp = new INDI_D(parent, this, QString(valuXMLAtt(ap)), parent->currentLabel);

	parent->currentLabel = "";

	indi_dev.append(dp);
	
       /* ok */
	return dp;
}

INDI_D * DeviceManager::findDev (XMLEle *root, int create, char errmsg[])
{
	XMLAtt *ap;
	char *dn;

	/* get device name */
	ap = findAtt (root, "device", errmsg);
	if (!ap)
	    return (NULL);
	dn = valuXMLAtt(ap);

	/* search for existing */
	for (uint i = 0; i < indi_dev.count(); i++)
	{
	    if (indi_dev.at(i)->name == QString(dn))
		return (indi_dev.at(i));
	}

	/* not found, create if ok */
	if (create)
		return (addDevice (root, errmsg));


	sprintf (errmsg, "INDI: <%s> no such device %s", tagXMLEle(root), dn);
	return NULL;
}

/* a general message command received from the device.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int DeviceManager::messageCmd (XMLEle *root, char errmsg[])
{
	checkMsg (root, findDev (root, 0, errmsg));
	return (0);
}

/* display message attribute.
 * N.B. don't put carriage control in msg, we take care of that.
 */
void DeviceManager::checkMsg (XMLEle *root, INDI_D *dp)
{
	XMLAtt *ap;
	ap = findXMLAtt(root, "message");
	  
	if (ap)
	  doMsg(root, dp);
}

/* display valu of message and timestamp in dp's scrolled area, if any, else general.
 * prefix our time stamp if not included.
 * N.B. don't put carriage control in msg, we take care of that.
 */
void DeviceManager::doMsg (XMLEle *msg, INDI_D *dp)
{
	QTextEdit *txt_w;
	XMLAtt *message;
	XMLAtt *timestamp;

	/* determine whether device or generic message
	   
	   FIXME: The policy now is that generic messages get posted in all windows but with
	   different color and/or font as to get attention. Make this configurable. Also make it 
	   configurable to automatically save all INDI logs.
	 */
	/*if (dp)*/
	    txt_w = dp->msgST_w;
	/*else
	    txt_w = parent->msgST_w;*/

	/* prefix our timestamp if not with msg */
	timestamp = findXMLAtt (msg, "timestamp");
	if (timestamp)
            txt_w->insert(QString(valuXMLAtt(timestamp)) + QString(" "));
	else
	   txt_w->insert( (QDateTime().currentDateTime()).toString("yyyy/mm/dd - h:m:s ap "));

	/* finally! the msg */
        message = findXMLAtt(msg, "message");
	
       txt_w->insert( QString(valuXMLAtt(message)) + QString("\n"));
       
       if (parent->ksw->options()->indiMessages)
	    	parent->ksw->statusBar()->changeItem( QString(valuXMLAtt(message)), 0);

}

void DeviceManager::sendNewText (INDI_P *pp)
{
        INDI_E *lp;
	
	fprintf(serverFP, "<newTextVector\n");
	fprintf(serverFP, "  device='%s'\n", pp->pg->dp->name.ascii());
	fprintf(serverFP, "  name='%s'\n>", pp->name.ascii());
	
	for (lp = pp->el.first(); lp != NULL; lp = pp->el.next())
	{
	    fprintf(serverFP, "  <oneText\n");
	    fprintf(serverFP, "    name='%s'>\n", lp->name.ascii());
	    fprintf(serverFP, "      %s\n", lp->text.ascii());
	    fprintf(serverFP, "  </oneText>\n");
	}
	fprintf(serverFP, "</newTextVector>\n");
}

void DeviceManager::sendNewNumber (INDI_P *pp)
{
        INDI_E *lp;
	
        fprintf(serverFP, "<newNumberVector\n");
	fprintf(serverFP, "  device='%s'\n", pp->pg->dp->name.ascii());
	fprintf(serverFP, "  name='%s'\n>", pp->name.ascii());
	
	for (lp = pp->el.first(); lp != NULL; lp = pp->el.next())
	{
	    fprintf(serverFP, "  <oneNumber\n");
	    fprintf(serverFP, "    name='%s'>\n", lp->name.ascii());
	    fprintf(serverFP, "      %g\n", lp->targetValue);
	    fprintf(serverFP, "  </oneNumber>\n");
	}
	fprintf(serverFP, "</newNumberVector>\n");

}

void DeviceManager::sendNewSwitch (INDI_P *pp)
{  
        INDI_E *lp;

	fprintf (serverFP,"<newSwitchVector\n");
	fprintf (serverFP,"  device='%s'\n", pp->pg->dp->name.ascii());
	fprintf (serverFP,"  name='%s'>\n", pp->name.ascii());
	
	for (lp = pp->el.first(); lp != NULL; lp = pp->el.next())
        {
	    fprintf (serverFP,"  <oneSwitch\n");
	    fprintf (serverFP,"    name='%s'>\n", lp->name.ascii());
	    fprintf (serverFP,"      %s\n", lp->state == PS_ON ? "On" : "Off");
	    fprintf (serverFP,"  </oneSwitch>\n");
	}
	fprintf (serverFP, "</newSwitchVector>\n");

}


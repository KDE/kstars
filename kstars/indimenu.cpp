/*  INDI frontend for KStars
    Copyright (C) 2003 Elwood C. Downey

    Adapted to KStars by Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    JM Changelog:
    2003-04-28 Used indimenu.c as a template. C --> C++, Xm --> KDE/Qt
    2003-05-01 Added tab for devices and a group feature
    2003-05-02 Added scrolling area. Most things are rewritten

 */

#include "indimenu.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>

#include <qlineedit.h>
#include <qtextedit.h>
#include <qframe.h>
#include <qtabwidget.h>
#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qsocketnotifier.h>
#include <qdatetime.h>
#include <qbuttongroup.h>
#include <qscrollview.h>
#include <qvbox.h>

#include <kled.h>
#include <klineedit.h>
#include <klistbox.h>
#include <kpushbutton.h>
#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <klistview.h>
#include <kdebug.h>
#include <kcombobox.h>
#include <knuminput.h>
#include <kdialogbase.h>

//TODO class seperation into multiple files


/*******************************************************************
** INDI Label
*******************************************************************/
INDI_L::INDI_L(INDI_P *parentProperty, char *inName)
{
  name = new char[64];
  strcpy(name, inName);
  pp = parentProperty;
}

/*******************************************************************
** INDI Property: contains widgets, labels, and their status
*******************************************************************/
INDI_P::INDI_P(INDI_D *parentDevice, char * inName)
{
  name = new char[64];
  strcpy(name, inName);
  dp = parentDevice;

  u.multi.push_v  = new std::vector<QPushButton *>;
  u.multi.check_v = new std::vector<QCheckBox   *>;
  u.multi.label_v = new std::vector<QLabel      *>;
  u.multi.led_v   = new std::vector<KLed        *>;

  light = NULL;
  label_w = NULL;

  u.text.read_w = NULL;
  u.text.write_w = NULL;
  u.text.set_w = NULL;

  u.scale.read_w = NULL;
  u.scale.spin_w = NULL;
  u.scale.set_w  = NULL;

  u.multi.om_w = NULL;
  u.multi.groupB = NULL;
  u.multi.hLayout = NULL;

  u.multi.labels  = new std::vector<INDI_L *>;

}

/* INDI property desstructor, makes sure everything is "gone" right */
INDI_P::~INDI_P()
{
    if (light)
      delete(light);

    if (label_w)
      delete(label_w);

    switch (guitype)
    {
      case PG_TEXT:
      case PG_NUMERIC:
        switch (u.text.perm)
	{
	  case PP_RW:
	   delete (u.text.read_w);
	   delete (u.text.write_w);
	   delete (u.text.set_w);
	   break;

	  case PP_RO:
	  delete (u.text.read_w);
	  break;

	  case PP_WO:
	  delete (u.text.write_w);
	  delete (u.text.set_w);
	  break;
        }
	return;
	break;

     case PG_SCALE:
        switch (u.scale.perm)
	{
   	  case PP_RW:
	   delete (u.scale.read_w);
	   delete (u.scale.spin_w);
	   delete (u.scale.set_w);
	   break;

	  case PP_RO:
	  delete (u.scale.read_w);
	  break;

	  case PP_WO:
	  delete (u.scale.spin_w);
	  delete (u.scale.set_w);
	  break;
        }
	return;
	break;

      case PG_BUTTONS:
       delete (u.multi.groupB);
       delete (u.multi.hLayout);
       for (uint i=0; i < u.multi.push_v->size(); i++)
          delete ((*u.multi.push_v)[i]);
      delete u.multi.push_v;
      break;

      case PG_RADIO:
       delete (u.multi.groupB);
       delete (u.multi.hLayout);
       for (uint i=0; i < u.multi.check_v->size(); i++)
        delete ((*u.multi.check_v)[i]);

       delete u.multi.check_v;
       break;

      case PG_MENU:
       delete (u.multi.om_w);
       break;

      case PG_LIGHTS:
      for (uint i=0; i < u.multi.led_v->size(); i++)
      delete ((*u.multi.led_v)[i]);

      delete u.multi.led_v;

     default: break;
  }

     for (uint i=0; i < u.multi.labels->size(); i++)
      {
        delete ((*u.multi.labels)[i]);
	u.multi.labels->erase(u.multi.labels->begin() + i);
      }

}

/*******************************************************************
** INDI Group: a group box for common properties. All properties
** belong to a group, whether they have one or not but how the group
** is displayed differs
*******************************************************************/
INDI_G::INDI_G(INDI_P *pp, char *inName)
{
  nElements = -1;


 if (!strcmp(inName, ""))
  {
    box = new QGroupBox(pp->dp->propertyLayout);
    box->setFrameShape(QFrame::NoFrame);
    layout = new QGridLayout(box, 1, 1, 5 , 5);
  }
  else
  {
  box = new QGroupBox(QString(inName), pp->dp->propertyLayout);
  box->setColumnLayout(0, Qt::Vertical );
  box->layout()->setSpacing( 5 );
  box->layout()->setMargin( 5 );
  layout = new QGridLayout(box->layout());
  }

}

INDI_G::~INDI_G()
{
  delete(layout);
  delete(box);
}

/*******************************************************************
** INDI Device: The work-horse. Responsible for handling its
** child properties and managing signal and changes.
*******************************************************************/
INDI_D::INDI_D(INDIMenu *menuParent, char * inName)
{
  name = new char[64];
  strcpy(name, inName);
  parent = menuParent;

 tabContainer  = new QFrame( parent->deviceContainer, name);
 parent->deviceContainer->addTab (tabContainer, i18n(name));

 biggestLabel = 0;

 sv = new QScrollView(tabContainer);
 sv->setResizePolicy(QScrollView::AutoOneFit);

 mainLayout	   = new QVBoxLayout (tabContainer, 11, 6, "dev_mainLayout");

 propertyLayout= new QVBox(sv);
 propertyLayout->setMargin(5);
 propertyLayout->setSpacing(10);

 sv->addChild(propertyLayout);

 msgST_w	   = new QTextEdit(tabContainer, "msgST_w");
 msgST_w->setReadOnly(true);
 msgST_w->setMaximumHeight(100);

 vSpacer       = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Minimum);

 hSpacer       = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

 buttonLayout = new QHBoxLayout(0, 5, 5);

 clear   = new QPushButton(i18n("Clear"), tabContainer);
 savelog = new QPushButton(i18n("Save log..."), tabContainer);

 buttonLayout->addWidget(clear);
 buttonLayout->addWidget(savelog);
 buttonLayout->addItem(hSpacer);

 mainLayout->addWidget(sv);
 mainLayout->addItem(vSpacer);
 mainLayout->addWidget(msgST_w);
 mainLayout->addLayout(buttonLayout);

 msgST_w->setFocus();

 QObject::connect(clear, SIGNAL(clicked()), this, SLOT(clearMsg()));

 /* hmmmmmm, a way to know the optimal widget height, there is a probably
 * a better way to do it, but oh well! */
  KLineEdit *tempLine = new KLineEdit(tabContainer);
  uniHeight = tempLine->height() - 10;
  delete(tempLine);

}

INDI_D::~INDI_D()
{
  delete (clear);
  delete (savelog);
  delete (buttonLayout);
  delete (curGroup);
  delete (propertyLayout);
  delete (sv);
  delete (mainLayout);

  delete (tabContainer);
}

void INDI_D::clearMsg()
{
  msgST_w->clear();
}


/*******************************************************************
** INDI Menu: Handles communication to server and fetching basic XML
** data.
*******************************************************************/
INDIMenu::INDIMenu(QWidget *parent, const char *name ) : KDialogBase(KDialogBase::Plain, i18n("INDI Control Panel"), 0, KDialogBase::Default, parent, name)
{

 serverFD  = -1;
 serverFP  = NULL;
 XMLParser = NULL;
 sNotifier = NULL;

 mainLayout = new QVBoxLayout(plainPage(), 5, 5);

 deviceContainer = new QTabWidget(plainPage(), "deviceContainer");
 msgST_w	 = new QTextEdit(plainPage(), "genericMsgForm");
 msgST_w->setReadOnly(true);
 msgST_w->setMaximumHeight(100);

 mainLayout->addWidget(deviceContainer);
 mainLayout->addWidget(msgST_w);

 resize( 640, 480);

 host = QString("localhost");
 port = QString("7624");

 indiConnect();

}

INDIMenu::~INDIMenu()
{
  if (serverFD >= 0)
  {
    if (serverFP)
    	fclose(serverFP);
    serverFP = NULL;
    serverFD = -1;
  }

  if (XMLParser)
  {
   delLilXML(XMLParser);
   XMLParser = NULL;
  }

}

bool INDIMenu::indiConnect()
{
	struct sockaddr_in pin;
	struct hostent *serverHostName = gethostbyname(host.ascii());

	bzero(&pin, sizeof(pin));
	pin.sin_family 		= AF_INET;
	pin.sin_addr.s_addr 	= ((struct in_addr *) (serverHostName->h_addr))->s_addr;
	pin.sin_port 		= htons(port.toInt());

	if ( (serverFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	 return false;

	if ( ::connect(serverFD, (struct sockaddr*) &pin, sizeof(pin)) == -1)
	  return false;

	// callback notified
	sNotifier = new QSocketNotifier( serverFD, QSocketNotifier::Read, this);
        QObject::connect( sNotifier, SIGNAL(activated(int)), this, SLOT(dataReceived()));

	if (XMLParser)
	   delLilXML(XMLParser);
	XMLParser = newLilXML();

	// ready for fprintf
	serverFP = fdopen(serverFD, "w");

	if (serverFP == NULL)
	 return false;

	setbuf (serverFP, NULL);

	fprintf(serverFP, "<getProperties version='0.1'/>\n");

	return true;
}

void INDIMenu::dataReceived()
{
	char ibuf[32];	/* not so much user input lags */
	char msg[1024];
	int i, nr;

	/* read INDI command */
	nr = read (serverFD, ibuf, sizeof(ibuf));
	if (nr <= 0) {
	    if (nr < 0)
		sprintf (msg, "INDI: input error");
	    else
		sprintf (msg, "INDI: agent closed connection");
	    kdDebug() << msg << endl;

	    return;
	}

	/* process each char */
	for (i = 0; i < nr; i++)
	{
	    XMLEle *root = readXMLEle (XMLParser, (int)ibuf[i], msg);
	    if (root)
	    {
		if (dispatchCommand(root, msg) < 0)
		    prXMLEle (stdout, root, 0);

                 if (deviceContainer->currentPage())
		 {
		 	// Gloria Gloria Gloria!
		 	for (uint j=0; j < indi_dev.size(); j++)
		   	for (uint k=0; k < indi_dev[j]->gl.size(); k++)
		   	{
		     	indi_dev[j]->gl[k]->box->layout()->activate();
		     	indi_dev[j]->gl[k]->box->show();
		   	}

			deviceContainer->showPage ( deviceContainer->currentPage() );
		}


		delXMLEle (root);
	    }
	    else if (msg[0])
	    {
		kdDebug() << msg << endl;
	    }
	}
}

int INDIMenu::dispatchCommand(XMLEle *root, char errmsg[])
{

  if  (!strcmp (root->tag, "message"))
  	 return messageCmd(root, errmsg);
  else if  (!strcmp (root->tag, "delProperty"))
        return delPropertyCmd(root, errmsg);

  /* Get the device, if not available, create it */
  INDI_D *dp = findDev (root, 1, errmsg);
      if (dp == NULL)
	    return -1;

   if (!strcmp (root->tag, "defText"))
      return dp->buildTextGUI(root, errmsg);
   else if (!strcmp (root->tag, "defNumber"))
      return dp->buildNumberGUI(root, errmsg);
   else if (!strcmp (root->tag, "defSwitches"))
       return dp->buildSwitchesGUI(root, errmsg);
   else if (!strcmp (root->tag, "defLights"))
        return dp->buildLightsGUI(root, errmsg);
   else if (!strcmp (root->tag, "setText") ||
   	    !strcmp (root->tag, "setNumber") ||
	    !strcmp (root->tag, "setSwitch") ||
	    !strcmp (root->tag, "setLights"))
	return dp->setAnyCmd(root, errmsg);
  else if  (!strcmp (root->tag, "newText") ||
  	    !strcmp (root->tag, "newNumber") ||
	    !strcmp (root->tag, "newSwitch"))
	 return dp->newAnyCmd(root, errmsg);


   return (-1);
}

/* delete the property in the given device, including widgets and data structs.
 * when last property is deleted, delete the device too.
 * if no property name attribute at all, delete the whole device regardless.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDIMenu::delPropertyCmd (XMLEle *root, char errmsg[])
{

	XMLAtt *ap;
	INDI_D *dp;

	/* dig out device and optional property name */
	dp = findDev (root, 0, errmsg);
	if (!dp)
	    return (-1);
	ap = findXMLAtt (root, "name");

	/* delete just pname, or all if no property specified.
	 * N.B. remember delP decrements dp->npl
	 */
	if (ap)
	{
	    char *pname = ap->valu;
	    return dp->removeProperty(pname, errmsg);
	}
	// delete the whole device
	else
	{
            deviceContainer->removePage(dp->tabContainer);

	    for (uint i=0; i < dp->pl.size(); i++)
	      dp->removeProperty(dp->pl[0]->name, errmsg);

	    delete(dp);
	}

        return (0);
}

int INDI_D::removeProperty(char *pname, char errmsg[])
{
  INDI_G *pg;

    for (uint i= 0; i < pl.size(); i++)
    {
	if (!strcmp (pname, pl[i]->name))
	{
	    pg = pl[i]->pg;

	    delete(pl[i]);
	    pl.erase(pl.begin() + i);

	    pg->removeProperty();

       for (uint j=0; j < gl.size(); j++)
	{
	   if (gl[j]->nElements < 0)
	   {
	        delete (gl[j]);
		gl.erase(gl.begin() + j);
	   }
	}
        return (0);
      }
   }
   sprintf (errmsg, "INDI: Device %s has no property named %s", name, pname);
	    return (-1);
}


/* a general message command received from the device.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDIMenu::messageCmd (XMLEle *root, char errmsg[])
{
	checkMsg (root, findDev (root, 0, errmsg));
	return (0);
}


/* implement any <set???> received from the device.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::setAnyCmd (XMLEle *root, char errmsg[])
{
	XMLAtt *ap;
	INDI_P *pp;

	ap = parent->findAtt (root, "name", errmsg);
	if (!ap)
	    return (-1);

	pp = findProp (ap->valu);
	if (!pp)
	{
	    sprintf (errmsg,"INDI: <%s> device %s has no property named %s",
						root->tag, name, ap->valu);
	    return (-1);
	}

	parent->checkMsg (root, this);

	return (setValue (pp, root, errmsg));
}

/* set the given GUI property according to the XML command.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setValue (INDI_P *pp, XMLEle *root, char errmsg[])
{
	char *pn = pp->name;
	XMLAtt *ap;

	/* set overall property state, if any */
	ap = findXMLAtt (root, "state");
	if (ap)
	{
	    if (crackLightState (ap->valu, &pp->state) == 0)
	      pp->drawLt (pp->light, pp->state);
	    else
	    {
		sprintf (errmsg, "INDI: <%s> bogus state %s for %s %s",
						root->tag, ap->valu, name, pn);
		return (-1);
	    }
	}

	/* allow changing the timeout */
	ap = findXMLAtt (root, "timeout");
	if (ap)
	    pp->timeout = atof(ap->valu);

	/* process specific GUI features */
	switch (pp->guitype)
	{
	case PG_NONE:
	    break;

	case PG_SCALE:
	    return (setScaleValue (pp, root, errmsg));
	    break;

	case PG_NUMERIC:	/* FALLTHRU */
	case PG_TEXT:
	    return (setTextValue (pp, root, errmsg));
	    break;

	case PG_BUTTONS:	/* FALLTHRU */
	case PG_LIGHTS:
	    return (setLabelState (pp, root, errmsg));
	    break;

	case PG_RADIO:		/* FALLTHRU */
	case PG_MENU:
	    return (setMenuChoice (pp, root, errmsg));
	    break;
	}

	return (0);
}

/* set the given SCALE property from the given element.
 * root should have <number> child.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setScaleValue (INDI_P *pp, XMLEle *root, char errmsg[])
{
	XMLEle *ep = parent->findEle (root, pp, "number", errmsg);

	if (!ep)
	    return (-1);

	switch (pp->u.scale.perm)
	{
	case PP_RW:	/* FALLTHRU */
	case PP_RO:
	    /* set where the user can only see, not edit */
	    pp->u.scale.read_w->setText(QString(ep->pcdata));
	    break;
	case PP_WO:
	    /* set where the user edits */
	    pp->u.scale.read_w->setText(QString(ep->pcdata));
	    pp->u.scale.spin_w->setValue(atof(ep->pcdata));
	    break;
	}

	return (0);
}

/* set the given TEXT or NUMERIC property from the given element.
 * root should have <text> or <number> child.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setTextValue (INDI_P *pp, XMLEle *root, char errmsg[])
{
	const char *tag = pp->guitype == PG_TEXT ? "text" : "number";
	XMLEle *ep = parent->findEle (root, pp, tag, errmsg);

	if (!ep)
	    return (-1);

	switch (pp->u.text.perm)
	{
	case PP_RW:	/* FALLTHRU */
	case PP_RO:
	    /* set where the user can only see, not edit */
	    pp->u.text.read_w->setText(QString(ep->pcdata));
	    break;
	case PP_WO:
	    /* set where the user edits */
	    pp->u.text.write_w->setText(QString(ep->pcdata));
	    break;
	}

	return (0);
}


/* set the user input SCALE field from the given element.
 * root should have <number> child.
 * ignore if property is not RW.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::newScaleValue (INDI_P *pp, XMLEle *root, char errmsg[])
{
	XMLEle *ep = parent->findEle (root, pp, "number", errmsg);

	if (!ep)
	    return (-1);

	if (pp->u.scale.perm == PP_RW)
	    pp->u.scale.read_w->setText(QString(ep->pcdata));

	return (0);
}

/* set the user input TEXT or NUMERIC field from the given element.
 * root should have <text> or <number> child.
 * ignore if property is not RW.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::newTextValue (INDI_P *pp, XMLEle *root, char errmsg[])
{
	const char *tag = pp->guitype == PG_TEXT ? "text" : "number";
	XMLEle *ep = parent->findEle (root, pp, tag, errmsg);

	if (!ep)
	    return (-1);

	if (pp->u.text.perm == PP_RW)
	    pp->u.text.read_w->setText(QString(ep->pcdata));

	return (0);
}

/* set the given BUTTONS or LIGHTS property from the given element.
 * root should have some <switch> or <light> children.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setLabelState (INDI_P *pp, XMLEle *root, char errmsg[])
{
	char *pn = pp->name;
	uint i, j;

	/* for each child element */
	for (i = 0; i < (uint) root->nel; i++)
	{
	    XMLEle *ep = root->el[i];
	    XMLAtt *ap = findXMLAtt (ep, "state");
	    char *lname = ep->pcdata;
	    INDI_L *lp = NULL;
	    int islight;
	    PState state;

	    /* only using light and switch */
	    islight = !strcmp (ep->tag, "light");
	    if (!islight && strcmp (ep->tag, "switch"))
		continue;

	    /* get state */
	    if (!ap)
	    {
		sprintf (errmsg, "INDI: <%s> %s %s %s requires state",
						    root->tag, name, pn, lname);
		return (-1);
	    }
	    if ((islight && crackLightState (ap->valu, &state) < 0)
		    || (!islight && crackSwitchState (ap->valu, &state) < 0))
	    {
		sprintf (errmsg, "INDI: <%s> unknown state %s for %s %s %s",
					    root->tag, ap->valu, name, pn, lname);
		return (-1);
	    }

	    /* find matching label */
	    for (j = 0; j < pp->u.multi.labels->size(); j++)
	    {
		lp = (*pp->u.multi.labels)[j];
		if (!strcmp (lname, lp->name))
		    break;
	    }

	    if (j == pp->u.multi.labels->size())
	    {
		sprintf (errmsg,"INDI: <%s> %s %s has no choice named %s",
						    root->tag, name, pn, lname);
		return (-1);
	    }

	    /* engage new state */
	    lp->state = state;
	    if (pp->guitype == PG_BUTTONS && !islight)
		(*pp->u.multi.push_v)[i]->setDown(state == PS_ON ? true : false);
	    else if (pp->guitype == PG_LIGHTS && islight)
		pp->drawLt ((*pp->u.multi.led_v)[j], lp->state);

	}

	return (0);
}


/* set the given RADIO or MENU property from the given element.
 * root should have some <switch> children.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setMenuChoice (INDI_P *pp, XMLEle *root, char errmsg[])
{
        char *pn = pp->name;
	uint i, j;

	/* for each child element */
	for (i = 0; i < (uint) root->nel; i++)
	{
	    XMLEle *ep = root->el[i];
	    XMLAtt *ap = findXMLAtt (ep, "state");
	    char *lname = ep->pcdata;
	    INDI_L *lp = NULL;
	    PState state;

	    /* both RADIO and MENU use <switch> */
	    if (strcmp (ep->tag, "switch"))
		continue;

	    /* get state */
	    if (!ap)
	    {
		sprintf (errmsg, "INDI: <%s> %s %s %s requires state",
						    root->tag, name, pn, lname);
		return (-1);
	    }

	    if (crackSwitchState (ap->valu, &state) < 0)
	    {
		sprintf (errmsg, "INDI: <%s> unknown state %s for %s %s %s",
					    root->tag, ap->valu, name, pn, lname);
		return (-1);
	    }

	    /* find matching label */
	    for (j = 0; j < pp->u.multi.labels->size(); j++)
	    {
		lp = (*pp->u.multi.labels)[j];
		if (!strcmp (lname, lp->name))
		    break;
	    }



	    if (j == pp->u.multi.labels->size())
	    {
		sprintf (errmsg,"INDI: <%s> %s %s has no choice named %s",
						    root->tag, name, pn, lname);
		return (-1);
	    }

	    /* engage new state */
	    lp->state = state;
	    if (pp->guitype == PG_MENU && lp->state != PS_OFF)
		pp->u.multi.om_w->setCurrentItem(i-1);
	    else if (pp->guitype == PG_RADIO)
		(*pp->u.multi.check_v)[j]->setChecked(state == PS_ON ? true : false);
	}

	return (0);
}

/* implement any <new???> received from the device.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::newAnyCmd (XMLEle *root, char errmsg[])
{
	XMLAtt *ap;
	INDI_P *pp;

	ap = parent->findAtt (root, "name", errmsg);
	if (!ap)
	    return (-1);

	pp = findProp(ap->valu);
	if (!pp) {
	    sprintf (errmsg,"INDI: <%s> device %s has no property named %s",
						root->tag, name, ap->valu);
	    return (-1);
	}

	return (newValue (pp, root, errmsg));
}

/* set the user input field of the given RW GUI property according to the XML
 * command.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::newValue (INDI_P *pp, XMLEle *root, char errmsg[])
{
	/* process specific GUI features */
	switch (pp->guitype)
	{
	case PG_NONE:
	    break;

	case PG_SCALE:
	    return (newScaleValue (pp, root, errmsg));
	    break;

	case PG_NUMERIC:	/* FALLTHRU */
	case PG_TEXT:
	    return (newTextValue (pp, root, errmsg));
	    break;

	case PG_BUTTONS:	/* FALLTHRU */
	case PG_LIGHTS:
	    return (setLabelState (pp, root, errmsg));
	    break;

	case PG_RADIO:		/* FALLTHRU */
	case PG_MENU:
	    return (setMenuChoice (pp, root, errmsg));
	    break;
	}

	return (0);
}

int INDI_D::buildTextGUI(XMLEle *root, char errmsg[])
{
       	INDI_P *pp = NULL;
	XMLEle *text;
	char *initstr;
	PPerm p;

	/* build a new property */
	pp = addProperty (root, errmsg);

	if (pp == NULL)
	    return (-1);

	/* N.B. if trouble from here on must call delP() */
	/* get the permission, it will determine layout issues */
	if (findPerm (pp, root, &p, errmsg))
	{
	    delete(pp);
	    return (-1);
	}

	/* get initial string */
	text = parent->findEle (root, pp, "text", errmsg);
	if (!text)
	{
	    delete(pp);
	    return (-1);

	}

	initstr = text->pcdata;

	/* build */
	if (buildRawTextGUI (pp, initstr, p, errmsg) < 0)
	{
	    delete(pp);
	    return (-1);
	}

	/* we know it will be a general text GUI */
	pp->guitype = PG_TEXT;
	pp->u.text.perm = p;

	pl.push_back(pp);
	return (0);
}

INDI_P * INDI_D::addProperty (XMLEle *root, char errmsg[])
{
	INDI_P *pp = NULL;
	XMLAtt *ap = NULL;

	/* get property name and add new property to dp */
	ap = parent->findAtt (root, "name", errmsg);
	if (ap == NULL)
	    return NULL;

	if (findProp (ap->valu))
	{
	    sprintf (errmsg, "INDI: <%s %s %s> already exists", root->tag,
							name, ap->valu);
	    return NULL;
	}

	pp = new INDI_P(this, ap->valu);

	/* N.B. if trouble from here on must call delP() */

	/* init state */
	ap = parent->findAtt (root, "state", errmsg);
	if (!ap)
	{
	    // TODO use std::vector to add and remove
	    delete(pp);
	    return (NULL);
	}

	if (crackLightState (ap->valu, &pp->state) < 0)
	{
	    sprintf (errmsg, "INDI: <%s> bogus state %s for %s %s",
				root->tag, ap->valu, pp->dp->name, pp->name);
	    delete(pp);
	    return (NULL);
	}

	//TODO
	/* init timeout */
	ap = parent->findAtt (root, "timeout", NULL);
	/* default */
	pp->timeout = ap ? atof(ap->valu) : 0;

	/* log any messages */
	parent->checkMsg (root, this);

       if (findGroup(pp, root, errmsg) < 0)
	 return (NULL);

	curGroup->addProperty();
	pp->setGroup(curGroup);

	if (addGUI (pp, root, errmsg) < 0)
	{
	   delete(pp);
	   return (NULL);
	}

	/* ok! */
	return (pp);
}

/* search for a device named by root's device attribute.
 * if create: create if does not already exist.
 * if !create: must already exist.
 * return NULL with reason in errmsg if trouble.
 */

INDI_D * INDIMenu::findDev (XMLEle *root, int create, char errmsg[])
{
	XMLAtt *ap;
	char *dn;

	/* get device name */
	ap = findAtt (root, "device", errmsg);
	if (!ap)
	    return (NULL);
	dn = ap->valu;

	/* search for existing */
	for (uint i = 0; i < indi_dev.size(); i++)
	{
	    if (!strcmp (indi_dev[i]->name, dn))
		return (indi_dev[i]);
	}

	/* not found, create if ok */
	if (create)
		return (addDevice (root, errmsg));
	sprintf (errmsg, "INDI: <%s> no such device %s", root->tag, dn);
	return NULL;
}

/* search element for attribute.
 * return XMLAtt if find, else NULL with helpful info in errmsg.
 */
XMLAtt * INDIMenu::findAtt (XMLEle *ep, const char *name, char errmsg[])
{
	XMLAtt *ap = findXMLAtt (ep, name);
	if (ap)
	    return (ap);
	if (errmsg)
	    sprintf (errmsg, "INDI: <%s> missing attribute '%s'", ep->tag,name);
	return NULL;
}

INDI_P * INDI_D::findProp (const char *name)
{

	for (uint i = 0; i < pl.size(); i++)
	    if (!strcmp (name, pl[i]->name))
		return (pl[i]);

	return NULL;
}

/* search element for given child. pp is just to build a better errmsg.
 * return XMLEle if find, else NULL with helpful info in errmsg.
 */
XMLEle * INDIMenu::findEle (XMLEle *ep, INDI_P *pp, const char *child, char errmsg[])
{
	XMLEle *cp = findXMLEle (ep, child);
	if (cp)
	    return (cp);
	if (errmsg)
	    sprintf (errmsg, "INDI: <%s %s %s> missing child '%s'", ep->tag,
						pp->dp->name, pp->name, child);
	return (NULL);
}


int INDI_D::findGroup(INDI_P *pp  , XMLEle *root, char errmsg[])
{
  INDI_G *group;
  XMLAtt *ap;

  ap = parent->findAtt (root, "grouptag", errmsg);
  if (!ap)
  {
    kdDebug() << QString(errmsg) << endl;
    return (-1);
  }

  for (int i= gl.size() -1 ; i >= 0; i--)
  {
    curGroup      = gl[i];

    if (!strcmp(ap->valu, "") && !(curGroup->box->title().isEmpty()))
     break;

    if (QString(ap->valu) == curGroup->box->title() ||
       (!strcmp(ap->valu, "") && (curGroup->box->title().isEmpty())))
      return (0);

  }

  /* couldn't find an existing group, create a new one */
  group = new INDI_G(pp, ap->valu);
  gl.push_back(group);
  curGroup = group;

  return (0);
}

/* find "perm" attribute in root, crack and set *pp.
 * return 0 if ok else -1 with excuse in errmsg[]
 */

 int INDI_D::findPerm (INDI_P *pp, XMLEle *root, PPerm *permp, char errmsg[])
{
	XMLAtt *ap;

	ap = findXMLAtt (root, "perm");
	if (!ap) {
	    sprintf (errmsg, "INDI: <%s %s %s> missing attribute 'perm'",
					root->tag, pp->dp->name, pp->name);
	    return (-1);
	}
	if (!strcmp(ap->valu, "ro") || !strcmp(ap->valu, "r"))
	    *permp = PP_RO;
	else if (!strcmp(ap->valu, "wo"))
	    *permp = PP_WO;
	else if (!strcmp(ap->valu, "rw") || !strcmp(ap->valu, "w"))
	    *permp = PP_RW;
	else {
	    sprintf (errmsg, "INDI: <%s> unknown perm %s for %s %s",
				root->tag, ap->valu, pp->dp->name, pp->name);
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



/* add new device to mainrc_w using info in dep.
 * if trouble return NULL with reason in errmsg[]
 */
INDI_D * INDIMenu::addDevice (XMLEle *dep, char errmsg[])
{
	INDI_D *dp;
	XMLAtt *ap;

	/* allocate new INDI_D on indi_dev */
	ap = findAtt (dep, "device", errmsg);
	if (!ap)
	    return NULL;
	dp = new INDI_D(this, ap->valu);

	 indi_dev.push_back(dp);

	/* ok */
	return dp;
}

void INDI_P::drawLt(KLed *w, PState lstate)
{

        /* set state light */
	switch (lstate)
	{
	  case PS_IDLE:
	  w->setColor(Qt::gray);
	  break;

	  case PS_OK:
	  w->setColor(Qt::green);
	  break;

	  case PS_BUSY:
	  w->setColor(Qt::yellow);
	  break;

	  case PS_ALERT:
	  w->setColor(Qt::red);
	  break;

	  default:
	  break;

	}
}

void INDI_D::resizeGroups()
{

  for (uint i=0; i < gl.size(); i++)
  {
        gl[i]->layout->addColSpacing(1, biggestLabel);
	gl[i]->layout->addColSpacing(6, 50);
  }
}

/* build widgets for property pp using info in root.
 * we have already handled device+name+timeout+state attributes.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::addGUI (INDI_P *pp, XMLEle *root, char errmsg[])
{
	XMLEle *prompt;

	/* TODO add to GUI group */
	pp->light = new KLed (curGroup->box);
	pp->drawLt(pp->light, pp->state);
        pp->light->setMaximumSize(16,16);
	curGroup->layout->addWidget(pp->light, curGroup->nElements, 0);

	/* add label for prompt */
	prompt = parent->findEle (root, pp, "prompt", errmsg);
	if (prompt)
	{
	 pp->label_w = new QLabel(QString(prompt->pcdata), curGroup->box);
	 curGroup->layout->addWidget(pp->label_w, curGroup->nElements, 1);

	 if (pp->label_w->sizeHint().width() > biggestLabel)
	   biggestLabel = pp->label_w->sizeHint().width();

         pp->light->show();
	 pp->label_w->show();

 	 resizeGroups();

         return (0);
	}
	else
	  /* findEle already set errmsg */
	    return (-1);

}


/* build a basic text gui to be used for PG_TEXT or PG_NUMERIC.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::buildRawTextGUI (INDI_P *pp, char *initstr, PPerm p, char errmsg[])
{
  	switch (p) {

	case PP_RW:

	    pp->u.text.read_w  = new QLabel(QString(initstr), curGroup->box);
	    pp->u.text.read_w->setFrameShape(QFrame::Box);
	    pp->u.text.read_w->setMinimumWidth(100);

	    pp->u.text.write_w = new KLineEdit(QString(initstr), curGroup->box);
	    pp->u.text.set_w   = new QPushButton(i18n("Set"), curGroup->box);
	    pp->u.text.read_w->setFixedHeight(uniHeight);
	    pp->u.text.read_w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            curGroup->layout->addMultiCellWidget(pp->u.text.read_w, curGroup->nElements, curGroup->nElements, 2, 3);
            curGroup->layout->addMultiCellWidget(pp->u.text.write_w, curGroup->nElements , curGroup->nElements, 4, 5);
            curGroup->layout->addWidget(pp->u.text.set_w, curGroup->nElements , 6);

	    QObject::connect(pp->u.text.write_w, SIGNAL(returnPressed()),
	    		     pp, SLOT(newText()));
	    QObject::connect(pp->u.text.set_w, SIGNAL(clicked()),
	    		     pp, SLOT(newText()));

	    pp->u.text.read_w->show();
	    pp->u.text.write_w->show();
	    pp->u.text.set_w->show();
	    break;

	case PP_RO:

	    pp->u.text.read_w  = new QLabel(QString(initstr), curGroup->box);
	    pp->u.text.read_w->setFrameShape(QFrame::Box);
	    pp->u.text.read_w->setMinimumWidth(100);
	    pp->u.text.read_w->setFixedHeight(uniHeight);
	    curGroup->layout->addMultiCellWidget(pp->u.text.read_w, curGroup->nElements, curGroup->nElements, 2, 5);

	    pp->u.text.read_w->show();

		break;

	case PP_WO:
  	    pp->u.text.write_w = new KLineEdit(QString(initstr), curGroup->box);
	    pp->u.text.set_w   = new QPushButton(i18n("Set"), curGroup->box);

	    curGroup->layout->addMultiCellWidget(pp->u.text.write_w, curGroup->nElements , curGroup->nElements, 4, 5);
            curGroup->layout->addWidget(pp->u.text.set_w, curGroup->nElements , 6);

	    QObject::connect(pp->u.text.write_w, SIGNAL(returnPressed()),
	    		     pp, SLOT(newText()));
	    QObject::connect(pp->u.text.set_w, SIGNAL(clicked()),
	    		     pp, SLOT(newText()));

	    pp->u.text.write_w->show();
	    pp->u.text.set_w->show();
	    break;
	}

	// no warnings
        errmsg = NULL;
	return (0);
}

/* display each <msg> within root in dp's scrolled area, if any.
 * prefix our time stamp if not included.
 * N.B. don't put carriage control in msg, we take care of that.
 */
void INDIMenu::checkMsg (XMLEle *root, INDI_D *dp)
{
	int i;

	for (i = 0; i < root->nel; i++)
	    if (!strcmp (root->el[i]->tag, "msg"))
		doMsg (root->el[i], dp);
}

/* display pcdata of <msg> in dp's scrolled area, if any, else general.
 * prefix our time stamp if not included.
 * N.B. don't put carriage control in msg, we take care of that.
 */
void INDIMenu::doMsg (XMLEle *msg, INDI_D *dp)
{
	QTextEdit *txt_w;
	XMLAtt *timeatt;

	/* determine whether device or generic message */
	if (dp)
	    txt_w = dp->msgST_w;
	else
	    txt_w = msgST_w;

	/* prefix our timestamp if not with msg */
	timeatt = findXMLAtt (msg, "time");
	if (timeatt)
            txt_w->insert(QString(timeatt->valu) + QString(" "));
	else
	   txt_w->insert( (QDateTime().currentDateTime()).toString("yyyy/mm/dd - h:m:s ap "));


	/* finally! the msg */
	    txt_w->insert( QString(msg->pcdata) + QString("\n"));
}

void INDI_P::newText()
{
  QString text;

  if (guitype == PG_TEXT || guitype == PG_NUMERIC)
  {
     text = u.text.write_w->text();
     u.text.read_w->setText(text);

  }
  else
  {
    text = u.scale.spin_w->text();
    u.scale.read_w->setText(text);
  }

  state = PS_BUSY;

  drawLt(light, state);

  dp->parent->sendNewString(this, text.ascii());

}

void INDIMenu::sendNewString (INDI_P *pp, const char *txt)
{
   if (pp->guitype == PG_TEXT)
    fprintf (serverFP, "<newText device='%s' name='%s'><text>%s</text></newText>\n",
						pp->dp->name, pp->name, txt);
   else
    fprintf (serverFP, "<newNumber device='%s' name='%s'><number>%s</number></newNumber>\n", pp->dp->name, pp->name, txt);
}

void INDIMenu::sendNewSwitch (INDI_L *lp, int set)
{
	INDI_P *pp = lp->pp;

	fprintf (serverFP, "<newSwitch device='%s' name='%s'><switch state='%s'>%s</switch></newSwitch>\n",
			pp->dp->name, pp->name, set ? "On" : "Off", lp->name);
}


/* build GUI for a number property.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::buildNumberGUI (XMLEle *root, char *errmsg)
{
	INDI_P *pp;
	char *initstr;
	XMLEle *number, *range;
	PPerm p;

	/* build a new property */
	pp = addProperty (root, errmsg);
	if (!pp)
	    return (-1);

	/* N.B. if trouble from here on must call delP() */

	/* get the permission, it will determine layout issues */
	if (findPerm (pp, root, &p, errmsg) < 0)
	{
	    delete(pp);
	    return (-1);
	}

	/* get initial value */
	number = parent->findEle (root, pp, "number", errmsg);
	if (!number)
	{
	    delete(pp);
	    return (-1);
	}
	initstr = number->pcdata;

	/* get range, use scale if small enough, else same as text */
	pp->u.scale.min = -1e20;
	pp->u.scale.max = 1e20;
	pp->u.scale.step = 0;

	range = findXMLEle (root, "range");
	if (range)
	{
	    XMLEle *vp;

	    vp = findXMLEle (range, "min");
	    if (vp)
		pp->u.scale.min = atof(vp->pcdata);
	    vp = findXMLEle (range, "max");
	    if (vp)
		pp->u.scale.max = atof(vp->pcdata);
	    vp = findXMLEle (range, "step");
	    if (vp)
		pp->u.scale.step = atof(vp->pcdata);
	}

	if (pp->u.scale.step == 0 || (pp->u.scale.max -
			    pp->u.scale.min)/pp->u.scale.step > MAXSCSTEPS)
        {
	    if (buildRawTextGUI (pp, initstr, p, errmsg) < 0)
	    {
	        delete(pp);
		return (-1);
	    }

	    pp->guitype = PG_NUMERIC;
	    pp->u.text.perm = p;

	    pp->dp->pl.push_back(pp);
	    return (0);
	}

	/* ok, we build a scale */
	pp->guitype = PG_SCALE;
	pp->u.scale.perm = p;

	switch (pp->u.scale.perm)
	{
	case PP_RW:
	   pp->u.scale.read_w  = new QLabel(QString(initstr), curGroup->box);
           pp->u.scale.read_w->setFrameShape(QFrame::Box);
	   pp->u.scale.read_w->setMinimumWidth(100);
	   pp->u.scale.read_w->setFixedHeight(uniHeight);
	   pp->u.scale.read_w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	   pp->u.scale.spin_w = new KDoubleSpinBox(pp->u.scale.min, pp->u.scale.max,
	                       pp->u.scale.step, atoi(initstr), 2, curGroup->box);

           pp->u.scale.set_w  = new QPushButton(i18n("Set"), curGroup->box);

	   curGroup->layout->addMultiCellWidget(pp->u.scale.read_w, curGroup->nElements, curGroup->nElements, 2, 4);
	   curGroup->layout->addWidget(pp->u.scale.spin_w, curGroup->nElements, 5);
	   curGroup->layout->addWidget(pp->u.scale.set_w, curGroup->nElements, 6);

	    QObject::connect(pp->u.scale.set_w, SIGNAL(clicked()),
	    		     pp, SLOT(newText()));

            pp->u.scale.read_w->show();
	    pp->u.scale.spin_w->show();
	    pp->u.scale.set_w->show();
	    break;

	case PP_RO:
	   pp->u.scale.read_w  = new QLabel(QString(initstr), curGroup->box);
           pp->u.scale.read_w->setFrameShape(QFrame::Box);
	   pp->u.scale.read_w->setMinimumWidth(100);
	   pp->u.scale.read_w->setFixedHeight(uniHeight);
	   pp->u.scale.read_w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	   curGroup->layout->addMultiCellWidget(pp->u.scale.read_w, curGroup->nElements,
	   curGroup->nElements, 2, 5);

	   pp->u.scale.read_w->show();


         break;

	case PP_WO:
	   pp->u.scale.spin_w = new KDoubleSpinBox(pp->u.scale.min, pp->u.scale.max,
	                       pp->u.scale.step, atoi(initstr), 2, curGroup->box);

           pp->u.scale.set_w  = new QPushButton(i18n("Set"), curGroup->box);

	   curGroup->layout->addWidget(pp->u.scale.spin_w, curGroup->nElements, 5);
	   curGroup->layout->addWidget(pp->u.scale.set_w, curGroup->nElements, 6);

           QObject::connect(pp->u.scale.set_w, SIGNAL(clicked()),
	    		     pp, SLOT(newText()));

	    pp->u.scale.spin_w->show();
	    pp->u.scale.set_w->show();

	   break;
	}

	pl.push_back(pp);

	return (0);
}

/* build GUI for switches property.
 * rule and number of will determine exactly how the GUI is built.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::buildSwitchesGUI (XMLEle *root, char errmsg[])
{
	INDI_P *pp;
	XMLAtt *ap;
	int i, j, n;

	/* build a new property */
	pp = addProperty (root, errmsg);
	if (!pp)
	    return (-1);

	/* N.B. if trouble from here on must call delP() */

	ap = parent->findAtt (root, "rule", errmsg);
	if (!ap)
	{
	    delete(pp);
	    return (-1);
	}

	/* decide GUI. might use MENU if OneOf but too many for button array */
	if (!strcmp (ap->valu, "OneOfMany"))
	{
	    /* count number of switches -- make menu if too many */
	    for (n = i = 0; i < root->nel; i++)
		if (!strcmp (root->el[i]->tag, "switch"))
		    n++;
	    if (n > MAXRADIO)
	    {
		i = buildMenuGUI (pp, root, errmsg);
		if (i < 0)
		    delete(pp);
		return (i);
	    }

	    pp->guitype = PG_BUTTONS;

	}
	else if (!strcmp (ap->valu, "AnyOfMany"))
	    pp->guitype = PG_RADIO;
	else
	{
	    sprintf (errmsg, "INDI: <%s> unknown rule %s for %s %s",
				root->tag, ap->valu, name, pp->name);
	    delete(pp);
	    return (-1);
	}

	/* ok, build array of toggle buttons.
	 * only difference is /visual/ appearance of 1ofMany or anyofMany */

	 pp->u.multi.hLayout = new QHBoxLayout(0, 0, 5);

	pp->u.multi.groupB = new QButtonGroup(0);
	if (pp->guitype == PG_BUTTONS)
	  pp->u.multi.groupB->setExclusive(true);


        QObject::connect(pp->u.multi.groupB, SIGNAL(clicked(int)),
    		     pp, SLOT(newSwitch(int)));

	XMLEle *sep;
        char *name;
	QPushButton *button;
	QCheckBox   *checkbox;
	INDI_L *lp;

	for (i = 0, j = -1; i < root->nel; i++)
	{
	    sep = root->el[i];
	    name = sep->pcdata;

	    /* look for switch tage */
	    if (strcmp (sep->tag, "switch"))
		continue;

	    lp = new INDI_L(pp, name);

	    j++;

	    /* find state */
	    ap = parent->findAtt (sep, "state", errmsg);
	    if (!ap)
	    {
		delete(pp);
		return (-1);
	    }

	    if (crackSwitchState (ap->valu, &(lp->state)) < 0)
	    {
		sprintf (errmsg, "INDI: <%s> unknown state %s for %s %s %s",
			    root->tag, ap->valu, name, pp->name, name);
		delete(pp);
		return (-1);
	    }

	    /* build toggle */
	    switch (pp->guitype)
	    {
	      case PG_BUTTONS:
	       button = new KPushButton(QString(name), curGroup->box);
	       pp->u.multi.groupB->insert(button, j);

	       if (lp->state == PS_ON)
	           button->setDown(true);


	       pp->u.multi.push_v->push_back(button);
	       curGroup->layout->addWidget(button, curGroup->nElements, j + 2);
	       button->show();

	       break;

	      case PG_RADIO:
	      checkbox = new QCheckBox(QString(name), curGroup->box);
	      pp->u.multi.groupB->insert(checkbox, j);

	      if (lp->state == PS_ON)
	        checkbox->setChecked(true);

	      curGroup->layout->addWidget(checkbox, curGroup->nElements, j + 2);

	      pp->u.multi.check_v->push_back(checkbox);
	      checkbox->show();

	      break;

	      default:
	      break;

	    }

	    pp->u.multi.labels->push_back(lp);

	}

        if (j < 0)
	 return (-1);

	pp->u.multi.groupB->setFrameShape(QFrame::NoFrame);

        pl.push_back(pp);

	return (0);
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

void INDI_P::newSwitch(int id)
{

  if (guitype== PG_MENU)
  {

  if (u.multi.om_w->text(0).isEmpty())
  {
   kdDebug() << "The first empty option was selected, returning" << endl;
   return;
  }

  /* did it change? */
  if ((*u.multi.labels)[id]->state == PS_ON)
   return;

  // everything else off
  (*u.multi.labels)[u.multi.lastItem]->state = PS_OFF;

 }

  (*u.multi.labels)[id]->state = (*u.multi.labels)[id]->state == PS_ON ? PS_OFF : PS_ON;

  if (guitype == PG_BUTTONS)
  {
    for (int i=0; i < (int) u.multi.push_v->size(); i++)
     	if (i == id)
      (*u.multi.push_v)[i]->setDown(true);
     else
     {
       (*u.multi.push_v)[i]->setDown(false);
       (*u.multi.labels)[i]->state = PS_OFF;
     }

  }

  state = PS_BUSY;

  drawLt(light, state);

  u.multi.lastItem = id;

  dp->parent->sendNewSwitch ((*u.multi.labels)[id], (*u.multi.labels)[id]->state);

}

/* build a menu version of oneOfMany.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::buildMenuGUI (INDI_P *pp, XMLEle *root, char errmsg[])
{
	INDI_L *initlp = NULL;
	int i, j;

	pp->guitype = PG_MENU;

	pp->u.multi.om_w = new KComboBox(curGroup->box);
	curGroup->layout->addMultiCellWidget(pp->u.multi.om_w, curGroup->nElements, curGroup->nElements, 2, 3);

	/* build pulldown menu first */
	/* create each switch.
	 * N.B. can only be one in On state.
	 */
	for (i = 0, j = -1; i < root->nel; i++)
	{
	    XMLEle *sep = root->el[i];
	    char *name = sep->pcdata;
	    XMLAtt *ap;
	    INDI_L *lp;

            if (strcmp (sep->tag, "switch"))
		continue;

	    j++;
	    lp = new INDI_L(pp, name);

	    ap = parent->findAtt (sep, "state", errmsg);
	    if (!ap)
		return (-1);
	    if (crackSwitchState (ap->valu, &lp->state) < 0)
	    {
		sprintf (errmsg, "INDI: <%s> unknown state %s for %s %s %s",
			    root->tag, ap->valu, name, pp->name, name);
		return (-1);
	    }

	    pp->u.multi.om_w->insertItem(QString(name));

	    if (lp->state == PS_ON)
	    {
		if (initlp)
		{
		    sprintf (errmsg,"INDI: <%s> %s %s has multiple On switches",
					root->tag, name, pp->name);
		    return (-1);
		}
		lp->state = PS_ON;
		initlp = lp;
		pp->u.multi.om_w->setCurrentItem(j);
		pp->u.multi.lastItem = j;
	    }

            pp->u.multi.labels->push_back(lp);

	}

	/* empty default */
	if (!initlp)
	{
	  pp->u.multi.om_w->insertItem(QString(""), 0);
	  pp->u.multi.om_w->setCurrentItem(0);
	  pp->u.multi.lastItem = 0;
	}

	QObject::connect( pp->u.multi.om_w, SIGNAL(activated(int)), pp, SLOT(newSwitch(int)));

        pp->u.multi.om_w->show();
	pl.push_back(pp);

	return (0);
}

/* build GUI for a lights GUI.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::buildLightsGUI (XMLEle *root, char errmsg[])
{
	INDI_P *pp;
	int i;

	/* build a new property */
	pp = addProperty (root, errmsg);
	if (!pp)
	    return (-1);

	/* N.B. if trouble from here on must call delP() */

	pp->guitype = PG_LIGHTS;

	pp->u.multi.hLayout = new QHBoxLayout(0, 0, 5);
	curGroup->layout->addMultiCellLayout(pp->u.multi.hLayout, curGroup->nElements, curGroup->nElements, 3, 5);

	XMLEle *lep;
	char *lname;
	XMLAtt *ap;
	INDI_L *lp;
	QLabel *label;
	KLed   *led;

	for (i = 0; i < root->nel; i++)
	{
	    lep = root->el[i];
	    lname = lep->pcdata;

	    if (strcmp (lep->tag, "light"))
		continue;

	    lp = new INDI_L(pp, lname);

	    label = new QLabel(QString(lname), curGroup->box);
	    led   = new KLed(curGroup->box);

	    /* init state */
	    ap = parent->findAtt (lep, "state", errmsg);
	    if (!ap)
	    {
		delete(pp);
		return (-1);
	    }

	    if (crackLightState (ap->valu, &lp->state) < 0)
	     {
		sprintf (errmsg, "INDI: <%s> unknown state %s for %s %s %s",
			    root->tag, ap->valu, name, pp->name, lname);
		delete(pp);
		return (-1);
	    }
	    pp->drawLt(led, lp->state);

	    pp->u.multi.hLayout->addWidget(led);
            pp->u.multi.hLayout->addWidget(label);

            pp->u.multi.label_v->push_back(label);
	    pp->u.multi.led_v->push_back(led);

	    pp->u.multi.labels->push_back(lp);
	}

	pl.push_back(pp);

	return (0);
}


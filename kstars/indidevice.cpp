/*  INDI frontend for KStars
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)
    		       Elwood C. Downey.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    JM Changelog:
    2003-04-28 Used indimenu.c as a template. C --> C++, Xm --> KDE/Qt
    2003-05-01 Added tab for devices and a group feature
    2003-05-02 Added scrolling area. Most things are rewritten
    2003-05-05 Device/Group seperation
    2003-05-29 Replaced raw INDI time with KStars's timedialog
    2003-08-02 Upgrading to INDI v 1.11
    2003-08-09 Initial support for non-sidereal tracking

 */

#include "indidevice.h"
#include "indimenu.h"
#include "indidriver.h"
#include "indi/indicom.h"
#include "kstars.h"
#include "skyobject.h"
#include "timedialog.h"
#include "geolocation.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

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
#include <qbuttongroup.h>
#include <qscrollview.h>
#include <qsocketnotifier.h>
#include <qvbox.h>
#include <qdatetime.h>
#include <qtable.h>
#include <qstring.h>

#include <kled.h>
#include <klineedit.h>
#include <kpushbutton.h>
#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <klistview.h>
#include <kdebug.h>
#include <kcombobox.h>
#include <knuminput.h>
#include <kdialogbase.h>
#include <kstatusbar.h>
#include <kpopupmenu.h>

// Pulse tracking
#define INDI_PULSE_TRACKING   15000

/*******************************************************************
** INDI Label
*******************************************************************/
INDI_L::INDI_L(INDI_P *parentProperty, QString inName, QString inLabel)
{
  name = inName;
  label = inLabel;

  pp = parentProperty;

  label_w = NULL;
  spin_w = NULL;
  push_w = NULL;
  check_w = NULL;
  led_w = NULL;

}

INDI_L::~INDI_L()
{

  if (label_w)
    delete (label_w);
  if (spin_w)
    delete (spin_w);
  if (check_w)
    delete (check_w);
  if (push_w)
    delete (push_w);
  if (led_w)
    delete (led_w);

}

/*******************************************************************
** INDI Property: contains widgets, labels, and their status
*******************************************************************/
INDI_P::INDI_P(INDI_G *parentGroup, QString inName)
{
  name = inName;

  pg = parentGroup;
  isINDIStd = false;

  light = NULL;
  label_w = NULL;
  set_w = NULL;
  parentPopup = NULL;
  groupB = NULL;
  hLayout = NULL;
  table_w = NULL;
  om_w = NULL;

}

/* INDI property desstructor, makes sure everything is "gone" right */
INDI_P::~INDI_P()
{

   if (light)
      delete(light);

     //fprintf(stderr, "light ok\n");
    if (label_w)
      delete(label_w);

     // fprintf(stderr, "label ok\n");
    if (om_w)
      delete (om_w);

     // fprintf(stderr, "om_w ok\n");
    if (hLayout)
      delete (hLayout);

     // fprintf(stderr, "hLayout ok\n");
    if (set_w)
      delete (set_w);

      //fprintf(stderr, "set_w ok\n");
    if (groupB)
      delete (groupB);



     	/*for (uint i=0; i < labels.size(); i++)
	{
	      labels.erase(labels.begin() + i);
              delete (labels[i]);
	}*/

}

bool INDI_P::isOn(QString component)
{

  INDI_L *lp;

  lp = findLabel(component);

  if (!lp)
   return false;

  if (lp->check_w)
    if (lp->check_w->isChecked())
     return true;
  if (lp->push_w)
     if (lp->push_w->isDown())
      return true;

  return false;
}

INDI_L * INDI_P::findLabel(QString lname)
{
  for (unsigned int i=0; i < labels.size(); i++)
    if ( (labels[i]->name == lname) || (labels[i]->label == lname))
     return (labels[i]);

  return NULL;
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

void INDI_P::newText()
{

  for (unsigned int i=0; i < labels.size(); i++)
  {
    if (table_w->cellWidget(i, 1))
    {
     if ((table_w->cellWidget(i, 1))->hasFocus())
    	(table_w->cellWidget(i, 1))->clearFocus();
    }

       // PG_SCALE
      if (labels[i]->spin_w)
      {

        labels[i]->value = labels[i]->spin_w->value();
	//fprintf(stderr, "A spin value chanegd, the value is %g\n", labels[i]->value);
      }
      // PG_NUMERIC or PG_TEXT
      else
      {
        if (perm == PP_WO)
	{
	  if ((table_w->text( i , 0)).isNull())
	   labels[i]->text = "";
	  else
	   labels[i]->text = table_w->text( i , 0);
	}
      	else if ((table_w->text( i , 1)).isNull())
	   labels[i]->text = table_w->text( i , 0);
	else
	    labels[i]->text = table_w->text( i , 1);

	if (guitype == PG_NUMERIC)
        f_scansexa(labels[i]->text.ascii(), &(labels[i]->value));
      }

  }

  state = PS_BUSY;

  drawLt(light, state);

  if (guitype == PG_TEXT)
  pg->dp->parentMgr->sendNewText(this);
  else if (guitype == PG_NUMERIC)
  pg->dp->parentMgr->sendNewNumber(this);

}

void INDI_P::convertSwitch(int id)
{

 int RARowIndex, DECRowIndex;
 int switchIndex=0;
 INDI_P *prop;
 INDI_L *lp;

 if (parentPopup == NULL)
  return;

 //fprintf(stderr, "Property name is %s. Conevrting a switch, we have %s\n", name, parentPopup->text(id).ascii());

 lp = findLabel(parentPopup->text(id));

 if (!lp)
  return;

 //if (lp->state == PS_ON)
  //return;

 for (uint i=0; i < labels.size(); i++)
 {
   if (labels[i]->label == parentPopup->text(id))
   {
     switchIndex = i;
     break;
   }
 }

 //fprintf(stderr, "The label is %s and its state is %s. Also, the switch index is %d\n", lp->name, lp->state == PS_OFF ? "Off" : "On", switchIndex);

 if ((lp->name == "Slew") || (lp->name == "Sync") || (lp->name == "Track"))
 {
    // #1 set current object to NULL
    pg->dp->currentObject = NULL;
   // #2 Deactivate timer if present
   if (pg->dp->devTimer->isActive())
		 pg->dp->devTimer->stop();


   prop = pg->dp->findProp(QString("EQUATORIAL_COORD"));
       if (prop == NULL)
        return;

	// Track is a special case
	 if ((lp->name == "Track"))
        	if (pg->dp->handleNonSidereal(pg->dp->parent->ksw->map()->clickedObject()))
		        return;

        /*fprintf(stderr, "\n******** The point BEFORE it was precessed ********\n");
	fprintf(stderr, "RA : %s - DEC : %s\n", pg->dp->parent->ksw->map()->clickedPoint()->ra()->toHMSString().ascii(),
	pg->dp->parent->ksw->map()->clickedPoint()->dec()->toDMSString().ascii());*/

	SkyPoint sp;

       // We need to get from JNow (Skypoint) to J2000
       // The ra0() of a skyPoint is the same as its JNow ra() without this process
       if (pg->dp->currentObject)
         sp.set (pg->dp->currentObject->ra(), pg->dp->currentObject->dec());
       else
         sp.set (pg->dp->parent->ksw->map()->clickedPoint()->ra(), pg->dp->parent->ksw->map()->clickedPoint()->dec());

       sp.apparentCoord( pg->dp->parent->ksw->data()->clock()->JD() , (long double) J2000);

       // Use J2000 coordinate as required by INDI
       if ( (prop->labels[0]->name == "RA"))
         { RARowIndex = 0; DECRowIndex = 1;}
       else
         { RARowIndex = 1; DECRowIndex = 0;}

       switch (prop->perm)
       {
         case PP_RW:
	    prop->table_w->setText( RARowIndex, 1, QString("%1:%2:%3").arg(sp.ra()->hour()).arg(sp.ra()->minute()).arg(sp.ra()->second()));
            prop->table_w->setText( DECRowIndex, 1, QString("%1:%2:%3").arg(sp.dec()->degree()).arg(sp.dec()->arcmin()).arg(sp.dec()->arcsec()));

	   if (lp->state == PS_OFF)
	   	newSwitch(switchIndex);
	   prop->newText();
	   break;

	 case PP_WO:
	  prop->table_w->setText( RARowIndex, 0, QString("%1:%2:%3").arg(sp.ra()->hour()).arg(sp.ra()->minute()).arg(sp.ra()->second()));
            prop->table_w->setText( DECRowIndex, 0, QString("%1:%2:%3").arg(sp.dec()->degree()).arg(sp.dec()->arcmin()).arg(sp.dec()->arcsec()));

           if (lp->state == PS_OFF)
		   newSwitch(switchIndex);
	   prop->newText();
	 default:
	  break;
        }
	return;
   }
   else if ((lp->name == "Abort"))
   {
         fprintf(stderr, "Stopping timer.\n");
	 pg->dp->devTimer->stop();
	 if (lp->state == PS_OFF)
	 	newSwitch(switchIndex);
	 return;
   }
   else if (lp->state == PS_OFF)
         newSwitch(switchIndex);

}

void INDI_P::updateTime()
{
  QTime newTime( pg->dp->parent->ksw->data()->clock()->UTC().time());
  QDate newDate( pg->dp->parent->ksw->data()->clock()->UTC().date());

  switch (perm)
  {
   case PP_RW:
     table_w->setText(0, 1, QString("%1-%2-%3T%4:%5:%6").arg(newDate.year()).arg(newDate.month())
					.arg(newDate.day()).arg(newTime.hour())
					.arg(newTime.minute()).arg(newTime.second()));
     newText();
     break;
   case PP_WO:
     table_w->setText(0, 0, QString("%1-%2-%3T%4:%5:%6").arg(newDate.year()).arg(newDate.month())
					.arg(newDate.day()).arg(newTime.hour())
					.arg(newTime.minute()).arg(newTime.second()));
     newText();
     break;
    default:
     break;
  }

}

void INDI_P::updateLocation()
{

   GeoLocation *geo = pg->dp->parent->ksw->geo();
   int longRowIndex, latRowIndex;

  if (name == "GEOGRAPHIC_COORD")
  {
    dms tempLong (geo->lng()->degree(), geo->lng()->arcmin(), geo->lng()->arcsec());
    dms fullCir(360,0,0);

    if (tempLong.degree() < 0)
      tempLong.setD ( fullCir.Degrees() + tempLong.Degrees());

    if (labels[0]->name == "LONG")
    { longRowIndex = 0; latRowIndex = 1;}
    else
    { longRowIndex = 1; latRowIndex = 0;}

    switch (perm)
    {
      case PP_RW:
          table_w->setText(longRowIndex, 1, QString("%1:%2:%3").arg(tempLong.degree()).arg(tempLong.arcmin()).arg(tempLong.arcsec()));
	  table_w->setText(latRowIndex, 1, QString("%1:%2:%3").arg(geo->lat()->degree()).arg(geo->lat()->arcmin()).arg(geo->lat()->arcsec()));
	  newText();
	  break;
      case PP_WO:
          table_w->setText(longRowIndex, 0, QString("%1:%2:%3").arg(tempLong.degree()).arg(tempLong.arcmin()).arg(tempLong.arcsec()));
	  table_w->setText(latRowIndex, 0, QString("%1:%2:%3").arg(geo->lat()->degree()).arg(geo->lat()->arcmin()).arg(geo->lat()->arcsec()));
	  newText();
	  break;
      default:
         break;
  }
 }
}


void INDI_P::newTime()
{

TimeDialog timedialog ( pg->dp->parent->ksw->data()->UTime, pg->dp->parent->ksw, true );

	if ( timedialog.exec() == QDialog::Accepted )
	{
		QTime newTime( timedialog.selectedTime() );
		QDate newDate( timedialog.selectedDate() );

                table_w->setText(0,1, QString("%1-%2-%3T%4:%5:%6")
					.arg(newDate.year()).arg(newDate.month())
					.arg(newDate.day()).arg(newTime.hour())
					.arg(newTime.minute()).arg(newTime.second()));
	        newText();
	}

}

void INDI_P::newSwitch(int id)
{

  QFont buttonFont;
  INDI_L *lp;

  lp = labels[id];

  if (guitype== PG_MENU)
  {

   //fprintf(stderr, "We received menu ID %d\n", id);
   if (lp->state == PS_ON)
    return;

   for (unsigned int i=0; i < labels.size(); i++)
   {
     if (i == (unsigned) id)
       labels[i]->state = PS_ON;
     else
       labels[i]->state = PS_OFF;
   }

  }

  if (guitype == PG_BUTTONS)
  {
    for (unsigned int i=0; i < labels.size(); i++)
     	if (i != (unsigned) id)
	{
	  if (!labels[i])
	   continue;

	  labels[i]->push_w->setDown(false);
       	  buttonFont = labels[i]->push_w->font();
	  buttonFont.setBold(FALSE);
	  labels[i]->push_w->setFont(buttonFont);
          labels[i]->state = PS_OFF;
        }

        lp->push_w->setDown(true);
	buttonFont = lp->push_w->font();
	buttonFont.setBold(TRUE);
	lp->push_w->setFont(buttonFont);
	lp->state = lp->state == PS_ON ? PS_OFF : PS_ON;

	if (name == "ABORT_MOTION")
	  pg->dp->devTimer->stop();

  }

  state = PS_BUSY;

  drawLt(light, state);

  pg->dp->parentMgr->sendNewSwitch (this);


}

/*******************************************************************
** INDI Group: a group box for common properties. All properties
** belong to a group, whether they have one or not but how the group
** is displayed differs
*******************************************************************/
INDI_G::INDI_G(INDI_D *parentDevice, QString inName)
{
  dp = parentDevice;

 if (inName.isEmpty())
  {
    box = new QGroupBox(dp->propertyLayout);
    box->setFrameShape(QFrame::NoFrame);
    layout = new QGridLayout(box, 1, 1, 5 , 5);
  }
  else
  {
  box = new QGroupBox(inName, dp->propertyLayout);
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

void INDI_G::addProperty(INDI_P *pp)
{
   dp->registerProperty(pp);
   pl.push_back(pp);
}

int INDI_G::removeProperty(INDI_P *pp)
{

  for (uint i=0; i < pl.size(); i++)
   if (pl[i]->name == pp->name)
   {
     delete (pp);
     pl.erase(pl.begin() + i);
     return (0);
   }

   return (-1);

}

/*******************************************************************
** INDI Device: The work-horse. Responsible for handling its
** child properties and managing signal and changes.
*******************************************************************/
INDI_D::INDI_D(INDIMenu *menuParent, DeviceManager *parentManager, QString inName, QString inLabel)
{
  name = inName;
  label = inLabel;

  parent = menuParent;
  parentMgr = parentManager;

 tabContainer  = new QFrame( parent->deviceContainer);
 parent->deviceContainer->addTab (tabContainer, label);

 curGroup       = NULL;
 biggestLabel   = 0;
 widestTableCol = 0;
 initDevCounter = 0;
 currentObject  = NULL;

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
 clear->setDefault(false);
 //savelog = new QPushButton(i18n("Save Log..."), tabContainer);

 buttonLayout->addWidget(clear);
 //buttonLayout->addWidget(savelog);
 buttonLayout->addItem(hSpacer);

 mainLayout->addWidget(sv);
 mainLayout->addItem(vSpacer);
 mainLayout->addWidget(msgST_w);
 mainLayout->addLayout(buttonLayout);

 msgST_w->setFocus();

 QObject::connect(clear, SIGNAL(clicked()), msgST_w, SLOT(clear()));

 /* hmmmmmm, a way to know the optimal widget height, there is a probably
 * a better way to do it, but oh well! */
  KLineEdit *tempLine = new KLineEdit(tabContainer);
  uniHeight = tempLine->sizeHint().height();
  delete(tempLine);

  INDIStdSupport = false;
  devTimer = new QTimer(this);
  QObject::connect( devTimer, SIGNAL(timeout()), this, SLOT(timerDone()) );

}

INDI_D::~INDI_D()
{
   for (uint j=0; j < gl.size(); j++)
	      for (uint k=0; k < gl[j]->pl.size(); k++)
	          removeProperty(gl[j]->pl[0]);

  if (tabContainer)
    delete (tabContainer);

}

void INDI_D::registerProperty(INDI_P *pp)
{
   if (isINDIStd(pp))
   {
    pp->isINDIStd = true;
    pp->pg->dp->INDIStdSupport = true;
   }
}

bool INDI_D::isINDIStd(INDI_P *pp)
{

  if ( (pp->name == QString("CONNECTION"))   || (pp->name == QString("EQUATORIAL_COORD")) ||
     (pp->name == QString("ON_COORD_SET")) || (pp->name == QString("ABORT_MOTION"))     ||
     (pp->name == QString("SOLAR_SYSTEM")) || (pp->name == QString("GEOGRAPHIC_COORD")) ||
     (pp->name == QString("HORIZONTAL_COORD")) || (pp->name == QString("TIME")))
    return true;

  return false;
}

bool INDI_D::handleNonSidereal(SkyObject *o)
{

   if (!o)
    return false;

    int trackIndex=0;

    kdDebug() << "Object of type " << o->typeName() << endl;
  //TODO Meade claims that the library access is available to
  // all telescopes, which is unture. Only classic meade support
  // that. They claim that library funcion will be available to all
  // in "later" firmware revisions for the autostar and GPS.
  // As a temprory solution, I'm going to explicity check for the
  // device name which ideally I'm not supposed to do since all properties
  // should be defined from the INDI driver, but since the INDI autostar
  // and gps define the library functions (based on Meade's future claims)
  // I will check the device name until Meade's respondes to my query.

  // Update: Solution
  // Only Meade Classic will offer an explicit SOLAR_SYSTEM property. If such a property exists
  // then we take advantage of it. Otherwise, we send RA/DEC to the telescope and start a timer
  // based on the object type. Objects with high proper motions will require faster updates.
  // handle Non Sideral is ONLY called when tracking an object, not slewing.


  INDI_P *prop = findProp(QString("SOLAR_SYSTEM"));
  INDI_P *setMode = findProp(QString("ON_COORD_SET"));

  // If the device support it
  if (prop && setMode)
  {
    for (uint i=0; i < setMode->labels.size(); i++)
      if (setMode->labels[i]->name == "Track")
      { trackIndex = i; break; }

    kdDebug() << "Device supports SOLAR_SYSTEM property" << endl;

    for (unsigned int i=0; i < prop->labels.size(); i++)
     if (o->name().lower() == prop->labels[i]->label.lower())
     {
       setMode->newSwitch(trackIndex);
       prop->newSwitch(i);
       return true;
     }
  }

   kdDebug() << "Device doesn't support SOLAR_SYSTEM property, issuing a timer" << endl;
   kdDebug() << "Starting timer for object of type " << o->typeName() << endl;
   currentObject = o;

   switch (currentObject->type())
   {
    // Planet/Moon
    case 2:
       kdDebug() << "Initiating pulse tracking for " << currentObject->name() << endl;
       devTimer->start(INDI_PULSE_TRACKING);
       break;
    // Comet/Asteroid
    case 9:
    case 10:
      kdDebug() << "Initiating pulse tracking for " << currentObject->name() << endl;
      devTimer->start(INDI_PULSE_TRACKING);
      break;
    default:
      break;
    }

   return false;
}

void INDI_D::timerDone()
{
      INDI_P *prop;

       int RARowIndex, DECRowIndex;

       if (!isOn())
       {
        devTimer->stop();
	return;
       }

       prop = findProp(QString("ON_COORD_SET"));
       if (prop == NULL || !currentObject)
        return;

       // wait until slew is done
       if (prop->state == PS_BUSY)
        return;

       kdDebug() << "Timer called, starting processing" << endl;


       prop = findProp(QString("EQUATORIAL_COORD"));
       if (prop == NULL)
        return;


	SkyPoint sp(currentObject->ra(), currentObject->dec());

        sp.apparentCoord( parent->ksw->data()->clock()->JD() , (long double) J2000);


       // We need to get from JNow (Skypoint) to J2000
       // The ra0() of a skyPoint is the same as its JNow ra() without this process

       // Use J2000 coordinate as required by INDI
       if (prop->labels[0]->name == "RA")
         { RARowIndex = 0; DECRowIndex = 1;}
       else
         { RARowIndex = 1; DECRowIndex = 0;}

       switch (prop->perm)
       {
         case PP_RW:
	    prop->table_w->setText( RARowIndex, 1,  QString("%1:%2:%3").arg(sp.ra()->hour())
        						 .arg(sp.ra()->minute())
							 .arg(sp.ra()->second()));
            prop->table_w->setText( DECRowIndex, 1, QString("%1:%2:%3").arg(sp.dec()->degree())
                                                         .arg(sp.dec()->arcmin())
							 .arg(sp.dec()->arcsec()));
	    prop->newText();
	 case PP_WO:
	    prop->table_w->setText( RARowIndex, 0,  QString("%1:%2:%3").arg(sp.ra()->hour())
        						 .arg(sp.ra()->minute())
							 .arg(sp.ra()->second()));
            prop->table_w->setText( DECRowIndex, 0, QString("%1:%2:%3").arg(sp.dec()->degree())
                                                         .arg(sp.dec()->arcmin())
							 .arg(sp.dec()->arcsec()));
	    prop->newText();
	 default:
	  break;
        }

}


int INDI_D::removeProperty(INDI_P *pp)
{
    QString propName(pp->name);

    if (!pp->pg->removeProperty(pp))
    {
    	for (unsigned int j=0; j < gl.size(); j++)
    	{
		if (gl[j]->pl.size() == 0)
		{
	        	delete (gl[j]);
			gl.erase(gl.begin() + j);
		}
    	}

	return (0);
    }

    kdDebug() << "INDI: Device " << name << " has no property named " << propName << endl;
    return (-1);

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
						root->tag, name.ascii(), ap->valu);
	    return (-1);
	}

	parentMgr->checkMsg (root, this);

	return (setValue (pp, root, errmsg));
}

/* set the given GUI property according to the XML command.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setValue (INDI_P *pp, XMLEle *root, char errmsg[])
{
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
						root->tag, ap->valu, name.ascii(), pp->name.ascii());
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

	case PG_NUMERIC:	/* FALLTHRU */
	case PG_TEXT:
	    return (setTextValue (pp, root, errmsg));
	    break;

	case PG_BUTTONS:	/* FALLTHRU */
	case PG_LIGHTS:
	case PG_RADIO:		/* FALLTHRU */
	case PG_MENU:
	    return (setLabelState (pp, root, errmsg));
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
int INDI_D::setTextValue (INDI_P *pp, XMLEle *root, char errmsg[])
{
	XMLEle *ep;
	char iNumber[32];

	for (int i = 0; i < root->nel; i++)
	{
	    ep = root->el[i];

	    if (strcmp (ep->tag, "setText") && strcmp(ep->tag, "setNumber"))
		continue;

	    //fprintf(stderr, "tag okay, getting perm\n");
	   switch (pp->perm)
	   {
	   case PP_RW:	// FALLTHRU
	   case PP_RO:
	     if (pp->guitype == PG_TEXT)
	     {
	     	pp->table_w->setText(i , 0, QString(ep->pcdata));
		pp->labels[i]->text = QString(ep->pcdata);
             }
	     else if (pp->guitype == PG_NUMERIC)
	     {
	       pp->labels[i]->value = atof(ep->pcdata);
	       numberFormat(iNumber, pp->labels[i]->format.ascii(), pp->labels[i]->value);
	       pp->labels[i]->text = iNumber;
	       pp->table_w->setText(i , 0, pp->labels[i]->text);
	     }
	     break;

	    break;
	case PP_WO:
	    if (pp->guitype == PG_TEXT)
	       pp->table_w->setText(i , 0, QString(ep->pcdata));
	    else if (pp->guitype == PG_NUMERIC)
	    {
	      pp->labels[i]->value = atof(ep->pcdata);
	      numberFormat(iNumber, pp->labels[i]->format.ascii(), pp->labels[i]->value);
	      pp->labels[i]->text = iNumber;

	      if (pp->labels[i]->spin_w)
                pp->labels[i]->spin_w->setValue(pp->labels[i]->value);
	      else
 	        pp->table_w->setText(i , 0, pp->labels[i]->text);
	    }


	    break;
	   }
	}

        if (pp->name == "EQUATORIAL_COORD")
	  parent->ksw->map()->forceUpdateNow();
	else if  ( ((pp->name == "TIME") && parent->ksw->options()->indiAutoTime) ||
	            ((pp->name == "GEOGRAPHIC_COORD") && parent->ksw->options()->indiAutoGeo))
		  handleDevCounter();

        // suppress warning
	errmsg = errmsg;

	return (0);
}

void INDI_D::handleDevCounter()
{

  if (initDevCounter <= 0)
   return;

  initDevCounter--;

  if (initDevCounter == 0 && parent->ksw->options()->indiMessages)
    parent->ksw->statusBar()->changeItem( i18n("%1 is online and ready.").arg(name), 0);

}

/* set the user input TEXT or NUMERIC field from the given element.
 * root should have <text> or <number> child.
 * ignore if property is not RW.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::newTextValue (INDI_P *pp, XMLEle *root, char errmsg[])
{
        XMLEle *ep;
	char iNumber[32];

	for (int i = 0; i < root->nel; i++)
	{
	    ep = root->el[i];

	    if (strcmp (ep->tag, "newText") && strcmp(ep->tag, "newNumber"))
		continue;

	   switch (pp->perm)
	   {
	   case PP_RW:	// FALLTHRU
	   case PP_WO:
	     if (pp->guitype == PG_TEXT)
	     	pp->table_w->setText(i , pp->perm == PP_RW ? 1 : 0, QString(ep->pcdata));
             else if (pp->guitype == PG_NUMERIC)
	     {
	       pp->labels[i]->value = atof(ep->pcdata);
	       numberFormat(iNumber, pp->labels[i]->format.ascii(), pp->labels[i]->value);
	       pp->labels[i]->text = iNumber;

	       pp->table_w->setText(i , pp->perm == PP_RW ? 1 : 0, pp->labels[i]->text);
	     }

	   case PP_RO:
	      break;
	   }
	}

	// suppress warning
	errmsg = errmsg;

	return (0);
}

/* set the given BUTTONS or LIGHTS property from the given element.
 * root should have some <switch> or <light> children.
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI_D::setLabelState (INDI_P *pp, XMLEle *root, char errmsg[])
{
	int menuChoice=0;

	/* for each child element */
	for (unsigned i = 0; i < (unsigned int) root->nel; i++)
	{
	    XMLEle *ep = root->el[i];
	    XMLAtt *ap;
	    INDI_L *lp = NULL;
	    int islight;
	    PState state;

	    /* only using light and switch */
	    islight = !strcmp (ep->tag, "setLight");
	    if (!islight && strcmp (ep->tag, "setSwitch"))
		continue;

	    ap =  findXMLAtt (ep, "name");
	    /* no name */
	    if (!ap)
	    {
		sprintf (errmsg, "INDI: <%s> %s %s %s requires name",
						    root->tag, name.ascii(), pp->name.ascii(), ep->tag);
		return (-1);
	    }

	    if ((islight && crackLightState (ep->pcdata, &state) < 0)
		    || (!islight && crackSwitchState (ep->pcdata, &state) < 0))
	    {
		sprintf (errmsg, "INDI: <%s> unknown state %s for %s %s %s",
					    root->tag, ep->pcdata, name.ascii(), pp->name.ascii(), ep->tag);
		return (-1);
	    }

	    /* find matching label */
	    //fprintf(stderr, "Find matching label. Name from XML is %s\n", ap->valu);
	    lp = pp->findLabel(QString(ap->valu));

	    if (!lp)
	    {
		sprintf (errmsg,"INDI: <%s> %s %s has no choice named %s",
						    root->tag, name.ascii(), pp->name.ascii(), ap->valu);
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
		buttonFont.setBold(state == PS_ON ? TRUE : FALSE);
		lp->push_w->setFont(buttonFont);

		if (state == PS_ON && (lp->name == "CONNECT"))
		{
		  initDeviceOptions();
		  emit linkAccepted();
		}
		else if (state == PS_ON && (lp->name == "DISCONNECT"))
		{
		  parent->ksw->map()->forceUpdateNow();
		  emit linkRejected();
		}
		break;

	     case PG_RADIO:
                 lp->check_w->setChecked(state == PS_ON ? true : false);
		 break;
	     case PG_MENU:
	       if (state == PS_ON)
	       {
	      	if (menuChoice)
	      	{
	        	sprintf(errmsg, "INDI: <%s> %s %s has multiple ON states", root->tag, name.ascii(), pp->name.ascii());
			return (-1);
              	}
	      	menuChoice = 1;
	        pp->om_w->setCurrentItem(i);
	       }
	       break;

	     case PG_TEXT:
	      if (!islight)
	       break;
        	pp->drawLt (lp->led_w, lp->state);
		break;
	     default:
	      break;
	   }

	}

	return (0);
}

void INDI_D::initDeviceOptions()
{
  INDI_P *prop;

  initDevCounter = 0;

  if (parent->ksw->options()->indiAutoTime)
  {
  	prop = findProp(QString("TIME"));
  	  if (prop)
	  {
            prop->updateTime();
	    initDevCounter += 2;
	  }
  }

  if (parent->ksw->options()->indiAutoGeo)
  {
  	prop = findProp(QString("GEOGRAPHIC_COORD"));
   	   if (prop)
	   {
   		prop->updateLocation();
         	initDevCounter += 2;
	   }
  }

  if (parent->ksw->options()->indiMessages)
    parent->ksw->statusBar()->changeItem( i18n("%1 is online.").arg(name), 0);

  parent->ksw->map()->forceUpdateNow();

}

bool INDI_D::isOn()
{

  INDI_P *prop;

  prop = findProp(QString("CONNECTION"));
  if (!prop)
   return false;

  return (prop->isOn(QString("CONNECT")));
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
						root->tag, name.ascii(), ap->valu);
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

	case PG_NUMERIC:	/* FALLTHRU */
	case PG_TEXT:
	    return (newTextValue (pp, root, errmsg));
	    break;

	case PG_BUTTONS:	/* FALLTHRU */
	case PG_LIGHTS:
	case PG_RADIO:		/* FALLTHRU */
	case PG_MENU:
	    return (setLabelState (pp, root, errmsg));
	    break;


	default:
	    break;
	}

	return (0);
}

INDI_P * INDI_D::addProperty (XMLEle *root, char errmsg[])
{
	INDI_P *pp = NULL;
	INDI_G *pg = NULL;
	XMLAtt *ap = NULL;

        // Search for group tag
	ap = parent->findAtt (root, "grouptag", errmsg);
        if (!ap)
        {
                kdDebug() << QString(errmsg) << endl;
                return NULL;
        }
	// Find an existing group, if none found, create one
        pg = findGroup(QString(ap->valu), 1, errmsg);

	if (!pg)
	 return NULL;

        /* get property name and add new property to dp */
	ap = parent->findAtt (root, "name", errmsg);
	if (ap == NULL)
	    return NULL;

	if (findProp (ap->valu))
	{
	    sprintf (errmsg, "INDI: <%s %s %s> already exists", root->tag,
							name.ascii(), ap->valu);
	    return NULL;
	}

	pp = new INDI_P(pg, QString(ap->valu));

	/* N.B. if trouble from here on must call delP() */

	/* init state */
	ap = parent->findAtt (root, "state", errmsg);
	if (!ap)
	{
	    delete(pp);
	    return (NULL);
	}

	if (crackLightState (ap->valu, &pp->state) < 0)
	{
	    sprintf (errmsg, "INDI: <%s> bogus state %s for %s %s",
				root->tag, ap->valu, pp->pg->dp->name.ascii(), pp->name.ascii());
	    delete(pp);
	    return (NULL);
	}

	//TODO
	/* init timeout */
	ap = parent->findAtt (root, "timeout", NULL);
	/* default */
	pp->timeout = ap ? atof(ap->valu) : 0;

	/* log any messages */
	parentMgr->checkMsg (root, this);

	if (addGUI (pp, root, errmsg) < 0)
	{
	   delete(pp);
	   return (NULL);
	}

	/* ok! */
	return (pp);
}

INDI_P * INDI_D::findProp (QString name)
{
       for (unsigned int i = 0; i < gl.size(); i++)
	for (unsigned int j = 0; j < gl[i]->pl.size(); j++)
	    if (name == gl[i]->pl[j]->name)
		return (gl[i]->pl[j]);

	return NULL;
}

INDI_G *  INDI_D::findGroup (QString grouptag, int create, char errmsg[])
{
  for (int i= gl.size() - 1 ; i >= 0; i--)
  {
    curGroup      = gl[i];

    // If the new group is empty and the LAST group is not, then
    // create a new group, otherwise return the LAST empty group
    if (grouptag.isEmpty())
    {
      if ((!curGroup->box->title().isEmpty()) && create)
       break;
      else
       return curGroup;
    }

    if (grouptag == curGroup->box->title())
      return (curGroup);
  }

  /* couldn't find an existing group, create a new one if create is 1*/
  if (create)
  {
  	curGroup = new INDI_G(this, grouptag);
  	gl.push_back(curGroup);
        return curGroup;
  }

  sprintf (errmsg, "INDI: group %s not found in %s", grouptag.ascii(), name.ascii());
  return NULL;
}

/* find "perm" attribute in root, crack and set *pp.
 * return 0 if ok else -1 with excuse in errmsg[]
 */

 int INDI_D::findPerm (INDI_P *pp, XMLEle *root, PPerm *permp, char errmsg[])
{
	XMLAtt *ap;

	ap = findXMLAtt(root, "perm");
	if (!ap) {
	    sprintf (errmsg, "INDI: <%s %s %s> missing attribute 'perm'",
					root->tag, pp->pg->dp->name.ascii(), pp->name.ascii());
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
				root->tag, ap->valu, pp->pg->dp->name.ascii(), pp->name.ascii());
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

void INDI_D::resizeTableHeaders()
{

  for (uint i=0; i < gl.size(); i++)
   for (uint j=0; j < gl[i]->pl.size(); j++)
  {
        if (gl[i]->pl[j]->table_w)
	     gl[i]->pl[j]->table_w->setLeftMargin(widestTableCol);
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
	//XMLEle *prompt;
	XMLAtt *prompt;

	/* add to GUI group */
	pp->light = new KLed (curGroup->box);
	pp->drawLt(pp->light, pp->state);
        pp->light->setMaximumSize(16,16);
	curGroup->layout->addWidget(pp->light, curGroup->pl.size(), 0);

	/* add label for prompt */
	//prompt = parent->findEle (root, pp, "prompt", errmsg);
	  prompt = parent->findAtt(root, "label", errmsg);
	if (prompt)
	{
	 // use prop name is label is empty
	 if (!strcmp (prompt->valu, ""))
	 pp->label_w = new QLabel(QString(pp->name), curGroup->box);
	 else
	   pp->label_w = new QLabel(QString(prompt->valu), curGroup->box);
	 curGroup->layout->addWidget(pp->label_w, curGroup->pl.size(), 1);

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

int INDI_D::buildTextGUI(XMLEle *root, char errmsg[])
{
       	INDI_P *pp = NULL;
	INDI_L *lp = NULL;
	XMLEle *text;
	XMLAtt *ap;
	//char *tlabel;
	//char *tname;
	PPerm p;


	/* build a new property */
	pp = addProperty (root, errmsg);

	if (pp == NULL)
	    return (-1);

	//fprintf(stderr, "in build new Text for Property %s\n", pp->name);

	/* N.B. if trouble from here one must call delP() */
	/* get the permission, it will determine layout issues */
	if (findPerm (pp, root, &p, errmsg))
	{
	    delete(pp);
	    return (-1);
	}

	//pp->hLayout = new QHBoxLayout(0, 0, 5);
	//curGroup->layout->addMultiCellLayout(pp->hLayout, curGroup->pl.size(), curGroup->pl.size(), 2, 5);
        pp->table_w = new QTable (curGroup->box);

	switch (p)
	{
	  case PP_RW:
	   pp->table_w->setNumCols(2);
	   pp->table_w->horizontalHeader()->setLabel( 0, i18n("Current"));
	   pp->table_w->horizontalHeader()->setLabel( 1, i18n("Set"));
	   pp->table_w->setColumnReadOnly(0, true);
	   break;

	  case PP_RO:
	   pp->table_w->setNumCols(1);
	   pp->table_w->horizontalHeader()->setLabel( 0, i18n("Current"));
	   pp->table_w->setColumnReadOnly(0, true);
	   break;

	  case PP_WO:
	   pp->table_w->setNumCols(1);
	   pp->table_w->horizontalHeader()->setLabel( 0, i18n("Set"));

	   break;
	 }

	 pp->table_w->setColumnStretchable(0, true);

	for (int i = 0; i < root->nel; i++)
	{
	    text = root->el[i];

	    if (strcmp (text->tag, "defText"))
		continue;

	    pp->table_w->insertRows(i);
	    pp->table_w->setRowHeight(i, uniHeight);

	    ap = findXMLAtt(text, "name");
	    if (!ap)
	    {
	        kdDebug() << "Error: unable to find attribute 'name' for property " << pp->name << endl;
	        delete(pp);
	        return (-1);
	    }

	    char * tname = new char[ strlen(ap->valu) + 1];
	    strcpy(tname, ap->valu);

	    ap = findXMLAtt(text, "label");

	    if (!ap)
	    {
	      kdDebug() << "Error: unable to find attribute 'label' for property " << pp->name << endl;
	      delete(pp);
	      return (-1);
	    }

            char * tlabel = new char[ strlen(ap->valu) + 1];
	    strcpy(tlabel, ap->valu);

	    if (!strcmp(tlabel, ""))
	    {
	     tlabel = new char [ strlen(tname) + 1];
	     strcpy(tlabel, tname);
	    }

	    lp = new INDI_L(pp, QString(tname), QString(tlabel));

	     pp->labels.push_back(lp);

	     pp->table_w->verticalHeader()->setLabel(i, tlabel);
	     pp->table_w->setText(i , 0, QString(text->pcdata));

	     if (pp->table_w->verticalHeader()->sizeHint().width() > widestTableCol)
	     {
	        widestTableCol = pp->table_w->verticalHeader()->sizeHint().width();
		if (widestTableCol > 200)
		  widestTableCol = 200;

		resizeTableHeaders();
 	     }
	     else
	      pp->table_w->setLeftMargin(widestTableCol);


	 }

	/* we know it will be a general text GUI */
	pp->guitype = PG_TEXT;
	pp->perm = p;

	pp->table_w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	pp->table_w->setMaximumHeight(pp->labels.size() * uniHeight + pp->table_w->horizontalHeader()->height() + 5);


	  curGroup->layout->addMultiCellWidget(pp->table_w, curGroup->pl.size(), curGroup->pl.size(), 2, 5);

        if (pp->perm != PP_RO)
	{
	    // INDI STD, but we use our own controls
	    if (pp->name == "TIME")
	    {
	     	pp->set_w   = new QPushButton(i18n("Set Time..."), curGroup->box);
	        QObject::connect(pp->set_w, SIGNAL(clicked()), pp, SLOT(newTime()));
	    }
	    else
	    {
		pp->set_w   = new QPushButton(i18n("Set"), curGroup->box);
        	QObject::connect(pp->set_w, SIGNAL(clicked()), pp, SLOT(newText()));
            }
		curGroup->layout->addWidget(pp->set_w, curGroup->pl.size(), 6);
	}

	curGroup->addProperty(pp);

	return (0);
}

/* build GUI for a number property.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::buildNumberGUI (XMLEle *root, char *errmsg)
{

        INDI_P *pp = NULL;
	INDI_L *lp = NULL;
	XMLEle *number;
	XMLAtt *ap;
	char *nlabel=NULL;
	char *nname=NULL;
	char iNumber[32];
	PPerm p;

	/* build a new property */
	pp = addProperty (root, errmsg);

	if (pp == NULL)
	    return (-1);

	/* N.B. if trouble from here one must call delP() */
	/* get the permission, it will determine layout issues */
	if (findPerm (pp, root, &p, errmsg))
	{
	    delete(pp);
	    return (-1);
	}

	//pp->hLayout = new QHBoxLayout(0, 0, 5);
	//curGroup->layout->addMultiCellLayout(pp->hLayout, curGroup->pl.size(), curGroup->pl.size(), 2, 5);
        pp->table_w = new QTable (curGroup->box);

	switch (p)
	{
	  case PP_RW:
	   pp->table_w->setNumCols(2);
	   pp->table_w->horizontalHeader()->setLabel( 0, i18n("Current"));
	   pp->table_w->horizontalHeader()->setLabel( 1, i18n("Set"));
	   pp->table_w->setColumnReadOnly(0, true);
	   break;

	  case PP_RO:
	   pp->table_w->setNumCols(1);
	   pp->table_w->horizontalHeader()->setLabel( 0, i18n("Current"));
	   pp->table_w->setColumnReadOnly(0, true);
	   break;

	  case PP_WO:
	   pp->table_w->setNumCols(1);
	   pp->table_w->horizontalHeader()->setLabel( 0, i18n("Set"));

	   break;
	 }

	 pp->table_w->setColumnStretchable(0, true);


	for (int i = 0; i < root->nel; i++)
	{
	    number = root->el[i];

	    if (strcmp (number->tag, "defNumber"))
		continue;

	    //initstr = text->pcdata;

	    pp->table_w->insertRows(i);
	    pp->table_w->setRowHeight(i, uniHeight);


	    ap = findXMLAtt(number, "name");
	    if (!ap)
	    {
	        kdDebug() << "Error: unable to find attribute 'name' for property " << pp->name << endl;
	        delete(pp);
	        return (-1);
	    }

	    nname = new char [strlen(ap->valu) + 1];
	    strcpy(nname, ap->valu);

	    ap = findXMLAtt(number, "label");

	    if (!ap)
	    {
	      kdDebug() << "Error: unable to find attribute 'label' for property " << pp->name << endl;
	      delete(pp);
	      return (-1);
	    }

	    nlabel = new char [strlen(ap->valu) + 1];
	    strcpy(nlabel, ap->valu);

	    if (!strcmp(nlabel, ""))
	    {
	     nlabel = new char [ strlen(nname) + 1];
	     strcpy(nlabel, nname);
	    }

	    lp = new INDI_L(pp, nname, nlabel);

	    ap = findXMLAtt (number, "min");
	    if (ap)
		lp->min = atof(ap->valu);
	    ap = findXMLAtt (number, "max");
	    if (ap)
		lp->max = atof(ap->valu);
	    ap = findXMLAtt (number, "step");
	    if (ap)
		lp->step = atof(ap->valu);
	    ap = findXMLAtt (number, "format");
	    if (ap)
		lp->format = ap->valu;

	    lp->value = atof(number->pcdata);

	    numberFormat(iNumber, lp->format.ascii(), lp->value);
	    lp->text = iNumber;

	    pp->labels.push_back(lp);

	     pp->table_w->verticalHeader()->setLabel(i, QString(nlabel));
	     pp->table_w->setText(i , 0, QString(lp->text));

	     // SCALE
	     if (lp->step != 0 && (lp->max - lp->min)/lp->step <= MAXSCSTEPS)
	     {
	       if (p == PP_RW)
	       {
	         lp->spin_w = new KDoubleSpinBox(lp->min, lp->max, lp->step, lp->value, 2, curGroup->box);
 	         pp->table_w->setCellWidget(i, 1, lp->spin_w);
	       }
	       else if (p == PP_WO)
	       {
	         lp->spin_w = new KDoubleSpinBox(lp->min, lp->max, lp->step, lp->value, 2, curGroup->box);
 	         pp->table_w->setCellWidget(i, 0, lp->spin_w);
	       }

	     }

	     if (pp->table_w->verticalHeader()->sizeHint().width() > widestTableCol)
	     {
	        widestTableCol = pp->table_w->verticalHeader()->sizeHint().width();
		if (widestTableCol > 200)
		  widestTableCol = 200;
		//fprintf(stderr, "Found a new wide col %d for %s, readjusting\n", widestTableCol, nlabel);
		resizeTableHeaders();
 	     }
	     else
	       pp->table_w->setLeftMargin(widestTableCol);

	 }

	/* we know it will be a general text GUI */
	pp->guitype = PG_NUMERIC;
	pp->perm = p;

	pp->table_w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	pp->table_w->setMaximumHeight(pp->labels.size() * uniHeight + pp->table_w->horizontalHeader()->height() + 5);
	curGroup->layout->addMultiCellWidget(pp->table_w, curGroup->pl.size(), curGroup->pl.size(), 2, 5);
	//pp->hLayout->addWidget(pp->table_w);


	if (p != PP_RO)
	{
		pp->set_w   = new QPushButton(i18n("Set"), curGroup->box);
        	QObject::connect(pp->set_w, SIGNAL(clicked()), pp, SLOT(newText()));
		curGroup->layout->addWidget(pp->set_w, curGroup->pl.size(), 6);
	}


	curGroup->addProperty(pp);

	if (nlabel)
	delete [] nlabel;
	if (nname);
	delete [] nname;

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
		if (!strcmp (root->el[i]->tag, "defSwitch"))
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
				root->tag, ap->valu, name.ascii(), pp->name.ascii());
	    delete(pp);
	    return (-1);
	}

	/* ok, build array of toggle buttons.
	 * only difference is /visual/ appearance of 1ofMany or anyofMany */

	 //pp->hLayout = new QHBoxLayout(0, 0, 5);

	pp->groupB = new QButtonGroup(0);
	if (pp->guitype == PG_BUTTONS)
	  pp->groupB->setExclusive(true);


        QObject::connect(pp->groupB, SIGNAL(clicked(int)),
    		     pp, SLOT(newSwitch(int)));

	XMLEle *sep;
	char *sname=NULL;
	char *slabel=NULL;
	QPushButton *button;
	QCheckBox   *checkbox;
	INDI_L *lp;
	QFont buttonFont;

	for (i = 0, j = -1; i < root->nel; i++)
	{
	    sep = root->el[i];

	    /* look for switch tage */
	    if (strcmp (sep->tag, "defSwitch"))
                   continue;

            /* find name  */
	    ap = parent->findAtt (sep, "name", errmsg);
	    if (!ap)
	    {
		delete(pp);
		return (-1);
	    }

	    sname = new char[strlen(ap->valu) + 1];
	    strcpy(sname, ap->valu);

	    /* find label */
	    ap = parent->findAtt (sep, "label", errmsg);
	     if (!ap)
	     {
		delete(pp);
		return (-1);
	     }

	    slabel = new char[strlen(ap->valu)+1];
	    strcpy(slabel, ap->valu);

	    if (!strcmp (slabel, ""))
	    {
	     slabel = new char[strlen(sname) + 1];
	     strcpy(slabel, sname);
	    }

            lp = new INDI_L(pp, QString(sname), QString(slabel));

	    if (crackSwitchState (sep->pcdata, &(lp->state)) < 0)
	    {
		sprintf (errmsg, "INDI: <%s> unknown state %s for %s %s %s",
			    root->tag, ap->valu, name.ascii(), pp->name.ascii(), name.ascii());
		delete(pp);
		return (-1);
	    }

	    j++;

	    /* build toggle */
	    switch (pp->guitype)
	    {
	      case PG_BUTTONS:
	       button = new KPushButton(QString(slabel), curGroup->box);
	       pp->groupB->insert(button, j);

	       if (lp->state == PS_ON)
	       {
	           button->setDown(true);
		   buttonFont = button->font();
		   buttonFont.setBold(TRUE);
		   button->setFont(buttonFont);
	       }

	       lp->push_w = button;

	       //pp->u.multi.push_v->push_back(button);
	       curGroup->layout->addWidget(button, curGroup->pl.size(), j + 2);
	       button->show();

	       break;

	      case PG_RADIO:
	      checkbox = new QCheckBox(QString(slabel), curGroup->box);
	      pp->groupB->insert(checkbox, j);

	      if (lp->state == PS_ON)
	        checkbox->setChecked(true);

	      curGroup->layout->addWidget(checkbox, curGroup->pl.size(), j + 2);

	      lp->check_w = checkbox;
	      //pp->u.multi.check_v->push_back(checkbox);
	      checkbox->show();

	      break;

	      default:
	      break;

	    }

	    pp->labels.push_back(lp);

	}

        if (j < 0)
	 return (-1);

	pp->groupB->setFrameShape(QFrame::NoFrame);

        curGroup->addProperty(pp);

	if (sname)
	delete [] sname;
	if (slabel)
	delete [] slabel;

	return (0);
}


/* build a menu version of oneOfMany.
 * return 0 if ok, else -1 with reason in errmsg[]
 */

int INDI_D::buildMenuGUI (INDI_P *pp, XMLEle *root, char errmsg[])
{

	INDI_L *initlp = NULL;
	XMLEle *sep = NULL;
	XMLAtt *ap;
	INDI_L *lp;
	char *sname=NULL;
	char *slabel=NULL;
	int i=0, onItem=0;

	pp->guitype = PG_MENU;

	QStringList menuOptions;

	/*pp->table_w = new QTable(curGroup->box);
	pp->table_w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

	switch (pp->perm)
	{
	  case PP_RW:
	   pp->table_w->setNumCols(2);
	   pp->table_w->horizontalHeader()->setLabel( 0, i18n("Current"));
	   pp->table_w->horizontalHeader()->setLabel( 1, i18n("Set"));
	   pp->table_w->setColumnReadOnly(0, true);
	   break;

	  case PP_RO:
	   pp->table_w->setNumCols(1);
	   pp->table_w->horizontalHeader()->setLabel( 0, i18n("Current"));
	   pp->table_w->setColumnReadOnly(0, true);
	   break;

	  case PP_WO:
	   pp->table_w->setNumCols(1);
	   pp->table_w->horizontalHeader()->setLabel( 0, i18n("Set"));

	   break;
	 }*/

	// build pulldown menu first
	// create each switch.
	// N.B. can only be one in On state.

	for (i = 0; i < root->nel; i++)
	{
	    sep = root->el[i];

	    /* look for switch tage */
	    if (strcmp (sep->tag, "defSwitch"))
                   continue;

            /* find name  */
	    ap = parent->findAtt (sep, "name", errmsg);
	    if (!ap)
	    {
		delete(pp);
		return (-1);
	    }

	    sname = new char [ strlen(ap->valu) + 1];
	    strcpy(sname, ap->valu);

	    /* find label */
	    ap = parent->findAtt (sep, "label", errmsg);
	     if (!ap)
	     {
		delete(pp);
		return (-1);
	     }

	    slabel = new char [strlen(ap->valu) + 1];
	    strcpy(slabel, ap->valu);

	    if (!strcmp (slabel, ""))
	    {
	     slabel = new char[strlen(sname) + 1];
	     strcpy(slabel, sname);
	    }

            lp = new INDI_L(pp, QString(sname), QString(slabel));

	    if (crackSwitchState (sep->pcdata, &(lp->state)) < 0)
	    {
		sprintf (errmsg, "INDI: <%s> unknown state %s for %s %s %s",
			    root->tag, ap->valu, name.ascii(), pp->name.ascii(), name.ascii());
		delete(pp);
		return (-1);
	    }


             menuOptions.append(QString(slabel));
	    //pp->u.multi.om_w->insertItem(QString(name));

	    if (lp->state == PS_ON)
	    {
		if (initlp)
		{
		    sprintf (errmsg,"INDI: <%s> %s %s has multiple On switches",
					root->tag, name.ascii(), pp->name.ascii());
		    return (-1);
		}
		initlp = lp;
		onItem = i;
		//pp->u.multi.om_w->setCurrentItem(j);
		//pp->u.multi.lastItem = j;
	    }

            pp->labels.push_back(lp);

	}


	//pp->table_w->insertRows(0);
	//pp->table_w->setRowHeight(i, uniHeight);
	//pp->table_w->setColumnStrechable(0, true);
	pp->om_w = new KComboBox(curGroup->box);
	pp->om_w->insertStringList(menuOptions);
	pp->om_w->setCurrentItem(onItem);


	/*if (pp->perm == PP_RW)
	 pp->table_w->setCellWidget(0, 1, pp->om_w);
	else
	 pp->table_w->setCellWidget(0, 0, pp->om_w);*/

	curGroup->layout->addMultiCellWidget(pp->om_w, curGroup->pl.size(), curGroup->pl.size(), 2, 3);

	QObject::connect( pp->om_w, SIGNAL(activated(int)), pp, SLOT(newSwitch(int)));

        //pp->u.multi.om_w->show();

	curGroup->addProperty(pp);

	if (sname)
	delete [] sname;
	if (slabel)
	delete [] slabel;

	return (0);
}

/* build GUI for a lights GUI.
 * return 0 if ok, else -1 with reason in errmsg[]
 */
int INDI_D::buildLightsGUI (XMLEle *root, char errmsg[])
{

	INDI_P *pp;
	int i;

	// build a new property
	pp = addProperty (root, errmsg);
	if (!pp)
	    return (-1);

	// N.B. if trouble from here on must call delP()

	pp->guitype = PG_LIGHTS;

	pp->hLayout = new QHBoxLayout(0, 0, 5);
	curGroup->layout->addMultiCellLayout(pp->hLayout, curGroup->pl.size(), curGroup->pl.size(), 3, 5);

	XMLEle *lep;
	XMLAtt *ap;
	INDI_L *lp;
	QLabel *label;
	KLed   *led;
	char *sname=NULL;
	char *slabel=NULL;

	for (i = 0; i < root->nel; i++)
	{
	    lep = root->el[i];

	    if (strcmp (lep->tag, "defLight"))
		continue;

	/* find name  */
	    ap = parent->findAtt (lep, "name", errmsg);
	    if (!ap)
	    {
		delete(pp);
		return (-1);
	    }

	    sname = new char[strlen(ap->valu) +1];
	    strcpy(sname, ap->valu);

	    /* find label */
	    ap = parent->findAtt (lep, "label", errmsg);
	     if (!ap)
	     {
		delete(pp);
		return (-1);
	     }

	    slabel = new char[strlen(ap->valu) +1];
	    strcpy(slabel, ap->valu);

	    if (!strcmp (slabel, ""))
	    {
             slabel = new char[strlen(sname) + 1];
	     strcpy(slabel, sname);
	    }

	   lp = new INDI_L(pp, QString(sname), QString(slabel));

	    if (crackLightState (lep->pcdata, &lp->state) < 0)
	     {
		sprintf (errmsg, "INDI: <%s> unknown state %s for %s %s %s",
			    root->tag, ap->valu, name.ascii(), pp->name.ascii(), sname);
		delete(pp);
		return (-1);
	    }


	    label = new QLabel(QString(slabel), curGroup->box);
	    led   = new KLed(curGroup->box);

	    pp->drawLt(led, lp->state);

	    pp->hLayout->addWidget(led);
            pp->hLayout->addWidget(label);

	    lp->label_w  = label;
	    lp->led_w    = led;
            //pp->u.multi.label_v->push_back(label);
	    //pp->u.multi.led_v->push_back(led);

	    pp->labels.push_back(lp);
	}

	curGroup->addProperty(pp);

	if (sname)
	delete [] sname;
	if (slabel)
	delete [] slabel;

	return (0);
}



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

 serverFD  = -1;
 serverFP  = NULL;
 XMLParser = NULL;
 sNotifier = NULL;

}

DeviceManager::~DeviceManager()
{
  if (serverFD >= 0)
  {
    serverFD = -1;
    if (serverFP)
    	fclose(serverFP);
    serverFP = NULL;
  }

  if (XMLParser)
  {
   delLilXML(XMLParser);
   XMLParser = NULL;
  }

}

bool DeviceManager::indiConnect(QString host, QString port)
{

	struct sockaddr_in pin;
	struct hostent *serverHostName = gethostbyname(host.ascii());
	QString errMsg;
	errMsg = QString("Connection to INDI host at %1 on port %2 failed.").arg(host).arg(port);


	bzero(&pin, sizeof(pin));
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
		if (dispatchCommand(root, msg) < 0)
		{
		    //kdDebug() << msg << endl;
		    fprintf(stderr, msg);
		    prXMLEle (stdout, root, 0);
		}

                else if (parent->deviceContainer->currentPage() && parent->deviceContainer->isShown())
		 {
		 	for (uint j=0; j < indi_dev.size(); j++)
		   	   for (uint k=0; k < indi_dev[j]->gl.size(); k++)
		   	{
		     	indi_dev[j]->gl[k]->box->layout()->activate();
		     	indi_dev[j]->gl[k]->box->show();
		   	}

			parent->deviceContainer->showPage ( parent->deviceContainer->currentPage() );
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

  if  (!strcmp (root->tag, "message"))
  	 return messageCmd(root, errmsg);
  else if  (!strcmp (root->tag, "delProperty"))
        return delPropertyCmd(root, errmsg);

  /* Get the device, if not available, create it */
  INDI_D *dp = findDev (root, 1, errmsg);
      if (dp == NULL)
	    return -1;

   if (!strcmp (root->tag, "defTextVector"))
      return dp->buildTextGUI(root, errmsg);
   else if (!strcmp (root->tag, "defNumberVector"))
      return dp->buildNumberGUI(root, errmsg);
   else if (!strcmp (root->tag, "defSwitchVector"))
       return dp->buildSwitchesGUI(root, errmsg);
   else if (!strcmp (root->tag, "defLightVector"))
        return dp->buildLightsGUI(root, errmsg);
   else if (!strcmp (root->tag, "setTextVector") ||
   	    !strcmp (root->tag, "setNumberVector") ||
	    !strcmp (root->tag, "setSwitchVector") ||
	    !strcmp (root->tag, "setLightVector"))
	return dp->setAnyCmd(root, errmsg);
  else if  (!strcmp (root->tag, "newTextVector") ||
  	    !strcmp (root->tag, "newNumberVector") ||
	    !strcmp (root->tag, "newSwitchVector"))
	 return dp->newAnyCmd(root, errmsg);


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
	ap = findXMLAtt (root, "name");

	/* delete just pname, or all if no property specified.
	 * N.B. remember delP decrements dp->npl
	 */
	if (ap)
	{
	  pp = dp->findProp(QString(ap->valu));

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
    	for (unsigned int i=0; i < indi_dev.size(); i++)
          	delete(indi_dev[i]);

        indi_dev.clear();

	return (0);
    }

    for (unsigned int i=0; i < indi_dev.size(); i++)
    {
         if (indi_dev[i]->name ==  devName)
	 {
	   kdDebug() << "Device Manager: Device found, deleting " << devName << endl;

	   parent->deviceContainer->removePage(indi_dev[i]->tabContainer);

	    delete(indi_dev[i]);
	    indi_dev.erase(indi_dev.begin() + i);
            return (0);
	 }
   }

   sprintf(errmsg, "Device %s not found" , devName.ascii());
   return -1;
}

INDI_D * DeviceManager::findDev (QString devName, char errmsg[])
{
	/* search for existing */
	for (unsigned int i = 0; i < indi_dev.size(); i++)
	{
	    if (indi_dev[i]->name == devName)
		return (indi_dev[i]);
	}

	sprintf (errmsg, "INDI: no such device %s", devName.ascii());
	kdDebug() << errmsg;

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

/* add new device to mainrc_w using info in dep.
- * if trouble return NULL with reason in errmsg[]
- */
INDI_D * DeviceManager::addDevice (XMLEle *dep, char errmsg[])
{
	INDI_D *dp;
	XMLAtt *ap;

	/* allocate new INDI_D on indi_dev */
	ap = parent->findAtt (dep, "device", errmsg);
	if (!ap)
	    return NULL;

	if (parent->currentLabel.isEmpty())
	 parent->setCustomLabel(ap->valu);

	//fprintf(stderr, "\n\n\n ***************** Adding a device %s with label %s *************** \n\n\n", ap->valu, label);
	dp = new INDI_D(parent, this, QString(ap->valu), parent->currentLabel);

	parent->currentLabel = "";

	indi_dev.push_back(dp);

	/* ok */
	return dp;
}

INDI_D * DeviceManager::findDev (XMLEle *root, int create, char errmsg[])
{
	XMLAtt *ap;
	char *dn;

	/* get device name */
	ap = parent->findAtt (root, "device", errmsg);
	if (!ap)
	    return (NULL);
	dn = ap->valu;

	/* search for existing */
	for (uint i = 0; i < indi_dev.size(); i++)
	{
	    if (indi_dev[i]->name == QString(dn))
		return (indi_dev[i]);
	}

	/* not found, create if ok */
	if (create)
		return (addDevice (root, errmsg));


	sprintf (errmsg, "INDI: <%s> no such device %s", root->tag, dn);
	return NULL;
}

/* display each <msg> within root in dp's scrolled area, if any.
 * prefix our time stamp if not included.
 * N.B. don't put carriage control in msg, we take care of that.
 */
void DeviceManager::checkMsg (XMLEle *root, INDI_D *dp)
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
void DeviceManager::doMsg (XMLEle *msg, INDI_D *dp)
{
	QTextEdit *txt_w;
	XMLAtt *timeatt;

	/* determine whether device or generic message */
	if (dp)
	    txt_w = dp->msgST_w;
	else
	    txt_w = parent->msgST_w;

	/* prefix our timestamp if not with msg */
	timeatt = findXMLAtt (msg, "time");
	if (timeatt)
            txt_w->insert(QString(timeatt->valu) + QString(" "));
	else
	   txt_w->insert( (QDateTime().currentDateTime()).toString("yyyy/mm/dd - h:m:s ap "));


	/* finally! the msg */
	    txt_w->insert( QString(msg->pcdata) + QString("\n"));

	    if (parent->ksw->options()->indiMessages)
	    	parent->ksw->statusBar()->changeItem( QString(msg->pcdata), 0);

}

void DeviceManager::sendNewText (INDI_P *pp)
{

	fprintf(serverFP, "<newTextVector\n");
	fprintf(serverFP, "  device='%s'\n", pp->pg->dp->name.ascii());
	fprintf(serverFP, "  name='%s'\n>", pp->name.ascii());
	for (unsigned int i = 0; i < pp->labels.size(); i++)
	{
	    INDI_L *lp = pp->labels[i];
	    fprintf(serverFP, "  <newText\n");
	    fprintf(serverFP, "    name='%s'>\n", lp->name.ascii());
	    fprintf(serverFP, "      %s\n", lp->text.ascii());
	    fprintf(serverFP, "  </newText>\n");
	}
	fprintf(serverFP, "</newTextVector>\n");
}

void DeviceManager::sendNewNumber (INDI_P *pp)
{
        fprintf(serverFP, "<newNumberVector\n");
	fprintf(serverFP, "  device='%s'\n", pp->pg->dp->name.ascii());
	fprintf(serverFP, "  name='%s'\n>", pp->name.ascii());
	for (unsigned int i = 0; i < pp->labels.size(); i++)
	{
	    INDI_L *lp = pp->labels[i];
	    fprintf(serverFP, "  <newNumber\n");
	    fprintf(serverFP, "    name='%s'>\n", lp->name.ascii());
	    fprintf(serverFP, "      %g\n", lp->value);
	    fprintf(serverFP, "  </newNumber>\n");
	}
	fprintf(serverFP, "</newNumberVector>\n");

}

void DeviceManager::sendNewSwitch (INDI_P *pp)//, int set)
{
	//INDI_P *pp = lp->pp;

	//fprintf (serverFP, "<newSwitch device='%s' name='%s'><switch state='%s'>%s</switch></newSwitch>\n",
	//		pp->pg->dp->name, pp->name, set ? "On" : "Off", lp->name);

	fprintf (serverFP,"<newSwitchVector\n");
	fprintf (serverFP,"  device='%s'\n", pp->pg->dp->name.ascii());
	fprintf (serverFP,"  name='%s'>\n", pp->name.ascii());
        for (unsigned int i = 0; i < pp->labels.size(); i++)
	{
	    INDI_L *lp = pp->labels[i];
	    fprintf (serverFP,"  <newSwitch\n");
	    fprintf (serverFP,"    name='%s'>\n", lp->name.ascii());
	    fprintf (serverFP,"      %s\n", lp->state == PS_ON ? "On" : "Off");
	    fprintf (serverFP,"  </newSwitch>\n");
	}
	fprintf (serverFP, "</newSwitchVector>\n");

}



#include "indidevice.moc"

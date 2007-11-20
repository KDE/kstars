/*  INDI Property
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
   
 */
 
 #include "indiproperty.h"
 #include "indigroup.h"
 #include "indidevice.h"
 #include "devicemanager.h"
 #include "indimenu.h"
 #include "indistd.h"
 #include "indi/indicom.h"
 #include "Options.h"
 #include "kstars.h"
 #include "timedialog.h"
 #include "skymap.h"

 #include "indi/base64.h"
 #include "indi/indicom.h"
 
 #include <kpopupmenu.h>
 #include <klineedit.h>
 #include <kled.h>
 #include <klocale.h>
 #include <kcombobox.h>
 #include <kpushbutton.h>
 #include <knuminput.h> 
 #include <kdebug.h>
 #include <kmessagebox.h>
 
 #include <qbuttongroup.h> 
 #include <qcheckbox.h>
 #include <qlabel.h>
 #include <qlayout.h>
 #include <qtimer.h>
 #include <qfile.h>
 #include <qdatastream.h>
 #include <qtooltip.h>

 #include <unistd.h>
 #include <stdlib.h>
 
/*******************************************************************
** INDI Property: contains widgets, labels, and their status
*******************************************************************/
INDI_P::INDI_P(INDI_G *parentGroup, QString inName)
{
  name = inName;

  pg = parentGroup;
  
  el.setAutoDelete(true);
  
  stdID 	  = -1;
  
  indistd 	  = new INDIStdProperty(this, pg->dp->parent->ksw, pg->dp->stdDev);
  
  PHBox           = new QHBoxLayout(0, 0, KDialogBase::spacingHint());
  PVBox           = new QVBoxLayout(0, 0, KDialogBase::spacingHint());
  light           = NULL;
  label_w         = NULL;
  set_w           = NULL;
  assosiatedPopup = NULL;
  groupB          = NULL;
}

/* INDI property desstructor, makes sure everything is "gone" right */
INDI_P::~INDI_P()
{
      pg->propertyLayout->removeItem(PHBox);
      el.clear();
      delete (light);
      delete (label_w);
      delete (set_w);
      delete (PHBox);
      delete (indistd);
      delete (groupB);
}

bool INDI_P::isOn(QString component)
{

  INDI_E *lp;

  lp = findElement(component);

  if (!lp)
   return false;

  if (lp->check_w && lp->check_w->isChecked())
     return true;
  if (lp->push_w && lp->push_w->isDown())
      return true;

  return false;
}

INDI_E * INDI_P::findElement(QString elementName)
{
  INDI_E *element = NULL;
  
  for (element = el.first(); element != NULL; element = el.next())
     if (element->name == elementName || element->label == elementName)
     	break;

  return element;
}

void INDI_P::drawLt(PState lstate)
{

  
        /* set state light */
	switch (lstate)
	{
	  case PS_IDLE:
	  light->setColor(Qt::gray);
	  break;

	  case PS_OK:
	  light->setColor(Qt::green);
	  emit okState();
	  disconnect( this, SIGNAL(okState()), 0, 0 );
	  break;

	  case PS_BUSY:
	  light->setColor(Qt::yellow);
	  break;

	  case PS_ALERT:
	  light->setColor(Qt::red);
	  break;

	  default:
	  break;

	}
	
}

void INDI_P::newText()
{
  INDI_E * lp;
  
  for (lp = el.first(); lp != NULL; lp = el.next())
  {
      /* If PG_SCALE */
      if (lp->spin_w)
       lp->targetValue = lp->spin_w->value();
      /* PG_NUMERIC or PG_TEXT */
      else
      {
        switch (perm)
	{
	  case PP_RW:
	  if (lp->write_w->text().isEmpty())
	   lp->text = lp->read_w->text();
	  else
	   lp->text = lp->write_w->text();
	  break;
	  
	  case PP_RO:
	    break;
	  
	  case PP_WO:
	   lp->text = lp->write_w->text();
	  break;
	}
	
	if (guitype == PG_NUMERIC)
	{
           f_scansexa(lp->text.ascii(), &(lp->targetValue));
	   if ((lp->targetValue > lp->max || lp->targetValue < lp->min))
	   {
	     KMessageBox::error(0, i18n("Invalid range for element %1. Valid range is from %2 to %3").arg(lp->label).arg(lp->min).arg(lp->max));
	     return;
	   }
        }
     }
  }

  state = PS_BUSY;

  drawLt(state);
  
  /* perform any std functions */
  indistd->newText();

  if (guitype == PG_TEXT)
  	pg->dp->parentMgr->sendNewText(this);
  else if (guitype == PG_NUMERIC)
  	pg->dp->parentMgr->sendNewNumber(this);

}

void INDI_P::convertSwitch(int id)
{
 
 INDI_E *lp;
 QString mLabel;
 int switchIndex=0;
 
 if (assosiatedPopup == NULL)
  return;

  mLabel = assosiatedPopup->text(id).replace("&", "");

  //kdDebug() << "Name: " << name << " ID: " << id << endl;
 /* Special case is CCD_EXPOSE_DURATION, not a switch */
 if (stdID == CCD_EXPOSE_DURATION && mLabel.find(label) != -1)
 {
   newText();
   return;
 }

 /* Another special case, center telescope */
 if (mLabel.find("Crosshair") != -1)
 {
        if (!indistd->stdDev->dp->isOn()) return;
	if (indistd->stdDev->telescopeSkyObject == NULL) return;
	indistd->ksw->map()->setClickedObject(indistd->stdDev->telescopeSkyObject);
	indistd->ksw->map()->setClickedPoint(indistd->stdDev->telescopeSkyObject);
	indistd->ksw->map()->slotCenter();
	return;
 }
   
 lp = findElement(mLabel);
 
 if (!lp)
   return;
   
 for (uint i=0; i < el.count(); i++)
 {
   if (el.at(i)->label == mLabel)
   {
     switchIndex = i;
     break;
   }
 }

 if (indistd->convertSwitch(switchIndex, lp))
   return;
 else if (lp->state == PS_OFF)
         newSwitch(switchIndex);	 
}


void INDI_P::newSwitch(int id)
{

  QFont buttonFont;
  INDI_E *lp;

  lp = el.at(id);

  switch (guitype)
  {
    case PG_MENU:
   	if (lp->state == PS_ON)
    		return;

   	for (unsigned int i=0; i < el.count(); i++)
	  el.at(i)->state = PS_OFF;
	  
	lp->state = PS_ON;
        break;
 
   case PG_BUTTONS:
      for (unsigned int i=0; i < el.count(); i++)
      {
          if (i == (unsigned int) id) continue;
	  
     	  el.at(i)->push_w->setDown(false);
       	  buttonFont = el.at(i)->push_w->font();
	  buttonFont.setBold(FALSE);
	  el.at(i)->push_w->setFont(buttonFont);
          el.at(i)->state = PS_OFF;
       }

        lp->push_w->setDown(true);
	buttonFont = lp->push_w->font();
	buttonFont.setBold(TRUE);
	lp->push_w->setFont(buttonFont);
	lp->state = PS_ON;

       break;
       
   case PG_RADIO:
   	lp->state = lp->state == PS_ON ? PS_OFF : PS_ON;
   	lp->check_w->setChecked(lp->state == PS_ON);
   break;
   
   default:
    break;

  }

  state = PS_BUSY;

  drawLt(state);
  
  if (indistd->newSwitch(id, lp))
   return;

  pg->dp->parentMgr->sendNewSwitch (this, id);


}

/* Display file dialog to select and upload a file to the client
   Loop through blobs and send each accordingly
 */
void INDI_P::newBlob()
{
 QFile fp;
 QString filename;
 QString format;
 QDataStream binaryStream;
 int data64_size=0, pos=0;
 char *data_file;
 unsigned char *data, *data64;
 bool sending (false);
 bool valid (true);
 
  for (unsigned int i=0; i < el.count(); i++)
  {
     filename = el.at(i)->write_w->text();
     if (filename.isEmpty())
     {
        valid = false;
        continue; 
     }
     
      fp.setName(filename);

      if ( (pos = filename.findRev(".")) != -1)
		format = filename.mid (pos, filename.length());

      if (!fp.open(IO_ReadOnly))
      {
          KMessageBox::error(0, i18n("Cannot open file %1 for reading").arg(filename));
          valid = false;
          continue;
     }
     
     binaryStream.setDevice(&fp);

     data_file = new char[fp.size()];
     if (data_file == NULL)
     {
 	KMessageBox::error(0, i18n("Not enough memory to load %1").arg(filename));
	fp.close();
        valid = false;
        continue;
     }

     binaryStream.readRawBytes(data_file, fp.size());
     data = (unsigned char *) data_file;

     data64 = new unsigned char[4*fp.size()/3+4];
     if (data64 == NULL)
     {
	KMessageBox::error(0, i18n("Not enough memory to convert file %1 to base64").arg(filename));
        fp.close();
        valid = false;
        continue;
     }

     data64_size = to64frombits (data64, data, fp.size());

     delete (data);

    if (sending == false)
    {
        sending = true;
	pg->dp->parentMgr->startBlob (pg->dp->name, name, QString(timestamp()));
    }

    pg->dp->parentMgr->sendOneBlob(el.at(i)->name, data64_size, format, data64);

    fp.close();
    delete (data64);

  }

  /* Nothing been made, no changes */
  if (!sending && !valid)
    return;
  else if (sending)
    pg->dp->parentMgr->finishBlob();

  if (valid) 
	state = PS_BUSY;
  else
	state = PS_ALERT;

  drawLt(state);

}

/* build widgets for property pp using info in root.
 */
void INDI_P::addGUI (XMLEle *root)
{
	XMLAtt *prompt;
	char errmsg[512];

	/* add to GUI group */
	light = new KLed (pg->propertyContainer);
	light->setMaximumSize(16,16);
	light->setLook(KLed::Sunken);
	//light->setShape(KLed::Rectangular);
	drawLt(state);
	
	/* #1 First widegt is the LED status indicator */
	PHBox->addWidget(light);
        
	/* #2 add label for prompt */
	prompt = findAtt(root, "label", errmsg);
	
	if (!prompt)
	    label = name;
	else
	    label = valuXMLAtt(prompt);
	
	// use property name if label is empty
	if (label.isEmpty())
	{
	   label = name;
	   label_w = new QLabel(label, pg->propertyContainer);
	}
	else
	   label_w = new QLabel(label, pg->propertyContainer);
	 
	   label_w->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)5, 0, 0, label_w->sizePolicy().hasHeightForWidth() ) );
	   label_w->setFrameShape( QLabel::GroupBoxPanel );
	   label_w->setMinimumWidth(PROPERTY_LABEL_WIDTH);
	   label_w->setMaximumWidth(PROPERTY_LABEL_WIDTH);
	   label_w->setTextFormat( QLabel::RichText );
	   label_w->setAlignment( int( QLabel::WordBreak | QLabel::AlignVCenter | QLabel::AlignHCenter) );
	   
	   PHBox->addWidget(label_w);
	 
         light->show();
	 label_w->show();
	 
	 /* #3 Add the Vertical layout thay may contain several elements */
	 PHBox->addLayout(PVBox);
}

int INDI_P::buildTextGUI(XMLEle *root, char errmsg[])
{
        INDI_E *lp;
	XMLEle *text;
	XMLAtt *ap;
        QString textName, textLabel;
	errmsg=errmsg;
	
        for (text = nextXMLEle (root, 1); text != NULL; text = nextXMLEle (root, 0))
	{
	    if (strcmp (tagXMLEle(text), "defText"))
		continue;

	    ap = findXMLAtt(text, "name");
	    if (!ap)
	    {
	        kdDebug() << "Error: unable to find attribute 'name' for property " << name << endl;
	        return (-1);
	    }

	    textName = valuXMLAtt(ap);
            textName.truncate(MAXINDINAME);

	    ap = findXMLAtt(text, "label");

	    if (!ap)
	    {
	      kdDebug() << "Error: unable to find attribute 'label' for property " << name << endl;
	      return (-1);
	    }

	    textLabel = valuXMLAtt(ap);
            

	    if (textLabel.isEmpty())
	      textLabel = textName;

	      textLabel.truncate(MAXINDINAME);
	     
             lp = new INDI_E(this, textName, textLabel);

	     lp->buildTextGUI(QString(pcdataXMLEle(text)));
	     
	     el.append(lp);
	     
	 }
	
	if (perm == PP_RO)
	  return 0;
	
	 // INDI STD, but we use our own controls
	 if (name == "TIME")
	 {
	        setupSetButton("Time");
	     	QObject::connect(set_w, SIGNAL(clicked()), indistd, SLOT(newTime()));
	 }
	 else
	 {
		setupSetButton("Set");
        	QObject::connect(set_w, SIGNAL(clicked()), this, SLOT(newText()));
         }
	 
	 return 0;
	 
}

int INDI_P::buildNumberGUI  (XMLEle *root, char errmsg[])
{
  	char format[32];
	double min=0, max=0, step=0;
	XMLEle *number;
	XMLAtt *ap;
	INDI_E *lp;
	QString numberName, numberLabel;
	errmsg=errmsg;
	
	for (number = nextXMLEle (root, 1); number != NULL; number = nextXMLEle (root, 0))
	{
	    if (strcmp (tagXMLEle(number), "defNumber"))
		continue;

	    ap = findXMLAtt(number, "name");
	    if (!ap)
	    {
	        kdDebug() << "Error: unable to find attribute 'name' for property " << name << endl;
	        return (-1);
	    }

	    numberName = valuXMLAtt(ap);
            numberName.truncate(MAXINDINAME);

	    ap = findXMLAtt(number, "label");

	    if (!ap)
	    {
	      kdDebug() << "Error: unable to find attribute 'label' for property " << name << endl;
	      return (-1);
	    }

	    numberLabel = valuXMLAtt(ap);
	    
	    if (numberLabel.isEmpty())
	      numberLabel = numberName;

           numberLabel.truncate(MAXINDINAME);
	    
	    lp = new INDI_E(this, numberName, numberLabel);

	    ap = findXMLAtt (number, "min");
	    if (ap)
		min = atof(valuXMLAtt(ap));
	    ap = findXMLAtt (number, "max");
	    if (ap)
		max = atof(valuXMLAtt(ap));
	    ap = findXMLAtt (number, "step");
	    if (ap)
		step = atof(valuXMLAtt(ap));
	    ap = findXMLAtt (number, "format");
	    if (ap)
		strcpy(format,valuXMLAtt(ap));

	    lp->initNumberValues(min, max, step, format);

	    lp->buildNumberGUI(atof(pcdataXMLEle(number)));

	    el.append(lp);

	 }
	 
	if (perm == PP_RO)
	  return 0;
	  
        if (name == "CCD_EXPOSE_DURATION")
	          setupSetButton("Start");
        else
		  setupSetButton("Set");
		  
	QObject::connect(set_w, SIGNAL(clicked()), this, SLOT(newText()));
		   
	return (0);
}


void INDI_P::setupSetButton(QString caption)
{
	set_w = new QPushButton(caption, pg->propertyContainer);
	set_w->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)5, (QSizePolicy::SizeType)0, 0, 0, set_w->sizePolicy().hasHeightForWidth() ) );
	set_w->setMinimumWidth( MIN_SET_WIDTH );
	set_w->setMaximumWidth( MAX_SET_WIDTH );

	PHBox->addWidget(set_w);
}

int INDI_P::buildMenuGUI(XMLEle *root, char errmsg[])
{
	XMLEle *sep = NULL;
	XMLAtt *ap;
	INDI_E *lp;
	QString switchName, switchLabel;
	QStringList menuOptions;
	int i=0, onItem=-1;

	guitype = PG_MENU;

        // build pulldown menu first
	// create each switch.
	// N.B. can only be one in On state.
	for (sep = nextXMLEle (root, 1), i=0; sep != NULL; sep = nextXMLEle (root, 0), i++)
	{
	    /* look for switch tage */
	    if (strcmp (tagXMLEle(sep), "defSwitch"))
                   continue;

            /* find name  */
	    ap = findAtt (sep, "name", errmsg);
	    if (!ap)
		return (-1);

	    switchName = valuXMLAtt(ap);
            switchName.truncate(MAXINDINAME);
	    
	    /* find label */
	    ap = findAtt (sep, "label", errmsg);
	     if (!ap)
		return (-1);

	    switchLabel = valuXMLAtt(ap);

	    if (switchLabel.isEmpty())
	      switchLabel = switchName;

            switchLabel.truncate(MAXINDINAME);

            lp = new INDI_E(this, switchName, switchLabel);

	    if (pg->dp->crackSwitchState (pcdataXMLEle(sep), &(lp->state)) < 0)
	    {
		snprintf (errmsg, ERRMSG_SIZE, "INDI: <%.64s> unknown state %.64s for %.64s %.64s %.64s",
			    tagXMLEle(root), valuXMLAtt(ap), name.ascii(), lp->name.ascii(), name.ascii());
		return (-1);
	    }

             menuOptions.append(switchLabel);
	    
	    if (lp->state == PS_ON)
	    {
		if (onItem != -1)
		{
		    snprintf (errmsg, ERRMSG_SIZE, "INDI: <%.64s> %.64s %.64s has multiple On switches",
					tagXMLEle(root), name.ascii(), lp->name.ascii());
		    return (-1);
		}
		
		onItem = i;
	    }
            
	    el.append(lp);
	}

	om_w = new KComboBox(pg->propertyContainer);
	om_w->insertStringList(menuOptions);
	om_w->setCurrentItem(onItem);
	
	HorSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
	
	PHBox->addWidget(om_w);
	PHBox->addItem(HorSpacer);

	QObject::connect(om_w, SIGNAL(activated(int)), this, SLOT(newSwitch(int)));

       	return (0);
}

int INDI_P::buildSwitchesGUI(XMLEle *root, char errmsg[])
{
        XMLEle *sep;
	XMLAtt *ap;
	INDI_E *lp;
	KPushButton *button(NULL);
	QCheckBox   *checkbox;
	QFont buttonFont;
	QString switchName, switchLabel;
	int j;

	groupB = new QButtonGroup(0);
        groupB->setFrameShape(QFrame::NoFrame);
	if (guitype == PG_BUTTONS)
	  groupB->setExclusive(true);
	  
	QObject::connect(groupB, SIGNAL(clicked(int)), this, SLOT(newSwitch(int)));

       for (sep = nextXMLEle (root, 1), j=-1; sep != NULL; sep = nextXMLEle (root, 0))
       {
	    /* look for switch tage */
	    if (strcmp (tagXMLEle(sep), "defSwitch"))
                   continue;
            
	    /* find name  */
	    ap = findAtt (sep, "name", errmsg);
	    if (!ap)
		return (-1);

	    switchName = valuXMLAtt(ap);
            switchName.truncate(MAXINDINAME);

	    /* find label */
	    ap = findAtt (sep, "label", errmsg);
	     if (!ap)
	    	return (-1);

	    switchLabel = valuXMLAtt(ap);
	    
	    if (switchLabel.isEmpty())
	      switchLabel = switchName;

            switchLabel.truncate(MAXINDINAME);
	    
            lp = new INDI_E(this, switchName, switchLabel);

	    if (pg->dp->crackSwitchState (pcdataXMLEle(sep), &(lp->state)) < 0)
	    {
		snprintf (errmsg, ERRMSG_SIZE, "INDI: <%.64s> unknown state %.64s for %.64s %.64s %.64s",
			    tagXMLEle(root), valuXMLAtt(ap), name.ascii(), name.ascii(), lp->name.ascii());
		return (-1);
	    }

	    j++;

	    /* build toggle */
	    switch (guitype)
	    {
	      case PG_BUTTONS:
	       button = new KPushButton(switchLabel, pg->propertyContainer);

	       groupB->insert(button, j);

	       if (lp->state == PS_ON)
	       {
	           button->setDown(true);
		   buttonFont = button->font();
		   buttonFont.setBold(TRUE);
		   button->setFont(buttonFont);
	       }

	       lp->push_w = button;

	       PHBox->addWidget(button);
	       	       
	       button->show();

	      break;

	      case PG_RADIO:
	      checkbox = new QCheckBox(switchLabel, pg->propertyContainer);
	      groupB->insert(checkbox, j);

	      if (lp->state == PS_ON)
	        checkbox->setChecked(true);

	      lp->check_w = checkbox;
	      
	      PHBox->addWidget(checkbox);
	      
              checkbox->show();

	      break;

	      default:
	      break;

	    }

	    el.append(lp);
	}

        if (j < 0)
	 return (-1);
	 
        HorSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
	
	PHBox->addItem(HorSpacer);

        return (0);
}

int INDI_P::buildLightsGUI(XMLEle *root, char errmsg[])
{
        XMLEle *lep;
	XMLAtt *ap;
	INDI_E *lp;
	QString sname, slabel;
	
   	for (lep = nextXMLEle (root, 1); lep != NULL; lep = nextXMLEle (root, 0))
    	{
	    if (strcmp (tagXMLEle(lep), "defLight"))
		continue;

	    /* find name  */
	    ap = findAtt (lep, "name", errmsg);
	    if (!ap) return (-1);
	
	    sname = valuXMLAtt(ap);
            sname.truncate(MAXINDINAME);

	    /* find label */
	    ap = findAtt (lep, "label", errmsg);
	     if (!ap) return (-1);

	    slabel = valuXMLAtt(ap);
	    if (slabel.isEmpty())
	      slabel = sname;

            slabel.truncate(MAXINDINAME);
	   
	   lp = new INDI_E(this, sname, slabel);

	   if (pg->dp->crackLightState (pcdataXMLEle(lep), &lp->state) < 0)
	    {
		snprintf (errmsg, ERRMSG_SIZE, "INDI: <%.64s> unknown state %.64s for %.64s %.64s %.64s",
			    tagXMLEle(root), valuXMLAtt(ap), pg->dp->name.ascii(), name.ascii(), sname.ascii());
		return (-1);
	   }
	   
	   lp->buildLightGUI();
           el.append(lp);
	}
	
	HorSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
	
	PHBox->addItem(HorSpacer);

	return (0);
}

/* Build BLOB GUI
 * Return 0 if okay, -1 if error */
int INDI_P::buildBLOBGUI(XMLEle *root, char errmsg[])
{
  INDI_E *lp;
  XMLEle *blob;
  XMLAtt *ap;
  QString blobName, blobLabel ;
  errmsg=errmsg;
	
  for (blob = nextXMLEle (root, 1); blob != NULL; blob = nextXMLEle (root, 0))
  {
    if (strcmp (tagXMLEle(blob), "defBLOB"))
      continue;

    ap = findXMLAtt(blob, "name");
    if (!ap)
    {
      kdDebug() << "Error: unable to find attribute 'name' for property " << name << endl;
      return (-1);
    }

    blobName = valuXMLAtt(ap);
    blobName.truncate(MAXINDINAME);

    ap = findXMLAtt(blob, "label");

    if (!ap)
    {
      kdDebug() << "Error: unable to find attribute 'label' for property " << name << endl;
      return (-1);
    }

    blobLabel = valuXMLAtt(ap);

    if (blobLabel.isEmpty())
      blobLabel = blobName;

    blobLabel.truncate(MAXINDINAME);

    lp = new INDI_E(this, blobName, blobLabel);

    lp->buildBLOBGUI();
	     
    el.append(lp);
	     
  }
	
  if (perm == PP_RO)
    return 0;
	
   setupSetButton(i18n("Upload"));
   QObject::connect(set_w, SIGNAL(clicked()), this, SLOT(newBlob()));

  return 0;
	 
}

void INDI_P::activateSwitch(QString name)
{
  int iCounter = 0;
  INDI_E *element;
  
  for (element = el.first(); element != NULL; element = el.next())
  {
     if (element->name == name && element->push_w)
       newSwitch(iCounter);
     
     iCounter++;
  }
  
}



#include "indiproperty.moc"

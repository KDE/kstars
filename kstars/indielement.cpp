/*  INDI Element
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-01-15	INDI element is the most basic unit of the INDI KStars client.
 */
 
#include "indielement.h"
#include "indiproperty.h"
#include "indigroup.h"
#include "indidevice.h"

#include "indi/indicom.h"

#include <qcheckbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qstring.h>
#include <qptrlist.h>

#include <kled.h>
#include <ksqueezedtextlabel.h> 
#include <klineedit.h>
#include <kpushbutton.h>
#include <klocale.h>
#include <kdebug.h>
#include <kcombobox.h>
#include <knuminput.h>
#include <kdialogbase.h>

/* search element for attribute.
 * return XMLAtt if find, else NULL with helpful info in errmsg.
 */
XMLAtt * findAtt (XMLEle *ep, const char *name, char errmsg[])
{
	XMLAtt *ap = findXMLAtt (ep, name);
	if (ap)
	    return (ap);
	if (errmsg)
	    sprintf (errmsg, "INDI: <%s> missing attribute '%s'", tagXMLEle(ep),name);
	return NULL;
}

/* search element for given child. pp is just to build a better errmsg.
 * return XMLEle if find, else NULL with helpful info in errmsg.
 */
XMLEle * findEle (XMLEle *ep, INDI_P *pp, const char *child, char errmsg[])
{
	XMLEle *cp = findXMLEle (ep, child);
	if (cp)
	    return (cp);
	if (errmsg)
	    sprintf (errmsg, "INDI: <%s %s %s> missing child '%s'", tagXMLEle(ep),
						pp->pg->dp->name.ascii(), pp->name.ascii(), child);
	return (NULL);
}

/*******************************************************************
** INDI Element
*******************************************************************/
INDI_E::INDI_E(INDI_P *parentProperty, QString inName, QString inLabel)
{
  name = inName;
  label = inLabel;

  pp = parentProperty;

  EHBox     = new QHBoxLayout(0, 0, KDialog::spacingHint());
  label_w   = NULL;
  read_w    = NULL;
  write_w   = NULL;
  spin_w    = NULL;
  push_w    = NULL;
  check_w   = NULL;
  led_w     = NULL;
  hSpacer   = NULL;

}

INDI_E::~INDI_E()
{
    delete (EHBox);
    delete (label_w);
    delete (read_w);
    delete (write_w);
    delete (spin_w);
    delete (push_w);
    delete (check_w);
    delete (led_w);
    delete (hSpacer);
}

void INDI_E::setupElementLabel()
{
label_w = new KSqueezedTextLabel(pp->pg->propertyContainer);
label_w->setMinimumWidth(ELEMENT_LABEL_WIDTH);
label_w->setMaximumWidth(ELEMENT_LABEL_WIDTH);
label_w->setFrameShape( KSqueezedTextLabel::Box );
label_w->setPaletteBackgroundColor( QColor( 224, 232, 238 ) );
label_w->setTextFormat( QLabel::RichText );
label_w->setAlignment( int( QLabel::WordBreak | QLabel::AlignCenter ) );
label_w->setText(label);

EHBox->addWidget(label_w);
}

int INDI_E::buildTextGUI(QString initText)
{

  setupElementLabel();  
  
  text = initText;
  
  switch (pp->perm)
  {
    case PP_RW:
    setupElementRead(ELEMENT_READ_WIDTH);
    setupElementWrite(ELEMENT_WRITE_WIDTH);
    
    break;
    
    case PP_RO:
    setupElementRead(ELEMENT_FULL_WIDTH);
    break;
    
    case PP_WO:
    setupElementWrite(ELEMENT_FULL_WIDTH);
    break;
  }
  
  pp->PVBox->addLayout(EHBox);
  return (0);
    
}

int INDI_E::buildNumberGUI  (double initValue)
{
  bool scale = false;
  
  updateValue(initValue);
  setupElementLabel();
  
  if (step != 0 && (max - min)/step <= MAXSCSTEPS)
    scale = true;
  
  switch (pp->perm)
  {
    case PP_RW:
     setupElementRead(ELEMENT_READ_WIDTH);
     if (scale)
       setupElementScale(ELEMENT_WRITE_WIDTH);
     else
       setupElementWrite(ELEMENT_WRITE_WIDTH);
       
       pp->PVBox->addLayout(EHBox);
     break;
     
    case PP_RO:
    setupElementRead(ELEMENT_READ_WIDTH);
    pp->PVBox->addLayout(EHBox);
    break;
    
    case PP_WO:
    if (scale)
     setupElementScale(ELEMENT_FULL_WIDTH);
    else
     setupElementWrite(ELEMENT_FULL_WIDTH);
     
     pp->PVBox->addLayout(EHBox);
    
    break;
  }
  
  return (0);
    
}

int INDI_E::buildLightGUI()
{

        led_w = new KLed (pp->pg->propertyContainer);
	led_w->setMaximumSize(16,16);
	led_w->setLook( KLed::Sunken );
	drawLt();
	
	EHBox->addWidget(led_w);
	
	setupElementLabel();
	
	pp->PVBox->addLayout(EHBox);
	
	return (0);
}

void INDI_E::drawLt()
{
        /* set state light */
	switch (state)
	{
	  case PS_IDLE:
	  led_w->setColor(Qt::gray);
	  break;

	  case PS_OK:
	  led_w->setColor(Qt::green);
	  break;

	  case PS_BUSY:
	  led_w->setColor(Qt::yellow);
	  break;

	  case PS_ALERT:
	  led_w->setColor(Qt::red);
	  break;

	  default:
	  break;

	}
}


void INDI_E::updateValue(double newValue)
{
  char iNumber[32];
  
  value = newValue; 

  numberFormat(iNumber, format.ascii(), value);
  text = iNumber;

}

void INDI_E::setupElementScale(int length)
{

spin_w = new KDoubleSpinBox(min, max, step, value, 2, pp->pg->propertyContainer );

  if (length == ELEMENT_FULL_WIDTH)
	spin_w->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)0, 0, 0, spin_w->sizePolicy().hasHeightForWidth() ) );
  else
	spin_w->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, spin_w->sizePolicy().hasHeightForWidth() ) );
	
spin_w->setMinimumWidth( length );

EHBox->addWidget(spin_w);
}

void INDI_E::setupElementWrite(int length)
{
    write_w = new KLineEdit( pp->pg->propertyContainer);
    write_w->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, write_w->sizePolicy().hasHeightForWidth() ));
    write_w->setMinimumWidth( length );
    write_w->setMaximumWidth( length);
    
    QObject::connect(write_w, SIGNAL(returnPressed()), pp, SLOT(newText()));
    EHBox->addWidget(write_w);
}


void INDI_E::setupElementRead(int length)
{

  read_w = new KLineEdit( pp->pg->propertyContainer );
  read_w->setMinimumWidth( length );
  read_w->setFocusPolicy( KLineEdit::NoFocus );
  read_w->setFrameShape( KLineEdit::GroupBoxPanel );
  read_w->setFrameShadow( KLineEdit::Plain );
  read_w->setCursorPosition( 0 );
  read_w->setAlignment( int( KLineEdit::AlignHCenter ) );
  read_w->setReadOnly( TRUE );
  read_w->setText(text);
  
  EHBox->addWidget(read_w);

}

void INDI_E::initNumberValues(double newMin, double newMax, double newStep, char * newFormat)
{
  min = newMin;
  max = newMax;
  step = newStep;
  format = newFormat;
}



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
#include <qslider.h>
#include <qdir.h>

#include <kurl.h>
#include <kfiledialog.h>
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
	    snprintf (errmsg, ERRMSG_SIZE, "INDI: <%.64s> missing attribute '%.64s'", tagXMLEle(ep),name);
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
	    snprintf (errmsg, ERRMSG_SIZE, "INDI: <%.64s %.64s %.64s> missing child '%.64s'", tagXMLEle(ep),
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
  slider_w  = NULL;
  push_w    = NULL;
  browse_w  = NULL;
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
    delete (slider_w);
    delete (push_w);
    delete (browse_w);
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

if (label.length() > MAX_LABEL_LENGTH)
{
  QFont tempFont(  label_w->font() );
  tempFont.setPointSize( tempFont.pointSize() - MED_INDI_FONT );
  label_w->setFont( tempFont );
}

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

int INDI_E::buildBLOBGUI()
{

  setupElementLabel();  
  
  text = "INDI DATA STREAM";
  
  switch (pp->perm)
  {
    case PP_RW:
      setupElementRead(ELEMENT_READ_WIDTH);
      setupElementWrite(ELEMENT_WRITE_WIDTH);
      setupBrowseButton();
    
      break;
    
    case PP_RO:
      setupElementRead(ELEMENT_FULL_WIDTH);
      break;
    
    case PP_WO:
      setupElementWrite(ELEMENT_FULL_WIDTH);
      setupBrowseButton();
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

int steps = (int) ((max - min) / step);
spin_w    = new KDoubleSpinBox(min, max, step, value, 2, pp->pg->propertyContainer );
slider_w  = new QSlider(0, steps, 1, (int) ((value - min) / step),  Qt::Horizontal, pp->pg->propertyContainer );

connect(spin_w, SIGNAL(valueChanged(double)), this, SLOT(spinChanged(double )));
connect(slider_w, SIGNAL(sliderMoved(int)), this, SLOT(sliderChanged(int )));

//kdDebug() << "For element " << label << " we have step of " << step << endl;

  if (length == ELEMENT_FULL_WIDTH)
	spin_w->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)0, 0, 0, spin_w->sizePolicy().hasHeightForWidth() ) );
  else
	spin_w->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, spin_w->sizePolicy().hasHeightForWidth() ) );
	
spin_w->setMinimumWidth( (int) (length * 0.45) );
slider_w->setMinimumWidth( (int) (length * 0.55) );

EHBox->addWidget(slider_w);
EHBox->addWidget(spin_w);
}

void INDI_E::spinChanged(double value)
{
  int slider_value = (int) ((value - min) / step);
  slider_w->setValue(slider_value);
}

void INDI_E::sliderChanged(int value)
{

 double spin_value = (value * step) + min;
 spin_w->setValue(spin_value);  

}
   
void INDI_E::setMin (double inMin)
{
  min = inMin;
  if (spin_w)
  {
    spin_w->setMinValue(min);
    spin_w->setValue(value);
  }
  if (slider_w)
  {
    slider_w->setMaxValue((int) ((max - min) / step));
    slider_w->setMinValue(0);
    slider_w->setPageStep(1);
    slider_w->setValue( (int) ((value - min) / step ));
  }
  
}
   
void INDI_E::setMax (double inMax)
{
 max = inMax;
 if (spin_w)
 {
   spin_w->setMaxValue(max);
   spin_w->setValue(value);
 }
 if (slider_w)
 {
    slider_w->setMaxValue((int) ((max - min) / step));
    slider_w->setMinValue(0);
    slider_w->setPageStep(1);
    slider_w->setValue( (int) ((value - min) / step ));
 }
 
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

void INDI_E::setupBrowseButton()
{
   browse_w = new KPushButton("...", pp->pg->propertyContainer);
   browse_w->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)5, (QSizePolicy::SizeType)0, 0, 0, browse_w->sizePolicy().hasHeightForWidth() ) );
   browse_w->setMinimumWidth( MIN_SET_WIDTH );
   browse_w->setMaximumWidth( MAX_SET_WIDTH );

   EHBox->addWidget(browse_w);
   QObject::connect(browse_w, SIGNAL(clicked()), this, SLOT(browseBlob()));
}


void INDI_E::initNumberValues(double newMin, double newMax, double newStep, char * newFormat)
{
  min = newMin;
  max = newMax;
  step = newStep;
  format = newFormat;
}

void INDI_E::browseBlob()
{

  KURL currentURL;

  currentURL = KFileDialog::getOpenURL( QDir::homeDirPath(), "*");

  // if user presses cancel
  if (currentURL.isEmpty())
		  return;

  if ( currentURL.isValid() )
    write_w->setText(currentURL.path());

}


#include "indielement.moc"

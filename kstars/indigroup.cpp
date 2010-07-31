/*  INDI Group
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    JM Changelog
    2004-16-1:	Start
   
 */
 
 #include "indiproperty.h"
 #include "indigroup.h"
 #include "indidevice.h"
 #include "devicemanager.h"
 
 #include <klocale.h>
 
 #include <tqlayout.h>
 #include <tqframe.h>
 #include <tqtimer.h>
 #include <tqtabwidget.h> 
 
 /*******************************************************************
** INDI Group: a tab widget for common properties. All properties
** belong to a group, whether they have one or not but how the group
** is displayed differs
*******************************************************************/
INDI_G::INDI_G(INDI_D *parentDevice, TQString inName)
{
  dp = parentDevice;
  
  name = inName;

  pl.setAutoDelete(true);
  
  // FIXME what's the parent exactly?
  // You can do this eaither way:
  // 1. Propertycontainer is a TQFrame, then you make TQVBoxLayout for it (check form1.cpp)
  // 2. Keep it as TQVBox and let it handle its children.
  // Depends on which one works best.
  propertyContainer = new TQFrame(dp->groupContainer);
  propertyLayout    = new TQVBoxLayout(propertyContainer, 20, KDialog::spacingHint() );
  VerticalSpacer    = new TQSpacerItem( 20, 20, TQSizePolicy::Minimum, TQSizePolicy::Expanding );
  
  propertyLayout->addItem(VerticalSpacer); 
    
  dp->groupContainer->addTab(propertyContainer, name);
}

INDI_G::~INDI_G()
{
  pl.clear();
  
  delete(propertyContainer);
}

void INDI_G::addProperty(INDI_P *pp)
{
   dp->registerProperty(pp);
   
   propertyLayout->addLayout(pp->PHBox);
   propertyLayout->addItem(VerticalSpacer);   
   
   pl.append(pp);
}

bool INDI_G::removeProperty(INDI_P *pp)
{

  return  (pl.remove(pp));

}

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
 
 #include <qlayout.h>
 #include <qframe.h>
 #include <qtimer.h>
 #include <qtabwidget.h> 
 
 /*******************************************************************
** INDI Group: a tab widget for common properties. All properties
** belong to a group, whether they have one or not but how the group
** is displayed differs
*******************************************************************/
INDI_G::INDI_G(INDI_D *parentDevice, QString inName)
{
  dp = parentDevice;
  
  name = inName;

  pl.setAutoDelete(true);
  
  // FIXME what's the parent exactly?
  // You can do this eaither way:
  // 1. Propertycontainer is a QFrame, then you make QVBoxLayout for it (check form1.cpp)
  // 2. Keep it as QVBox and let it handle its children.
  // Depends on which one works best.
  propertyContainer = new QFrame(dp->groupContainer);
  propertyLayout    = new QVBoxLayout(propertyContainer, 20, KDialog::spacingHint() );
  VerticalSpacer    = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
  
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

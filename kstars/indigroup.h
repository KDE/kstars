/*  INDI Group
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    JM Changelog
    2004-16-1:	Start
   
 */

#ifndef INDIGROUP_H
#define INDIGROUP_H

#include "indielement.h"
#include <qstring.h>
//Added by qt3to4:
#include <QVBoxLayout>
#include <Q3Frame>

class INDI_P;
class INDI_D;

class Q3Frame;
class QVBoxLayout;

/* INDI group */
class INDI_G
{
  public:
  INDI_G(INDI_D *parentDevice, QString inName);
  ~INDI_G();

  QString       name;			/* Group name */
  INDI_D 	*dp;			/* Parent device */
  Q3Frame        *propertyContainer;	/* Properties container */
  QVBoxLayout   *propertyLayout;        /* Properties layout */
  QSpacerItem   *VerticalSpacer;	/* Vertical spacer */

  QList<INDI_P*> pl;			/* malloced list of pointers to properties */
  
  void addProperty(INDI_P *pp);
  bool removeProperty(INDI_P *pp);
};

#endif

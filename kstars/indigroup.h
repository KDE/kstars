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
#include <qptrlist.h>
#include <qstring.h>

class INDI_P;
class INDI_D;

class QFrame;
class QVBoxLayout;

/* INDI group */
class INDI_G
{
  public:
  INDI_G(INDI_D *parentDevice, QString inName);
  ~INDI_G();

  QString       name;			/* Group name */
  INDI_D 	*dp;			/* Parent device */
  QFrame        *propertyContainer;	/* Properties container */
  QVBoxLayout   *propertyLayout;        /* Properties layout */
  QSpacerItem   *VerticalSpacer;	/* Vertical spacer */

  QPtrList<INDI_P> pl;			/* malloced list of pointers to properties */
  
  void addProperty(INDI_P *pp);
  bool removeProperty(INDI_P *pp);
};

#endif

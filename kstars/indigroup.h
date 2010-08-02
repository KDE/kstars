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
#include <tqptrlist.h>
#include <tqstring.h>

class INDI_P;
class INDI_D;

class TQFrame;
class TQVBoxLayout;

/* INDI group */
class INDI_G
{
  public:
  INDI_G(INDI_D *parentDevice, TQString inName);
  ~INDI_G();

  TQString       name;			/* Group name */
  INDI_D 	*dp;			/* Parent device */
  TQFrame        *propertyContainer;	/* Properties container */
  TQVBoxLayout   *propertyLayout;        /* Properties layout */
  TQSpacerItem   *VerticalSpacer;	/* Vertical spacer */

  TQPtrList<INDI_P> pl;			/* malloced list of pointers to properties */
  
  void addProperty(INDI_P *pp);
  bool removeProperty(INDI_P *pp);
};

#endif

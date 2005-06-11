/***************************************************************************
                          telescopeprop.cpp  -  description
                             -------------------
    begin                : Wed June 8th 2005
    copyright            : (C) 2005 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TELESCOPEPROP_H
#define TELESCOPEPROP_H

class KStars;
class INDIDriver;

#include "telescopepropui.h"

class telescopeProp : public scopeProp
{
  Q_OBJECT

public:
  telescopeProp(QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
  ~telescopeProp();
  /*$PUBLIC_FUNCTIONS$*/

public slots:
  void newScope();
  void saveScope();
  void updateScopeDetails(int index);
  void removeScope();
  /*$PUBLIC_SLOTS$*/

protected:
  /*$PROTECTED_FUNCTIONS$*/

protected slots:
  /*$PROTECTED_SLOTS$*/

 private:
  int findDeviceIndex(int listIndex);

  bool newScopePending;
  KStars *ksw;
  INDIDriver *indi_driver;

};

#endif


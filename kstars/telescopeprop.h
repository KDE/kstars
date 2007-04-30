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

#ifndef TELESCOPEPROP_H_
#define TELESCOPEPROP_H_

class KStars;
class INDIDriver;

#include "ui_telescopeprop.h"

class telescopeProp : public QDialog
{
  Q_OBJECT

public:
  explicit telescopeProp(QWidget* parent = 0, const char* name = 0, bool modal = false, Qt::WFlags fl = 0 );
  ~telescopeProp();

public slots:
  void newScope();
  void saveScope();
  void updateScopeDetails(int index);
  void removeScope();

 private:
  int findDeviceIndex(int listIndex);
  Ui::scopeProp *ui;

  bool newScopePending;
  KStars *ksw;
  INDIDriver *indi_driver;

};

#endif


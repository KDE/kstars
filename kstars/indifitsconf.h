/*  INDI Options (subclass)
    Copyright (C) 2005 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIFITSCONF_H_
#define INDIFITSCONF_H_

#include "ui_indiconf.h"
#include <kdialog.h>

class INDIFITSConfUI : public QFrame, public Ui::INDIConf
{

  public:

  INDIFITSConfUI(QWidget *parent=0);

};

class INDIFITSConf : public KDialog
{
  Q_OBJECT

public:
  INDIFITSConf(QWidget* parent = 0);
  ~INDIFITSConf();
  /*$PUBLIC_FUNCTIONS$*/

   void loadOptions();
   void saveOptions();
   INDIFITSConfUI *ui;

public slots:
  void comboUpdate(int newIndex);
  void saveFITSDirectory();

 private:
 int lastIndex;
 QStringList filterList;


};

#endif


/*  INDI Options (subclass)
    Copyright (C) 2005 Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIFITSCONF_H
#define INDIFITSCONF_H

#include "indiconf.h"

class INDIFITSConf : public INDIConf
{
  Q_OBJECT

public:
  INDIFITSConf(QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
  ~INDIFITSConf();
  /*$PUBLIC_FUNCTIONS$*/

   void loadOptions();
   void saveOptions();

public slots:
  void comboUpdate(int newIndex);
  void saveFITSDirectory();

 private:
 int lastIndex;
 QStringList filterList;


};

#endif


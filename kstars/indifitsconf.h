//
// C++ Interface: 
//
// Description: 
//
//
// Author: Jason Harris <kstars@30doradus.org>, (C) 2004
//
// Copyright: See COPYING file that comes with this distribution
//
//

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

public slots:
  void saveFITSDirectory();

protected:
  /*$PROTECTED_FUNCTIONS$*/

protected slots:
  /*$PROTECTED_SLOTS$*/

};

#endif


//
// C++ Implementation: 
//
// Description: 
//
//
// Author: Jason Harris <kstars@30doradus.org>, (C) 2004
//
// Copyright: See COPYING file that comes with this distribution
//
//


#include "indifitsconf.h"
#include <kiconloader.h>
#include <kpushbutton.h>
#include <kfiledialog.h>
#include <klineedit.h>

INDIFITSConf::INDIFITSConf(QWidget* parent, const char* name, bool modal, WFlags fl)
: INDIConf(parent,name, modal,fl)
{

  KIconLoader *icons = KGlobal::iconLoader();
  selectDirB->setPixmap( icons->loadIcon( "fileopen", KIcon::Toolbar ) );
  connect(selectDirB, SIGNAL(clicked()), this, SLOT(saveFITSDirectory()));

}

INDIFITSConf::~INDIFITSConf()
{
}

/*$SPECIALIZATION$*/

void INDIFITSConf::saveFITSDirectory()
{
  QString dir = KFileDialog::getExistingDirectory(fitsDIR_IN->text());
  
  if (!dir.isEmpty())
  	fitsDIR_IN->setText(dir);
}


#include "indifitsconf.moc"


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

#include <qcheckbox.h>
#include <qstringlist.h>
#include <qcombobox.h>

#include "Options.h"

INDIFITSConf::INDIFITSConf(QWidget* parent, const char* name, bool modal, WFlags fl)
: INDIConf(parent,name, modal,fl)
{

  KIconLoader *icons = KGlobal::iconLoader();
  selectDirB->setPixmap( icons->loadIcon( "fileopen", KIcon::Toolbar ) );
  connect(selectDirB, SIGNAL(clicked()), this, SLOT(saveFITSDirectory()));
  connect(filterCombo, SIGNAL(activated (int)), this, SLOT(comboUpdate(int)));
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

void INDIFITSConf::loadOptions()
{
   QStringList filterNumbers;
   lastIndex = 0;

   filterNumbers << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9";
   filterCombo->insertStringList(filterNumbers);

   timeCheck->setChecked( Options::indiAutoTime() );
   GeoCheck->setChecked( Options::indiAutoGeo() );
   crosshairCheck->setChecked( Options::indiCrosshairs() );
   messagesCheck->setChecked ( Options::indiMessages() );
   fitsAutoDisplayCheck->setChecked( Options::indiFITSDisplay() );
   telPort_IN->setText ( Options::indiTelescopePort());
   vidPort_IN->setText ( Options::indiVideoPort());

   if (Options::fitsSaveDirectory().isEmpty())
   {
     fitsDIR_IN->setText (QDir:: homeDirPath());
     Options::setFitsSaveDirectory( fitsDIR_IN->text());
   }
   else
     fitsDIR_IN->setText ( Options::fitsSaveDirectory());

   if (Options::filterAlias().empty())
         filterList << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9";
   else
         filterList = Options::filterAlias();

     filterCombo->setCurrentItem(lastIndex);
     filterAlias->setText(filterList[lastIndex]);

}

void INDIFITSConf::saveOptions()
{
  
     Options::setIndiAutoTime( timeCheck->isChecked() );
     Options::setIndiAutoGeo( GeoCheck->isChecked() );
     Options::setIndiCrosshairs( crosshairCheck->isChecked() );
     Options::setIndiMessages( messagesCheck->isChecked() );
     Options::setIndiFITSDisplay (fitsAutoDisplayCheck->isChecked());
     Options::setIndiTelescopePort ( telPort_IN->text());
     Options::setIndiVideoPort( vidPort_IN->text());
     Options::setFitsSaveDirectory( fitsDIR_IN->text());

     filterList[lastIndex] = filterAlias->text();
     Options::setFilterAlias(filterList);

}

void INDIFITSConf::comboUpdate(int newIndex)
{
  filterList[lastIndex] = filterAlias->text();
  lastIndex = newIndex;

  filterAlias->setText(filterList[lastIndex]);

}



#include "indifitsconf.moc"


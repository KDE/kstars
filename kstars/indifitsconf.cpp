/*  INDI FITS Conf
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
 */



#include "indifitsconf.h"
#include <kiconloader.h>
#include <kpushbutton.h>
#include <kfiledialog.h>
#include <klineedit.h>

#include <qcheckbox.h>
#include <qstringlist.h>
#include <qcombobox.h>

#include "Options.h"

INDIFITSConfUI::INDIFITSConfUI(QWidget *parent) : QFrame(parent)
{
  setupUi(this);
}

INDIFITSConf::INDIFITSConf(QWidget* parent)
: KDialog( parent )
{
   ui = new INDIFITSConfUI( this );
   setMainWidget( ui );
   setCaption( i18n( "Configure INDI" ) );
   setButtons( KDialog::Ok|KDialog::Cancel );

  KIconLoader *icons = KGlobal::iconLoader();
  ui->selectDirB->setPixmap( icons->loadIcon( "fileopen", K3Icon::Toolbar ) );
  connect(ui->selectDirB, SIGNAL(clicked()), this, SLOT(saveFITSDirectory()));
  connect(ui->filterCombo, SIGNAL(activated (int)), this, SLOT(comboUpdate(int)));
}


INDIFITSConf::~INDIFITSConf()
{
}

/*$SPECIALIZATION$*/

void INDIFITSConf::saveFITSDirectory()
{
  QString dir = KFileDialog::getExistingDirectory(ui->fitsDIR_IN->text());

  if (!dir.isEmpty())
  	ui->fitsDIR_IN->setText(dir);
}

void INDIFITSConf::loadOptions()
{
   QStringList filterNumbers;
   lastIndex = 0;

   filterNumbers << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9";
   ui->filterCombo->addItems(filterNumbers);

   ui->timeCheck->setChecked( Options::indiAutoTime() );
   ui->GeoCheck->setChecked( Options::indiAutoGeo() );
   ui->crosshairCheck->setChecked( Options::indiCrosshairs() );
   ui->messagesCheck->setChecked ( Options::indiMessages() );
   ui->fitsAutoDisplayCheck->setChecked( Options::indiFITSDisplay() );
   ui->telPort_IN->setText ( Options::indiTelescopePort());
   ui->vidPort_IN->setText ( Options::indiVideoPort());
   ui->portStart->setText( Options::portStart());
   ui->portEnd->setText( Options::portEnd());

   if (Options::fitsSaveDirectory().isEmpty())
   {
     ui->fitsDIR_IN->setText (QDir:: homePath());
     Options::setFitsSaveDirectory( ui->fitsDIR_IN->text());
   }
   else
     ui->fitsDIR_IN->setText ( Options::fitsSaveDirectory());

   if (Options::filterAlias().empty())
         filterList << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9";
   else
         filterList = Options::filterAlias();

     ui->filterCombo->setCurrentIndex(lastIndex);
     ui->filterAlias->setText(filterList[lastIndex]);

}

void INDIFITSConf::saveOptions()
{

     Options::setIndiAutoTime( ui->timeCheck->isChecked() );
     Options::setIndiAutoGeo( ui->GeoCheck->isChecked() );
     Options::setIndiCrosshairs( ui->crosshairCheck->isChecked() );
     Options::setIndiMessages( ui->messagesCheck->isChecked() );
     Options::setIndiFITSDisplay (ui->fitsAutoDisplayCheck->isChecked());
     Options::setIndiTelescopePort ( ui->telPort_IN->text());
     Options::setIndiVideoPort( ui->vidPort_IN->text());
     Options::setFitsSaveDirectory( ui->fitsDIR_IN->text());
     Options::setPortStart( ui->portStart->text());
     Options::setPortEnd ( ui->portEnd->text());

     filterList[lastIndex] = ui->filterAlias->text();
     Options::setFilterAlias(filterList);

}

void INDIFITSConf::comboUpdate(int newIndex)
{
  filterList[lastIndex] = ui->filterAlias->text();
  lastIndex = newIndex;

  ui->filterAlias->setText(filterList[lastIndex]);

}



#include "indifitsconf.moc"


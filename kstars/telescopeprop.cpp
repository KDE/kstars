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


#include <kpushbutton.h>
#include <klistbox.h>
#include <kcombobox.h>
#include <klineedit.h>
#include <kmessagebox.h>

#include <vector>

#include "telescopeprop.h"
#include "kstars.h"
#include "indimenu.h"
#include "indidriver.h"

telescopeProp::telescopeProp(QWidget* parent, const char* name, bool modal, WFlags fl)
: scopeProp(parent,name, modal,fl)
{

  ksw = (KStars *) parent;
  
  ksw->establishINDI();
  indi_driver = ksw->getINDIDriver();
  newScopePending = false;

  connect (newB, SIGNAL(clicked()), this, SLOT(newScope()));
  connect (saveB, SIGNAL(clicked()), this, SLOT(saveScope()));
  connect (removeB, SIGNAL(clicked()), this, SLOT(removeScope()));
  connect (telescopeListBox, SIGNAL(highlighted(int)),this, SLOT(updateScopeDetails(int)));
  connect(closeB, SIGNAL(clicked()), this, SLOT(close()));

  // Fill the combo box with drivers
  driverCombo->insertStringList(indi_driver->driversList);

  // Fill the list box with telescopes
  for (unsigned int i=0; i < indi_driver->devices.size(); i++)
  {
    if (indi_driver->devices[i]->deviceType == KSTARS_TELESCOPE)
    	telescopeListBox->insertItem(indi_driver->devices[i]->label);
  }

  telescopeListBox->setCurrentItem(0);
  updateScopeDetails(0);
     

}

telescopeProp::~telescopeProp()
{
}

void telescopeProp::newScope()
{

  driverCombo->clearEdit();
  labelEdit->clear();
  focalEdit->clear();
  versionEdit->clear();
  apertureEdit->clear();

  driverCombo->setFocus();
  telescopeListBox->clearFocus();
  telescopeListBox->clearSelection();

  newScopePending = true;

}

void telescopeProp::saveScope()
{
  IDevice *dev (NULL);
  double focal_length(-1), aperture(-1);
  int finalIndex = -1;

  if (labelEdit->text().isEmpty())
    {
       KMessageBox::error(NULL, i18n("Telescope label is missing."));
       return;
    }

    if (driverCombo->currentText().isEmpty())
    {
      KMessageBox::error(NULL, i18n("Telescope driver is missing."));
      return;
    }

   if (versionEdit->text().isEmpty())
   {
     KMessageBox::error(NULL, i18n("Telescope driver version is missing."));
     return;
   }

   if (telescopeListBox->currentItem() != -1)
   	finalIndex = findDeviceIndex(telescopeListBox->currentItem());

  // Add new scope
  if (newScopePending)
  {

    dev = new IDevice(labelEdit->text(), driverCombo->currentText(), versionEdit->text());

    dev->deviceType = KSTARS_TELESCOPE;

    focal_length = focalEdit->text().toDouble();
    aperture = apertureEdit->text().toDouble();

    if (focal_length > 0)
     dev->focal_length = focal_length;
    if (aperture > 0)
     dev->aperture = aperture;

    indi_driver->devices.push_back(dev);

    telescopeListBox->insertItem(labelEdit->text());

    telescopeListBox->setCurrentItem(telescopeListBox->count() - 1);

  }
  else
  {
    if (finalIndex == -1) return;
    indi_driver->devices[finalIndex]->label  = labelEdit->text();
    indi_driver->devices[finalIndex]->version = versionEdit->text();
    indi_driver->devices[finalIndex]->driver = driverCombo->currentText();
    
    
    focal_length = focalEdit->text().toDouble();
    aperture = apertureEdit->text().toDouble();

    if (focal_length > 0)
     indi_driver->devices[finalIndex]->focal_length = focal_length;
    if (aperture > 0)
     indi_driver->devices[finalIndex]->aperture = aperture;
  }

  indi_driver->saveDevicesToDisk();

  newScopePending = false;

  driverCombo->clearFocus();
  labelEdit->clearFocus();
  focalEdit->clearFocus();
  apertureEdit->clearFocus();

  KMessageBox::information(NULL, i18n("You need to restart KStars for changes to take effect."));

}

int telescopeProp::findDeviceIndex(int listIndex)
{
  int finalIndex = -1;

  for (unsigned int i=0; i < indi_driver->devices.size(); i++)
  {
    if (indi_driver->devices[i]->label == telescopeListBox->text(listIndex))
    {
	finalIndex = i;
        break;
    }
   }

 return finalIndex;

}

void telescopeProp::updateScopeDetails(int index)
{

  int finalIndex = -1;
  newScopePending = false;
  bool foundFlag(false);
 
  focalEdit->clear();
  apertureEdit->clear();


   finalIndex = findDeviceIndex(index);
   if (finalIndex == -1)
   {
      kdDebug() << "final index is invalid. internal error." << endl;
      return;
   }

  for (int i=0; i < driverCombo->count(); i++)
    if (indi_driver->devices[finalIndex]->driver == driverCombo->text(i))
     {
       driverCombo->setCurrentItem(i);
       foundFlag = true;
       break;
     }

  if (foundFlag == false)
    driverCombo->setCurrentText(indi_driver->devices[finalIndex]->driver);

  labelEdit->setText(indi_driver->devices[finalIndex]->label);

  versionEdit->setText(indi_driver->devices[finalIndex]->version);

  if (indi_driver->devices[finalIndex]->focal_length != -1)
  	focalEdit->setText(QString("%1").arg(indi_driver->devices[finalIndex]->focal_length));

  if (indi_driver->devices[finalIndex]->aperture != -1)
        apertureEdit->setText(QString("%1").arg(indi_driver->devices[finalIndex]->aperture));

}

void telescopeProp::removeScope()
{

  int index, finalIndex;

  index = telescopeListBox->currentItem();
  finalIndex = findDeviceIndex(index);

  if (KMessageBox::warningContinueCancel( 0, i18n("Are you sure you want to remove %1?").arg(indi_driver->devices[finalIndex]->label), i18n("Delete Confirmation"),KStdGuiItem::del())!=KMessageBox::Continue)
           return;

  telescopeListBox->removeItem(index);

  delete (indi_driver->devices[finalIndex]);
  indi_driver->devices.erase(indi_driver->devices.begin() + finalIndex);
  
  indi_driver->saveDevicesToDisk();

}


#include "telescopeprop.moc"


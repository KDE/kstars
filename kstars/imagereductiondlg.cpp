/***************************************************************************
                          imagereductiondlg.cpp  -  Image reduction utility
                          ---------------------
    begin                : Tue Feb 24 2004
    copyright            : (C) 2004 by Jasem Mutlaq
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
 
 #include <QListWidget>
 #include <QPushButton>
 
 #include <kurl.h>
 #include <kfiledialog.h>
 #include <klocale.h>
 
 #include "imagereductiondlg.h"
 
 ImageReductionDlg::ImageReductionDlg(QWidget * parent) : QDialog(parent)
{
  
  ui = new Ui::ImageReduction();
  ui->setupUi(this);

  /*ui->darkListView->setSortingEnabled(false);
  ui->flatListView->setSortingEnabled(false);
  ui->darkflatListView->setSortingEnable(false);*/

  connect(ui->darkAddB, SIGNAL(clicked()), this, SLOT(addDarkFile()));
  connect(ui->flatAddB, SIGNAL(clicked()), this, SLOT(addFlatFile()));
  connect(ui->darkRemoveB, SIGNAL(clicked()), this, SLOT(removeDarkFile()));
  connect(ui->flatRemoveB, SIGNAL(clicked()), this, SLOT(removeFlatFile()));
  connect(ui->darkDetailsB, SIGNAL(clicked()), this, SLOT(detailsDarkFile()));
  connect(ui->flatDetailsB, SIGNAL(clicked()), this, SLOT(detailsFlatFile()));
  connect(ui->darkflatAddB, SIGNAL(clicked()), this, SLOT(addDarkFlatFile()));
  connect(ui->darkflatRemoveB, SIGNAL(clicked()), this, SLOT(removeDarkFlatFile()));
  connect(ui->darkflatDetailsB, SIGNAL(clicked()), this, SLOT(detailsDarkFlatFile()));
  connect(ui->buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
  connect(ui->buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
}

ImageReductionDlg::~ImageReductionDlg()
{
}

void ImageReductionDlg::addDarkFile()
{
   KUrl::List fileURLs = KFileDialog::getOpenUrls( QString(), "*.fits *.fit *.fts|Flexible Image Transport System", 0, i18n("Dark Frames"));
  
  const int limit = (int) fileURLs.size();
  for (int i=0; i < limit ; ++i)
  	new QListWidgetItem( fileURLs[i].path(), ui->darkListView);
  
  ui->darkRemoveB->setEnabled(true);
  ui->darkDetailsB->setEnabled(true);

}

void ImageReductionDlg::addFlatFile()
{
   KUrl::List fileURLs = KFileDialog::getOpenUrls( QString(), "*.fits *.fit *.fts|Flexible Image Transport System", 0, i18n("Flat Frames"));
  
  const int limit = (int) fileURLs.size();
  
  for (int i=0; i < limit; ++i) 
  	new QListWidgetItem( fileURLs[i].path(), ui->flatListView);
  
  ui->flatRemoveB->setEnabled(true);
  ui->flatDetailsB->setEnabled(true);

}

void ImageReductionDlg::addDarkFlatFile()
{
     KUrl::List fileURLs = KFileDialog::getOpenUrls( QString(), "*.fits *.fit *.fts|Flexible Image Transport System", 0, i18n("Dark Flat Frames"));
  
     const int limit = (int) fileURLs.size();
     for (int i=0; i < limit; ++i) 
  	new QListWidgetItem(fileURLs[i].path(), ui->darkflatListView);
  
  ui->darkflatRemoveB->setEnabled(true);
  ui->darkflatDetailsB->setEnabled(true);
}

void ImageReductionDlg::removeDarkFile()
{
  if (ui->darkListView->currentItem() == NULL)
    return;
  
  ui->darkListView->takeItem(ui->darkListView->row(ui->darkListView->currentItem()));
}

void ImageReductionDlg::removeDarkFlatFile()
{
  if (ui->darkflatListView->currentItem() == NULL)
    return;
  
  
  ui->darkflatListView->takeItem(ui->darkflatListView->row(ui->darkflatListView->currentItem()));
}

void ImageReductionDlg::removeFlatFile()
{

 if (ui->flatListView->currentItem() == NULL)
    return;
  
  ui->flatListView->takeItem(ui->flatListView->row(ui->flatListView->currentItem()));

}

void ImageReductionDlg::detailsDarkFile()
{




}

void ImageReductionDlg::detailsFlatFile()
{





}

void ImageReductionDlg::detailsDarkFlatFile()
{





}

#include "imagereductiondlg.moc"

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
 
 #include <qlistview.h>
 #include <qpushbutton.h>
 
 #include <kurl.h>
 #include <kfiledialog.h>
 
 #include "imagereductiondlg.h"
 
 ImageReductionDlg::ImageReductionDlg(QWidget * parent, const char * name) : imageReductionUI(parent, name)
{
  connect(darkAddB, SIGNAL(clicked()), this, SLOT(addDarkFile()));
  connect(flatAddB, SIGNAL(clicked()), this, SLOT(addFlatFile()));
  connect(darkRemoveB, SIGNAL(clicked()), this, SLOT(removeDarkFile()));
  connect(flatRemoveB, SIGNAL(clicked()), this, SLOT(removeFlatFile()));
  connect(darkDetailsB, SIGNAL(clicked()), this, SLOT(detailsDarkFile()));
  connect(flatDetailsB, SIGNAL(clicked()), this, SLOT(detailsFlatFile()));
  
  darkListView->setSorting(-1);
  flatListView->setSorting(-1);
}

ImageReductionDlg::~ImageReductionDlg()
{




}

void ImageReductionDlg::addDarkFile()
{
   KURL fileURL = KFileDialog::getOpenURL( QDir::homeDirPath(), "*.fits *.fit *.fts|Flexible Image Transport System");
  
  if (fileURL.isEmpty())
    return;
    
  new QListViewItem( darkListView, fileURL.path());
  
  darkRemoveB->setEnabled(true);
  darkDetailsB->setEnabled(true);

}

void ImageReductionDlg::addFlatFile()
{
   KURL fileURL = KFileDialog::getOpenURL( QDir::homeDirPath(), "*.fits *.fit *.fts|Flexible Image Transport System");
  
  if (fileURL.isEmpty())
    return;
    
  new QListViewItem( flatListView, fileURL.path());
  
  flatRemoveB->setEnabled(true);
  flatDetailsB->setEnabled(true);

}

void ImageReductionDlg::removeDarkFile()
{

  if (darkListView->currentItem() == NULL)
    return;
  
  darkListView->takeItem(darkListView->currentItem());

}

void ImageReductionDlg::removeFlatFile()
{

 if (flatListView->currentItem() == NULL)
    return;
  
  flatListView->takeItem(flatListView->currentItem());

}

void ImageReductionDlg::detailsDarkFile()
{




}

void ImageReductionDlg::detailsFlatFile()
{





}

#include "imagereductiondlg.moc"

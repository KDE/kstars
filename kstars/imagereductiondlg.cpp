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
 #include <klocale.h>
 
 #include "imagereductiondlg.h"
 
 ImageReductionDlg::ImageReductionDlg(QWidget * parent, const char * name) : imageReductionUI(parent, name)
{
  connect(darkAddB, SIGNAL(clicked()), this, SLOT(addDarkFile()));
  connect(flatAddB, SIGNAL(clicked()), this, SLOT(addFlatFile()));
  connect(darkRemoveB, SIGNAL(clicked()), this, SLOT(removeDarkFile()));
  connect(flatRemoveB, SIGNAL(clicked()), this, SLOT(removeFlatFile()));
  connect(darkDetailsB, SIGNAL(clicked()), this, SLOT(detailsDarkFile()));
  connect(flatDetailsB, SIGNAL(clicked()), this, SLOT(detailsFlatFile()));
  connect(darkflatAddB, SIGNAL(clicked()), this, SLOT(addDarkFlatFile()));
  connect(darkflatRemoveB, SIGNAL(clicked()), this, SLOT(removeDarkFlatFile()));
  connect(darkflatDetailsB, SIGNAL(clicked()), this, SLOT(detailsDarkFlatFile()));
  
  darkListView->setSorting(-1);
  flatListView->setSorting(-1);
  darkflatListView->setSorting(-1);
  
}

ImageReductionDlg::~ImageReductionDlg()
{




}

void ImageReductionDlg::addDarkFile()
{
   KURL::List fileURLs = KFileDialog::getOpenURLs( QString::null, "*.fits *.fit *.fts|Flexible Image Transport System", 0, i18n("Dark Frames"));
  
  const int limit = (int) fileURLs.size();
  for (int i=0; i < limit ; ++i)
  	new QListViewItem( darkListView, fileURLs[i].path());
  
  darkRemoveB->setEnabled(true);
  darkDetailsB->setEnabled(true);

}

void ImageReductionDlg::addFlatFile()
{
   KURL::List fileURLs = KFileDialog::getOpenURLs( QString::null, "*.fits *.fit *.fts|Flexible Image Transport System", 0, i18n("Flat Frames"));
  
  const int limit = (int) fileURLs.size();
  
  for (int i=0; i < limit; ++i) 
  	new QListViewItem( flatListView, fileURLs[i].path());
  
  flatRemoveB->setEnabled(true);
  flatDetailsB->setEnabled(true);

}

void ImageReductionDlg::addDarkFlatFile()
{
     KURL::List fileURLs = KFileDialog::getOpenURLs( QString::null, "*.fits *.fit *.fts|Flexible Image Transport System", 0, i18n("Dark Flat Frames"));
  
     const int limit = (int) fileURLs.size();
     for (int i=0; i < limit; ++i) 
  	new QListViewItem( darkflatListView, fileURLs[i].path());
  
  darkflatRemoveB->setEnabled(true);
  darkflatDetailsB->setEnabled(true);


}

void ImageReductionDlg::removeDarkFile()
{

  if (darkListView->currentItem() == NULL)
    return;
  
  darkListView->takeItem(darkListView->currentItem());

}

void ImageReductionDlg::removeDarkFlatFile()
{

  if (darkflatListView->currentItem() == NULL)
    return;
  
  darkflatListView->takeItem(darkflatListView->currentItem());

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

void ImageReductionDlg::detailsDarkFlatFile()
{





}

#include "imagereductiondlg.moc"

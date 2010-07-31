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
 
 #include <tqlistview.h>
 #include <tqpushbutton.h>
 
 #include <kurl.h>
 #include <kfiledialog.h>
 #include <klocale.h>
 
 #include "imagereductiondlg.h"
 
 ImageReductionDlg::ImageReductionDlg(TQWidget * parent, const char * name) : imageReductionUI(parent, name)
{
  connect(darkAddB, TQT_SIGNAL(clicked()), this, TQT_SLOT(addDarkFile()));
  connect(flatAddB, TQT_SIGNAL(clicked()), this, TQT_SLOT(addFlatFile()));
  connect(darkRemoveB, TQT_SIGNAL(clicked()), this, TQT_SLOT(removeDarkFile()));
  connect(flatRemoveB, TQT_SIGNAL(clicked()), this, TQT_SLOT(removeFlatFile()));
  connect(darkDetailsB, TQT_SIGNAL(clicked()), this, TQT_SLOT(detailsDarkFile()));
  connect(flatDetailsB, TQT_SIGNAL(clicked()), this, TQT_SLOT(detailsFlatFile()));
  connect(darkflatAddB, TQT_SIGNAL(clicked()), this, TQT_SLOT(addDarkFlatFile()));
  connect(darkflatRemoveB, TQT_SIGNAL(clicked()), this, TQT_SLOT(removeDarkFlatFile()));
  connect(darkflatDetailsB, TQT_SIGNAL(clicked()), this, TQT_SLOT(detailsDarkFlatFile()));
  
  darkListView->setSorting(-1);
  flatListView->setSorting(-1);
  darkflatListView->setSorting(-1);
  
}

ImageReductionDlg::~ImageReductionDlg()
{




}

void ImageReductionDlg::addDarkFile()
{
   KURL::List fileURLs = KFileDialog::getOpenURLs( TQString::null, "*.fits *.fit *.fts|Flexible Image Transport System", 0, i18n("Dark Frames"));
  
  const int limit = (int) fileURLs.size();
  for (int i=0; i < limit ; ++i)
  	new TQListViewItem( darkListView, fileURLs[i].path());
  
  darkRemoveB->setEnabled(true);
  darkDetailsB->setEnabled(true);

}

void ImageReductionDlg::addFlatFile()
{
   KURL::List fileURLs = KFileDialog::getOpenURLs( TQString::null, "*.fits *.fit *.fts|Flexible Image Transport System", 0, i18n("Flat Frames"));
  
  const int limit = (int) fileURLs.size();
  
  for (int i=0; i < limit; ++i) 
  	new TQListViewItem( flatListView, fileURLs[i].path());
  
  flatRemoveB->setEnabled(true);
  flatDetailsB->setEnabled(true);

}

void ImageReductionDlg::addDarkFlatFile()
{
     KURL::List fileURLs = KFileDialog::getOpenURLs( TQString::null, "*.fits *.fit *.fts|Flexible Image Transport System", 0, i18n("Dark Flat Frames"));
  
     const int limit = (int) fileURLs.size();
     for (int i=0; i < limit; ++i) 
  	new TQListViewItem( darkflatListView, fileURLs[i].path());
  
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

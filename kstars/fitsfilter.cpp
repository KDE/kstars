/***************************************************************************
                          fitsfilter.cpp -   FITS Filters
                          ----------------
    begin                : Wed Mar 10th 2004
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
 *  									   *
 ***************************************************************************/
 
 #include <qpushbutton.h>
 #include <qlayout.h>
 #include <qframe.h>
 
 #include <kicontheme.h>
 #include <klocale.h>
 #include <kiconloader.h>
 #include <kpushbutton.h>
  
 #include "fitsfilter.h"
 #include "fitsfilterui.h"
 #include "fitsviewer.h"
 #include "fitsimage.h"
  
 FITSFilter::FITSFilter(QWidget * parent, const char * name) : KDialogBase(KDialogBase::Plain, i18n( "Filters" ), Ok|Apply|Close, Ok, parent )
 {
    viewer = (FITSViewer *) parent;
    
    QFrame *page = plainPage();
    QVBoxLayout *vlay = new QVBoxLayout( page, 0, 0 );
    filterWidget = new FilterUI( page );
    vlay->addWidget( filterWidget );
    
    KIconLoader *icons = KGlobal::iconLoader();
    filterWidget->CopyButton->setPixmap( icons->loadIcon( "reload", KIcon::Toolbar ) );
    filterWidget->AddButton->setPixmap( icons->loadIcon( "back", KIcon::Toolbar ) );
    filterWidget->RemoveButton->setPixmap( icons->loadIcon( "forward", KIcon::Toolbar ) );
    filterWidget->UpButton->setPixmap( icons->loadIcon( "up", KIcon::Toolbar ) );
    filterWidget->DownButton->setPixmap( icons->loadIcon( "down", KIcon::Toolbar ) );
    
    filterBrowser << "Inverse" << "Normalize" << "Equalize" << "Distort" << "Flatten" << "Threshhold";
    
    for (int i=0; i < filterBrowser.count(); i++)
    filterWidget->browserListBox->insertItem(filterBrowser[i], -1);
    
    
    connect( filterWidget->browserListBox, SIGNAL( doubleClicked(QListBoxItem*) ), this, SLOT( slotAddFilter() ) );
    connect( filterWidget->UpButton, SIGNAL( clicked() ), this, SLOT( slotMoveFilterUp() ) );
    connect( filterWidget->DownButton, SIGNAL( clicked() ), this, SLOT( slotMoveFilterDown() ) );
    connect( filterWidget->CopyButton, SIGNAL( clicked() ), this, SLOT( slotCopyFilter() ) );
    connect( filterWidget->RemoveButton, SIGNAL( clicked() ), this, SLOT( slotRemoveFilter() ) );
    connect( filterWidget->AddButton, SIGNAL( clicked() ), this, SLOT( slotAddFilter() ) );
    
}

FITSFilter::~FITSFilter() {}

void FITSFilter::slotMoveFilterUp()
{






}

void FITSFilter::slotMoveFilterDown()
{

}

void FITSFilter::slotCopyFilter()
{




}

void FITSFilter::slotRemoveFilter()
{
  int index=0;
  if ( (index = filterWidget->currentListBox->currentItem()) != -1)
    filterWidget->currentListBox->removeItem(index);
}

void FITSFilter::slotAddFilter()
{

 int index=0;
  if ( (index = filterWidget->browserListBox->currentItem()) != -1)
    filterWidget->currentListBox->insertItem(filterWidget->browserListBox->currentText(), -1);
}

#include "fitsfilter.moc"


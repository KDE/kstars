/***************************************************************************
                          opscatalog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 29  2004
    copyright            : (C) 2004 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qlistview.h> //QCheckListItem

#include "opscatalog.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "addcatdialog.h"
#include "magnitudespinbox.h"

OpsCatalog::OpsCatalog( QWidget *p, const char *name, WFlags fl ) 
	: OpsCatalogUI( p, name, fl ) 
{
	ksw = (KStars *)p;

	//Populate CatalogList
	showIC = new QCheckListItem( CatalogList, i18n( "Index Catalog (IC)" ), QCheckListItem::CheckBox );
	showIC->setOn( Options::showIC() );

	showNGC = new QCheckListItem( CatalogList, i18n( "New General Catalog (NGC)" ), QCheckListItem::CheckBox );
	showNGC->setOn( Options::showNGC() );

	showMessImages = new QCheckListItem( CatalogList, i18n( "Messier Catalog (images)" ), QCheckListItem::CheckBox );
	showMessImages->setOn( Options::showMessierImages() );

	showMessier = new QCheckListItem( CatalogList, i18n( "Messier Catalog (symbols)" ), QCheckListItem::CheckBox );
	showMessier->setOn( Options::showMessier() );

	kcfg_MagLimitDrawStar->setValue( Options::magLimitDrawStar() );
	kcfg_MagLimitDrawStarZoomOut->setValue( Options::magLimitDrawStarZoomOut() );
	kcfg_MagLimitDrawStar->setMinValue( Options::magLimitDrawStarZoomOut() );
	kcfg_MagLimitDrawStarZoomOut->setMaxValue( Options::magLimitDrawStar() );
	
	kcfg_MagLimitDrawDeepSky->setMaxValue( 16.0 );
	kcfg_MagLimitDrawDeepSkyZoomOut->setMaxValue( 16.0 );
	
	//Add custom catalogs, if necessary
	for ( unsigned int i=0; i<Options::catalogCount(); ++i ) { //loop over custom catalogs
		QCheckListItem *newItem = new QCheckListItem( CatalogList, Options::catalogName()[i], QCheckListItem::CheckBox );
		newItem->setOn( Options::showCatalog()[i] );
	}

	connect( CatalogList, SIGNAL( clicked( QListViewItem* ) ), this, SLOT( updateDisplay() ) );
	connect( CatalogList, SIGNAL( selectionChanged() ), this, SLOT( selectCatalog() ) );
	connect( AddCatalog, SIGNAL( clicked() ), this, SLOT( slotAddCatalog() ) );
	connect( RemoveCatalog, SIGNAL( clicked() ), this, SLOT( slotRemoveCatalog() ) );

	// draw star magnitude box
	connect( kcfg_MagLimitDrawStar, SIGNAL( valueChanged(double) ),
		SLOT( slotSetDrawStarMagnitude(double) ) );
	
	// draw star zoom out magnitude box
	connect( kcfg_MagLimitDrawStarZoomOut, SIGNAL( valueChanged(double) ),
		SLOT( slotSetDrawStarZoomOutMagnitude(double) ) );
}

//empty destructor
OpsCatalog::~OpsCatalog() {}

void OpsCatalog::updateDisplay() {
	//Modify display according to settings in the CatalogList
	if ( sender()->name() == QString( "CatalogList" ) )
		Options::setShowDeepSky( true );

	Options::setShowMessier( showMessier->isOn() );
	Options::setShowMessierImages( showMessImages->isOn() );
	Options::setShowNGC( showNGC->isOn() );
	Options::setShowIC( showIC->isOn() );
	for ( unsigned int i=0; i<Options::catalogCount(); ++i ) {
		QCheckListItem *item = (QCheckListItem*)( CatalogList->findItem( Options::catalogName()[i], 0 ));
		Options::showCatalog()[i] = item->isOn();
	}

	// update time for all objects because they might be not initialized
	// it's needed when using horizontal coordinates
	ksw->data()->setFullTimeUpdate();
	ksw->updateTime();
	ksw->map()->forceUpdate();
}

void OpsCatalog::selectCatalog() {
//If selected item is a custom catalog, enable the remove button (otherwise, disable it)
	RemoveCatalog->setEnabled( false );
	for ( unsigned int i=0; i < Options::catalogName().count(); ++i ) {
		if ( CatalogList->currentItem()->text( 0 ) == Options::catalogName()[i] ) {
			RemoveCatalog->setEnabled( true );
			break;
		}
	}
}

void OpsCatalog::slotAddCatalog() {
	AddCatDialog ac(this);
	if ( ac.exec()==QDialog::Accepted ) {
		//compute Horizontal coords for custom objects:
		for ( unsigned int i=0; i < ac.objectList().count(); ++i )
			ac.objectList().at(i)->EquatorialToHorizontal( ksw->LST(), ksw->geo()->lat() );

		//Add new custom catalog, based on the list of SkyObjects we just parsed
		ksw->data()->addCatalog( ac.name(), ac.objectList() );
		QCheckListItem *newCat = new QCheckListItem( CatalogList, ac.name(), QCheckListItem::CheckBox );
		newCat->setOn( true );
		CatalogList->insertItem( newCat );

		Options::setCatalogCount( Options::catalogCount() + 1 );
		Options::catalogName().append( ac.name() );
		Options::catalogFile().append( ac.filename() );
		Options::showCatalog().append( true );
		
		ksw->map()->forceUpdate();
	}
}

void OpsCatalog::slotRemoveCatalog() {
	//Remove CatalogName, CatalogFile, and ShowCatalog entries, and decrement CatalogCount
	for ( unsigned int i=0; i < Options::catalogCount(); ++i ) {
		if ( CatalogList->currentItem()->text( 0 ) == Options::catalogName()[i] ) {
			Options::catalogName().remove( Options::catalogName()[i] );
			Options::catalogFile().remove( Options::catalogFile()[i] );
			Options::showCatalog().remove( Options::showCatalog()[i] );
			Options::setCatalogCount( Options::catalogCount() - 1 );
			break;
		}
	}

	//Remove entry in the QListView
	CatalogList->takeItem( CatalogList->currentItem() );

	ksw->map()->forceUpdate();
}

void OpsCatalog::slotSetDrawStarMagnitude(double newValue) {
	kcfg_MagLimitDrawStarZoomOut->setMaxValue( newValue );
	ksw->data()->setMagnitude( newValue );
}

void OpsCatalog::slotSetDrawStarZoomOutMagnitude(double newValue) {
	kcfg_MagLimitDrawStar->setMinValue( newValue );
	Options::setMagLimitDrawStarZoomOut( newValue );
	// force redraw
	ksw->map()->forceUpdate();
}

#include "opscatalog.moc"

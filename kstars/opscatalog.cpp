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

#include <tqlistview.h> //QCheckListItem
#include <tqcheckbox.h>
#include <tqlabel.h>
#include <kfiledialog.h>

#include "opscatalog.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "addcatdialog.h"
#include "magnitudespinbox.h"
#include "customcatalog.h"

OpsCatalog::OpsCatalog( TQWidget *p, const char *name, WFlags fl ) 
	: OpsCatalogUI( p, name, fl ) 
{
	ksw = (KStars *)p;

	//Populate CatalogList
	showIC = new TQCheckListItem( CatalogList, i18n( "Index Catalog (IC)" ), TQCheckListItem::CheckBox );
	showIC->setOn( Options::showIC() );

	showNGC = new TQCheckListItem( CatalogList, i18n( "New General Catalog (NGC)" ), TQCheckListItem::CheckBox );
	showNGC->setOn( Options::showNGC() );

	showMessImages = new TQCheckListItem( CatalogList, i18n( "Messier Catalog (images)" ), TQCheckListItem::CheckBox );
	showMessImages->setOn( Options::showMessierImages() );

	showMessier = new TQCheckListItem( CatalogList, i18n( "Messier Catalog (symbols)" ), TQCheckListItem::CheckBox );
	showMessier->setOn( Options::showMessier() );

	kcfg_MagLimitDrawStar->setValue( Options::magLimitDrawStar() );
	kcfg_MagLimitDrawStarZoomOut->setValue( Options::magLimitDrawStarZoomOut() );
	kcfg_MagLimitDrawStar->setMinValue( Options::magLimitDrawStarZoomOut() );
	kcfg_MagLimitDrawStarZoomOut->setMaxValue( Options::magLimitDrawStar() );
	
	kcfg_MagLimitDrawDeepSky->setMaxValue( 16.0 );
	kcfg_MagLimitDrawDeepSkyZoomOut->setMaxValue( 16.0 );
	
	//disable star-related widgets if not showing stars
	if ( ! kcfg_ShowStars->isChecked() ) slotStarWidgets(false);
	
	//Add custom catalogs, if necessary
	for ( unsigned int i=0; i<ksw->data()->customCatalogs().count(); ++i ) { //loop over custom catalogs
		TQCheckListItem *newItem = new TQCheckListItem( CatalogList, ksw->data()->customCatalogs().at(i)->name(), TQCheckListItem::CheckBox );
		newItem->setOn( Options::showCatalog()[i] );
	}

	connect( CatalogList, TQT_SIGNAL( clicked( TQListViewItem* ) ), this, TQT_SLOT( updateDisplay() ) );
	connect( CatalogList, TQT_SIGNAL( selectionChanged() ), this, TQT_SLOT( selectCatalog() ) );
	connect( AddCatalog, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotAddCatalog() ) );
	connect( LoadCatalog, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotLoadCatalog() ) );
	connect( RemoveCatalog, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotRemoveCatalog() ) );

	connect( kcfg_MagLimitDrawStar, TQT_SIGNAL( valueChanged(double) ),
		TQT_SLOT( slotSetDrawStarMagnitude(double) ) );
	connect( kcfg_MagLimitDrawStarZoomOut, TQT_SIGNAL( valueChanged(double) ),
		TQT_SLOT( slotSetDrawStarZoomOutMagnitude(double) ) );
	connect( kcfg_ShowStars, TQT_SIGNAL( toggled(bool) ), TQT_SLOT( slotStarWidgets(bool) ) );
}

//empty destructor
OpsCatalog::~OpsCatalog() {}

void OpsCatalog::updateDisplay() {
	//Modify display according to settings in the CatalogList
	if ( sender()->name() == TQString( "CatalogList" ) )
		Options::setShowDeepSky( true );

	Options::setShowMessier( showMessier->isOn() );
	Options::setShowMessierImages( showMessImages->isOn() );
	Options::setShowNGC( showNGC->isOn() );
	Options::setShowIC( showIC->isOn() );
	for ( unsigned int i=0; i<ksw->data()->customCatalogs().count(); ++i ) {
		TQCheckListItem *item = (TQCheckListItem*)( CatalogList->findItem( ksw->data()->customCatalogs().at(i)->name(), 0 ));
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
	for ( unsigned int i=0; i < ksw->data()->customCatalogs().count(); ++i ) {
		if ( CatalogList->currentItem()->text( 0 ) == ksw->data()->customCatalogs().at(i)->name() ) {
			RemoveCatalog->setEnabled( true );
			break;
		}
	}
}

void OpsCatalog::slotAddCatalog() {
	AddCatDialog ac(this);
	if ( ac.exec()==TQDialog::Accepted ) 
		insertCatalog( ac.filename() );
}

void OpsCatalog::slotLoadCatalog() {
	//Get the filename from the user
	TQString filename = KFileDialog::getOpenFileName( TQDir::homeDirPath(), "*");
	if ( ! filename.isEmpty() ) {
		//test integrity of file before trying to add it
		CustomCatalog *newCat = ksw->data()->createCustomCatalog( filename, true ); //true = show errors
		if ( newCat ) {
			int nObjects = newCat->objList().count();
			delete newCat;
			if ( nObjects )
				insertCatalog( filename );
		}
	}
}

void OpsCatalog::insertCatalog( const TQString &filename ) {
	//Add new custom catalog, based on the list of SkyObjects we just parsed
	ksw->data()->addCatalog( filename );

	//Get the new catalog's name, add entry to the listbox
	TQString name = ksw->data()->customCatalogs().current()->name();
	TQCheckListItem *newCat = new TQCheckListItem( CatalogList, name, TQCheckListItem::CheckBox );
	newCat->setOn( true );
	CatalogList->insertItem( newCat );

	//update Options object
	TQStringList tFileList = Options::catalogFile();
	TQValueList<int> tShowList = Options::showCatalog();
	tFileList.append( filename );
	tShowList.append( true );
	Options::setCatalogFile( tFileList );
	Options::setShowCatalog( tShowList );
	
	ksw->map()->forceUpdate();
}

void OpsCatalog::slotRemoveCatalog() {
	//Remove CatalogName, CatalogFile, and ShowCatalog entries, and decrement CatalogCount
	for ( unsigned int i=0; i < ksw->data()->customCatalogs().count(); ++i ) {
		if ( CatalogList->currentItem()->text( 0 ) == ksw->data()->customCatalogs().at(i)->name() ) {

			ksw->data()->removeCatalog( i );

			//Update Options object
			TQStringList tFileList = Options::catalogFile();
			TQValueList<int> tShowList = Options::showCatalog();
			tFileList.remove( tFileList[i] );
			tShowList.remove( tShowList[i] );
			Options::setCatalogFile( tFileList );
			Options::setShowCatalog( tShowList );
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

void OpsCatalog::slotStarWidgets(bool on) {
	textLabelMagStars->setEnabled(on);
	textLabelMagStarsZoomOut->setEnabled(on);
	textLabelMagStarInfo->setEnabled(on);
	textLabelMag1->setEnabled(on);
	textLabelMag2->setEnabled(on);
	textLabelMag3->setEnabled(on);
	kcfg_MagLimitDrawStar->setEnabled(on);
	kcfg_MagLimitDrawStarZoomOut->setEnabled(on);
	kcfg_MagLimitDrawStarInfo->setEnabled(on);
	kcfg_ShowStarNames->setEnabled(on);
	kcfg_ShowStarMagnitudes->setEnabled(on);
}

#include "opscatalog.moc"

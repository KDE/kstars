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

#include <QList>
#include <QListWidgetItem>
#include <QTextStream>

#include <kfiledialog.h>

#include "opscatalog.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "addcatdialog.h"
#include "widgets/magnitudespinbox.h"
#include "skycomponents/customcatalogcomponent.h"

OpsCatalog::OpsCatalog( KStars *_ks ) 
	: QFrame( _ks ), ksw(_ks)
{
	setupUi(this);

	//Populate CatalogList
	showIC = new QListWidgetItem( i18n( "Index Catalog (IC)" ), CatalogList );
	showIC->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
	showIC->setCheckState( Options::showIC() ?  Qt::Checked : Qt::Unchecked );

	showNGC = new QListWidgetItem( i18n( "New General Catalog (NGC)" ), CatalogList );
	showNGC->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
	showNGC->setCheckState( Options::showNGC() ?  Qt::Checked : Qt::Unchecked );

	showMessImages = new QListWidgetItem( i18n( "Messier Catalog (images)" ), CatalogList );
	showMessImages->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
	showMessImages->setCheckState( Options::showMessierImages()  ?  Qt::Checked : Qt::Unchecked );

	showMessier = new QListWidgetItem( i18n( "Messier Catalog (symbols)" ), CatalogList );
	showMessier->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
	showMessier->setCheckState( Options::showMessier() ?  Qt::Checked : Qt::Unchecked );

	kcfg_MagLimitDrawStar->setValue( Options::magLimitDrawStar() );
	kcfg_MagLimitDrawStarZoomOut->setValue( Options::magLimitDrawStarZoomOut() );
	kcfg_MagLimitDrawStar->setMinimum( Options::magLimitDrawStarZoomOut() );
	kcfg_MagLimitDrawStarZoomOut->setMaximum( Options::magLimitDrawStar() );
	
	kcfg_MagLimitDrawDeepSky->setMaximum( 16.0 );
	kcfg_MagLimitDrawDeepSkyZoomOut->setMaximum( 16.0 );
	
	//disable star-related widgets if not showing stars
	if ( ! kcfg_ShowStars->isChecked() ) slotStarWidgets(false);
	
	//Add custom catalogs, if necessary
	foreach ( SkyComponent *sc, ksw->data()->skyComposite()->customCatalogs() ) {
		CustomCatalogComponent *cc = (CustomCatalogComponent*)sc;
		int i = ksw->data()->skyComposite()->customCatalogs().indexOf( sc );
		QListWidgetItem *newItem = new QListWidgetItem( cc->name(), CatalogList );
		newItem->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
		newItem->setCheckState( Options::showCatalog()[i] ?  Qt::Checked : Qt::Unchecked );
	}

	connect( CatalogList, SIGNAL( clicked( QListWidgetItem* ) ), this, SLOT( updateDisplay() ) );
	connect( CatalogList, SIGNAL( selectionChanged() ), this, SLOT( selectCatalog() ) );
	connect( AddCatalog, SIGNAL( clicked() ), this, SLOT( slotAddCatalog() ) );
	connect( LoadCatalog, SIGNAL( clicked() ), this, SLOT( slotLoadCatalog() ) );
	connect( RemoveCatalog, SIGNAL( clicked() ), this, SLOT( slotRemoveCatalog() ) );

	connect( kcfg_MagLimitDrawStar, SIGNAL( valueChanged(double) ),
		SLOT( slotSetDrawStarMagnitude(double) ) );
	connect( kcfg_MagLimitDrawStarZoomOut, SIGNAL( valueChanged(double) ),
		SLOT( slotSetDrawStarZoomOutMagnitude(double) ) );
	connect( kcfg_ShowStars, SIGNAL( toggled(bool) ), SLOT( slotStarWidgets(bool) ) );
}

//empty destructor
OpsCatalog::~OpsCatalog() {}

void OpsCatalog::updateDisplay() {
	//Modify display according to settings in the CatalogList
	if ( sender()->objectName() == QString( "CatalogList" ) )
		Options::setShowDeepSky( true );

	Options::setShowMessier( showMessier->checkState() );
	Options::setShowMessierImages( showMessImages->checkState() );
	Options::setShowNGC( showNGC->checkState() );
	Options::setShowIC( showIC->checkState() );
	for ( int i=0; i < ksw->data()->skyComposite()->customCatalogs().size(); ++i ) {
		QString name = ((CustomCatalogComponent*)ksw->data()->skyComposite()->customCatalogs()[i])->name();
		QList<QListWidgetItem*> l = CatalogList->findItems( name, Qt::MatchExactly );
		Options::showCatalog()[i] = (l[0]->checkState()==Qt::Checked) ? true : false;
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
	foreach ( SkyComponent *sc, ksw->data()->skyComposite()->customCatalogs() ) {
		CustomCatalogComponent *cc = (CustomCatalogComponent*)sc;
		if ( CatalogList->currentItem()->text() == cc->name() ) {
			RemoveCatalog->setEnabled( true );
			break;
		}
	}
}

void OpsCatalog::slotAddCatalog() {
	AddCatDialog ac( ksw );
	if ( ac.exec()==QDialog::Accepted ) 
		insertCatalog( ac.filename() );
}

void OpsCatalog::slotLoadCatalog() {
	//Get the filename from the user
	QString filename = KFileDialog::getOpenFileName( QDir::homePath(), "*");
	if ( ! filename.isEmpty() ) {
		//test integrity of file before trying to add it
		CustomCatalogComponent newCat( 0, filename, true, Options::showOther );
		newCat.init( ksw->data() );
		if ( newCat.objectList().size() )
			insertCatalog( filename );
	}
}

void OpsCatalog::insertCatalog( const QString &filename ) {
	//Get the new catalog's name, add entry to the listbox
	QString name = getCatalogName( filename );

	QListWidgetItem *newItem = new QListWidgetItem( name, CatalogList );
	newItem->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
	newItem->setCheckState( Qt::Checked );

	//update Options object
	QStringList tFileList = Options::catalogFile();
	QList<int> tShowList = Options::showCatalog();
	tFileList.append( filename );
	tShowList.append( true );
	Options::setCatalogFile( tFileList );
	Options::setShowCatalog( tShowList );
	
	//FIXME: Can't use Options::showCatalog[tShowList.size()-1] as visibility fcn, 
	//because it returns QList rather than bool
	//Add new custom catalog, based on the list of SkyObjects we just parsed
	ksw->data()->skyComposite()->addCustomCatalog( filename, Options::showOther );

	ksw->map()->forceUpdate();
}

void OpsCatalog::slotRemoveCatalog() {
	//Remove CatalogName, CatalogFile, and ShowCatalog entries, and decrement CatalogCount
	foreach ( SkyComponent *sc, ksw->data()->skyComposite()->customCatalogs() ) {
		int i = ksw->data()->skyComposite()->customCatalogs().indexOf( sc );
		CustomCatalogComponent *cc = (CustomCatalogComponent*)sc;
		QString name = cc->name();

		if ( CatalogList->currentItem()->text() == name ) {
			ksw->data()->skyComposite()->removeCustomCatalog( name );

			//Update Options object
			QStringList tFileList = Options::catalogFile();
			QList<int> tShowList = Options::showCatalog();
			tFileList.remove( tFileList[i] );
			tShowList.remove( tShowList[i] );
			Options::setCatalogFile( tFileList );
			Options::setShowCatalog( tShowList );
			break;
		}
	}

	//Remove entry in the QListView
	CatalogList->takeItem( CatalogList->row( CatalogList->currentItem() ) );

	ksw->map()->forceUpdate();
}

void OpsCatalog::slotSetDrawStarMagnitude(double newValue) {
	kcfg_MagLimitDrawStarZoomOut->setMaximum( newValue );
	ksw->data()->skyComposite()->setFaintStarMagnitude( newValue );
}

void OpsCatalog::slotSetDrawStarZoomOutMagnitude(double newValue) {
	kcfg_MagLimitDrawStar->setMinimum( newValue );
	Options::setMagLimitDrawStarZoomOut( newValue );
	// force redraw
	ksw->map()->forceUpdate();
}

void OpsCatalog::slotStarWidgets(bool on) {
	LabelMagStars->setEnabled(on);
	LabelMagStarsZoomOut->setEnabled(on);
	LabelMagStarInfo->setEnabled(on);
	LabelMag1->setEnabled(on);
	LabelMag2->setEnabled(on);
	LabelMag3->setEnabled(on);
	kcfg_MagLimitDrawStar->setEnabled(on);
	kcfg_MagLimitDrawStarZoomOut->setEnabled(on);
	kcfg_MagLimitDrawStarInfo->setEnabled(on);
	kcfg_ShowStarNames->setEnabled(on);
	kcfg_ShowStarMagnitudes->setEnabled(on);
}

QString OpsCatalog::getCatalogName( const QString &filename ) {
	QString name = QString();
	QFile f( filename );

	if ( f.open( QIODevice::ReadOnly ) ) {
		QTextStream stream( &f );
		while ( ! stream.atEnd() ) {
			QString line = stream.readLine();
			if ( line.indexOf( "# Name: " ) == 0 ) {
				name = line.mid( line.indexOf(":")+2 );
				break;
			}
		}
	}

	return name;  //no name was parsed
}

#include "opscatalog.moc"

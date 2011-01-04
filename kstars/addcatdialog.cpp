/***************************************************************************
                          addcatdialog.cpp  -  description
                             -------------------
    begin                : Sun Mar 3 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <tqlabel.h>
#include <tqlayout.h>
#include <kcolorbutton.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <knuminput.h>
#include <ktempfile.h>
#include <kurl.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "customcatalog.h"

#include "addcatdialog.h"

AddCatDialog::AddCatDialog( TQWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Import Catalog" ), Help|Ok|Cancel, Ok, parent ) {

	TQFrame *page = plainPage();
	setMainWidget(page);
	TQDir::setCurrent( TQDir::homeDirPath() );

	vlay = new TQVBoxLayout( page, 0, spacingHint() );
	acd = new AddCatDialogUI(page);
	vlay->addWidget( acd );
	
	connect( acd->DataURL->lineEdit(), TQT_SIGNAL( lostFocus() ), this, TQT_SLOT( slotShowDataFile() ) );
	connect( acd->DataURL, TQT_SIGNAL( urlSelected( const TQString & ) ), 
			this, TQT_SLOT( slotShowDataFile() ) );
	connect( acd->PreviewButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotPreviewCatalog() ) );
	connect( this, TQT_SIGNAL( okClicked() ), this, TQT_SLOT( slotCreateCatalog() ) );

	acd->FieldList->insertItem( i18n( "ID Number" ) );
	acd->FieldList->insertItem( i18n( "Right Ascension" ) );
	acd->FieldList->insertItem( i18n( "Declination" ) );
	acd->FieldList->insertItem( i18n( "Object Type" ) );

	acd->FieldPool->insertItem( i18n( "Common Name" ) );
	acd->FieldPool->insertItem( i18n( "Magnitude" ) );
	acd->FieldPool->insertItem( i18n( "Major Axis" ) );
	acd->FieldPool->insertItem( i18n( "Minor Axis" ) );
	acd->FieldPool->insertItem( i18n( "Position Angle" ) );
	acd->FieldPool->insertItem( i18n( "Ignore" ) );
}

AddCatDialog::~AddCatDialog(){
}

void AddCatDialog::slotOk() {
//Overriding slotOk() so that custom data file can be validated before
//TQDialog::accept() is emitted and the window is closed.

//the validation code needs to be aware of AddCatDialog members, so I will just
//emit the okClicked() signal, which is connected to AddCatDialog::validateFile()
	emit okClicked();
}

void AddCatDialog::slotHelp() {
	TQString message = 
			i18n( "A valid custom catalog file has one line per object, "
						"with the following fields in each line:") + "\n\t" +
			i18n( "1. Type identifier.  Must be one of: 0 (star), 3 (open cluster), 4 (globular cluster), "
						"5 (gaseous nebula), 6 (planetary nebula), 7 (supernova remnant), or 8 (galaxy)" ) + "\n\t" +
			i18n( "2. Right Ascension (floating-point value)" ) + "\n\t" +
			i18n( "3. Declination (floating-point value)" ) + "\n\t" +
			i18n( "4. Magnitude (floating-point value)" ) + "\n\t" +
			i18n( "5. Spectral type (if type=0); otherwise object's catalog name" ) + "\n\t" +
			i18n( "6. Star name (if type=0); otherwise object's common name. [field 6 is optional]" ) + "\n\n" +
			
			i18n( "The fields should be separated by whitespace.  In addition, the catalog "
						"may contain comment lines beginning with \'#\'." );

	KMessageBox::information( 0, message, i18n( "Help on custom catalog file format" ) );
}

/* Attempt to parse the catalog data file specified in the DataURL box.
 * We assume the data file has space-separated fields, and that each line has 
 * the data fields listed in the Catalog fields list, in the same order.
 * Each data field is parsed as follows:
 *
 * ID number: integer value
 * Right Ascension: colon-delimited hh:mm:ss.s or floating-point value
 * Declination: colon-delimited dd:mm:ss.s or floating-point value
 * Object type: integer value, one of [ 0,1,2,3,4,5,6,7,8 ]
 * Common name: string value (if it tqcontains a space, it *must* be enclosed in quotes!)
 * Magnitude: floating-point value
 * Major axis: floating-point value (length of major axis in arcmin)
 * Minor axis: floating-point value (length of minor axis in arcmin)
 * Position angle: floating-point value (position angle, in degrees)
 */
bool AddCatDialog::validateDataFile() {
	KStars *ksw = (KStars*) tqtopLevelWidget()->parent(); 

	//Create the catalog file contents: first the header
	CatalogContents = writeCatalogHeader();

	//Next, the data lines (fill from user-specified file)
	TQFile dataFile( acd->DataURL->url() );
	if ( ! acd->DataURL->url().isEmpty() && dataFile.open( IO_ReadOnly ) ) {
		TQTextStream dataStream( &dataFile );
		CatalogContents += dataStream.read();

		dataFile.close();
	}

	//Now create a temporary file for the Catalog, and attempt to parse it into a CustomCatalog
	KTempFile ktf;
	TQFile tmpFile( ktf.name() );
	ktf.unlink(); //just need filename
	if ( tmpFile.open( IO_WriteOnly ) ) {
		TQTextStream ostream( &tmpFile );
		ostream << CatalogContents;
		tmpFile.close();
		CustomCatalog *newCat;

		newCat = ksw->data()->createCustomCatalog( tmpFile.name(), true ); // true = showerrs
		if ( newCat ) {
			int nObjects = newCat->objList().count();
			delete newCat;
			if ( nObjects ) return true;
		}
	}

	return false;
}

TQString AddCatDialog::writeCatalogHeader() {
	TQString name = ( acd->CatalogName->text().isEmpty() ? i18n("Custom") : acd->CatalogName->text() );
	TQString pre = ( acd->Prefix->text().isEmpty() ? "CC" : acd->Prefix->text() );

	TQString h = TQString("# Name: %1\n").arg( name );
	h += TQString("# Prefix: %1\n").arg( pre );
	h += TQString("# Color: %1\n").arg( acd->ColorButton->color().name() );
	h += TQString("# Epoch: %1\n").arg( acd->Epoch->value() );
	h += TQString("# ");

	for ( uint i=0; i < acd->FieldList->count(); ++i ) {
		TQString f = acd->FieldList->text( i );

		if ( f == i18n( "ID Number" ) ) {
			h += "ID  ";
		} else if ( f == i18n( "Right Ascension" ) ) {
			h += "RA  ";
		} else if ( f == i18n( "Declination" ) ) {
			h += "Dc  ";
		} else if ( f == i18n( "Object Type" ) ) {
			h += "Tp  ";
		} else if ( f == i18n( "Common Name" ) ) {
			h += "Nm  ";
		} else if ( f == i18n( "Magnitude" ) ) {
			h += "Mg  ";
		} else if ( f == i18n( "Major Axis" ) ) {
			h += "Mj  ";
		} else if ( f == i18n( "Minor Axis" ) ) {
			h += "Mn  ";
		} else if ( f == i18n( "Position Angle" ) ) {
			h += "PA  ";
		} else if ( f == i18n( "Ignore" ) ) {
			h += "Ig  ";
		}
	}

	h += "\n";

	return h;
}

void AddCatDialog::slotShowDataFile() {
	TQFile dataFile( acd->DataURL->url() );
	if ( ! acd->DataURL->url().isEmpty() && dataFile.open( IO_ReadOnly ) ) {
		acd->DataFileBox->clear();
		TQTextStream dataStream( &dataFile );
		acd->DataFileBox->insertStringList( TQStringList::split( "\n", dataStream.read(), TRUE ) );
		dataFile.close();
	}
}

void AddCatDialog::slotPreviewCatalog() {
	if ( validateDataFile() ) {
		KMessageBox::informationList( 0, i18n( "Preview of %1" ).arg( acd->CatalogName->text() ),
			TQStringList::split( "\n", CatalogContents ), i18n( "Catalog Preview" ) );
	}
}

void AddCatDialog::slotCreateCatalog() {
	if ( validateDataFile() ) {
		//CatalogContents now tqcontains the text for the catalog file,
		//and objList tqcontains the parsed objects

		//Warn user if file exists!
		if ( TQFile::exists( acd->CatalogURL->url() ) )
		{
			KURL u( acd->CatalogURL->url() );
			int r=KMessageBox::warningContinueCancel( 0,
									i18n( "A file named \"%1\" already exists. "
											"Overwrite it?" ).arg( u.fileName() ),
									i18n( "Overwrite File?" ),
									i18n( "&Overwrite" ) );
			
			if(r==KMessageBox::Cancel) return;
		}

		TQFile OutFile( acd->CatalogURL->url() );
		if ( ! OutFile.open( IO_WriteOnly ) ) {
			KMessageBox::sorry( 0, 
				i18n( "Could not open the file %1 for writing." ).arg( acd->CatalogURL->url() ), 
				i18n( "Error Opening Output File" ) );
		} else {
			TQTextStream outStream( &OutFile );
			outStream << CatalogContents;
			OutFile.close();

			emit TQDialog::accept();
			close();
		}
	}
}

#include "addcatdialog.moc"

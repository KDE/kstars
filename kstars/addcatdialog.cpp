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

#include <qlabel.h>
#include <qlayout.h>
//Added by qt3to4:
#include <QVBoxLayout>
#include <QFrame>
#include <QTextStream>
#include <kcolorbutton.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <knuminput.h>
#include <ktemporaryfile.h>
#include <kurl.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skycomponents/customcatalogcomponent.h"

#include "addcatdialog.h"

AddCatDialogUI::AddCatDialogUI( QWidget *parent ) : QFrame( parent ) {
	setupUi(this);
}

AddCatDialog::AddCatDialog( KStars *_ks )
	: KDialog( ( QWidget* )_ks ),  ks( _ks )
{
	QDir::setCurrent( QDir::homePath() );
	acd = new AddCatDialogUI(this);
	setMainWidget(acd);
        setCaption( i18n( "Import Catalog" ) );
        setButtons( KDialog::Help|KDialog::Ok|KDialog::Cancel );

	connect( acd->DataURL->lineEdit(), SIGNAL( lostFocus() ), this, SLOT( slotShowDataFile() ) );
	connect( acd->DataURL, SIGNAL( urlSelected( const KUrl & ) ),
			this, SLOT( slotShowDataFile() ) );
	connect( acd->PreviewButton, SIGNAL( clicked() ), this, SLOT( slotPreviewCatalog() ) );
	connect( this, SIGNAL( okClicked() ), this, SLOT( slotCreateCatalog() ) );

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
	connect(this,SIGNAL(okClicked()),this,SLOT(slotOk()));
	connect(this,SIGNAL(cancelClicked()),this,SLOT(slotCancel()));
	connect(this,SIGNAL(helpClicked()),this,SLOT(slotHelp()));
}

AddCatDialog::~AddCatDialog(){
}

void AddCatDialog::slotOk() {
//Overriding slotOk() so that custom data file can be validated before
//QDialog::accept() is emitted and the window is closed.

//the validation code needs to be aware of AddCatDialog members, so I will just
//emit the okClicked() signal, which is connected to AddCatDialog::validateFile()
	emit okClicked();
}

void AddCatDialog::slotHelp() {
	QString message =
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
 * Common name: string value (if it contains a space, it *must* be enclosed in quotes!)
 * Magnitude: floating-point value
 * Major axis: floating-point value (length of major axis in arcmin)
 * Minor axis: floating-point value (length of minor axis in arcmin)
 * Position angle: floating-point value (position angle, in degrees)
 */
bool AddCatDialog::validateDataFile() {
	//Create the catalog file contents: first the header
	CatalogContents = writeCatalogHeader();

	//Next, the data lines (fill from user-specified file)
	QFile dataFile( acd->DataURL->url().path() );
	if ( ! acd->DataURL->url().isEmpty() && dataFile.open( QIODevice::ReadOnly ) ) {
		QTextStream dataStream( &dataFile );
		CatalogContents += dataStream.readAll();

		dataFile.close();
	}

	//Now create a temporary file for the Catalog, and attempt to parse it
	//into a temporary CustomCatalogComponent
	KTemporaryFile tmpFile;
	if ( tmpFile.open() ) {
		QTextStream ostream( &tmpFile );
		ostream << CatalogContents;
		ostream.flush();
		CustomCatalogComponent newCat( 0, tmpFile.fileName(), true, Options::showOther );
		newCat.init( ks->data() );

		int nObjects = newCat.objectList().size();
		if ( nObjects ) return true;
	}

	return false;
}

QString AddCatDialog::writeCatalogHeader() {
	QString name = ( acd->CatalogName->text().isEmpty() ? i18n("Custom") : acd->CatalogName->text() );
	QString pre = ( acd->Prefix->text().isEmpty() ? "CC" : acd->Prefix->text() );

	QString h = QString("# Name: %1\n").arg( name );
	h += QString("# Prefix: %1\n").arg( pre );
	h += QString("# Color: %1\n").arg( acd->ColorButton->color().name() );
	h += QString("# Epoch: %1\n").arg( acd->Epoch->value() );
	h += QString("# ");

	for ( uint i=0; i < acd->FieldList->count(); ++i ) {
		QString f = acd->FieldList->text( i );

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

	h += '\n';

	return h;
}

void AddCatDialog::slotShowDataFile() {
	QFile dataFile( acd->DataURL->url().path() );
	if ( ! acd->DataURL->url().isEmpty() && dataFile.open( QIODevice::ReadOnly ) ) {
		acd->DataFileBox->clear();
		QTextStream dataStream( &dataFile );
		acd->DataFileBox->insertStringList( dataStream.readAll().split( "\n" ) );
		dataFile.close();
	}
}

void AddCatDialog::slotPreviewCatalog() {
	if ( validateDataFile() ) {
		KMessageBox::informationList( 0, i18n( "Preview of %1", acd->CatalogName->text() ),
			CatalogContents.split( "\n" ), i18n( "Catalog Preview" ) );
	}
}

void AddCatDialog::slotCreateCatalog() {
	if ( validateDataFile() ) {
		//CatalogContents now contains the text for the catalog file,
		//and objList contains the parsed objects

		//Warn user if file exists!
		if ( QFile::exists( acd->CatalogURL->url().path() ) )
		{
			KUrl u( acd->CatalogURL->url() );
			int r=KMessageBox::warningContinueCancel( 0,
									i18n( "A file named \"%1\" already exists. "
											"Overwrite it?", u.fileName() ),
									i18n( "Overwrite File?" ),
									KStandardGuiItem::overwrite() );

			if(r==KMessageBox::Cancel) return;
		}

		QFile OutFile( acd->CatalogURL->url().path() );
		if ( ! OutFile.open( QIODevice::WriteOnly ) ) {
			KMessageBox::sorry( 0,
				i18n( "Could not open the file %1 for writing.", acd->CatalogURL->url().path() ),
				i18n( "Error Opening Output File" ) );
		} else {
			QTextStream outStream( &OutFile );
			outStream << CatalogContents;
			OutFile.close();

			emit QDialog::accept();
			close();
		}
	}
}

#include "addcatdialog.moc"

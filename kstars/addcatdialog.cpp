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
#include <qlayout.h>
#include <kdebug.h>
#include <kmessagebox.h>

#include "kstars.h"
#include "kstarsdata.h"
//#include "skyobject.h"
//#include "starobject.h"
#include "addcatdialog.h"

AddCatDialog::AddCatDialog( QWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Add Catalog" ), Help|Ok|Cancel, Ok, parent ) {

	QFrame *page = plainPage();
	setMainWidget(page);
	vlay = new QVBoxLayout( page, 0, spacingHint() );
	acd = new AddCatDialogUI(page);
	vlay->addWidget( acd );
	
	connect( acd->catFileName, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotCheckLineEdits() ) );
	connect( acd->catName, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotCheckLineEdits() ) );
	connect( this, SIGNAL( okClicked() ), this, SLOT( slotValidateFile() ) );
//	connect( catName, SIGNAL( returnPressed() ), this, SLOT( checkLineEdits() ) );

	enableButtonOK( false ); //disable until both lineedits are filled

	objList.setAutoDelete( false );
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

void AddCatDialog::slotValidateFile() {
//A Valid custom data file must satisfy the following conditions:
//1. Each line is either a comment (beginning with '#'), or a data line
//2. A data line consists of whitespace-delimited fields
//   a. the object type integer is the first field
//   b. the RA, Dec, and magnitude are the 2nd, 3rd and 4th fields (J2000 coords)
//   c. If type==0 (or 1) (star), the next fields are the spectral type and the name (if any)
//   d. If type==3-8, the next fields are the primary name and long name (if any)
//      the type cannot be 1 or 2 (redundant star category and planet)
//
//   Also, if names contain spaces, they should be enclosed in quotes so they
//   aren't split into multiple fields.

// I moved the file parse code to KStarsData, so we can read in custom
// data files without needing an AddCatDialog (on startup, for example).
// The bool argument below flags whether the detailed warning messagebox
// should appear when parse errors are found.
	KStars *ksw = (KStars*) parent()->parent(); // ViewOpsDialog->KStars
	bool result = ksw->data()->readCustomData( filename(), objList, true );

	if ( result ) {
		emit QDialog::accept();
		close();
	}
}

void AddCatDialog::slotCheckLineEdits() {
    enableButtonOK(! acd->catFileName->lineEdit()->text().isEmpty() && ! acd->catName->text().isEmpty());
}

#include "addcatdialog.moc"

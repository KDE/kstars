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
#include <qfile.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qtextstream.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <klocale.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <kurlcompletion.h>

#include "kstars.h"
#include "skyobject.h"
#include "starobject.h"
#include "addcatdialog.h"
#include "addcatdialog.moc"

AddCatDialog::AddCatDialog( QWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Add Catalog" ), Ok|Cancel, Ok, parent ) {

	QFrame *page = plainPage();
	vlay = new QVBoxLayout( page, 2, 2 );
	hlay = new QHBoxLayout();

	QLabel *catFileLabel = new QLabel( i18n( "Enter filename of custom catalog:" ), page );
	QLabel *catNameLabel = new QLabel( i18n( "Enter name for this catalog:" ), page );

	catFileName = new KLineEdit( "", page );
	catFileName->setMinimumWidth( 255 );
	KURLCompletion *comp = new KURLCompletion();
	catFileName->setCompletionObject( comp );

	browseForFile = new QPushButton( i18n( "Browse..." ), page );

	catName = new KLineEdit( "", page );

	hlay->addWidget( catFileName );
	hlay->addWidget( browseForFile );

	vlay->addWidget( catFileLabel );
	vlay->addLayout( hlay );
	vlay->addSpacing( 20 );
	vlay->addWidget( catNameLabel );
	vlay->addWidget( catName );

	connect( browseForFile, SIGNAL( clicked() ), this, SLOT( findFile() ) );
	connect( catFileName, SIGNAL( textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( catName, SIGNAL( textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( this, SIGNAL( okClicked() ), this, SLOT( validateFile() ) );
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

void AddCatDialog::validateFile() {
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
	KStars *ksw = (KStars*)kapp->mainWidget();
	bool result = ksw->data()->readCustomData( filename(), objList, true );

	if ( result ) {
		emit QDialog::accept();
		close();
	}
}

void AddCatDialog::findFile() {
	QString fname = KFileDialog::getOpenFileName();
	if ( !fname.isEmpty() ) catFileName->setText( fname );
}

void AddCatDialog::checkLineEdits() {
	if ( catFileName->text().length() > 0 && catName->text().length() > 0 )
		enableButtonOK( true );
	else
		enableButtonOK( false );
}

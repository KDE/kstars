/***************************************************************************
                          focusdialog.cpp  -  description
                             -------------------
    begin                : Sat Mar 23 2002
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

#include <qframe.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qregexp.h>
#include <qstringlist.h>

#include <kdebug.h>
#include <klineedit.h>
#include <klocale.h>
#include <kmessagebox.h>

#include "dms.h"
#include "skypoint.h"
#include "focusdialog.h"

FocusDialog::FocusDialog( QWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Set Focus Manually" ), Ok|Cancel, Ok, parent ) {

	Point = 0; //initialize pointer to null

	QFrame *page = plainPage();
	vlay = new QVBoxLayout( page, 2, 2 );
	hlayRA = new QHBoxLayout();
	hlayDec = new QHBoxLayout();

	QLabel *RALabel = new QLabel( i18n( "the new Right ascension", "New RA:" ), page );
	QLabel *DecLabel = new QLabel( i18n( "the new Declination", "New Dec:" ), page );

	editRA  = new KLineEdit( "", page );
	editRA->setFocus();
	editDec = new KLineEdit( "", page );

	hlayRA->addWidget( RALabel );
	hlayRA->addWidget( editRA );
	hlayDec->addWidget( DecLabel );
	hlayDec->addWidget( editDec );

	vlay->addLayout( hlayRA );
	vlay->addLayout( hlayDec );

	connect( editRA, SIGNAL( textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( editDec, SIGNAL( textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( this, SIGNAL( okClicked() ), this, SLOT( validatePoint() ) );

	enableButtonOK( false ); //disable until both lineedits are filled
}

FocusDialog::~FocusDialog(){
}

void FocusDialog::slotOk() {
//Overriding slotOk() so that custom data file can be validated before
//QDialog::accept() is emitted and the window is closed.

//the validation code needs to be aware of AddCatDialog members, so I will just
//emit the okClicked() signal, which is connected to AddCatDialog::validateFile()
	emit okClicked();
}

void FocusDialog::checkLineEdits() {
	if ( editRA->text().length() > 0 && editDec->text().length() > 0 )
		enableButtonOK( true );
	else
		enableButtonOK( false );
}

void FocusDialog::validatePoint( void ) {
	//There are several ways to specify the RA and Dec:
	//(we always assume RA is in hours or h,m,s and Dec is in degrees or d,m,s)
	//1. Integer numbers  ( 5;  -33 )
	//2. Floating-point numbers  ( 5.0; -33.0 )
	//3. colon-delimited integers ( 5:0:0; -33:0:0 )
	//4. colon-delimited with float seconds ( 5:0:0.0; -33:0:0.0 )
	//5. colon-delimited with float minutes ( 5:0.0; -33:0.0 )
	//6. space-delimited ( 5 0 0; -33 0 0 ) or ( 5 0.0 -33 0.0 )
	//7. space-delimited, with unit labels ( 5h 0m 0s, -33d 0m 0s )

	QString errMsg = i18n( "Could not parse %1 entry. Specify a value " ) +
						i18n( "as a simple integer, a floating-point number, or as a triplet " ) +
						i18n( "of values using colons or spaces as separators." );

	QString entry;
	bool valueFound[2], badEntry( false ), checkValue( false );
	int d(0), m(0), s(0);
	dms RA, Dec;

	//Parse RA/Dec string:
	for ( unsigned int i=0; i<2; ++i ) { //loop twice for RA and Dec
		valueFound[i] = false;

		if ( i==0 ) entry = editRA->text().stripWhiteSpace();
		else entry = editDec->text().stripWhiteSpace();

		//Try simplest cases: integer or double representation
		d = entry.toInt( &checkValue );
		if ( checkValue ) {
			if ( i==0 ) RA.setH( d, 0, 0 );
			else Dec.setD( d, 0, 0 );
			valueFound[i] = true;
		} else {
			double x = entry.toDouble( &checkValue );
			if ( checkValue ) {
				if ( i==0 ) RA.setH( x );
				else Dec.setD( x );
				valueFound[i] = true;
			}
		}

		if ( !valueFound[i] ) { //no success yet...try assuming multiple fields
			QStringList fields;
			//check for colon-delimiters or space-delimiters
			if ( entry.contains(':') ) fields = QStringList::split( ':', entry );
			else fields = QStringList::split( " ", entry ); //probably space-delimited

			if ( fields.count() == 2 ) { //we will add a third field, and then parse with the 3-field code block
				//If field[1] is a double, convert it to integer arcmin, and convert the remainder to arcsec
				double mx = fields[1].toDouble( &checkValue );
				if ( checkValue ) {
					fields[1] = QString("%1").arg( int(mx) );
					fields.append( QString("%1").arg( int( 60.0*(mx - int(mx)) ) ) );
				} else { //assume for now that field[1] is an integer...append a third integer, 0.
					fields.append( QString( "0" ) );
				}
			}

			if ( fields.count() >= 3 ) { //space-delimited ( h/d m s ); ignore all after 3rd field
				fields[0].replace( QRegExp("h"), "" );
				fields[0].replace( QRegExp("d"), "" );
				fields[1].replace( QRegExp("m"), "" );
				fields[2].replace( QRegExp("s"), "" );

				//See if all three fields parse as integers
				d = fields[0].toInt( &checkValue );
				if ( !checkValue ) badEntry = true;
				m = fields[1].toInt( &checkValue );
				if ( !checkValue ) badEntry = true;
				s = fields[2].toInt( &checkValue );
				if ( !checkValue ) { //it's possible that seconds are a double
					double x = fields[2].toInt( &checkValue );
					if ( checkValue ) s = int( x );
					else badEntry = true;
				}

				if ( !badEntry ) {
					if ( i==0 ) RA.setH( d, m, s );
					else Dec.setD( d, m, s );
					valueFound[i] = true;
				}
			}
		}
	}

	if ( !valueFound[0] )
		KMessageBox::sorry( 0, errMsg.arg( i18n("short for Right Ascension", "RA") ), i18n( "Could not set RA Value" ) );

	if ( !valueFound[1] )
		KMessageBox::sorry( 0, errMsg.arg( i18n("short for Declination", "Dec") ), i18n( "Could not set Dec Value" ) );

	if ( valueFound[0] && valueFound[1] ) {
		//Check to make sure entered values are in bounds.
		QString warnRAMess = i18n( "The RA value you entered is not between 0 and 24 hours. " )
						+ i18n( "Would you like me to wrap the value?" );
		QString warnDecMess = i18n( "The Dec value you entered is not between -90 and +90 degrees. " )
						+ i18n( "Please enter a new value." );

		kdDebug() << "RA, Dec: " << RA.Hours() << ", " << Dec.Degrees() << endl;

		//can't check using RA.Hours() because this automatically calls reduce().
		if ( RA.Degrees() < 0.0 || RA.Degrees() > 360.0 ) {
			if ( KMessageBox::warningYesNo( 0,
						warnRAMess, i18n( "RA out-of-bounds" ) )==KMessageBox::No ) {
				editRA->clear();
				editRA->setFocus();
				return;
			}
		}

		if ( Dec.Degrees() < -90.0 || Dec.Degrees() > 90.0 ) {
			KMessageBox::sorry( 0,
					warnDecMess, i18n( "Dec out-of-bounds" ) );
			editDec->clear();
			if ( ! editRA->text().isEmpty() ) editDec->setFocus();
			return;
		}

		Point = new SkyPoint( RA, Dec );
		emit QDialog::accept();
		close();
	}
}

#include "focusdialog.moc"

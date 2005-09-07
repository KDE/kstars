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

#include <qtabwidget.h>
#include <qlayout.h>

#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>

#include <qstring.h>
#include <knumvalidator.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "dms.h"
#include "skypoint.h"
#include "dmsbox.h"
#include "focusdialog.h"

FocusDialog::FocusDialog( QWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Set Focus Manually" ), Ok|Cancel, Ok, parent ) {

	Point = 0; //initialize pointer to null
	UsedAltAz = false; //assume RA/Dec by default

	QFrame *page = plainPage();
	setMainWidget(page);
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );
	fdlg = new FocusDialogDlg(page);
	fdlg->epochName->setValidator( new KDoubleValidator( fdlg->epochName ) );
	vlay->addWidget( fdlg );
	
	connect( fdlg->raBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( fdlg->decBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( fdlg->azBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( fdlg->altBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( this, SIGNAL( okClicked() ), this, SLOT( validatePoint() ) );

	fdlg->raBox->setDegType(false); //RA box should be HMS-style
	fdlg->raBox->setFocus(); //set input focus
	enableButtonOK( false ); //disable until both lineedits are filled
}

FocusDialog::~FocusDialog(){
}

void FocusDialog::checkLineEdits() {
	bool raOk(false), decOk(false), azOk(false), altOk(false);
	fdlg->raBox->createDms( false, &raOk );
	fdlg->decBox->createDms( true, &decOk );
	fdlg->azBox->createDms( true, &azOk );
	fdlg->altBox->createDms( true, &altOk );
	if ( ( raOk && decOk ) || ( azOk && altOk ) )
		enableButtonOK( true );
	else
		enableButtonOK( false );
}

void FocusDialog::slotOk() {
	emit okClicked();
}

void FocusDialog::validatePoint() {
	bool raOk(false), decOk(false), azOk(false), altOk(false);
	dms ra( fdlg->raBox->createDms( false, &raOk ) ); //false means expressed in hours
	dms dec( fdlg->decBox->createDms( true, &decOk ) );
	QString message;

	KStars *ks = (KStars*) parent();

	if ( raOk && decOk ) {
		//make sure values are in valid range
		if ( ra.Hours() < 0.0 || ra.Hours() > 24.0 )
			message = i18n( "The Right Ascension value must be between 0.0 and 24.0." );
		if ( dec.Degrees() < -90.0 || dec.Degrees() > 90.0 )
			message += "\n" + i18n( "The Declination value must be between -90.0 and 90.0." );
		if ( ! message.isEmpty() ) {
			KMessageBox::sorry( 0, message, i18n( "Invalid Coordinate Data" ) );
			return;
		}

		Point = new SkyPoint( ra, dec );
		double epoch0 = getEpoch( fdlg->epochName->text() );
		long double jd0 = epochToJd ( epoch0 );
		Point->apparentCoord(jd0, ks->data()->ut().djd() );

		QDialog::accept();
	} else {
		dms az(  fdlg->azBox->createDms( true, &azOk ) );
		dms alt( fdlg->altBox->createDms( true, &altOk ) );

		if ( azOk && altOk ) {
			//make sure values are in valid range
			if ( az.Degrees() < 0.0 || az.Degrees() > 360.0 )
				message = i18n( "The Azimuth value must be between 0.0 and 360.0." );
			if ( alt.Degrees() < -90.0 || alt.Degrees() > 90.0 )
				message += "\n" + i18n( "The Altitude value must be between -90.0 and 90.0." );
			if ( ! message.isEmpty() ) {
				KMessageBox::sorry( 0, message, i18n( "Invalid Coordinate Data" ) );
				return;
			}

			Point = new SkyPoint();
			Point->setAz( az );
			Point->setAlt( alt );
			UsedAltAz = true;

			QDialog::accept();
		} else {
			QDialog::reject();
		}
	}
}

double FocusDialog::getEpoch (QString eName) {
	//If eName is empty (or not a number) assume 2000.0
	bool ok(false);
	double epoch = eName.toDouble( &ok );
	if ( eName.isEmpty() || ! ok )
		return 2000.0;

	return epoch;
}

long double FocusDialog::epochToJd (double epoch) {

	double yearsTo2000 = 2000.0 - epoch;

	if (epoch == 1950.0) {
		return 2433282.4235;
	} else if ( epoch == 2000.0 ) {
		return J2000;
	} else {
		return ( J2000 - yearsTo2000 * 365.2425 );
	}

}


QSize FocusDialog::sizeHint() const
{
  return QSize(240,210);
}

void FocusDialog::activateAzAltPage() {
	fdlg->fdTab->showPage( fdlg->aaTab );
	fdlg->azBox->setFocus();
}
#include "focusdialog.moc"

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

#include <kdebug.h>
#include <klocale.h>

#include "dms.h"
#include "skypoint.h"
#include "dmsbox.h"
#include "focusdialog.h"

FocusDialog::FocusDialog( QWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Set Focus Manually" ), Ok|Cancel, Ok, parent ) {

	Point = 0; //initialize pointer to null
	UsedAltAz = false; //assume RA/Dec by default
	
	fdlg = new FocusDialogDlg(this);
	setMainWidget(fdlg);
	this->show();

	connect( fdlg->raBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( fdlg->decBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( fdlg->azBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( fdlg->altBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
	connect( this, SIGNAL( okClicked() ), this, SLOT( validatePoint() ) );

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

void FocusDialog::validatePoint( void ) {
	bool raOk(false), decOk(false), azOk(false), altOk(false);
	dms ra( fdlg->raBox->createDms( false, &raOk ) ); //false means expressed in hours
	dms dec( fdlg->decBox->createDms( true, &decOk ) );
	
	if ( raOk && decOk ) { 
		Point = new SkyPoint( ra, dec );
		emit QDialog::accept();
	} else {
		dms az(  fdlg->azBox->createDms( true, &azOk ) );
		dms alt( fdlg->altBox->createDms( true, &altOk ) );
	
		if ( azOk && altOk ) {
			Point = new SkyPoint();
			Point->setAz( az );
			Point->setAlt( alt );
			UsedAltAz = true;
			emit QDialog::accept();
		}
	}
	
	close();
}

QSize FocusDialog::sizeHint() const
{
  return QSize(240,170);
}

void FocusDialog::activateAzAltPage() {
	fdlg->fdTab->showPage( fdlg->aaTab );
}
#include "focusdialog.moc"

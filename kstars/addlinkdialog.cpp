/***************************************************************************
                          addlinkdialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Oct 21 2001
    copyright            : (C) 2001 by Jason Harris
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

#include <klineedit.h>
#include <klocale.h>
#include <kurl.h>
#include <kmessagebox.h>

#include <qlayout.h>
#include <qlabel.h>
#include <qframe.h>
#include <qstring.h>
#include <qbuttongroup.h>
#include <qpushbutton.h>

#include "skymap.h"
#include "addlinkdialog.h"

#if (KDE_VERSION <= 222)
#include <kapp.h>
#else
#include <kapplication.h>
#endif

AddLinkDialog::AddLinkDialog( QWidget *parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Add Custom URL" ), Ok|Cancel, Ok, parent ) {

	SkyMap *map = (SkyMap *)parent;
	QFrame *page = plainPage();

//Create widgets and layout managers
	vlay = new QVBoxLayout( page, 2, 2 );
	hlayTitle = new QHBoxLayout();
	hlayURL = new QHBoxLayout();
	hlayBrowse = new QHBoxLayout();

	TypeGroup = new QButtonGroup( 1, Qt::Vertical, i18n( "Resource Type" ), page );
	ImageRadio = new QRadioButton( i18n( "Image" ), TypeGroup );
	ImageRadio->setChecked( true );
	InfoRadio = new QRadioButton( i18n( "Information" ), TypeGroup );
	InfoRadio->setChecked( false );

	QLabel *titleLabel = new QLabel( page, "TitleLabel" );
	titleLabel->setText( i18n( "Menu Text: " ) );
	TitleEntry = new KLineEdit( i18n( "Show image of " ) + map->clickedObject()->name(), page );

	QLabel *URLLabel = new QLabel( page, "URLLabel" );
	URLLabel->setText( i18n( "URL: " ) );
	URLEntry = new KLineEdit( "http://", page );
	URLEntry->setMinimumWidth( 250 );

	BrowserButton = new QPushButton( i18n( "Check URL" ), page );

	QSpacerItem *spacer = new QSpacerItem( 10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum );
	QSpacerItem *vspacer = new QSpacerItem( 10, 25, QSizePolicy::Minimum, QSizePolicy::Expanding );

	//pack widgets into layout managers
	hlayTitle->addWidget( titleLabel );
	hlayTitle->addWidget( TitleEntry );
	hlayURL->addWidget( URLLabel );
	hlayURL->addWidget( URLEntry );
	hlayBrowse->addItem( spacer );
	hlayBrowse->addWidget( BrowserButton );

	vlay->addWidget( TypeGroup );
	vlay->addLayout( hlayTitle );
	vlay->addLayout( hlayURL );
	vlay->addLayout( hlayBrowse );
	vlay->addItem( vspacer );
	vlay->activate();

//connect signals to slots
	connect( BrowserButton, SIGNAL( clicked() ), this, SLOT( checkURL() ) );
	connect( TypeGroup, SIGNAL( clicked( int ) ), this, SLOT( changeDefaultTitle( int ) ) );
}

void AddLinkDialog::checkURL( void ) {
	KURL url ( URLEntry->text() );
	if (url.isValid()) {   //Is the string a valid URL?
		kapp->invokeBrowser( url.url() );   //If so, launch the browser to see if it's the correct document
	} else {   //If not, print a warning message box that offers to open the browser to a search engine.
		QString message = i18n( "The URL is not valid. Would you like me to open a browser window\nto the Google search engine?" );
		QString caption = i18n( "Invalid URL" );
		if ( KMessageBox::warningYesNo( 0, message, caption )==KMessageBox::Yes ) {
			kapp->invokeBrowser( "http://www.google.com" );
		}
	}
}

void AddLinkDialog::changeDefaultTitle( int id ) {
	SkyMap *map = (SkyMap *)parent();

//If the user hasn't changed the default title text, but the link type (image/webpage)
//has been toggled, update the default title text
	if ( id==1 && TitleEntry->text().startsWith( i18n( "Show image of " ) ) ) {
		TitleEntry->setText( i18n( "Show webpage about " ) + map->clickedObject()->name() );
	}

	if ( id==0 && TitleEntry->text().startsWith( i18n( "Show webpage about " ) ) ) {
		TitleEntry->setText( i18n( "Show image of " ) + map->clickedObject()->name() );
	}
}

#include "addlinkdialog.moc"

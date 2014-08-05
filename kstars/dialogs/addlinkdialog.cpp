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

#include "addlinkdialog.h"

#include <QPushButton>
#include <QUrl>

#include <KMessageBox>
#include <KToolInvocation>

#include "skyobjects/skyobject.h"

AddLinkDialogUI::AddLinkDialogUI( QWidget *parent ) : QFrame( parent ) {
    setupUi(this);
}

AddLinkDialog::AddLinkDialog( QWidget *parent, const QString &oname )
        : QDialog( parent ),  ObjectName( oname )
{
    ald = new AddLinkDialogUI(this);

    setWindowTitle( xi18n( "Add Custom URL to %1", oname ) );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ald);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    //connect signals to slots
    connect( ald->URLButton, SIGNAL( clicked() ), this, SLOT( checkURL() ) );
    connect( ald->TypeBox, SIGNAL( clicked( int ) ), this, SLOT( changeDefaultDescription( int ) ) );

    ald->ImageRadio->setChecked(true);
    ald->DescBox->setText( xi18n( "Show image of " ) + ObjectName );
}

void AddLinkDialog::checkURL( void ) {
    QUrl _url ( url() );
    if ( _url.isValid() ) {   //Is the string a valid URL?
        KToolInvocation::invokeBrowser( _url.url() );   //If so, launch the browser to see if it's the correct document
    } else {   //If not, print a warning message box that offers to open the browser to a search engine.
        QString message = xi18n( "The URL is not valid. Would you like to open a browser window\nto the Google search engine?" );
        QString caption = xi18n( "Invalid URL" );
        if ( KMessageBox::warningYesNo( 0, message, caption, KGuiItem(xi18n("Browse Google")), KGuiItem(xi18n("Do Not Browse")) )==KMessageBox::Yes ) {
            KToolInvocation::invokeBrowser( "http://www.google.com" );
        }
    }
}

void AddLinkDialog::changeDefaultDescription( int id ) {
    //If the user hasn't changed the default desc text, but the link type (image/webpage)
    //has been toggled, update the default desc text
    if ( id==1 && desc().startsWith( xi18n( "Show image of " ) ) ) {
        ald->DescBox->setText( xi18n( "Show webpage about " ) + ObjectName );
    }

    if ( id==0 && desc().startsWith( xi18n( "Show webpage about " ) ) ) {
        ald->DescBox->setText( xi18n( "Show image of " ) + ObjectName );
    }
}



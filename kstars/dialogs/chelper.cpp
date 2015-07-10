/***************************************************************************
                          CHelper.cpp  -  description
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

#include "chelper.h"

#include <QVBoxLayout>
#include <QDebug>
#include <KLocalizedString>
#include <KMessageBox>

#include "skyqpainter.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "dms.h"
#include "skyobjects/skypoint.h"
#include "skymap.h"

CHelperUI::CHelperUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

CHelper::CHelper()
        : QDialog( KStars::Instance() )
{
    //initialize point to the current focus position


    fd = new CHelperUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(fd);
    setLayout(mainLayout);

    setWindowTitle( xi18n( "Set Constellation Parameters" ) );

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(validatePoint()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    fd->midRA->setDegType(false); //RA box should be HMS-style
    fd->midRA->setFocus(); //set input focus
}

CHelper::~CHelper(){
}

void CHelper::validatePoint() {
    bool raOk(false), decOk(false), paOK(false), widthOK(false), heightOK(false);
    dms ra( fd->midRA->createDms( false, &raOk ) ); //false means expressed in hours
    dms dec( fd->midDE->createDms( true, &decOk ) );
    dms pa( fd->PA->createDms( true, &paOK ) );
    dms scaleW( fd->scaleW->createDms( true, &widthOK ) );
    dms scaleH( fd->scaleH->createDms( true, &heightOK ) );

    if ( raOk && decOk && paOK && widthOK && heightOK )
    {
        SkyQPainter::setCHelper(ra, dec, pa, scaleW, scaleH);


    } else
    {
         qDebug() << " raOK " << raOk << " DE ok " << decOk << "PA OK " << paOK << " width OK " << widthOK << " heightOK " << heightOK;
          return;
    }

}



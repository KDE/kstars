/***************************************************************************
                          kswizard.cpp  -  description
                             -------------------
    begin                : Wed 28 Jan 2004
    copyright            : (C) 2004 by Jason Harris
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
#include <qpixmap.h>
#include <qlabel.h>

#include "ksutils.h"
#include "kswizardui.h"
#include "kswizard.h"

KSWizard::KSWizard( QWidget *parent, const char *name )
 : KSWizardUI( parent, name )
{
	//Load images into banner frames.
	QFile imFile;
	QPixmap im = QPixmap();
	
	if ( KSUtils::openDataFile( imFile, "wzstars.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	Banner1->setPixmap( im );
	
	if ( KSUtils::openDataFile( imFile, "wzgeo.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	Banner2->setPixmap( im );
	
	if ( KSUtils::openDataFile( imFile, "wzscope.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	Banner3->setPixmap( im );
}


KSWizard::~KSWizard()
{
}



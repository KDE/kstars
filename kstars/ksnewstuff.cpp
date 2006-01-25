/***************************************************************************
                          ksnewstuff.cpp  -  description
                             -------------------
    begin                : Wed 21 May 2004
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

#include <kdeversion.h>

#include <kapplication.h>
#include <kaction.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdirwatch.h>
#include <kprogressdialog.h>
#include <ktar.h>
#include <qdir.h>
#include <qcursor.h>
#include <qregexp.h>

#include "ksnewstuff.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "deepskyobject.h"
#include "skymap.h"

KSNewStuff::KSNewStuff( QWidget *parent ) :
  QObject(), KNewStuff( "kstars", parent ), NGCUpdated( false )
{
	ks = (KStars*)parent;
	kdw = new KDirWatch( this );
	kdw->addDir( KGlobal::dirs()->saveLocation("data", kapp->instanceName(), true) );
}

bool KSNewStuff::install( const QString &fileName )
{
	kdDebug() << "KSNewStuff::install(): " << fileName << endl;
	
	KTar archive( fileName );
	if ( !archive.open( QIODevice::ReadOnly ) )
			return false;
	
	const KArchiveDirectory *archiveDir = archive.directory();
	const QString destDir = KGlobal::dirs()->saveLocation("data", kapp->instanceName(), true);      
	KStandardDirs::makeDir( destDir );

	//monitor destDir for changes; inform updateData when files are created.
	connect( kdw, SIGNAL( dirty( const QString & ) ), this, SLOT( updateData( const QString & ) ) );

	archiveDir->copyTo(destDir);
	archive.close();
	kapp->processEvents();
	
	//read the new data into the program
	//this return might be the result of checking if everything is installed ok
	return true;
}

void KSNewStuff::updateData( const QString &path ) {
	QDir qd( path );
	qd.setSorting( QDir::Time );
	qd.setFilter( QDir::Files );

	//Show the Wait cursor
	ks->setCursor(QCursor(Qt::WaitCursor));
	
	
	//Handle the Steinicke NGC/IC catalog
	if ( !NGCUpdated && qd[0].contains( "ngcic" ) ) {
		//Build a progress dialog to show during data installation.
		KProgressDialog prog( 0, i18n( "Please Wait" ), 
			i18n( "Installing Steinicke NGC/IC catalog..." ), false /*modal*/ );
		prog.setAllowCancel( false );
		prog.setMinimumDuration( 0 /*millisec*/ );
		prog.progressBar()->setMinimum( 0 );  
		prog.progressBar()->setMaximum( 0 );  //show generic progress activity
		prog.show();
		kapp->processEvents();
		
		ks->data()->skyComposite()->reloadDeepSky( ks->data() );

		//Avoid redundant installs
		NGCUpdated = true;

		//Re-assign image/info links.  3rd param means deep-sky objects only
		ks->data()->readURLData( "image_url.dat", 0, true ); 
		ks->data()->readURLData( "info_url.dat", 1, true ); 
		
		ks->data()->setFullTimeUpdate();
		ks->data()->updateTime( ks->geo(), ks->map() );
		ks->map()->forceUpdate();
	}
	
	//Handle the inline Messier images
	// **No action required**
	
	//Handle the ephemerides
	if ( qd[0] == "asteroids.dat" || qd[0] == "comets.dat" ) {
		//Build a progress dialog to show during data installation.
		KProgressDialog prog( 0, i18n( "Please Wait" ), 
				i18n( "Installing comet and asteroid ephemerides..." ), true /*modal*/ );
		prog.setAllowCancel( false );
		prog.setMinimumDuration( 50 /*millisec*/ );
		prog.progressBar()->setMinimum( 0 );
		prog.progressBar()->setMaximum( 0 );  //generic progress activity
		
		ks->data()->skyComposite()->reloadAsteroids( ks->data() );
		ks->data()->skyComposite()->reloadComets( ks->data() );

		//Do a full update
		ks->data()->setFullTimeUpdate();
		ks->data()->updateTime( ks->geo(), ks->map() );
		ks->map()->forceUpdate();
	}

	//Restore arrow cursor
	ks->setCursor(QCursor(Qt::ArrowCursor));
}

void KSNewStuff::slotProcessEvents() { kapp->processEvents(); }

#include "ksnewstuff.moc"


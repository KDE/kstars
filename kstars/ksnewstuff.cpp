#include <kapplication.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdirwatch.h>
#include <ktar.h>
#include <qdir.h>
#include <kaction.h>

#include "ksnewstuff.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "deepskyobject.h"
#include "objectnamelist.h"
#include "skymap.h"

KSNewStuff::KSNewStuff( QWidget *parent ) :
  QObject(), KNewStuff( "kstars", parent )
{
	ks = (KStars*)parent;
	kdw = new KDirWatch( this );
	kdw->addDir( KGlobal::dirs()->saveLocation("data", kapp->instanceName(), true) );
}

bool KSNewStuff::install( const QString &fileName )
{
	kdDebug() << "KSNewStuff::install(): " << fileName << endl;
	
	KTar archive( fileName );
	if ( !archive.open( IO_ReadOnly ) )
			return false;
	
	const KArchiveDirectory *archiveDir = archive.directory();
	const QString destDir = KGlobal::dirs()->saveLocation("data", kapp->instanceName(), true);      
	KStandardDirs::makeDir( destDir );

	//monitor destDir for changes; inform updateData when files are created.
	connect( kdw, SIGNAL( dirty( const QString & ) ), this, SLOT( updateData( const QString & ) ) );

	archiveDir->copyTo(destDir);
	archive.close();
	kapp->processEvents(10000);
	
	//read the new data into the program
	//this return might be the result of checking if everything is installed ok
	return true;
}

void KSNewStuff::updateData( const QString &path ) {
	QDir qd( path );
	qd.setSorting( QDir::Time );
	qd.setFilter( QDir::Files );
	
	//Handle the Steinicke NGC/IC catalog
	if ( qd[0].contains( "ngcic" ) ) {
		//First, remove the existing NGC/IC objects from the ObjectNameList.
		for ( DeepSkyObject *o = ks->data()->deepSkyList.first(); o; o = ks->data()->deepSkyList.next() ) {
			if ( o->hasLongName() && o->longname() != o->name() ) ks->data()->ObjNames.remove( o->longname() );
			ks->data()->ObjNames.remove( o->name() );
		}
		
		//We can safely clear the Messier/NGC/IC/Other lists, since their pointers are secondary
		ks->data()->deepSkyListMessier.clear();
		ks->data()->deepSkyListNGC.clear();
		ks->data()->deepSkyListIC.clear();
		ks->data()->deepSkyListOther.clear();
		
		//Finally, we can clear deepSkyList.  This will automatically delete the SkyObjects
		ks->data()->deepSkyList.clear();
		
		//Send progress messages to the console
		connect( ks->data(), SIGNAL( progressText(QString) ), ks->data(), SLOT( slotConsoleMessage(QString) ) );
		
		//We are now ready to read the new NGC/IC catalog
		ks->data()->readDeepSkyData();
		
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
		//First, remove the existing asteroids and comets from the ObjectNameList.
		for ( SkyObject *o = (SkyObject*)(ks->data()->asteroidList.first()); o; o = (SkyObject*)(ks->data()->asteroidList.next()) ) {
			if ( o->hasLongName() && o->longname() != o->name() ) ks->data()->ObjNames.remove( o->longname() );
			ks->data()->ObjNames.remove( o->name() );
		}
		for ( SkyObject *o = (SkyObject*)(ks->data()->cometList.first()); o; o = (SkyObject*)(ks->data()->cometList.next()) ) {
			if ( o->hasLongName() && o->longname() != o->name() ) ks->data()->ObjNames.remove( o->longname() );
			ks->data()->ObjNames.remove( o->name() );
		}
		
		//Clear the asteroids and comets lists
		ks->data()->asteroidList.clear();
		ks->data()->cometList.clear();
		
		//Send progress messages to the console
		connect( ks->data(), SIGNAL( progressText(QString) ), ks->data(), SLOT( slotConsoleMessage(QString) ) );
		
		//add new asteroids and comets
		ks->data()->readAsteroidData();
		ks->data()->readCometData();
		
	}
}

#include "ksnewstuff.moc"

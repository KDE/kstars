#include <kapplication.h>
#include <kdebug.h>
#include <kstandarddirs.h>
#include <ktar.h>
#include <qdir.h>
#include <kaction.h>

#include "ksnewstuff.h"

KSNewStuff::KSNewStuff( QWidget *parent ) :
  KNewStuff( "kstars", parent )
{
}

bool KSNewStuff::install( const QString &fileName )
{
	kdDebug() << "KSNewStuff::install(): " << fileName << endl;
	
	KTar archive( fileName );
	if ( !archive.open( IO_ReadOnly ) )
			return false;
	
	const KArchiveDirectory *archiveDir = archive.directory();
	KStandardDirs myStdDir;
	const QString destDir =myStdDir.saveLocation("data", kapp->instanceName(), true);      
	KStandardDirs::makeDir( destDir );
	archiveDir->copyTo(destDir);
	archive.close();
	
	//read the new data into the program
	
	//this return might be the result of checking if everything is installed ok
	return true;
}


/***************************************************************************
                          main.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Feb  5 01:11:45 PST 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <dcopclient.h>
#include <klocale.h>
#include <kdebug.h>

#include "kstars.h"

#define KSTARS_VERSION "1.0"

static const char description[] =
	I18N_NOOP("Desktop Planetarium");
static const char notice[] =
	I18N_NOOP("Some images in KStars are for non-commercial use only.  See README.images.");
	
	
static KCmdLineOptions options[] =
{
	{ "!dump", I18N_NOOP( "dump sky image to file" ), 0 },
	{ "width ", I18N_NOOP( "width of sky image" ), "640" },
	{ "height ", I18N_NOOP( "height of sky image" ), "480" },
	{ "filename ", I18N_NOOP( "filename for sky image" ), 0 },
	KCmdLineLastOption
};

int main(int argc, char *argv[])
{

	KAboutData aboutData( "kstars", I18N_NOOP("KStars"),
		KSTARS_VERSION, description, KAboutData::License_GPL,
		"(c) 2001-2003, The KStars Team", notice, "http://edu.kde.org/kstars");
	aboutData.addAuthor("Jason Harris",0, "jharris@30doradus.org", "http://www.30doradus.org");
	aboutData.addAuthor("Heiko Evermann",0, "heiko@evermann.de", "http://www.evermann.de");
	aboutData.addAuthor("Thomas Kabelmann", 0, "tk78@gmx.de", 0);
	aboutData.addAuthor("Pablo de Vicente", 0, "pvicentea@wanadoo.es", 0);
	aboutData.addAuthor("Jasem Mutlaq", 0, "mutlaqja@ku.edu", 0 );
	aboutData.addAuthor("Carsten Niehaus", 0, "cniehaus@gmx.de", 0);
	aboutData.addAuthor("Mark Hollomon", 0, "mhh@mindspring.com", 0);
	KCmdLineArgs::init( argc, argv, &aboutData );
	KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.
	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
	
	if ( args->isSet( "dump" ) ) {
		kdDebug() << "Dumping sky image" << endl;
		kdDebug() << "Width: " << args->getOption( "width" ) << "  Height: " << args->getOption( "height" ) << endl;
		return 1;
	}
	
	//start up normally
	KApplication a;
	KStars *kstars = new KStars( true );
	QObject::connect(kapp, SIGNAL(lastWindowClosed()), kapp, SLOT(quit()));
	return a.exec();

}

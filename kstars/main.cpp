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
#include <klocale.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "kstarssplash.h"

#define KSTARS_VERSION "0.9"

static const char *description =
	I18N_NOOP("Desktop Planetarium");
// INSERT A DESCRIPTION FOR YOUR APPLICATION HERE
	
	
static KCmdLineOptions options[] =
{
  { 0, 0, 0 }
  // INSERT YOUR COMMANDLINE OPTIONS HERE
};

int main(int argc, char *argv[])
{

  KAboutData aboutData( "kstars", I18N_NOOP("KStars"),
    KSTARS_VERSION, description, KAboutData::License_GPL,
    "(c) 2001, Jason Harris", 0, "http://edu.kde.org/kstars");
  aboutData.addAuthor("Jason Harris",0, "jharris@30doradus.org", "http://www.30doradus.org");
  aboutData.addAuthor("Heiko Evermann",0, "heiko@evermann.de", "http://www.evermann.de");
  aboutData.addAuthor("Thomas Kabelmann", 0, "tk78@gmx.de", 0);
  KCmdLineArgs::init( argc, argv, &aboutData );
  KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

	KApplication a;

	KStarsData *   kstarsData = new KStarsData();
	KStarsSplash*  splashDialog = new KStarsSplash(kstarsData, 0, "Splash", true );
	// show splash screen and load KStarsData from data files
	splashDialog->disableResize();
	if ( splashDialog->exec()==QDialog::Accepted ) {
		KStars *kstars = new KStars( kstarsData );
		kstars->show();
		return a.exec();
	} else {
		a.exit(1);
	}
}

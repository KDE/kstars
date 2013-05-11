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
#include <kdebug.h>
#include <kapplication.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "skymap.h"
#include "simclock.h"
#include "ksnumbers.h"
#include "Options.h"
//Added by qt3to4:
#include <QPixmap>
#include <kglobal.h>

#define KSTARS_VERSION "2.0.0"

static const char description[] =
    I18N_NOOP("Desktop Planetarium");
static const char notice[] =
    I18N_NOOP("Some images in KStars are for non-commercial use only.  See README.images.");


int main(int argc, char *argv[])
{
    KAboutData aboutData( "kstars", 0, ki18n("KStars"),
                          KSTARS_VERSION, ki18n(description), KAboutData::License_GPL,
                          ki18n("(c) 2001-2012, The KStars Team"), ki18n(notice), "http://edu.kde.org/kstars");
    aboutData.addAuthor(ki18n("Jason Harris"),KLocalizedString(), "jharris@30doradus.org", "http://www.30doradus.org");
    aboutData.addAuthor(ki18n("Jasem Mutlaq"), KLocalizedString(), "mutlaqja@ikarustech.com");
    aboutData.addAuthor(ki18n("James Bowlin"), KLocalizedString(), "bowlin@mindspring.com");
    aboutData.addAuthor(ki18n("Pablo de Vicente"), KLocalizedString(), "pvicentea@wanadoo.es");
    aboutData.addAuthor(ki18n("Thomas Kabelmann"), KLocalizedString(), "tk78@gmx.de");
    aboutData.addAuthor(ki18n("Heiko Evermann"),KLocalizedString(), "heiko@evermann.de", "http://www.evermann.de");
    aboutData.addAuthor(ki18n("Carsten Niehaus"), KLocalizedString(), "cniehaus@gmx.de");
    aboutData.addAuthor(ki18n("Mark Hollomon"), KLocalizedString(), "mhh@mindspring.com");
    aboutData.addAuthor(ki18n("Alexey Khudyakov"), KLocalizedString(), "alexey.skladnoy@gmail.com");
    aboutData.addAuthor(ki18n("M&eacute;d&eacute;ric Boquien"), KLocalizedString(), "mboquien@free.fr");
    aboutData.addAuthor(ki18n("Akarsh Simha"), KLocalizedString(), "akarsh.simha@kdemail.net", "http://www.ph.utexas.edu/~asimha");
    aboutData.addAuthor(ki18n("J&eacute;r&ocirc;me Sonrier"), KLocalizedString(), "jsid@emor3j.fr.eu.org");
    aboutData.addAuthor(ki18n("Prakash Mohan"), KLocalizedString(), "prakash.mohan@kdemail.net");
    aboutData.addAuthor(ki18n("Victor Cărbune"), KLocalizedString(), "victor.carbune@kdemail.net");
    aboutData.addAuthor(ki18n("Henry de Valence"), KLocalizedString(), "hdevalence@gmail.com");
    aboutData.addAuthor(ki18n("Samikshan Bairagya"), KLocalizedString(), "samikshan.bairagya@kdemail.net");
    aboutData.addAuthor(ki18n("Rafał Kułaga"), KLocalizedString(), "rl.kulaga@gmail.com");
    aboutData.addAuthor(ki18n("Rishab Arora"), KLocalizedString(), "ra.rishab@gmail.com");

    aboutData.addCredit(ki18n("Valery Kharitonov"), ki18n("Converted labels containing technical terms to links to documentation") );
    aboutData.addCredit(ki18n("Ana-Maria Constantin"), ki18n("Technical documentation on Astronomy and KStars") );
    aboutData.addCredit(ki18n("Andrew Stepanenko"), ki18n("Guiding code based on lin_guider") );
    aboutData.addCredit(ki18n("Nuno Pinheiro"), ki18n("Artwork") );

    KCmdLineArgs::init( argc, argv, &aboutData );

    KCmdLineOptions options;
    options.add("!dump", ki18n( "Dump sky image to file" ));
    options.add("script ", ki18n( "Script to execute" ));
    options.add("width ", ki18n( "Width of sky image" ), "640");
    options.add("height ", ki18n( "Height of sky image" ), "480");
    options.add("filename ", ki18n( "Filename for sky image" ), "kstars.png");
    options.add("date ", ki18n( "Date and time" ));
    options.add("!paused", ki18n( "Start with clock paused" ));
    KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    KApplication a;

    if ( args->isSet( "dump" ) ) {
        kDebug() << i18n( "Dumping sky image" );

        //parse filename and image format
        const char* format = "PNG";
        QString fname = args->getOption( "filename" );
        QString ext = fname.mid( fname.lastIndexOf(".")+1 );
        if ( ext.toLower() == "png" ) { format = "PNG"; }
        else if ( ext.toLower() == "jpg" || ext.toLower() == "jpeg" ) { format = "JPG"; }
        else if ( ext.toLower() == "gif" ) { format = "GIF"; }
        else if ( ext.toLower() == "pnm" ) { format = "PNM"; }
        else if ( ext.toLower() == "bmp" ) { format = "BMP"; }
        else { kWarning() << i18n( "Could not parse image format of %1; assuming PNG.", fname ) ; }

        //parse width and height
        bool ok(false);
        int w(0), h(0);
        w = args->getOption( "width" ).toInt( &ok );
        if ( ok ) h =  args->getOption( "height" ).toInt( &ok );
        if ( !ok ) {
            kWarning() << "Unable to parse arguments: " ;
            kWarning() << "Width: " << args->getOption( "width" )
            << "  Height: " << args->getOption( "height" ) << endl;
            return 1;
        }

        KStarsData *dat = KStarsData::Create();
        QObject::connect( dat, SIGNAL( progressText(QString) ), dat, SLOT( slotConsoleMessage(QString) ) );
        dat->initialize();

        //Set Geographic Location
        dat->setLocationFromOptions();

        //Set color scheme
        dat->colorScheme()->loadFromConfig();

        //set clock now that we have a location:
        //Check to see if user provided a date/time string.  If not, use current CPU time
        QString datestring = args->getOption( "date" );
        KStarsDateTime kdt;
        if ( ! datestring.isEmpty() ) {
            if ( datestring.contains( "-" ) ) { //assume ISODate format
                if ( datestring.contains( ":" ) ) { //also includes time
                    kdt = KDateTime::fromString( datestring, KDateTime::ISODate );
                } else { //string probably contains date only
                    kdt.setDate( QDate::fromString( datestring, Qt::ISODate ) );
                    kdt.setTime( QTime( 0, 0, 0 ) );
                }
            } else { //assume Text format for date string
                kdt = dat->geo()->LTtoUT( KDateTime::fromString( datestring, KDateTime::QtTextDate ) );
            }

            if ( ! kdt.isValid() ) {
                kWarning() << i18n( "Using CPU date/time instead." ) ;

                kdt = KStarsDateTime::currentUtcDateTime();
            }
        } else {
            kdt = KStarsDateTime::currentUtcDateTime();
        }
        dat->clock()->setUTC( kdt );

        KSNumbers num( dat->ut().djd() );
        //		dat->initGuides(&num);

        SkyMap *map = SkyMap::Create();
        map->resize( w, h );
        QPixmap sky( w, h );

        dat->setFullTimeUpdate();
        dat->updateTime(dat->geo(), map );

        SkyPoint dest( Options::focusRA(), Options::focusDec() );
        map->setDestination( dest );
        map->destination()->EquatorialToHorizontal( dat->lst(), dat->geo()->lat() );
        map->setFocus( map->destination() );
        map->focus()->EquatorialToHorizontal( dat->lst(), dat->geo()->lat() );

        //Execute the specified script
        QString scriptfile = args->getOption( "script" );
        if ( ! scriptfile.isEmpty() ) {
            if ( dat->executeScript( scriptfile, map ) ) {
                std::cout << i18n( "Script executed." ).toUtf8().data() << std::endl;
            } else {
                kWarning() << i18n( "Could not execute script." ) ;
            }
        }

        qApp->processEvents();
        map->setupProjector();
        map->exportSkyImage( &sky );
        qApp->processEvents();

        if ( ! sky.save( fname, format ) ) kWarning() << i18n( "Unable to save image: %1 ", fname ) ;
        else kDebug() << i18n( "Saved to file: %1", fname );

        delete map;
        delete dat;
        return 0;
    }

    //start up normally in GUI mode

    //Try to parse the given date string
    QString datestring = args->getOption( "date" );
    //DEBUG
    kDebug() << "Date string: " << datestring;

    if ( ! datestring.isEmpty() && ! KStarsDateTime::fromString( datestring ).isValid() ) {
        kWarning() << i18n( "Using CPU date/time instead." ) ;
        datestring.clear();
    }

    KStars::createInstance( true, ! args->isSet( "paused" ), datestring );
    args->clear();
    QObject::connect(kapp, SIGNAL(lastWindowClosed()), kapp, SLOT(quit()));
    return a.exec();

}

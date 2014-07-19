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


#include <QDebug>
#include <QPixmap>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include <KAboutData>
#include <KLocalizedString>

#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "skymap.h"
#include "simclock.h"
#include "ksnumbers.h"
#include "version.h"
#include "Options.h"


static const char description[] =
    I18N_NOOP("Desktop Planetarium");
static const char notice[] =
    I18N_NOOP("Some images in KStars are for non-commercial use only.  See README.images.");


int main(int argc, char *argv[])
{
    KAboutData aboutData( "kstars", xi18n("KStars"), KSTARS_VERSION, xi18n(description), KAboutLicense::GPL,
                          xi18n("(c) 2001-2014, The KStars Team"), xi18n(notice), "http://edu.kde.org/kstars");
    aboutData.addAuthor(xi18n("Jason Harris"),QString(), "jharris@30doradus.org", "http://www.30doradus.org");
    aboutData.addAuthor(xi18n("Jasem Mutlaq"), QString(), "mutlaqja@ikarustech.com", "http://www.indilib.org");
    aboutData.addAuthor(xi18n("James Bowlin"), QString(), "bowlin@mindspring.com");
    aboutData.addAuthor(xi18n("Pablo de Vicente"), QString(), "pvicentea@wanadoo.es");
    aboutData.addAuthor(xi18n("Thomas Kabelmann"), QString(), "tk78@gmx.de");
    aboutData.addAuthor(xi18n("Heiko Evermann"),QString(), "heiko@evermann.de", "http://www.evermann.de");
    aboutData.addAuthor(xi18n("Carsten Niehaus"), QString(), "cniehaus@gmx.de");
    aboutData.addAuthor(xi18n("Mark Hollomon"), QString(), "mhh@mindspring.com");
    aboutData.addAuthor(xi18n("Alexey Khudyakov"), QString(), "alexey.skladnoy@gmail.com");
    aboutData.addAuthor(xi18n("M&eacute;d&eacute;ric Boquien"), QString(), "mboquien@free.fr");
    aboutData.addAuthor(xi18n("Akarsh Simha"), QString(), "akarsh.simha@kdemail.net", "http://www.ph.utexas.edu/~asimha");
    aboutData.addAuthor(xi18n("J&eacute;r&ocirc;me Sonrier"), QString(), "jsid@emor3j.fr.eu.org");
    aboutData.addAuthor(xi18n("Prakash Mohan"), QString(), "prakash.mohan@kdemail.net");
    aboutData.addAuthor(xi18n("Victor Cărbune"), QString(), "victor.carbune@kdemail.net");
    aboutData.addAuthor(xi18n("Henry de Valence"), QString(), "hdevalence@gmail.com");
    aboutData.addAuthor(xi18n("Samikshan Bairagya"), QString(), "samikshan.bairagya@kdemail.net");
    aboutData.addAuthor(xi18n("Rafał Kułaga"), QString(), "rl.kulaga@gmail.com");
    aboutData.addAuthor(xi18n("Rishab Arora"), QString(), "ra.rishab@gmail.com");

    aboutData.addCredit(xi18n("Valery Kharitonov"), xi18n("Converted labels containing technical terms to links to documentation") );
    aboutData.addCredit(xi18n("Ana-Maria Constantin"), xi18n("Technical documentation on Astronomy and KStars") );
    aboutData.addCredit(xi18n("Andrew Stepanenko"), xi18n("Guiding code based on lin_guider") );
    aboutData.addCredit(xi18n("Nuno Pinheiro"), xi18n("Artwork") );

    KAboutData::setApplicationData(aboutData);

    QCommandLineParser *parser = new QCommandLineParser;    
    parser->addVersionOption();
    //parser->addHelpOption(INSERT_DESCRIPTION_HERE);
    parser->addOption(QCommandLineOption(QStringList() << "!dump", xi18n( "Dump sky image to file" )));
    parser->addOption(QCommandLineOption(QStringList() << "script ", xi18n( "Script to execute" )));
    parser->addOption(QCommandLineOption(QStringList() << "width ", xi18n( "Width of sky image" ),  "640"));
    parser->addOption(QCommandLineOption(QStringList() << "height ", xi18n( "Height of sky image" ), "480"));
    parser->addOption(QCommandLineOption(QStringList() << "filename ", xi18n( "Filename for sky image" ), "kstars.png"));
    parser->addOption(QCommandLineOption(QStringList() << "date ", xi18n( "Date and time" )));
    parser->addOption(QCommandLineOption(QStringList() << "!paused", xi18n( "Start with clock paused" )));

    QApplication a(argc, argv);
    a.setApplicationVersion(KSTARS_VERSION);

    if ( parser->isSet( "dump" ) )
    {
        qDebug() << xi18n( "Dumping sky image" );

        //parse filename and image format
        const char* format = "PNG";
        QString fname = parser->value( "filename" );
        QString ext = fname.mid( fname.lastIndexOf(".")+1 );
        if ( ext.toLower() == "png" ) { format = "PNG"; }
        else if ( ext.toLower() == "jpg" || ext.toLower() == "jpeg" ) { format = "JPG"; }
        else if ( ext.toLower() == "gif" ) { format = "GIF"; }
        else if ( ext.toLower() == "pnm" ) { format = "PNM"; }
        else if ( ext.toLower() == "bmp" ) { format = "BMP"; }
        else { qWarning() << xi18n( "Could not parse image format of %1; assuming PNG.", fname ) ; }

        //parse width and height
        bool ok(false);
        int w(0), h(0);
        w = parser->value( "width" ).toInt( &ok );
        if ( ok ) h =  parser->value( "height" ).toInt( &ok );
        if ( !ok ) {
            qWarning() << "Unable to parse arguments: " ;
            qWarning() << "Width: " << parser->value( "width" )
            << "  Height: " << parser->value( "height" ) << endl;
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
        QString datestring = parser->value( "date" );
        KStarsDateTime kdt;
        if ( ! datestring.isEmpty() ) {
            if ( datestring.contains( "-" ) ) { //assume ISODate format
                if ( datestring.contains( ":" ) ) { //also includes time
                    //kdt = QDateTime::fromString( datestring, QDateTime::ISODate );
                    kdt = QDateTime::fromString( datestring, Qt::ISODate );
                } else { //string probably contains date only
                    //kdt.setDate( QDate::fromString( datestring, Qt::ISODate ) );
                    kdt.setDate( QDate::fromString( datestring, Qt::ISODate ) );
                    kdt.setTime( QTime( 0, 0, 0 ) );
                }
            } else { //assume Text format for date string
                kdt = dat->geo()->LTtoUT( QDateTime::fromString( datestring, Qt::TextDate ) );
            }

            if ( ! kdt.isValid() ) {
                qWarning() << xi18n( "Using CPU date/time instead." ) ;

                kdt = KStarsDateTime::currentDateTimeUtc();
            }
        } else {
            kdt = KStarsDateTime::currentDateTimeUtc();
        }
        dat->clock()->setUTC( kdt );

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
        QString scriptfile = parser->value( "script" );
        if ( ! scriptfile.isEmpty() ) {
            if ( dat->executeScript( scriptfile, map ) ) {
                std::cout << xi18n( "Script executed." ).toUtf8().data() << std::endl;
            } else {
                qWarning() << xi18n( "Could not execute script." ) ;
            }
        }

        qApp->processEvents();
        map->setupProjector();
        map->exportSkyImage( &sky );
        qApp->processEvents();

        if ( ! sky.save( fname, format ) ) qWarning() << xi18n( "Unable to save image: %1 ", fname ) ;
        else qDebug() << xi18n( "Saved to file: %1", fname );

        delete map;
        delete dat;
        return 0;
    }

    //start up normally in GUI mode

    //Try to parse the given date string
    QString datestring = parser->value( "date" );
    //DEBUG
    qDebug() << "Date string: " << datestring;

    if ( ! datestring.isEmpty() && ! KStarsDateTime::fromString( datestring ).isValid() )
    {
        qWarning() << xi18n( "Using CPU date/time instead." ) ;
        datestring.clear();
    }

    // Create writable data dir if it does not exist
    QDir writableDir;
    writableDir.mkdir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));

    KStars::createInstance( true, ! parser->isSet( "paused" ), datestring );

    QObject::connect(qApp, SIGNAL(lastWindowClosed()), qApp, SLOT(quit()));
    return a.exec();

}

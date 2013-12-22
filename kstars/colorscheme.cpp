/***************************************************************************
                          colorscheme.cpp  -  description
                             -------------------
    begin                : Wed May 8 2002
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

#include "colorscheme.h"

#include <QFile>
#include <QTextStream>

#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kconfiggroup.h>

#include "ksutils.h"
#include "Options.h"
#include "skyobjects/starobject.h"
#include "skyqpainter.h"

ColorScheme::ColorScheme() : FileName() {
    //Each color has two names associated with it.  The KeyName is its
    //identification in the QMap, the *.colors file, and the config file.
    //The Name is what appears in the ViewOpsDialog ListBox.
    //In addition, we define default RGB strings for each item.
    //To add another color to the Palette, just add an entry for KeyName,
    //Name and Default here.

    appendItem("SkyColor",         i18n("Sky"),                    "#000000");
    appendItem("MessColor",        i18n("Messier Object"),         "#008f00");
    appendItem("NGCColor",         i18nc("New General Catalog object", "NGC Object"), "#006666");
    appendItem("ICColor",          i18nc("Index Catalog object", "IC Object"), "#382a7d");
    appendItem("HSTColor",
               i18nc("Object with extra attached URLs", "Object w/ Links"), "#930000");
    appendItem("SNameColor",       i18n("Star Name"),              "#577d7d");
    appendItem("DSNameColor",      i18n("Deep Sky Object Name"),   "#75759c");
    appendItem("PNameColor",       i18n("Planet Name"),            "#ac9800");
    appendItem("CNameColor",
               i18nc("Constellation Name", "Constell. Name"),      "#718488");
    appendItem("CLineColor",
               i18nc("Constellation Line", "Constell. Line"),      "#3d3d3d");
    appendItem("CBoundColor",
               i18nc("Constellation Boundary", "Constell. Boundary"), "#222f2f");
    appendItem("CBoundHighColor",
               i18nc("Highlighted Constellation Boundary", "Constell. Boundary Highlight"), "#445555");
    appendItem("MWColor",
               i18nc("refers to the band of stars in the sky due to the Galactic plane", "Milky Way"), "#0d1115");
    appendItem("EqColor",          i18n("Equator"),                "#909090");
    appendItem("EclColor",         i18n("Ecliptic"),               "#613d12");
    appendItem("HorzColor",        i18n("Horizon"),                "#091f14");
    appendItem("CompassColor",     i18n("Compass Labels"),         "#909055");
    appendItem("EquatorialGridColor", i18n("Equatorial Coordinate Grid"), "#445566");
    appendItem("HorizontalGridColor", i18n("Horizontal Coordinate Grid"), "#091f14");
    appendItem("BoxTextColor",     i18n("Info Box Text"),          "#d2dbef");
    appendItem("BoxGrabColor",     i18n("Info Box Selected"),      "#900000");
    appendItem("BoxBGColor",       i18n("Info Box Background"),    "#000000");
    appendItem("TargetColor",      i18n("Target Indicator"),       "#DD0000");
    appendItem("UserLabelColor",   i18n("User Labels"),            "#AAAAAA");
    appendItem("PlanetTrailColor", i18n("Planet Trails"),          "#993311");
    appendItem("AngularRuler",     i18n("Angular Distance Ruler"), "#445566");
    appendItem("ObsListColor",     i18n("Observing List Label"),   "#FF0000");
    appendItem("StarHopRouteColor", i18n("Star-Hop Route"),        "#00FFFF");
    appendItem("VisibleSatColor",  i18n("Visible Satellites"),     "#00FF00");
    appendItem("SatColor",         i18n("Satellites"),             "#FF0000");
    appendItem("SatLabelColor",    i18n("Satellites Labels"),      "#640000");
    appendItem("SupernovaColor",   i18n("Supernovae"),             "#FFA500");

    //Load colors from config object
    loadFromConfig();

    //Default values for integer variables:
    StarColorMode = 0;
    StarColorIntensity = 4;
}

void ColorScheme::appendItem(QString key, QString name, QString def) {
    KeyName.append( key );
    Name.append( name );
    Default.append( def );

}

QColor ColorScheme::colorNamed( const QString &name ) const {
    if ( ! hasColorNamed( name ) ) {
        kWarning() << i18n( "No color named \"%1\" found in color scheme.", name ) ;
        // Return white if no color found
        return QColor( Qt::white );
    }
    return QColor( Palette[ name ] );
}

QColor ColorScheme::colorAt( int i ) const {
    return QColor( Palette[ KeyName.at(i) ] );
}

QString ColorScheme::nameAt( int i ) const {
    return Name.at(i);
}

QString ColorScheme::keyAt( int i ) const {
    return KeyName.at(i);
}

QString ColorScheme::nameFromKey( const QString &key ) const {
    return nameAt( KeyName.indexOf( key ) );
}

void ColorScheme::setColor( const QString &key, const QString &color ) {
    //We can blindly insert() the new value; if the key exists, the old value is replaced
    Palette.insert( key, color );

    KConfigGroup cg = KGlobal::config()->group( "Colors" );
    cg.writeEntry( key, color );
}

bool ColorScheme::load( const QString &name ) {
    QString filename = name.toLower().trimmed();
    QFile file;
    int inew = 0, iold = 0;
    bool ok = false;

    //Parse default names which don't follow the regular file-naming scheme
    if ( name == i18nc("use default color scheme", "Default Colors") )
        filename = "classic.colors";
    if ( name == i18nc("use 'star chart' color scheme", "Star Chart") )
        filename = "chart.colors";
    if ( name == i18nc("use 'night vision' color scheme", "Night Vision") )
        filename = "night.colors";

    //Try the filename if it ends with ".colors"
    if ( filename.endsWith( QLatin1String( ".colors" ) ) )
        ok = KSUtils::openDataFile( file, filename );

    //If that didn't work, try assuming that 'name' is the color scheme name
    //convert it to a filename exactly as ColorScheme::save() does
    if ( ! ok ) {
        if ( !filename.isEmpty() ) {
            filename.replace( ' ', '-' ).append( ".colors" );
            ok = KSUtils::openDataFile( file, filename );
        }

        if ( ! ok ) {
            kDebug() << i18n( "Unable to load color scheme named %1. Also tried %2.", name, filename );
            return false;
        }
    }

    //If we reach here, the file should have been successfully opened
    QTextStream stream( &file );

    //first line is the star-color mode and star color intensity
    QString line = stream.readLine();
    int newmode = line.left(1).toInt( &ok );
    if( ok )
        setStarColorMode( newmode );
    if( line.contains(':') ) {
        int newintens = line.mid( line.indexOf(':')+1, 2 ).toInt( &ok );
        if ( ok )
            setStarColorIntensity( newintens );
    }

    //More flexible method for reading in color values.  Any order is acceptable, and
    //missing entries are ignored.
    while ( !stream.atEnd() ) {
        line = stream.readLine();

        if ( line.count(':')==1 ) { //the new color preset format contains a ":" in each line, followed by the name of the color
            ++inew;
            if ( iold ) return false; //we read at least one line without a colon...file is corrupted.

            //If this line has a valid Key, set the color.
            QString tkey = line.mid( line.indexOf(':')+1 ).trimmed();
            QString tname = line.left( line.indexOf(':')-1 );

            if ( KeyName.contains( tkey ) ) {
                setColor( tkey, tname );
            } else { //attempt to translate from old color ID
                QString k( line.mid( 5 ).trimmed() + "Color" );
                if ( KeyName.contains( k ) ) {
                    setColor( k, tname );
                } else {
                    kWarning() << "Could not use the key \"" << tkey
                               << "\" from the color scheme file \"" << filename
                               << "\".  I also tried \"" << k << "\"." << endl;
                }
            }

        } else { // no ':' seen in the line, so we must assume the old format
            ++iold;
            if ( inew ) return false; //a previous line had a colon, this line doesn't.  File is corrupted.

            //Assuming the old *.colors format.  Loop through the KeyName list,
            //and assign each color.  Note that order matters here, but only here
            //(so if you don't use the old format, then order doesn't ever matter)
            foreach(const QString& key, KeyName)
                setColor( key, line.left( 7 ) );
        }
    }

    FileName = filename;
    return true;
}

bool ColorScheme::save( const QString &name ) {
    QFile file;

    //Construct a file name from the scheme name.  Make lowercase, replace spaces with "-",
    //and append ".colors".
    QString filename = name.toLower().trimmed();
    if ( !filename.isEmpty() ) {
        for( int i=0; i<filename.length(); ++i)
            if ( filename.at(i)==' ' ) filename.replace( i, 1, "-" );

        filename = filename.append( ".colors" );
        file.setFileName( KStandardDirs::locateLocal( "appdata", filename ) ); //determine filename in local user KDE directory tree.

        if ( file.exists() || !file.open( QIODevice::ReadWrite | QIODevice::Append ) ) {
            QString message = i18n( "Local color scheme file could not be opened.\nScheme cannot be recorded." );
            KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
            return false;
        } else {
            QTextStream stream( &file );
            stream << StarColorMode << ":" << StarColorIntensity << endl;

            foreach(const QString& key, KeyName )
                stream << Palette[ key ] << " :" << key << endl;
            file.close();
        }

        file.setFileName( KStandardDirs::locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.

        if ( !file.open( QIODevice::ReadWrite | QIODevice::Append ) ) {
            QString message = i18n( "Local color scheme index file could not be opened.\nScheme cannot be recorded." );
            KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
            return false;
        } else {
            QTextStream stream( &file );
            stream << name << ":" << filename << endl;
            file.close();
        }
    } else {
        QString message = i18n( "Invalid filename requested.\nScheme cannot be recorded." );
        KMessageBox::sorry( 0, message, i18n( "Invalid Filename" ) );
        return false;
    }

    FileName = filename;
    saveToConfig();
    return true;
}

void ColorScheme::loadFromConfig() {
    KConfigGroup cg = KGlobal::config()->group( "Colors" );

    for ( int i=0; i < KeyName.size(); ++i )
        setColor( KeyName.at(i), cg.readEntry( KeyName.at(i).toUtf8().constData(), Default.at( i ) ) );

    setStarColorModeIntensity( cg.readEntry( "StarColorMode", 0 ), cg.readEntry( "StarColorIntensity", 5 ) );

    FileName = cg.readEntry( "ColorSchemeFile", "classic.colors" );
}

void ColorScheme::saveToConfig() {
    KConfigGroup cg = KGlobal::config()->group( "Colors" );
    for ( int i=0; i < KeyName.size(); ++i ) {
        QString c = colorNamed( KeyName.at(i) ).name();
        cg.writeEntry( KeyName.at(i), c );
    }

    cg.writeEntry( "StarColorMode", starColorMode() );
    cg.writeEntry( "StarColorIntensity", starColorIntensity() );
}

void ColorScheme::setStarColorMode( int mode ) { 
    StarColorMode = mode;
    Options::setStarColorMode( mode );
    SkyQPainter::initStarImages();
}

void ColorScheme::setStarColorIntensity( int intens ) { 
    StarColorIntensity = intens;
    Options::setStarColorIntensity( intens );
    SkyQPainter::initStarImages();
}

void ColorScheme::setStarColorModeIntensity( int mode, int intens) {
    StarColorMode = mode;
    StarColorIntensity = intens;
    Options::setStarColorMode( mode );
    Options::setStarColorIntensity( intens );
    SkyQPainter::initStarImages();
}

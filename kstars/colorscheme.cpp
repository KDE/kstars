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

#include <qfile.h>
//Added by qt3to4:
#include <QTextStream>
#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>

#include "ksutils.h"
#include "colorscheme.h"

ColorScheme::ColorScheme() : FileName() {
	//Each color has two names associated with it.  The KeyName is its
	//identification in the QMap, the *.colors file, and the config file.
	//The Name is what appears in the ViewOpsDialog ListBox.
	//In addition, we define default RGB strings for each item.
	//To add another color to the Palette, just add an entry for KeyName,
	//Name and Default here.

	KeyName.append( "SkyColor" );
	Name.append( i18n( "Sky" ) );
	Default.append( "#002" );
	KeyName.append( "MessColor" );
	Name.append( i18n( "Messier Object" ) );
	Default.append( "#0F0" );
	KeyName.append( "NGCColor" );
	Name.append( i18nc( "New General Catalog object", "NGC Object" ) );
	Default.append( "#066" );
	KeyName.append( "ICColor" );
	Name.append( i18nc( "Index Catalog object", "IC Object" ) );
	Default.append( "#439" );
	KeyName.append( "HSTColor" );
	Name.append( i18nc( "Object with extra attached URLs", "Object w/ Links" ) );
	Default.append( "#A00" );
	KeyName.append( "SNameColor" );
	Name.append( i18n( "Star Name" ) );
	Default.append( "#7AA" );
	KeyName.append( "PNameColor" );
	Name.append( i18n( "Planet Name" ) );
	Default.append( "#A77" );
	KeyName.append( "CNameColor" );
	Name.append( i18nc( "Constellation Name", "Constell. Name" ) );
	Default.append( "#AA7" );
	KeyName.append( "CLineColor" );
	Name.append( i18nc( "Constellation Line", "Constell. Line" ) );
	Default.append( "#555" );
	KeyName.append( "CBoundColor" );
	Name.append( i18nc( "Constellation Boundary", "Constell. Boundary" ) );
	Default.append( "#222" );
	KeyName.append( "CBoundHighColor" );
	Name.append( i18nc( "Highlighted Constellation Boundary", "Constell. Boundary Highlight" ) );
	Default.append( "#999" );
	KeyName.append( "MWColor" );
	Name.append( i18nc( "refers to the band of stars in the sky due to the Galactic plane", "Milky Way" ) );
	Default.append( "#123" );
	KeyName.append( "EqColor" );
	Name.append( i18n( "Equator" ) );
	Default.append( "#FFF" );
	KeyName.append( "EclColor" );
	Name.append( i18n( "Ecliptic" ) );
	Default.append( "#663" );
	KeyName.append( "HorzColor" );
	Name.append( i18n( "Horizon" ) );
	Default.append( "#5A3" );
	KeyName.append( "CompassColor" );
	Name.append( i18n( "Compass Labels" ) );
	Default.append( "#002" );
	KeyName.append( "GridColor" );
	Name.append( i18n( "Coordinate Grid" ) );
	Default.append( "#456" );
	KeyName.append( "BoxTextColor" );
	Name.append( i18n( "Info Box Text" ) );
	Default.append( "#FFF" );
	KeyName.append( "BoxGrabColor" );
	Name.append( i18n( "Info Box Selected" ) );
	Default.append( "#F00" );
	KeyName.append( "BoxBGColor" );
	Name.append( i18n( "Info Box Background" ) );
	Default.append( "#000" );
	KeyName.append( "TargetColor" );
	Name.append( i18n( "Target Indicator" ) );
	Default.append( "#8B8" );
	KeyName.append( "UserLabelColor" );
	Name.append( i18n( "User Labels" ) );
	Default.append( "#FFF" );
	KeyName.append( "PlanetTrailColor" );
	Name.append( i18n( "Planet Trails" ) );
	Default.append( "#963" );
	KeyName.append( "AngularRuler" );
	Name.append( i18n( "Angular Distance Ruler" ) );
	Default.append( "#FFF" );
	KeyName.append( "ObsListColor" );
	Name.append( i18n( "Observing List Label" ) );
	Default.append( "#F00" );
	KeyName.append( "SatColor" );
	Name.append( i18n( "Satellite Track" ) );
	Default.append( "#0AA" );

	//Set the default colors in the Palette.
	for ( int i=0; i<KeyName.size(); ++i ) {
		setColor( KeyName[i], Default[i] );
	}

	//Default values for integer variables:
	StarColorMode = 0;
	StarColorIntensity = 4;
}

ColorScheme::ColorScheme( const ColorScheme &cs ) {
	KeyName = cs.KeyName;
	Name = cs.Name;
	Default = cs.Default;
	StarColorMode = cs.StarColorMode;
	StarColorIntensity = cs.StarColorIntensity;
	Palette = cs.Palette;
	FileName = cs.FileName;
}

ColorScheme::~ColorScheme(){
}

void ColorScheme::copy( const ColorScheme &cs ) {
	KeyName = cs.KeyName;
	Name = cs.Name;
	Default = cs.Default;
	StarColorMode = cs.StarColorMode;
	StarColorIntensity = cs.StarColorIntensity;
	Palette = cs.Palette;
	FileName = cs.FileName;
}

QColor ColorScheme::colorNamed( const QString &name ) const {
	if ( ! hasColorNamed( name ) ) {
		kWarning() << i18n( "No color named \"%1\" found in color scheme.", name ) << endl;
		//color = "#FFFFFF"; //set to white if no color found
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
}

bool ColorScheme::load( const QString &filename ) {
	QFile file;
	int inew(0),iold(0);

	if ( !KSUtils::openDataFile( file, filename ) )
		return false;

	QTextStream stream( &file );
	QString line;

	//first line is the star-color mode and star color intensity
  line = stream.readLine();
	bool ok(false);
	int newmode = line.left(1).toInt( &ok );
	if ( ok ) setStarColorMode( newmode );
	if ( line.contains(':') ) {
		int newintens = line.mid( line.indexOf(':')+1, 2 ).toInt( &ok );
		if ( ok ) setStarColorIntensity( newintens );
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
					kWarning() << "Could not use the key \"" << tkey <<
							"\" from the color scheme file \"" << filename << "\".  I also tried \"" <<
							k << "\"." << endl;
				}
			}

		} else { // no ':' seen in the line, so we must assume the old format
			++iold;
			if ( inew ) return false; //a previous line had a colon, this line doesn't.  File is corrupted.

			//Assuming the old *.colors format.  Loop through the KeyName list,
			//and assign each color.  Note that order matters here, but only here
			//(so if you don't use the old format, then order doesn't ever matter)
			QStringList::Iterator it = KeyName.begin();
			QStringList::Iterator it_end = KeyName.end();
			for ( ; it != it_end; ++it )
				setColor( QString(*it), line.left( 7 ) );
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

			QStringList::Iterator it = KeyName.begin();
			QStringList::Iterator it_end = KeyName.end();
			for ( ; it != it_end; ++it )
				stream << Palette[ (*it) ] << " :" << (*it) << endl;

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
	return true;
}

void ColorScheme::loadFromConfig( KConfig *conf ) {
	for ( int i=0; i < KeyName.size(); ++i )
		setColor( KeyName.at(i), conf->readEntry( KeyName.at(i).toUtf8().constData(), Default.at( i ) ) );

	setStarColorMode( conf->readEntry( "StarColorMode", 0 ) );
	setStarColorIntensity( conf->readEntry( "StarColorIntensity", 5 ) );
}

void ColorScheme::saveToConfig( KConfig *conf ) {
	for ( int i=0; i < KeyName.size(); ++i ) {
		QString c = colorNamed( KeyName.at(i) ).name();
		conf->writeEntry( KeyName.at(i), c );
	}

	conf->writeEntry( "StarColorMode", starColorMode() );
	conf->writeEntry( "StarColorIntensity", starColorIntensity() );
}

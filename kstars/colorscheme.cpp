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

#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>


#if ( QT_VERSION < 300 )
#include <kstddirs.h>
#else
#include <kstandarddirs.h>
#endif

#include "ksutils.h"
#include "colorscheme.h"

#if (QT_VERSION < 300)
typedef QStringList::ConstIterator SL_it;
#else
typedef QStringList::const_iterator SL_it;
#endif

ColorScheme::ColorScheme(){
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
	Name.append( i18n( "New General Catalog object", "NGC Object" ) );
	Default.append( "#066" );
	KeyName.append( "ICColor" );
	Name.append( i18n( "Index Catalog object", "IC Object" ) );
	Default.append( "#439" );
	KeyName.append( "HSTColor" );
	Name.append( i18n( "Object with extra attached URLs", "Object w/ Links" ) );
	Default.append( "#A00" );
	KeyName.append( "SNameColor" );
	Name.append( i18n( "Star Name" ) );
	Default.append( "#7AA" );
	KeyName.append( "PNameColor" );
	Name.append( i18n( "Planet Name" ) );
	Default.append( "#A77" );
	KeyName.append( "CNameColor" );
	Name.append( i18n( "Constellation Name", "Constell. Name" ) );
	Default.append( "#AA7" );
	KeyName.append( "CLineColor" );
	Name.append( i18n( "Constellation Line", "Constell. Line" ) );
	Default.append( "#555" );
	KeyName.append( "MWColor" );
	Name.append( i18n( "refers to the band of stars in the sky due to the Galactic plane", "Milky Way" ) );
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
	Default.append( "#AFA" );
	
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
}

QString ColorScheme::colorNamed( QString name ) const {
	QString color( Palette[ name ] );
	if ( color.isEmpty() ) {
		kdWarning() << i18n( "No color named \"%1\" found in color scheme." ).arg( name ) << endl;
		color = "#FFFFFF"; //set to white if no color found
	}

	return color;
}

QString ColorScheme::colorAt( int i ) const {
	SL_it it = KeyName.at(i);
	return Palette[ QString(*it) ];
}

QString ColorScheme::nameAt( int i ) const {
	SL_it it = Name.at(i);
	return QString(*it);
}

QString ColorScheme::keyAt( int i ) const {
	SL_it it = KeyName.at(i);
	return QString(*it);
}

void ColorScheme::setColor( QString key, QString color ) {
	//We can blindly insert() the new value; if the key exists, the old value is replaced
	Palette.insert( key, color );
}

bool ColorScheme::load( QString filename ) {
	QFile file;
	int inew(0),iold(0);

	if ( !KSUtils::openDataFile( file, filename ) ) {
		file.setName( locateLocal( "appdata", filename ) ); //try filename in local user KDE directory tree.
		if ( !file.open( IO_ReadOnly ) ) {
			return false;
    }
	}

	QTextStream stream( &file );
	QString line;

	//first line is the star-color mode and star color intensity
  line = stream.readLine();
	bool ok(false);
	int newmode = line.left(1).toInt( &ok );
	if ( ok ) setStarColorMode( newmode );
	if ( line.contains(':') ) {
		int newintens = line.mid( line.find(':')+1, 2 ).toInt( &ok );
		if ( ok ) setStarColorIntensity( newintens );
	}

//More flexible method for reading in color values.  Any order is acceptable, and
//missing entries are ignored.
	while ( !stream.eof() ) {
		line = stream.readLine();

		if ( line.contains(':')==1 ) { //the new color preset format contains a ":" in each line, followed by the name of the color
      ++inew;
			if ( iold ) return false; //we read at least one line without a colon...file is corrupted.

//If this line has a valid Key, set the color.
			QString tkey = line.mid( line.find(':')+1 ).stripWhiteSpace();
			QString tname = line.left( line.find(':')-1 );

			if ( KeyName.contains( tkey ) ) {
				setColor( tkey, tname );
			} else { //attempt to translate from old color ID
				QString k( line.mid( 5 ).stripWhiteSpace() + "Color" );
				if ( KeyName.contains( k ) ) {
					setColor( k, tname );
				} else {
					kdWarning() << "Could not use the key \"" << tkey <<
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
			for ( QStringList::Iterator it = KeyName.begin(); it != KeyName.end(); ++it )
				setColor( QString(*it), line.left( 7 ) );
		}
	}

	return true;
}

bool ColorScheme::save( QString name ) {
	QFile file;

	//Construct a file name from the scheme name.  Make lowercase, replace spaces with "-",
	//and append ".colors".
	QString filename = name.lower().stripWhiteSpace();
	if ( !filename.isEmpty() ) {
		for( unsigned int i=0; i<filename.length(); ++i)
			if ( filename.at(i)==' ' ) filename.replace( i, 1, "-" );

		filename = filename.append( ".colors" );
		file.setName( locateLocal( "appdata", filename ) ); //determine filename in local user KDE directory tree.

		if ( file.exists() || !file.open( IO_ReadWrite | IO_Append ) ) {
			QString message = i18n( "Local color scheme file could not be opened.\nScheme cannot be recorded." );
			KMessageBox::sorry( 0, message, i18n( "Could not Open File" ) );
			return false;
		} else {
			QTextStream stream( &file );
			stream << StarColorMode << ":" << StarColorIntensity << endl;

			for ( QStringList::Iterator it = KeyName.begin(); it != KeyName.end(); ++it )
				stream << Palette[ (*it) ] << " :" << (*it) << endl;

			file.close();
		}

		file.setName( locateLocal( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.

		if ( !file.open( IO_ReadWrite | IO_Append ) ) {
			QString message = i18n( "Local color scheme index file could not be opened.\nScheme cannot be recorded." );
			KMessageBox::sorry( 0, message, i18n( "Could not Open File" ) );
			return false;
		} else {
			QTextStream stream( &file );
			stream << name << ":" << filename << endl;
			file.close();
		}
	} else {
		QString message = i18n( "Invalid Filename requested.\nScheme cannot be recorded." );
		KMessageBox::sorry( 0, message, i18n( "Invalid Filename" ) );
		return false;
	}

	return true;
}

void ColorScheme::loadFromConfig( KConfig *conf ) {
	for ( QStringList::Iterator it = KeyName.begin(); it != KeyName.end(); ++it )
		setColor( QString(*it), conf->readEntry( QString(*it), QString( *Default.at( KeyName.findIndex(*it) ) ) ) );
}

void ColorScheme::saveToConfig( KConfig *conf ) {
	for ( QStringList::Iterator it = KeyName.begin(); it != KeyName.end(); ++it ) {
		conf->writeEntry( QString(*it), colorNamed( QString(*it) ) );
	}
}

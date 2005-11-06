//Added by qt3to4:
#include <QPixmap>
#include <QTextStream>
#include <Q3PtrList>
/***************************************************************************
                          deepskycomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/17/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

CustomCatalogComponent::CustomCatalogComponent(SkyComponent *parent, bool (*visibleMethod)()) : ListComponent(parent, visibleMethod)
{
}

CustomCatalogComponent::~CustomCatalogComponent()
{
}

void CustomCatalogComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( !Options::showCatalog() ) return;

	SkyMap *map = ks->map();
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );

	//Draw Custom Catalogs
	for ( unsigned int i=0; i<ks->data()->CustomCatalogs.count(); ++i ) { 
		if ( Options::showCatalog()[i] ) {
			QList<SkyObject> cat = ks->data()->CustomCatalogs.at(i)->objList();

			for ( SkyObject *obj = cat.first(); obj; obj = cat.next() ) {

				if ( map->checkVisibility( obj ) ) {
					QPoint o = getXY( obj, Options::useAltAz(), Options::useRefraction(), scale );

					if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {

						if ( obj->type()==0 ) {
							StarObject *starobj = (StarObject*)obj;
							float zoomlim = 7.0 + ( Options::zoomFactor()/MINZOOM)/50.0;
							float mag = starobj->mag();
							float sizeFactor = 2.0;
							int size = int( sizeFactor*(zoomlim - mag) ) + 1;
							if (size>23) size=23;
							if ( size ) {
								QChar c = starobj->color();
								QPixmap *spixmap = starpix->getPixmap( &c, size );
								starobj->draw( psky, sky, spixmap, o.x(), o.y(), true, scale );
							}
						} else {
							//PA for Deep-Sky objects is 90 + PA because major axis is horizontal at PA=0
							DeepSkyObject *dso = (DeepSkyObject*)obj;
							double pa = 90. + findPA( dso, o.x(), o.y(), scale );
							dso->drawImage( psky, o.x(), o.y(), pa, Options::zoomFactor() );

							psky.setBrush( Qt::NoBrush );
							psky.setPen( QColor( data->CustomCatalogs.at(i)->color() ) );

							dso->drawSymbol( psky, o.x(), o.y(), pa, Options::zoomFactor() );
						}
					}
				}
			}
		}
	}
}

bool CustomCatalogComponent::readCustomCatalogs()
{
	bool result = false;

	for ( uint i=0; i < Options::catalogFile().count(); i++ ) {
		bool thisresult = addCatalog( Options::catalogFile()[i] );
		//Make result = true if at least one catalog is added
		result = ( result || thisresult );
	}
	return result;
}

bool CustomCatalogComponent::addCatalog( QString filename )
{
	CustomCatalog *newCat = createCustomCatalog( filename, false );
	if ( newCat ) {
		CustomCatalogs.append( newCat );

		// add object names to ObjNames list
		for ( uint i=0; i < newCat->objList().count(); i++ ) {
//		for ( SkyObject *o=newCat->objList().first(); o; o = newCat->objList().next() ) {
			SkyObject *o = newCat->objList().at(i);
			ObjNames.append( o );
			if ( o->hasLongName() && o->longname() != o->name() ) 
				ObjNames.append( o, true ); //Add long name
		}

		return true;
	} else
		kdWarning() << k_funcinfo << i18n("Error adding catalog: %1").arg( filename ) << endl;

	return false;
}

bool CustomCatalogComponent::removeCatalog( int i )
{
	if ( ! CustomCatalogs.at(i) ) return false;

	Q3PtrList<SkyObject> cat = CustomCatalogs.at(i)->objList();

	for ( SkyObject *o=cat.first(); o; o=cat.next() ) {
		ObjNames.remove( o->name() );
		if ( o->hasLongName() && o->longname() != o->name() ) 
			ObjNames.remove( o->longname() );
	}

	CustomCatalogs.remove(i);

	return true;
}

CustomCatalog* CustomCatalogComponent::createCustomCatalog( QString filename, bool showerrs )
{
	QDir::setCurrent( QDir::homePath() );  //for files with relative path
	Q3PtrList<SkyObject> objList;
	QString CatalogName, CatalogPrefix, CatalogColor;
	float CatalogEpoch;

	//If the filename begins with "~", replace the "~" with the user's home directory
	//(otherwise, the file will not successfully open)
	if ( filename.at(0)=='~' )
		filename = QDir::homePath() + filename.mid( 1, filename.length() );
	QFile ccFile( filename );

	if ( ccFile.open( QIODevice::ReadOnly ) ) {
		int iStart(0); //the line number of the first non-header line
		QStringList errs; //list of error messages 
		QStringList Columns; //list of data column descriptors in the header

		QTextStream stream( &ccFile );
		QStringList lines = QStringList::split( "\n", stream.read() );

		if ( parseCustomDataHeader( lines, Columns, CatalogName, CatalogPrefix, 
				CatalogColor, CatalogEpoch, iStart, showerrs, errs ) ) {
	
			QStringList::Iterator it = lines.begin();
			QStringList::Iterator itEnd  = lines.end();
			it += iStart; //jump ahead past header
		
			for ( uint i=iStart; i < lines.count(); i++ ) {
				QStringList d = QStringList::split( " ", lines[i] );
	
				//Now, if one of the columns is the "Name" field, the name may contain spaces.
				//In this case, the name field will need to be surrounded by quotes.  
				//Check for this, and adjust the d list accordingly
				int iname = Columns.findIndex( "Nm" );
				if ( iname >= 0 && d[iname].left(1) == "\"" ) { //multi-word name in quotes
					d[iname] = d[iname].mid(1); //remove leading quote
					//It's possible that the name is one word, but still in quotes
					if ( d[iname].right(1) == "\"" ) {
						d[iname] = d[iname].left( d[iname].length() - 1 );
					} else {
						int iend = iname + 1;
						while ( d[iend].right(1) != "\"" ) {
							d[iname] += " " + d[iend];
							++iend;
						}
						d[iname] += " " + d[iend].left( d[iend].length() - 1 );
	
						//remove the entries from d list that were the multiple words in the name
						for ( int j=iname+1; j<=iend; j++ ) {
							d.remove( d.at(iname + 1) ); //index is *not* j, because next item becomes "iname+1" after remove
						}
					}
				}
	
				if ( d.count() == Columns.count() ) {
					processCustomDataLine( i, d, Columns, CatalogPrefix, objList, showerrs, errs );
				} else {
					if ( showerrs ) errs.append( i18n( "Line %1 does not contain %2 fields.  Skipping it." ).arg( i ).arg( Columns.count() ) );
				}
			}
		}

		if ( objList.count() ) {
			if ( errs.count() > 0 ) { //some data parsed, but there are errs to report
				QString message( i18n( "Some lines in the custom catalog could not be parsed; see error messages below." ) + "\n" +
												i18n( "To reject the file, press Cancel. " ) +
												i18n( "To accept the file (ignoring unparsed lines), press Accept." ) );
				if ( KMessageBox::warningContinueCancelList( 0, message, errs,
						i18n( "Some Lines in File Were Invalid" ), i18n( "Accept" ) ) != KMessageBox::Continue ) {
					return 0; //User pressed Cancel, return NULL pointer
				}
			}
		} else { //objList.count() == 0
			if ( showerrs ) {
				QString message( i18n( "No lines could be parsed from the specified file, see error messages below." ) );
				KMessageBox::informationList( 0, message, errs,
						i18n( "No Valid Data Found in File" ) );
			}
			return 0; //no valid catalog data parsed, return NULL pointer
		}

	} else { //Error opening catalog file
		if ( showerrs )
			KMessageBox::sorry( 0, i18n( "Could not open custom data file: %1" ).arg( filename ), 
					i18n( "Error opening file" ) );
		else 
			kdDebug() << i18n( "Could not open custom data file: %1" ).arg( filename ) << endl;
	}

	//Return the catalog
	if ( objList.count() )
		return new CustomCatalog( CatalogName, CatalogPrefix, CatalogColor, CatalogEpoch, objList );
	else
		return 0;
}

bool CustomCatalogComponent::processCustomDataLine(int lnum, QStringList d, QStringList Columns, QString Prefix, QList<SkyObject*> &objList, bool showerrs, QStringList &errs )
{

	//object data
	unsigned char iType(0);
	dms RA, Dec;
	float mag(0.0), a(0.0), b(0.0), PA(0.0);
	QString name(""); QString lname("");

	for ( uint i=0; i<Columns.count(); i++ ) {
		if ( Columns[i] == "ID" ) 
			name = Prefix + " " + d[i];

		if ( Columns[i] == "Nm" )
			lname = d[i];

		if ( Columns[i] == "RA" ) {
			if ( ! RA.setFromString( d[i], false ) ) {
				if ( showerrs ) 
					errs.append( i18n( "Line %1, field %2: Unable to parse RA value: %3" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Dc" ) {
			if ( ! Dec.setFromString( d[i], true ) ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Dec value: %1" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Tp" ) {
			bool ok(false);
			iType = d[i].toUInt( &ok );
			if ( ok ) {
				if ( iType == 2 || iType > 8 ) {
					if ( showerrs )
						errs.append( i18n( "Line %1, field %2: Invalid object type: %1" )
								.arg(lnum).arg(i).arg(d[i]) +
								i18n( "Must be one of 0, 1, 3, 4, 5, 6, 7, 8." ) );
					return false;
				}
			} else {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Object type: %1" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Mg" ) {
			bool ok(false);
			mag = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Magnitude: %1" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Mj" ) {
			bool ok(false);
			a = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Major Axis: %1" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "Mn" ) {
			bool ok(false);
			b = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Minor Axis: %1" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}

		if ( Columns[i] == "PA" ) {
			bool ok(false);
			PA = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Position Angle: %1" )
							.arg(lnum).arg(i).arg(d[i]) );
				return false;
			}
		}
	}

	if ( iType == 0 ) { //Add a star
		StarObject *o = new StarObject( RA, Dec, mag, lname );
		objList.append( o );
	} else { //Add a deep-sky object
		DeepSkyObject *o = new DeepSkyObject( iType, RA, Dec, mag, 
					name, "", lname, Prefix, a, b, PA );
		objList.append( o );
	}

	return true;
}

bool CustomCatalogComponent::parseCustomDataHeader( QStringList lines, QStringList &Columns, QString &CatalogName, QString &CatalogPrefix, QString &CatalogColor, float &CatalogEpoch, int &iStart, bool showerrs, QStringList &errs )
{

	bool foundDataColumns( false ); //set to true if description of data columns found
	int ncol( 0 );

	QStringList::Iterator it = lines.begin();
	QStringList::Iterator itEnd  = lines.end();

	CatalogName = "";
	CatalogPrefix = "";
	CatalogColor = "";
	CatalogEpoch = 0.;
	for ( ; it != itEnd; it++ ) {
		QString d( *it ); //current data line
		if ( d.left(1) != "#" ) break;  //no longer in header!

		int iname   = d.find( "# Name: " );
		int iprefix = d.find( "# Prefix: " );
		int icolor  = d.find( "# Color: " );
		int iepoch  = d.find( "# Epoch: " );

		if ( iname == 0 ) { //line contains catalog name
			iname = d.find(":")+2;
			if ( CatalogName.isEmpty() ) { 
				CatalogName = d.mid( iname );
			} else { //duplicate name in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) + 
							i18n( "Extra Name field in header: %1.  Will be ignored" ).arg( d.mid(iname) ) );
			}
		} else if ( iprefix == 0 ) { //line contains catalog prefix
			iprefix = d.find(":")+2;
			if ( CatalogPrefix.isEmpty() ) { 
				CatalogPrefix = d.mid( iprefix );
			} else { //duplicate prefix in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) + 
							i18n( "Extra Prefix field in header: %1.  Will be ignored" ).arg( d.mid(iprefix) ) );
			}
		} else if ( icolor == 0 ) { //line contains catalog prefix
			icolor = d.find(":")+2;
			if ( CatalogColor.isEmpty() ) { 
				CatalogColor = d.mid( icolor );
			} else { //duplicate prefix in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) + 
							i18n( "Extra Color field in header: %1.  Will be ignored" ).arg( d.mid(icolor) ) );
			}
		} else if ( iepoch == 0 ) { //line contains catalog epoch
			iepoch = d.find(":")+2;
			if ( CatalogEpoch == 0. ) {
				bool ok( false );
				CatalogEpoch = d.mid( iepoch ).toFloat( &ok );
				if ( !ok ) {
					if ( showerrs )
						errs.append( i18n( "Parsing header: " ) + 
								i18n( "Could not convert Epoch to float: %1.  Using 2000. instead" ).arg( d.mid(iepoch) ) );
					CatalogEpoch = 2000.; //adopt default value
				}
			} else { //duplicate epoch in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) + 
							i18n( "Extra Epoch field in header: %1.  Will be ignored" ).arg( d.mid(iepoch) ) );
			}
		} else if ( ! foundDataColumns ) { //don't try to parse data column descriptors if we already found them
			//Chomp off leading "#" character
			d = d.replace( QRegExp( "#" ), "" );

			QStringList fields = QStringList::split( " ", d ); //split on whitespace

			//we need a copy of the master list of data fields, so we can 
			//remove fields from it as they are encountered in the "fields" list.
			//this allows us to detect duplicate entries
			QStringList master( KStarsData::CustomColumns ); 

			QStringList::Iterator itf    = fields.begin();
			QStringList::Iterator itfEnd = fields.end();

			for ( ; itf != itfEnd; itf++ ) {
				QString s( *itf );
				if ( master.contains( s ) ) {
					//add the data field
					Columns.append( s );

					// remove the field from the master list and inc the 
					// count of "good" columns (unless field is "Ignore")
					if ( s != "Ig" ) {
						master.remove( master.find( s ) );
						ncol++;
					}
				} else if ( fields.contains( s ) ) { //duplicate field
					fields.append( "Ig" ); //skip this column
					if ( showerrs )
						errs.append( i18n( "Parsing header: " ) + 
								i18n( "Duplicate data field descriptor \"%1\" will be ignored" ).arg( s ) );
				} else { //Invalid field
					fields.append( "Ig" ); //skip this column
					if ( showerrs )
						errs.append( i18n( "Parsing header: " ) + 
								i18n( "Invalid data field descriptor \"%1\" will be ignored" ).arg( s ) );
				}
			}

			if ( ncol ) foundDataColumns = true;
		}
	}

	if ( ! foundDataColumns ) {
		if ( showerrs )
			errs.append( i18n( "Parsing header: " ) + 
					i18n( "No valid column descriptors found.  Exiting" ) );
		return false;
	}

	if ( it == itEnd ) {
		if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No data lines found after header.  Exiting." ) );
		return false; //fatal error
	} else {
		//Make sure Name, Prefix, Color and Epoch were set
		if ( CatalogName.isEmpty() ) { 
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Name specified; setting to \"Custom\"" ) );
			CatalogName = i18n("Custom");
		}
		if ( CatalogPrefix.isEmpty() ) { 
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Prefix specified; setting to \"CC\"" ) );
			CatalogPrefix = "CC";
		}
		if ( CatalogColor.isEmpty() ) { 
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Color specified; setting to Red" ) );
			CatalogColor = "#CC0000";
		}
		if ( CatalogEpoch == 0. ) { 
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Epoch specified; assuming 2000." ) );
			CatalogEpoch = 2000.;
		}

		//the it iterator now points to the first line past the header
		iStart = lines.findIndex( QString( *it ) );
		return true;
	}
}

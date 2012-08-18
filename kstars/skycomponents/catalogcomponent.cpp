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

#include "catalogcomponent.h"

#include <QDir>
#include <QFile>
#include <QPixmap>
#include <QTextStream>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>

#include "Options.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/starobject.h"
#include "skyobjects/deepskyobject.h"
#include "skypainter.h"

QStringList CatalogComponent::m_Columns = QString( "ID RA Dc Tp Nm Mg Flux Mj Mn PA Ig" ).split( ' ', QString::SkipEmptyParts );

CatalogComponent::CatalogComponent(SkyComposite *parent, const QString &fname, bool showerrs, int index) :
    ListComponent(parent),
    m_Filename( fname ),
    m_Showerrs( showerrs ),
    m_ccIndex(index)
{
    loadData();
}

CatalogComponent::~CatalogComponent()
{
}

//TODO(spacetime): Save to DB using KSParser
//TODO(spacetime): Load from DB
//TODO(spacetime): Remove previous code

bool CatalogComponent::addCatalogContents(const QString& fname) {
    QDir::setCurrent( QDir::homePath() );  //for files with relative path
    QString filename = fname;
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
        // TODO(spacetime) : Decide appropriate number of lines to be read
        QStringList lines;
        for (int times=5; times >= 0; --times)
          lines.append(stream.readLine());
        /*WAS
         * = stream.readAll().split( '\n', QString::SkipEmptyParts );
         * Memory Hog!
         */

        bool showerrs = false;
        if ( !parseCatalogInfoToDB( lines, Columns) ) {
            kWarning() << "Incorrect header in catalog file: " << filename;
            return false;
        }
        
        /*
         * Now 'Columns' should be a StringList of the Header contents
         * Hence, we 1) Convert the Columns to a KSParser compatible format
         *           2) Use KSParser to read stuff
         */
        
        //Part 1) Conversion to KSParser compatible format
        QList< QPair<QString, KSParser::DataTypes> > sequence = 
                                            buildParserSequence(Columns);
        KSParser catalog_text_parser(filename, '#', sequence);

        QHash<QString, QVariant> row_content;
        while (catalog_text_parser.HasNextRow()){
          row_content = catalog_text_parser.ReadNextRow();
        }
    }
    /* NEW CODE ENDS */
    return true;
}


void CatalogComponent::loadData()
{
    emitProgressText( i18n("Loading custom catalog: %1", m_Filename ) );

/*    QFile ccFile( m_Filename );

    if ( ccFile.open( QIODevice::ReadOnly ) ) {
        int iStart(0); //the line number of the first non-header line
        QStringList errs; //list of error messages
        QStringList Columns; //list of data column descriptors in the header

        QTextStream stream( &ccFile );
        QStringList lines = stream.readAll().split( '\n', QString::SkipEmptyParts );

        if ( parseCustomDataHeader( lines, Columns, iStart, m_Showerrs, errs ) ) {

            for ( int i=iStart; i < lines.size(); ++i ) {
                QStringList d = lines.at(i).split( ' ', QString::SkipEmptyParts );

                //Now, if one of the columns is the "Name" field, the name may contain spaces.
                //In this case, the name field will need to be surrounded by quotes.
                //Check for this, and adjust the d list accordingly
                int iname = Columns.indexOf( "Nm" );
                if ( iname >= 0 && d[iname].left(1) == "\"" ) { //multi-word name in quotes
                    d[iname] = d[iname].mid(1); //remove leading quote
                    //It's possible that the name is one word, but still in quotes
                    if ( d[iname].right(1) == "\"" ) {
                        d[iname] = d[iname].left( d[iname].length() - 1 );
                    } else {
                        int iend = iname + 1;
                        while ( d[iend].right(1) != "\"" ) {
                            d[iname] += ' ' + d[iend];
                            ++iend;
                        }
                        d[iname] += ' ' + d[iend].left( d[iend].length() - 1 );

                        //remove the entries from d list that were the multiple words in the name
                        for ( int j=iname+1; j<=iend; j++ ) {
                            d.removeAll( d.at(iname + 1) ); //index is *not* j, because next item becomes "iname+1" after remove
                        }
                    }
                }

                if ( d.size() == Columns.size() ) {
                    processCustomDataLine( i, d, Columns, m_Showerrs, errs );
                } else {
                    if ( m_Showerrs ) errs.append( i18n( "Line %1 does not contain %2 fields.  Skipping it.", i, Columns.size() ) );
                }
            }
        }

        if ( m_ObjectList.size() ) {
            if ( errs.size() > 0 ) { //some data parsed, but there are errs to report
                QString message( i18n( "Some lines in the custom catalog could not be parsed; see error messages below." ) + '\n' +
                                 i18n( "To reject the file, press Cancel. " ) +
                                 i18n( "To accept the file (ignoring unparsed lines), press Accept." ) );
                if ( KMessageBox::warningContinueCancelList( 0, message, errs,
                        i18n( "Some Lines in File Were Invalid" ), KGuiItem( i18n( "Accept" ) ) ) != KMessageBox::Continue ) {
                    m_ObjectList.clear();
                    return ;
                }
            }
        } else {
            if ( m_Showerrs ) {
                QString message( i18n( "No lines could be parsed from the specified file, see error messages below." ) );
                KMessageBox::informationList( 0, message, errs,
                                              i18n( "No Valid Data Found in File" ) );
            }
            m_ObjectList.clear();
            return;
        }
        */

//     } else { //Error opening catalog file
//         if ( m_Showerrs )
//             KMessageBox::sorry( 0, i18n( "Could not open custom data file: %1", m_Filename ),
//                                 i18n( "Error opening file" ) );
//         else
//             kDebug() << i18n( "Could not open custom data file: %1", m_Filename );
//     }
}

void CatalogComponent::update( KSNumbers * )
{
    if ( selected() ) {
        KStarsData *data = KStarsData::Instance();
        foreach ( SkyObject *obj, m_ObjectList ) {
            DeepSkyObject *dso  = dynamic_cast< DeepSkyObject * >( obj );
            StarObject *so = dynamic_cast< StarObject *>( so );
            Q_ASSERT( dso || so ); // We either have stars, or deep sky objects
            if( dso ) {
                // Update the deep sky object if need be
                if ( dso->updateID != data->updateID() ) {
                    dso->updateID = data->updateID();
                    if ( dso->updateNumID != data->updateNumID() ) {
                        dso->updateCoords( data->updateNum() );

                    }
                    dso->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                }
            }
            else {
                // Do exactly the same thing for stars
                if ( so->updateID != data->updateID() ) {
                    so->updateID = data->updateID();
                    if ( so->updateNumID != data->updateNumID() ) {
                        so->updateCoords( data->updateNum() );
                    }
                    so->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                }
            }
        }
        this->updateID = data->updateID();
    }
}

void CatalogComponent::draw( SkyPainter *skyp )
{
    if ( ! selected() ) return;

    skyp->setBrush( Qt::NoBrush );
    skyp->setPen( QColor( m_catColor ) );

    // Check if the coordinates have been updated
    if( updateID != KStarsData::Instance()->updateID() )
        update( 0 );

    //Draw Custom Catalog objects
    foreach ( SkyObject *obj, m_ObjectList ) {
        if ( obj->type()==0 ) {
            StarObject *starobj = (StarObject*)obj;
            //FIXME_SKYPAINTER
            skyp->drawPointSource(starobj, starobj->mag(), starobj->spchar() );
        } else {
            //FIXME: this PA calc is totally different from the one that was in
            //DeepSkyComponent which is now in SkyPainter .... O_o
            //      --hdevalence
            //PA for Deep-Sky objects is 90 + PA because major axis is horizontal at PA=0
            //double pa = 90. + map->findPA( dso, o.x(), o.y() );
            DeepSkyObject *dso = (DeepSkyObject*)obj;
            skyp->drawDeepSkyObject(dso,true);
        }
    }
}

bool CatalogComponent::parseCatalogInfoToDB( const QStringList &lines, QStringList &Columns )
{

    bool foundDataColumns = false; //set to true if description of data columns found
    int ncol = 0;

    QStringList errs;
    QString catName, catPrefix, catColor, catFluxFreq, catFluxUnit;
    float catEpoch;
    bool showerrs = false;

    catName.clear();
    catPrefix.clear();
    catColor.clear();
    catFluxFreq.clear();
    catFluxUnit.clear();
    catEpoch = 0.;

    int i=0;
    for ( ; i < lines.size(); ++i ) {
        QString d( lines.at(i) ); //current data line
        if ( d.left(1) != "#" ) break;  //no longer in header!

        int iname      = d.indexOf( "# Name: " );
        int iprefix    = d.indexOf( "# Prefix: " );
        int icolor     = d.indexOf( "# Color: " );
        int iepoch     = d.indexOf( "# Epoch: " );
        int ifluxfreq  = d.indexOf( "# Flux Frequency: ");
        int ifluxunit  = d.indexOf( "# Flux Unit: ");

        if ( iname == 0 ) { //line contains catalog name
            iname = d.indexOf(":")+2;
            if ( catName.isEmpty() ) {
                catName = d.mid( iname );
            } else { //duplicate name in header
                if ( showerrs )
                    errs.append( i18n( "Parsing header: " ) +
                                 i18n( "Extra Name field in header: %1.  Will be ignored", d.mid(iname) ) );
            }
        } else if ( iprefix == 0 ) { //line contains catalog prefix
            iprefix = d.indexOf(":")+2;
            if ( catPrefix.isEmpty() ) {
                catPrefix = d.mid( iprefix );
            } else { //duplicate prefix in header
                if ( showerrs )
                    errs.append( i18n( "Parsing header: " ) +
                                 i18n( "Extra Prefix field in header: %1.  Will be ignored", d.mid(iprefix) ) );
            }
        } else if ( icolor == 0 ) { //line contains catalog prefix
            icolor = d.indexOf(":")+2;
            if ( catColor.isEmpty() ) {
                 catColor = d.mid( icolor );
            } else { //duplicate prefix in header
                if ( showerrs )
                    errs.append( i18n( "Parsing header: " ) +
                                 i18n( "Extra Color field in header: %1.  Will be ignored", d.mid(icolor) ) );
            }
        } else if ( iepoch == 0 ) { //line contains catalog epoch
            iepoch = d.indexOf(":")+2;
            if ( catEpoch == 0. ) {
                bool ok( false );
                 catEpoch = d.mid( iepoch ).toFloat( &ok );
                if ( !ok ) {
                    if ( showerrs )
                        errs.append( i18n( "Parsing header: " ) +
                                     i18n( "Could not convert Epoch to float: %1.  Using 2000. instead", d.mid(iepoch) ) );
                    catEpoch = 2000.; //adopt default value
                }
            }
        } else if ( ifluxfreq == 0 )
        { //line contains catalog flux frequnecy
            ifluxfreq = d.indexOf(":")+2;
            if ( catFluxFreq.isEmpty() )
            {
                catFluxFreq = d.mid( ifluxfreq );
            } else { //duplicate prefix in header
                if ( showerrs )
                    errs.append( i18n( "Parsing header: " ) +
                                 i18n( "Extra Flux Frequency field in header: %1.  Will be ignored", d.mid(ifluxfreq) ) );
            }
        } else if ( ifluxunit == 0 )
            { //line contains catalog flux unit
                       ifluxunit = d.indexOf(":")+2;
                       if ( catFluxUnit.isEmpty() )
                       {
                           catFluxUnit = d.mid( ifluxunit );
                       } else { //duplicate prefix in header
                           if ( showerrs )
                               errs.append( i18n( "Parsing header: " ) +
                                            i18n( "Extra Flux Unit field in header: %1.  Will be ignored", d.mid(ifluxunit) ) );
             }

        } else if ( ! foundDataColumns ) { //don't try to parse data column descriptors if we already found them
            //Chomp off leading "#" character
            d = d.remove( '#' );

            Columns.clear();
            QStringList fields = d.split( ' ', QString::SkipEmptyParts ); //split on whitespace

            //we need a copy of the master list of data fields, so we can
            //remove fields from it as they are encountered in the "fields" list.
            //this allows us to detect duplicate entries
            QStringList master( Columns );

            for ( int j=0; j < fields.size(); ++j ) {
                QString s( fields.at(j) );
                if ( master.contains( s ) ) {
                    //add the data field
                    Columns.append( s );

                    // remove the field from the master list and inc the
                    // count of "good" columns (unless field is "Ignore")
                    if ( s != "Ig" ) {
                        master.removeAt( master.indexOf( s ) );
                        ncol++;
                    }
                } else if ( fields.contains( s ) ) { //duplicate field
                    fields.append( "Ig" ); //ignore the duplicate column
                    if ( showerrs )
                        errs.append( i18n( "Parsing header: " ) +
                                     i18n( "Duplicate data field descriptor \"%1\" will be ignored", s ) );
                } else { //Invalid field
                    fields.append( "Ig" ); //ignore the invalid column
                    if ( showerrs )
                        errs.append( i18n( "Parsing header: " ) +
                                     i18n( "Invalid data field descriptor \"%1\" will be ignored", s ) );
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

    if ( i == lines.size() ) {
        if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
                                         i18n( "No data lines found after header.  Exiting." ) );
        return false;
    } else {
        //Make sure Name, Prefix, Color and Epoch were set
        if ( catName.isEmpty() ) {
            if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
                                             i18n( "No Catalog Name specified; setting to \"Custom\"" ) );
            catName = i18n("Custom");
        }
        if ( catPrefix.isEmpty() ) {
            if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
                                             i18n( "No Catalog Prefix specified; setting to \"CC\"" ) );
            catPrefix = "CC";
        }
        if ( catColor.isEmpty() ) {
            if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
                                             i18n( "No Catalog Color specified; setting to Red" ) );
            catColor = "#CC0000";
        }
        if ( catEpoch == 0. ) {
            if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
                                             i18n( "No Catalog Epoch specified; assuming 2000." ) );
            catEpoch = 2000.;
        }

        //index i now points to the first line past the header
//         iStart = i;
        return true;
    }
}

bool CatalogComponent::processCustomDataLine(int lnum, const QStringList &d, const QStringList &Columns, bool showerrs, QStringList &errs )
{

    //object data
    unsigned char iType(0);
    dms RA, Dec;
    float mag(0.0), a(0.0), b(0.0), PA(0.0), flux(0.0);
    QString name, lname;

    for ( int i=0; i<Columns.size(); i++ ) {
        if ( Columns.at(i) == "ID" )
            name = m_catPrefix + ' ' + d.at(i);

        if ( Columns.at(i) == "Nm" )
            lname = d.at(i);

        if ( Columns[i] == "RA" ) {
            if ( ! RA.setFromString( d.at(i), false ) ) {
                if ( showerrs )
                    errs.append( i18n( "Line %1, field %2: Unable to parse RA value: %3" ,
                                       lnum, i, d.at(i) ) );
                return false;
            }
        }

        if ( Columns.at(i) == "Dc" ) {
            if ( ! Dec.setFromString( d.at(i), true ) ) {
                if ( showerrs )
                    errs.append( i18n( "Line %1, field %2: Unable to parse Dec value: %3" ,
                                       lnum, i, d.at(i) ) );
                return false;
            }
        }

        if ( Columns.at(i) == "Tp" ) {
            bool ok(false);
            iType = d.at(i).toUInt( &ok );
            if ( ok ) {
                if ( iType == 2 || (iType > 8 && iType < 13) || iType == 19 || iType == 20 ) {
                    if ( showerrs )
                        errs.append( i18n( "Line %1, field %2: Invalid object type: %3" ,
                                           lnum, i, d.at(i) ) +
                                     i18n( "Must be one of 0, 1, 3, 4, 5, 6, 7, 8, 18" ) );
                    return false;
                }
            } else {
                if ( showerrs )
                    errs.append( i18n( "Line %1, field %2: Unable to parse Object type: %3" ,
                                       lnum, i, d.at(i) ) );
                return false;
            }
        }

        if ( Columns.at(i) == "Mg" ) {
            bool ok(false);
            mag = d.at(i).toFloat( &ok );
            if ( ! ok ) {
                if ( showerrs )
                    errs.append( i18n( "Line %1, field %2: Unable to parse Magnitude: %3" ,
                                       lnum, i, d.at(i) ) );
                return false;
            }
        }

        if ( Columns.at(i) == "Flux" ) {
            bool ok(false);
            flux = d.at(i).toFloat( &ok );
            if ( ! ok ) {
                if ( showerrs )
                    errs.append( i18n( "Line %1, field %2: Unable to parse Flux: %3" ,
                                       lnum, i, d.at(i) ) );
                return false;
            }
        }

        if ( Columns.at(i) == "Mj" ) {
            bool ok(false);
            a = d[i].toFloat( &ok );
            if ( ! ok ) {
                if ( showerrs )
                    errs.append( i18n( "Line %1, field %2: Unable to parse Major Axis: %3" ,
                                       lnum, i, d.at(i) ) );
                return false;
            }
        }

        if ( Columns.at(i) == "Mn" ) {
            bool ok(false);
            b = d[i].toFloat( &ok );
            if ( ! ok ) {
                if ( showerrs )
                    errs.append( i18n( "Line %1, field %2: Unable to parse Minor Axis: %3" ,
                                       lnum, i, d.at(i) ) );
                return false;
            }
        }

        if ( Columns.at(i) == "PA" ) {
            bool ok(false);
            PA = d[i].toFloat( &ok );
            if ( ! ok ) {
                if ( showerrs )
                    errs.append( i18n( "Line %1, field %2: Unable to parse Position Angle: %3" ,
                                       lnum, i, d.at(i) ) );
                return false;
            }
        }
    }

    // Precess the catalog coordinates to J2000.0
    SkyPoint t;
    t.set( RA, Dec );
    if( m_catEpoch == 1950 ) {
        // Assume B1950 epoch
        t.B1950ToJ2000(); // t.ra() and t.dec() are now J2000.0 coordinates
    }
    else if( m_catEpoch == 2000 ) {
        // Do nothing
        ;
    }
    else {
        // FIXME: What should we do?
        // FIXME: This warning will be printed for each line in the catalog rather than once for the entire catalog
        kWarning() << "Unknown epoch while dealing with custom catalog."
                      "Will ignore the epoch and assume J2000.0";
    }
    RA = t.ra();
    Dec = t.dec();

    if ( iType == 0 ) { //Add a star
        StarObject *o = new StarObject( RA, Dec, mag, lname );
        m_ObjectList.append( o );
    } else { //Add a deep-sky object
        DeepSkyObject *o = new DeepSkyObject( iType, RA, Dec, mag,
                                              name, QString(), lname, m_catPrefix, a, b, PA );
        o->setFlux(flux);
        o->setCustomCatalog(this);

        m_ObjectList.append( o );

        //Add name to the list of object names
        if ( ! name.isEmpty() )
            objectNames(iType).append( name );
    }
    if ( ! lname.isEmpty() && lname != name )
        objectNames(iType).append( lname );

    return true;
}


QList< QPair<QString, KSParser::DataTypes> > CatalogComponent::
                              buildParserSequence(const QStringList& Columns)
{
    QList< QPair<QString, KSParser::DataTypes> > sequence;
    QStringList::const_iterator iter = Columns.begin();

    while (iter != Columns.end()) {
    // Available Types: ID RA Dc Tp Nm Mg Flux Mj Mn PA Ig
      KSParser::DataTypes current_type;

      if (*iter == QString("ID"))
          current_type = KSParser::D_QSTRING;
      else if (*iter == QString("RA"))
          current_type = KSParser::D_DOUBLE;
      else if (*iter == QString("Dc"))
          current_type = KSParser::D_DOUBLE;
      else if (*iter == QString("Tp"))
          current_type = KSParser::D_INT;
      else if (*iter == QString("Nm"))
          current_type = KSParser::D_QSTRING;
      else if (*iter == QString("Mg"))
          current_type = KSParser::D_FLOAT;
      else if (*iter == QString("Flux"))
          current_type = KSParser::D_FLOAT;
      else if (*iter == QString("Mj"))
          current_type = KSParser::D_FLOAT;
      else if (*iter == QString("Mn"))
          current_type = KSParser::D_FLOAT;
      else if (*iter == QString("PA"))
          current_type = KSParser::D_FLOAT;
      else if (*iter == QString("Ig"))
          current_type = KSParser::D_SKIP;

      sequence.append(qMakePair(*iter, current_type));
      ++iter;
    }

    return sequence;
}

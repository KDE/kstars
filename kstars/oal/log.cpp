/***************************************************************************
                          log.cpp  -  description

                             -------------------
    begin                : Friday June 19, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "log.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"
#include "skymap.h"
#include "skycomponents/constellationboundarylines.h"
#include "skycomponents/skymapcomposite.h"
#include "kstarsdatetime.h"
#include "kstandarddirs.h"
#include "observer.h"
#include "site.h"
#include "session.h"
#include "scope.h"
#include "eyepiece.h"
#include "filter.h"
#include "lens.h"
#include "observation.h"
#include "observationtarget.h"

using namespace OAL;

void Log::writeBegin() {
    ks = KStars::Instance();
    output.clear();
    //m_targetList = ks->observingList()->sessionList();
    writer = new QXmlStreamWriter( &output );
    writer->setAutoFormatting( true );
    writer->writeStartDocument();
    writer->writeNamespace( "http://groups.google.com/group/openastronomylog", "oal" );
    writer->writeNamespace( "http://www.w3.org/2001/XMLSchema-instance", "xsi" );
    writer->writeNamespace( "http://groups.google.com/group/openastronomylog", "schemaLocation" );
    writer->writeStartElement( "oal:observations" );
    writer->writeAttribute( "version", "2.0" );
}

QString Log::writeLog( bool _native ) {
    native = _native;
    markUsedObservers();
    markUsedEquipment();

    writeBegin();
    if( native )
        writeGeoDate();
    writeUsedObservers();
    writeSites();
    writeSessions();
    writeTargets();
    writeUsedScopes();
    writeUsedEyepieces();
    writeUsedLenses();
    writeUsedFilters();
    writeImagers();
    writeObservations();
    writeEnd();
    return output;
}

void Log::writeEnd() {
    writer->writeEndDocument();
    delete writer;
}

void Log::writeObservers() {
    writer->writeStartElement( "observers" );
    foreach( Observer *o, m_observerList ) {
        writeObserver( o );
    }
    writer->writeEndElement();
}

void Log::writeUsedObservers() {
    writer->writeStartElement( "observers" );
    foreach( Observer *o, m_observerList ) {
        if( m_usedObservers.contains( o->id() ) )
        writeObserver( o );
    }
    writer->writeEndElement();
}

void Log::writeSites() {
    writer->writeStartElement( "sites" );
    foreach( Site *o, m_siteList )
        writeSite( o );
    writer->writeEndElement();
}

void Log::writeSessions() {
    writer->writeStartElement( "sessions" );
    foreach( Session *o, m_sessionList )
        writeSession( o );
    writer->writeEndElement();
}

void Log::writeTargets() {
    writer->writeStartElement( "targets" );
    foreach( ObservationTarget *t, m_targetList ) {
        writeTarget( t );
    }
    writer->writeEndElement();
}

void Log::writeScopes() {
    writer->writeStartElement( "scopes" );
    foreach( Scope *o, m_scopeList ) {
        writeScope( o );
    }
    writer->writeEndElement();
}

void Log::writeUsedScopes() {
    writer->writeStartElement( "scopes" );
    foreach( Scope *o, m_scopeList ) {
        if( m_usedScopes.contains( o->id() ) )
            writeScope( o );
    }
    writer->writeEndElement();
}

void Log::writeEyepieces() {
    writer->writeStartElement( "eyepieces" );
    foreach( Eyepiece *o, m_eyepieceList ) {
        writeEyepiece( o );
    }
    writer->writeEndElement();
}

void Log::writeUsedEyepieces() {
    writer->writeStartElement( "eyepieces" );
    foreach( Eyepiece *o, m_eyepieceList ) {
        if( m_usedEyepieces.contains( o->id() ) )
            writeEyepiece( o );
    }
    writer->writeEndElement();
}

void Log::writeLenses() {
    writer->writeStartElement( "lenses" );
    foreach( Lens *o, m_lensList ) {
        writeLens( o );
    }
    writer->writeEndElement();
}

void Log::writeUsedLenses() {
    writer->writeStartElement( "lenses" );
    foreach( Lens *o, m_lensList ) {
        if( m_usedLens.contains( o->id() ) )
            writeLens( o );
    }
    writer->writeEndElement();
}

void Log::writeFilters() {
    writer->writeStartElement( "filters" );
    foreach( Filter *o, m_filterList ) {
        writeFilter( o );
    }
    writer->writeEndElement();
}

void Log::writeUsedFilters() {
    writer->writeStartElement( "filters" );
    foreach( Filter *o, m_filterList ) {
        if( m_usedFilters.contains( o->id() ) )
            writeFilter( o );
    }
    writer->writeEndElement();
}

void Log::writeImagers() {
    writer->writeStartElement( "imagers" );
    writer->writeEndElement();
}

void Log::writeObservations() {
    foreach( Observation *o, m_observationList )
        writeObservation( o );
}

void Log::writeTarget( ObservationTarget *t ) {
    writer->writeStartElement( "target" );
    writer->writeAttribute( "id", "target_" + m_targetList.indexOf(t) );
//    QString typeString;
//    if( native )
//        writer->writeAttribute( "type", o->typeName() );
//    else {
//        switch( o->type() ) {
//            case 0: typeString = "oal:starTargetType"; break;
//            case 1: typeString = "oal:starTargetType"; break;
//            case 2: typeString = "oal:PlanetTargetType"; break;
//            case 3: typeString = "oal:deepSkyOC"; break;
//            case 4: typeString = "oal:deepSkyGC"; break;
//            case 5: typeString = "oal:deepSkyGN"; break;
//            case 6: typeString = "oal:deepSkyPN"; break;
//            case 8: typeString = "oal:deepSkyGX"; break;
//            case 9: typeString = "oal:CometTargetType"; break;
//            case 12: typeString = "oal:MoonTargetType"; break;
//            case 13: typeString = "oal:deepSkyAS"; break;
//            case 14: typeString = "oal:deepSkyCG"; break;
//            case 15: typeString = "oal:deepSkyDN"; break;
//            case 16: typeString = "oal:deepSkyQS"; break;
//            case 17: typeString = "oal:deepSkyMS"; break;
//            default: typeString = "oal:deepSkyNA"; break;
//        }
//        writer->writeAttribute( "xsi:type", typeString );
//    }

    if( t->fromDatasource() ) {
        writer->writeStartElement( "datasource" );
        writer->writeCDATA( t->datasource() );
        writer->writeEndElement();
    } else {
        writer->writeStartElement( "observer" );
        writer->writeCharacters( t->observer() );
        writer->writeEndElement();
    }

    writer->writeStartElement( "position" );
    writer->writeStartElement( "ra" );
    writer->writeAttribute( "unit", "rad" );
    writer->writeCharacters( t->position().ra().toDMSString() );
    writer->writeEndElement();
    writer->writeStartElement( "dec" );
    writer->writeAttribute( "unit", "rad" );
    writer->writeCharacters( t->position().dec().toDMSString() );
    writer->writeEndElement();
    writer->writeEndElement();

//    if( native && ! ks->observingList()->getTime( o ).isEmpty() ) {
//        writer->writeStartElement("time");
//        writer->writeCDATA( ks->observingList()->getTime( o ) );
//        writer->writeEndElement();
//    }

    writer->writeStartElement( "constellation" );
    writer->writeCDATA( t->constellation() );
    writer->writeEndElement();

    writer->writeStartElement( "notes" );
    writer->writeCDATA( t->notes() );
    writer->writeEndElement();

    writer->writeEndElement();
}

void Log::writeObserver( Observer *o ) {
    writer->writeStartElement( "observer" );
    writer->writeAttribute( "id", o->id() );
    writer->writeStartElement( "name" );
    writer->writeCDATA( o->name() );
    writer->writeEndElement();
    writer->writeStartElement( "surname" );
    writer->writeCDATA( o->surname() );
    writer->writeEndElement();
    writer->writeStartElement( "contact" );
    writer->writeCDATA( o->contact() );
    writer->writeEndElement();
    writer->writeEndElement();
}
void Log::writeSite( Site *s ) {
    writer->writeStartElement( "site" );
    writer->writeAttribute( "id", s->id() );
    writer->writeStartElement( "name" );
    writer->writeCDATA( s->name() );
    writer->writeEndElement();
    writer->writeStartElement( "longitude" );
    writer->writeAttribute( "unit", s->lonUnit() );
    writer->writeCharacters( QString::number( s->longitude() ) );
    writer->writeEndElement();
    writer->writeStartElement( "latitude" );
    writer->writeAttribute( "unit",  s->latUnit() );
    writer->writeCharacters( QString::number( s->latitude() ) );
    writer->writeEndElement();
    writer->writeStartElement( "timezone" );
    writer->writeCharacters( QString::number( 0 ) );
    writer->writeEndElement();
    writer->writeEndElement();
}
void Log::writeSession( Session *s ) {
    writer->writeStartElement( "session" );
    writer->writeAttribute( "id", s->id() ); 
    writer->writeStartElement( "begin" );
    writer->writeCharacters( s->begin().date().toString( "yyyy-MM-dd" ) + 'T' + s->begin().time().toString( "hh:mm:ss" ) );
    writer->writeEndElement();
    writer->writeStartElement( "end" );
    writer->writeCharacters( s->end().date().toString( "yyyy-MM-dd" ) + 'T' + s->end().time().toString( "hh:mm:ss" ) );
    writer->writeEndElement();
    writer->writeStartElement( "site" );
    writer->writeCharacters( s->site() );
    writer->writeEndElement();

    foreach( QString coObs, s->coobservers() ) {
        writer->writeStartElement( "coObserver" );
        writer->writeCharacters( coObs );
        writer->writeEndElement();
    }

    writer->writeStartElement( "weather" );
    writer->writeCDATA( s->weather() );
    writer->writeEndElement();
    writer->writeStartElement( "equipment" );
    writer->writeCDATA( s->equipment() );
    writer->writeEndElement();
    writer->writeStartElement( "comments" );
    writer->writeCDATA( s->comments() );
    writer->writeEndElement();
    writer->writeEndElement();
}
void Log::writeScope( Scope *scope ) {
    // Write Scope
    writer->writeStartElement( "scope" );
    writer->writeAttribute( "id", scope->id() );

    // Write Model
    writer->writeStartElement( "model" );
    writer->writeCDATA( scope->model() );
    writer->writeEndElement();

    // Write Type
    if( !scope->type().isEmpty() ) {
        writer->writeStartElement( "type" );
        writer->writeCDATA( scope->type().at(0) );
        writer->writeEndElement();
    }

    // Write Vendor
    if( !scope->vendor().isEmpty() ) {
        writer->writeStartElement( "vendor" );
        writer->writeCDATA( scope->vendor() );
        writer->writeEndElement();
    }

    // Write Driver // FIXME: driver element is incompatible with OAL 2.0 schema
//    if (s->driver() != i18n("None"))
//    {
//        writer->writeStartElement( "driver" );
//        writer->writeCharacters( s->driver());
//        writer->writeEndElement();
//    }

    // Write Aperture
    writer->writeStartElement( "aperture" );
    writer->writeCharacters( QString::number( scope->aperture() ) );
    writer->writeEndElement();

    // Write Light Grasp
    if(scope->isLightGraspDefined()) {
        writer->writeStartElement( "lightGrasp" );
        writer->writeCharacters( QString::number( scope->lightGrasp() ) );
        writer->writeEndElement();
    }

    // Write Orientation
    if(scope->isOrientationDefined()) {
        writer->writeStartElement( "orientation" );
        writer->writeAttribute( "erect", scope->orientationErect() ? "true" : "false" );
        writer->writeAttribute( "truesided", scope->orientationTruesided() ? "true" : "false" );
        writer->writeEndElement();
    }

    // Write Focal Length
    writer->writeStartElement( "focalLength" );
    writer->writeCharacters( QString::number( scope->focalLength() ) );
    writer->writeEndElement();

    writer->writeEndElement();
}
void Log::writeEyepiece( Eyepiece *eyepiece )
{
    // Write Eyepiece
    writer->writeStartElement( "eyepiece" );
    writer->writeAttribute( "id", eyepiece->id() );

    // Write Model
    writer->writeStartElement( "model" );
    writer->writeCDATA( eyepiece->model() );
    writer->writeEndElement();

    // Write Vendor
    if( !eyepiece->vendor().isEmpty() ) {
        writer->writeStartElement( "vendor" );
        writer->writeCDATA( eyepiece->vendor() );
        writer->writeEndElement();
    }

    // Write Focal Length
    writer->writeStartElement( "focalLength" );
    writer->writeCharacters( QString::number( eyepiece->focalLength() ) );
    writer->writeEndElement();

    // Write Max Focal Length
    if(eyepiece->isMaxFocalLengthDefined()) {
        writer->writeStartElement( "maxFocalLength" );
        writer->writeCharacters( QString::number( eyepiece->maxFocalLength() ) );
        writer->writeEndElement();
    }

    // Write Apparent FOV
    if(eyepiece->isFovDefined()) {
        writer->writeStartElement( "apparentFOV" );
        writer->writeAttribute( "unit", eyepiece->fovUnit() );
        writer->writeCharacters( QString::number( eyepiece->appFov() ) );
        writer->writeEndElement();
    }

    writer->writeEndElement();
}
void Log::writeLens( Lens *lens ) {
    writer->writeStartElement( "lens" );
    writer->writeAttribute( "id", lens->id() );

    // Write Model
    writer->writeStartElement( "model" );
    writer->writeCDATA( lens->model() );
    writer->writeEndElement();

    // Write Vendor
    if( !lens->vendor().isEmpty() ) {
        writer->writeStartElement( "vendor" );
        writer->writeCDATA( lens->vendor() );
        writer->writeEndElement();
    }

    // Write Factor
    writer->writeStartElement( "factor" );
    writer->writeCharacters( QString::number( lens->factor() ) );
    writer->writeEndElement();

    writer->writeEndElement();
}

void Log::writeFilter( Filter *filter ) {
    writer->writeStartElement( "filter" );
    writer->writeAttribute( "id", filter->id() );

    // Write Model
    writer->writeStartElement( "model" );
    writer->writeCDATA( filter->model() );
    writer->writeEndElement();

    // Write Vendor
    if( !filter->vendor().isEmpty() ) {
        writer->writeStartElement( "vendor" );
        writer->writeCDATA( filter->vendor() );
        writer->writeEndElement();
    }

    // Write Type
    writer->writeStartElement( "type" );
    writer->writeCDATA( filter->typeOALRepresentation() );
    writer->writeEndElement();

    // Write Color
    if( filter->color() != Filter::FC_NONE ) {
        writer->writeStartElement( "color" );
        writer->writeCDATA( filter->colorOALRepresentation() );
        writer->writeEndElement();
    }

    writer->writeEndElement();
}

void Log::writeObservation( Observation *o ) {
    writer->writeStartElement( "observation" );

    // Write Observer
    writer->writeStartElement( "observer" );
    writer->writeCharacters( o->observer() );
    writer->writeEndElement();

    // Write Site
    if( !o->site().isEmpty() ) {
        writer->writeStartElement( "site" );
        writer->writeCharacters( o->site() );
        writer->writeEndElement();
    }

    // Write Session
    if( !o->session().isEmpty() ) {
        writer->writeStartElement( "session" );
        writer->writeCharacters( o->session() );
        writer->writeEndElement();
    }

    // Write Target
    writer->writeStartElement( "target" );
    writer->writeCharacters( o->target() );
    writer->writeEndElement();

    // Write Begin
    writer->writeStartElement( "begin" );
    writer->writeCharacters( o->begin().date().toString( "yyyy-MM-dd" ) + 'T' + o->begin().time().toString( "hh:mm:ss" ) );
    writer->writeEndElement();

    // Write End
    if( o->isEndDefined() ) {
        writer->writeStartElement( "end" );
        writer->writeCharacters( o->end().date().toString( "yyyy-MM-dd" ) + 'T' + o->end().time().toString( "hh:mm:ss" ) );
        writer->writeEndElement();
    }

    // Write Faintest Star
    if( o->isFaintestStarDefined() ) {
        writer->writeStartElement( "faintestStar" );
        writer->writeCharacters( QString::number( o->faintestStar() ) );
        writer->writeEndElement();
    }

    // Write Sky Quality
    if( o->isSkyQualityDefined() ) {
        writer->writeStartElement( "sky-quality" );
        writer->writeCharacters( QString::number( o->skyQuality() ) );
        if( o->skyQualityUnit() == Observation::SB_MAGS_PER_ARCSEC ) {
            writer->writeAttribute( "unit", "mags-per-squarearcsec" );
        } else if( o->skyQualityUnit() == Observation::SB_MAGS_PAR_ARCMIN ) {
            writer->writeAttribute( "unit", "mags-per-squarearcmin" );
        } else {
            writer->writeAttribute( "unit", QString() );
        }
    }

    // Write Seeing
    if( o->isSeeingDefined() ) {
        writer->writeStartElement( "seeing" );
        writer->writeCharacters( QString::number( o->seeing() ) );
        writer->writeEndElement();
    }

    // Write Scope
    if( !o->scope().isEmpty() ) {
        writer->writeStartElement( "scope" );
        writer->writeCharacters( o->scope() );
        writer->writeEndElement();
    }

    // Write Accessories
    if( !o->accessories().isEmpty() ) {
        writer->writeStartElement( "accessories" );
        writer->writeCharacters( o->accessories() );
        writer->writeEndElement();
    }

    // Write Eyepiece
    if( !o->eyepiece().isEmpty() ) {
        writer->writeStartElement( "eyepiece" );
        writer->writeCharacters( o->eyepiece() );
        writer->writeEndElement();
    }

    // Write Lens
    if( !o->lens().isEmpty() ) {
        writer->writeStartElement( "lens" );
        writer->writeCharacters( o->lens() );
        writer->writeEndElement();
    }

    // Write Filter
    if( !o->filter().isEmpty() ) {
        writer->writeStartElement( "filter" );
        writer->writeCharacters( o->filter() );
        writer->writeEndElement();
    }

    // Write Magnification
    if( o->isMagnificationDefined() ) {
        writer->writeStartElement( "magnification" );
        writer->writeCharacters( QString::number( o->magnification() ) );
        writer->writeEndElement();
    }

    // Write Imager
    if( !o->imager().isEmpty() ) {
        writer->writeStartElement( "imager" );
        writer->writeCharacters( o->imager() );
        writer->writeEndElement();
    }

    // Write Results
    foreach( Result res, o->results() ) {
        writer->writeStartElement( "result" );
        writer->writeAttribute( "xsi:type", "oal:findingsType" );
        writer->writeAttribute( "lang", res.second );
        writer->writeStartElement( "description" );
        writer->writeCDATA( res.first );
        writer->writeEndElement();
        writer->writeEndElement();
    }

    // Write Image References
    if( !o->images().isEmpty() ) {
        foreach(QString img, o->images()) {
            writer->writeStartElement( "image" );
            writer->writeCharacters( img );
            writer->writeEndElement();
        }
    }

    writer->writeEndElement();
}
void Log::writeGeoDate() {
    writer->writeStartElement( "geodate" );
    writer->writeStartElement( "name" );
    writer->writeCDATA( ks->observingList()->geoLocation()->name() );
    writer->writeEndElement();
    writer->writeStartElement( "province" );
    writer->writeCDATA( ks->observingList()->geoLocation()->province() );
    writer->writeEndElement();
    writer->writeStartElement( "country" );
    writer->writeCDATA( ks->observingList()->geoLocation()->country() );
    writer->writeEndElement();
    writer->writeStartElement( "date" );
    writer->writeCDATA( ks->observingList()->dateTime().date().toString( "ddMMyyyy" ) );
    writer->writeEndElement();
    writer->writeEndElement();
}
void Log::readBegin( const QString &input ) {
    reader = new QXmlStreamReader( input );
    ks = KStars::Instance();
    while( ! reader->atEnd() ) {
        reader->readNext();
        if( reader->isStartElement() ) {
     //       if( reader->name() == "oal:observations" && reader->attributes().value("version") == "2.0" )
                readLog();
        }
    }
}

void Log::readUnknownElement() {
    while( ! reader->atEnd() ) {
        reader->readNext();
        
        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() )
            readUnknownElement();
    }
}

void Log::readLog() {
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
           if( reader->name() == "observers" )
                readObservers();
           else if( reader->name() == "sites" )
                readSites();
           else if( reader->name() == "sessions" )
                readSessions();
           else if( reader->name() == "targets" )
               readTargets();
           else if( reader->name() == "scopes" )
                readScopes();
           else if( reader->name() == "eyepieces" )
                readEyepieces();
           else if( reader->name() =="lenses" )
                readLenses();
           else if( reader->name() =="filters" )
                readFilters();
           else if( reader->name() == "observation" ) 
                readObservation( reader->attributes().value( "id" ).toString() );
           else if( reader->name() == "geodate" )
                readGeoDate();
            else
                readUnknownElement();
        }
    }
}

void Log::readTargets() {
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "target" )
                readTarget( reader->attributes().value( "id" ).toString() );
            else
                readUnknownElement();
        }
    }
}

void Log::readObservers() {
    qDeleteAll(m_observerList);
    m_observerList.clear();

    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "observer" )
                readObserver( reader->attributes().value( "id" ).toString() );
            else
                readUnknownElement();
        }
    }
}

void Log::readSites() {
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "site" )
                readSite( reader->attributes().value( "id" ).toString() );
            else
                readUnknownElement();
        }
    }
}

void Log::readSessions() {
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "session" )
                readSession( reader->attributes().value( "id" ).toString(), reader->attributes().value( "lang" ).toString() );
            else
                readUnknownElement();
        }
    }
}

void Log::readScopes() {
    qDeleteAll(m_scopeList);
    m_scopeList.clear();

    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "scope" )
                readScope( reader->attributes().value( "id" ).toString() );
            else
                readUnknownElement();
        }
    }
}

void Log::readEyepieces() {
    qDeleteAll(m_eyepieceList);
    m_eyepieceList.clear();

    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "eyepiece" )
                readEyepiece( reader->attributes().value( "id" ).toString() );
            else
                readUnknownElement();
        }
    }
}

void Log::readLenses() {
    qDeleteAll(m_lensList);
    m_lensList.clear();

    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "lens" )
                readLens( reader->attributes().value( "id" ).toString() );
            else
                readUnknownElement();
        }
    }
}

void Log::readFilters() {
    qDeleteAll(m_filterList);
    m_filterList.clear();

    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "filter" )
                readFilter( reader->attributes().value( "id" ).toString() );
            else
                readUnknownElement();
        }
    }
}

void Log::readTarget( const QString &id ) {
    QString datasourceObserver, name, constellation, notes;
    QStringList aliases;
    SkyPoint position;
    bool fromDatasource = true;
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "datasource" ) {
                datasourceObserver = reader->readElementText();
            } else if ( reader->name() == "observer" ) {
                datasourceObserver = reader->readElementText();
                fromDatasource = false;
            }
            else if( reader->name() == "name" ) {
                name = reader->readElementText();
            } else if( reader->name() == "alias" ) {
                aliases.append(reader->readElementText());
            } else if( reader->name() == "position" ) {
                bool ok;
                position = readPosition( ok );
            }
            else  if( reader->name() == "constellation" ) {
                constellation = reader->readElementText();
            }
            else if( reader->name() == "notes" ) {
                notes = reader->readElementText();
            }
            else
                readUnknownElement();
        }
    }
    ObservationTarget *t = new ObservationTarget( id, datasourceObserver, fromDatasource, name, aliases, position, constellation, notes );
    m_targetList.append( t );
}

void Log::readObserver( const QString &id ) {
    QString name, surname, contact;
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "name" ) {
                name = reader->readElementText();
            } else if( reader->name() == "surname" ) {
                surname = reader->readElementText();
            } else if( reader->name() == "contact" ) {
                contact = reader->readElementText();
            } else
                readUnknownElement();
        }
    }

    Observer *o= new Observer( id, name, surname, contact );
    m_observerList.append( o );
}

void Log::readSite( const QString &id ) {
    QString name, latUnit, lonUnit, lat, lon;
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "name" ) {
                name = reader->readElementText();
            } else if( reader->name() == "latitude" ) {
                lat = reader->readElementText() ;
                latUnit = reader->attributes().value( "unit" ).toString();
            } else if( reader->name() == "longitude" ) {
                lon = reader->readElementText() ;
                lonUnit = reader->attributes().value( "unit" ).toString();
            } else
                readUnknownElement();
        }
    }
    Site *o= new Site( id, name, lat.toDouble(), latUnit, lon.toDouble(), lonUnit, 0 );
    m_siteList.append( o );
}

void Log::readSession( const QString &id, const QString &lang ) {
    QString site, weather, equipment, comments, begin, end;
    KStarsDateTime beginDT, endDT;
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "site" ) {
                site = reader->readElementText();
            } else if( reader->name() == "begin" ) {
                begin = reader->readElementText() ;
                beginDT = KStarsDateTime::fromString( begin );
            } else if( reader->name() == "end" ) {
                end = reader->readElementText() ;
                endDT = KStarsDateTime::fromString( end );
            } else if( reader->name() == "weather" ) {
                weather = reader->readElementText() ;
            } else if( reader->name() == "equipment" ) {
                equipment = reader->readElementText() ;
            } else if( reader->name() == "comments" ) {
                comments = reader->readElementText() ;
            } else
                readUnknownElement();
        }
    }
    Session *o= new Session( id, site, QStringList(), beginDT, endDT, weather, equipment, comments, lang );
    m_sessionList.append( o );
}

void Log::readScope( const QString &id ) {
    QString model, vendor, type, driver = i18n("None");
    double focalLength(0), aperture(0), lightGrasp(0);
    bool erect(false), truesided(false);
    bool lightGraspDefined(false), orientationDefined(false);

    while( !reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "model" ) {
                model = reader->readElementText();
            } else if( reader->name() == "type" ) {
                type = reader->readElementText() ;
                if( type == "N" ) type = "Newtonian";
                if( type == "R" ) type = "Refractor";
                if( type == "M" ) type = "Maksutov";
                if( type == "S" ) type = "Schmidt-Cassegrain";
                if( type == "K" ) type = "Kutter (Schiefspiegler)";
                if( type == "C" ) type = "Cassegrain";
            } else if( reader->name() == "vendor" ) {
                vendor = reader->readElementText() ;
            } else if( reader->name() == "aperture" ) {
                aperture = reader->readElementText().toDouble();
            } else if( reader->name() == "driver") {
                driver = reader->readElementText();
            } else if( reader->name() == "lightGrasp" ) {
                lightGrasp = reader->readElementText().toDouble( &lightGraspDefined );
            } else if( reader->name() == "orientation" ) {
                erect = !( reader->attributes().value( "erect" ).toString().compare( "true", Qt::CaseInsensitive ) );
                truesided = !( reader->attributes().value( "truesided" ).toString().compare( "true", Qt::CaseInsensitive ) );
                orientationDefined = true;
                reader->readElementText();
            } else if( reader->name() == "focalLength" ) {
                focalLength = reader->readElementText().toDouble();
            } else {
                readUnknownElement();
            }
        }
    }

    Scope *scope= new Scope( id, model, vendor, type, focalLength, aperture, lightGrasp, lightGraspDefined, erect, truesided,
                             orientationDefined );
    scope->setINDIDriver( driver );
    m_scopeList.append( scope );
}

void Log::readEyepiece( const QString &id )
{
    QString model, vendor, fovUnit;
    double focalLength(0), maxFocalLength(0), fov(0);
    bool fovDefined(false), maxFocalLengthDefined(0);

    while( !reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "model" ) {
                model = reader->readElementText();
            } else if( reader->name() == "vendor" ) {
                vendor = reader->readElementText() ;
            } else if( reader->name() == "focalLength" ) {
                focalLength = reader->readElementText().toDouble();
            } else if( reader->name() == "maxFocalLength" ) {
                maxFocalLength = reader->readElementText().toDouble(&maxFocalLengthDefined);
            } else if( reader->name() == "apparentFOV" ) {
                fovUnit = reader->attributes().value( "unit" ).toString();
                fov = reader->readElementText().toDouble(&fovDefined);
            } else
                readUnknownElement();
        }
    }
    
    Eyepiece *eyepiece= new Eyepiece( id, model, vendor, fov, fovUnit, fovDefined, focalLength,
                                      maxFocalLength, maxFocalLengthDefined );
    m_eyepieceList.append( eyepiece );
}

void Log::readLens( const QString &id ) {
    QString model, vendor;
    double factor(0);

    while( !reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "model" ) {
                model = reader->readElementText();
            } else if( reader->name() == "vendor" ) {
                vendor = reader->readElementText() ;
            } else if( reader->name() == "factor" ) {
                factor = reader->readElementText().toDouble() ;
            } else
                readUnknownElement();
        }
    }
    
    Lens *lens= new Lens( id, model, vendor, factor );
    m_lensList.append( lens );
}

void Log::readFilter( const QString &id ) {
    QString model, vendor, type, color;
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "model" ) {
                model = reader->readElementText();
            } else if( reader->name() == "vendor" ) {
                vendor = reader->readElementText() ;
            } else if( reader->name() == "type" ) {
                type = reader->readElementText() ;
            } else if( reader->name() == "color" ) {
                color = reader->readElementText() ;
            } else
                readUnknownElement();
        }
    }

    Filter *filter= new Filter( id, model, vendor, Filter::oalRepresentationToFilterType( type ),
                                Filter::oalRepresentationToFilterColor( color ) );
    m_filterList.append( filter );
}

SkyPoint Log::readPosition( bool &ok ) {
    QString raStr, decStr;
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "ra" )
                raStr = reader->readElementText();
            else if( reader->name() == "dec" )
                decStr = reader->readElementText();
            else
                readUnknownElement();
        }
    }

    bool raOk( false ), decOk( false );
    dms ra( ( 1 / dms::DegToRad ) * raStr.toDouble( &raOk ) );
    dms dec( ( 1 / dms::DegToRad ) * decStr.toDouble( &decOk ) );
    ok = raOk && decOk;

    return SkyPoint(ra, dec);
}

void Log::readObservation( const QString &id ) {
    QString observer, site, session, target;
    QString scope, eyepiece, lens, filter, imager, accessories;
    double faintest(0), seeing(0), skyQuality(0), magnification(0);
    Observation::SURFACE_BRIGHTNESS_UNIT skyQualityUnit;
    KStarsDateTime begin, end;
    bool faintestDefined(false), seeingDefined(false), skyQualityDefined(false);
    bool magnificationDefined(false), endDefined(false);
    QStringList imageRefs;
    QList<Result> results;
    Observation *observation = new Observation;

    while( ! reader->atEnd() ) {
        reader->readNext();
        if( reader->isEndElement() )
            break;
        if( reader->isStartElement() ) {
            if( reader->name() == "observer" )
                observer = reader->readElementText();
            else if( reader->name() == "site" )
                site = reader->readElementText();
            else if( reader->name() == "session" )
                session = reader->readElementText();
            else if( reader->name() == "target" ) {
                target = reader->readElementText();
                ObservationTarget *t = findTargetById( target );
                if( t ) {
                    t->observationsList()->append( observation );
                }
            }
            else if( reader->name() == "begin" )
                begin.fromString( reader->readElementText() );
            else if( reader->name() == "end" ) {
                end.fromString( reader->readElementText() );
                endDefined = true;
            }
            else if( reader->name() == "faintestStar" )
                faintest = reader->readElementText().toDouble( &faintestDefined );
            else if( reader->name() == "sky-quality" ) {
                skyQuality = readSurfaceBrightness( &skyQualityUnit );
                skyQualityDefined = true;
            }
            else if( reader->name() == "seeing" )
                seeing = reader->readElementText().toDouble( &seeingDefined );
            else if( reader->name() == "scope" )
                scope = reader->readElementText();
            else if( reader->name() == "eyepiece" )
                eyepiece = reader->readElementText();
            else if( reader->name() == "lens" )
                lens = reader->readElementText();
            else if( reader->name() == "filter" )
                filter = reader->readElementText();
            else if( reader->name() == "imager" )
                imager = reader->readElementText();
            else if( reader->name() == "accessories" )
                accessories = reader->readElementText();
            else if( reader->name() == "magnification" )
                magnification = reader->readElementText().toDouble( &magnificationDefined );
            else if( reader->name() == "result" ) {
                Result res;
                res.first = readResult();
                res.second = reader->attributes().value( "lang" ).toString();
                results.append(res);
            }
            else if( reader->name() == "image" )
                imageRefs.append( reader->readElementText() );
            else
                readUnknownElement();
        }
    }

    observation->setObservation(id, target, observer, begin, results, site, session, scope, eyepiece, lens,
                                filter, imager, accessories, imageRefs, magnification, magnificationDefined,
                                faintest, faintestDefined, skyQuality, skyQualityUnit, skyQualityDefined,
                                seeing, seeingDefined, end, endDefined);

    m_observationList.append( observation );
}

QString Log::readResult() {
    QString result;
    while( !reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;
        if( reader->isStartElement() ) {
            if( reader->name() == "description" )
                result = reader->readElementText();
            else
                readUnknownElement();
        }
    }
    return result;
}

double Log::readSurfaceBrightness( Observation::SURFACE_BRIGHTNESS_UNIT *unit ) {
    double surfBr;
    QString unitStr = reader->attributes().value( "unit" ).toString();
    if( unitStr == "mags-per-squarearcsec" ) {
        *unit = Observation::SB_MAGS_PER_ARCSEC;
    } else if( unitStr == "mags-per-squarearcmin" ) {
        *unit = Observation::SB_MAGS_PAR_ARCMIN;
    } else {
        *unit = Observation::SB_WRONG_UNIT;
    }

    surfBr = reader->readElementText().toDouble();

    return surfBr;
}

void Log::readGeoDate() {
    QString name, province, country, date;
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "name" )
                name = reader->readElementText();
            else if( reader->name() == "province" )
                province = reader->readElementText();
            else if( reader->name() == "country" )
                country = reader->readElementText();
            else if( reader->name() == "date" ){
                date = reader->readElementText();
            } else
                readUnknownElement();
        }
    }
    geo = ks->data()->locationNamed( name, province, country );
    dt.setDate( QDate::fromString( date, "ddMMyyyy" ) );
}

Observer* Log::findObserverByName( const QString &name ) {
    foreach( Observer *obs, *observerList() )
        if( QString(obs->name() + ' ' + obs->surname() ) == name )
            return obs;
    return 0;
}

Observer* Log::findObserverById( const QString &id ) {
    foreach( Observer *obs, *observerList() )
        if( obs->id() == id )
            return obs;
    return 0;
}

Session* Log::findSessionById( const QString &id ) {
    foreach( Session *s, *sessionList() )
        if( s->id()  == id )
            return s;
    return 0;
}

Site* Log::findSiteById( const QString &id ) {
    foreach( Site *s, *siteList() )
        if( s->id()  == id )
            return s;
    return 0;
}

ObservationTarget* Log::findTargetByName( const QString &name )
{
    foreach( ObservationTarget *t, *targetList() ) {
        if( t->name() == name ) {
            return t;
        }
    }
    return 0;
}

ObservationTarget* Log::findTargetById( const QString &id )
{
    foreach( ObservationTarget *t, *targetList() ) {
        if( t->id() == id ) {
            return t;
        }
    }
    return 0;
}

Site* Log::findSiteByName( const QString &name ) {
    foreach( Site *s, *siteList() )
        if( s->name()  == name )
            return s;
    return 0;
}

Scope* Log::findScopeById( const QString &id ) {
    foreach( Scope *s, *scopeList() )
        if( s->id()  == id )
            return s;
    return 0;
}

Eyepiece* Log::findEyepieceById( const QString &id ) {
    foreach( Eyepiece *e, *eyepieceList() )
        if( e->id()  == id )
            return e;
    return 0;
}

Lens* Log::findLensById( const QString &id ) {
    foreach( Lens *l, *lensList() )
        if( l->id()  == id )
            return l;
    return 0;
}

Filter* Log::findFilterById( const QString &id ) {
    foreach( Filter *f, *filterList() )
        if( f->id()  == id )
            return f;
    return 0;
}

Eyepiece* Log::findEyepieceByName( const QString &name ) {
    foreach( Eyepiece *e, *eyepieceList() )
        if( e->name()  == name )
            return e;
    return 0;
}

Filter* Log::findFilterByName( const QString &name ) {
    foreach( Filter *f, *filterList() )
        if( f->name()  == name )
            return f;
    return 0;
}

Lens* Log::findLensByName( const QString &name ) {
    foreach( Lens *l, *lensList() )
        if( l->name()  == name )
            return l;
    return 0;
}

Observation* Log::findObservationByName( const QString &id ) {
    foreach( Observation *o, *observationList() )
        if( o->id()  == id )
            return o;
    return 0;
}

void Log::removeScope( int idx ) {
    if( idx < 0 || idx >= m_scopeList.size() )
        return;

    delete m_scopeList.at( idx );
    m_scopeList.removeAt( idx );
}

void Log::removeEyepiece( int idx ) {
    if( idx < 0 || idx >= m_eyepieceList.size() )
        return;

    delete m_eyepieceList.at( idx );
    m_eyepieceList.removeAt( idx );
}

void Log::removeLens( int idx ) {
    if( idx < 0 || idx >= m_lensList.size() )
        return;

    delete m_lensList.at( idx );
    m_lensList.removeAt( idx );
}

void Log::removeFilter( int idx ) {
    if( idx < 0 || idx >= m_filterList.size() )
        return;

    delete m_filterList.at( idx );
    m_filterList.removeAt( idx );
}

void Log::loadObserversFromFile() {
    QFile f;
    f.setFileName(KStandardDirs::locateLocal("appdata", "observerlist.xml"));
    if(!f.open(QIODevice::ReadOnly))
        return;
    QTextStream istream(&f);
    readBegin(istream.readAll());
    f.close();
}

void Log::saveObserversToFile() {
    QFile f;
    f.setFileName(KStandardDirs::locateLocal("appdata", "observerlist.xml"));
    if (!f.open(QIODevice::WriteOnly)) {
        KMessageBox::sorry(0, i18n("Could not save the observer list to the file."), i18n("Write Error"));
        return;
    }
    QTextStream ostream(&f);
    writeBegin();  //Initialize the xml document, etc.
    writeObservers();  //Write the observer list into the QString
    writeEnd();  //End the write process
    ostream << writtenOutput();
    f.close();
}

void Log::loadEquipmentFromFile() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "equipmentlist.xml" ) );
    if( ! f.open( QIODevice::ReadOnly ) )
        return;
    QTextStream istream( &f );
    readBegin( istream.readAll() );
    f.close();
}

void Log::saveEquipmentToFile() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "equipmentlist.xml" ) );
    if ( ! f.open( QIODevice::WriteOnly ) ) {
        kDebug() << "Cannot write list to  file";
        return;
    }
    QTextStream ostream( &f );
    writeBegin();
    writeScopes();
    writeEyepieces();
    writeLenses();
    writeFilters();
    writeEnd();
    ostream << writtenOutput();
    f.close();
}

void Log::markUsedObservers() {
    foreach( Observation *obs, m_observationList ) {
        m_usedObservers.insert(obs->observer());
    }

    foreach( Session *session, m_sessionList ) {
        foreach(QString obs, session->coobservers()) {
            m_usedObservers.insert(obs);
        }
    }
}

void Log::markUsedEquipment() {
    foreach( Observation *obs, m_observationList ) {
        m_usedEyepieces.insert(obs->eyepiece());
        m_usedLens.insert(obs->lens());
        m_usedFilters.insert(obs->filter());
        m_usedScopes.insert(obs->scope());
    }
}

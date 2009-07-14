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
#include "comast/log.h"
#include "kstars.h"
#include "skyobjects/skyobject.h"
#include "skymap.h"
#include "skycomponents/constellationboundary.h"
#include "kstarsdatetime.h"

void Comast::Log::writeBegin() {
    writer = new QXmlStreamWriter(&output);
    writer->setAutoFormatting( true );
    writer->writeStartDocument();
    writer->writeNamespace( "http://observation.sourceforge.net/openastronomylog", "oal" );
    writer->writeNamespace( "http://www.w3.org/2001/XMLSchema-instance", "xsi" );
    writer->writeNamespace( "http://observation.sourceforge.net/openastronomylog oal20.xsd", "schemaLocation" );
    writer->writeStartElement("oal:observations");
    writer->writeAttribute("version", "2.0");
}

QString Comast::Log::writeLog( bool _native ) {
    ks = KStars::Instance();
    m_targetList = ks->observingList()->sessionList();
    native = _native;
    writeBegin();
    if( native )
        writeGeoDate();
    writeObservers();
    writeSites();
    writeSessions();
    writeTargets();
    writeScopes();
    writeEyepieces();
    writeLenses();
    writeFilters();
    writeImagers();
    writeEnd();
    return output;
}

void Comast::Log::writeEnd() {
    writer->writeEndDocument();
    delete writer;
}

void Comast::Log::writeObservers() {
    writer->writeStartElement( "observers" );
    foreach( Comast::Observer *o, m_observerList )
        writeObserver( o );
    writer->writeEndElement();
}

void Comast::Log::writeSites() {
    writer->writeStartElement("sites");
    foreach( Comast::Site *o, m_siteList )
        writeSite( o );
    writer->writeEndElement();
}

void Comast::Log::writeSessions() {
    writer->writeStartElement("sessions");
    foreach( Comast::Session *o, m_sessionList )
        writeSession( o );
    writer->writeEndElement();
}

void Comast::Log::writeTargets() {
    writer->writeStartElement("targets");
    foreach( SkyObject *o, m_targetList ) {
        writeTarget( o );
    }
    writer->writeEndElement();
}

void Comast::Log::writeScopes() {
    writer->writeStartElement("scopes");
    foreach( Comast::Scope *o, m_scopeList )
        writeScope( o );
    writer->writeEndElement();
}

void Comast::Log::writeEyepieces() {
    writer->writeStartElement("eyepieces");
    foreach( Comast::Eyepiece *o, m_eyepieceList )
        writeEyepiece( o );
    writer->writeEndElement();
}

void Comast::Log::writeLenses() {
    writer->writeStartElement("lenses");
    foreach( Comast::Lens *o, m_lensList )
        writeLens( o );
    writer->writeEndElement();
}

void Comast::Log::writeFilters() {
    writer->writeStartElement("filters");
    foreach( Comast::Filter *o, m_filterList )
        writeFilter( o );
    writer->writeEndElement();
}

void Comast::Log::writeImagers() {
    writer->writeStartElement("imagers");
    writer->writeEndElement();
}

void Comast::Log::writeTarget( SkyObject *o ) {
    writer->writeStartElement( "target" );
    writer->writeAttribute("id", o->name() );
    QString typeString;
    if( native )
        writer->writeAttribute( "type", o->typeName() );
    else {
        switch( o->type() ) {
            case 0: typeString = "oal:starTargetType"; break;
            case 1: typeString = "oal:starTargetType"; break;
            case 2: typeString = "oal:PlanetTargetType"; break;
            case 3: typeString = "oal:deepSkyOC"; break;
            case 4: typeString = "oal:deepSkyGC"; break;
            case 5: typeString = "oal:deepSkyGN"; break;
            case 6: typeString = "oal:deepSkyPN"; break;
            case 8: typeString = "oal:deepSkyGX"; break;
            case 9: typeString = "oal:CometTargetType"; break;
            case 12: typeString = "oal:MoonTargetType"; break;
            case 13: typeString = "oal:deepSkyAS"; break;
            case 14: typeString = "oal:deepSkyCG"; break;
            case 15: typeString = "oal:deepSkyDN"; break;
            case 16: typeString = "oal:deepSkyQS"; break;
            case 17: typeString = "oal:deepSkyMS"; break;
            default: typeString = "oal:deepSkyNA"; break;
        }
        writer->writeAttribute("xsi:type", typeString );
    }
    writer->writeStartElement( "datasource" );
    writer->writeCDATA( "KStars" );
    writer->writeEndElement();
    writer->writeStartElement("name");
    writer->writeCDATA( o->name() );
    writer->writeEndElement();
    writer->writeStartElement( "position" );
    writer->writeStartElement( "ra" );
    writer->writeAttribute("unit", "rad" );
    writer->writeCharacters( QString::number( o->ra()->radians() ) );
    writer->writeEndElement();
    writer->writeStartElement( "dec" );
    writer->writeAttribute("unit", "rad" );
    writer->writeCharacters( QString::number( o->dec()->radians() ) );
    writer->writeEndElement();
    writer->writeEndElement();
    if( native && ! ks->observingList()->getTime( o ).isEmpty() ) {
        writer->writeStartElement("time");
        writer->writeCDATA( ks->observingList()->getTime( o ) );
        writer->writeEndElement();
    }
    writer->writeStartElement( "constellation" );
    writer->writeCDATA( ConstellationBoundary::Instance()->constellationName( o ) );
    writer->writeEndElement();
    writer->writeEndElement();
}

void Comast::Log::writeObserver( Comast::Observer *o ) {
    writer->writeStartElement( "observer" );
    writer->writeAttribute( "id", o->name() );
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
void Comast::Log::writeSite( Comast::Site *s ) {
    writer->writeStartElement( "site" );
    writer->writeAttribute( "id", s->name() );
    writer->writeStartElement( "name" );
    writer->writeCDATA( s->name() );
    writer->writeEndElement();
    writer->writeStartElement( "latitude" );
    writer->writeAttribute( "unit",  s->latUnit() );
    writer->writeCharacters( QString::number( s->latitude() ) );
    writer->writeStartElement( "longitude" );
    writer->writeAttribute( "unit", s->lonUnit() );
    writer->writeCharacters( QString::number( s->longitude() ) );
    writer->writeEndElement();
    writer->writeEndElement();
}
void Comast::Log::writeSession( Comast::Session *s ) {
    writer->writeStartElement( "session" );
    writer->writeAttribute( "id", s->id() ); 
    writer->writeStartElement( "begin" );
    writer->writeCharacters( s->begin().date().toString( "yyyy-MM-dd" ) + "T" + s->begin().time().toString( "hh:mm:ss" ) );
    writer->writeEndElement();
    writer->writeStartElement( "end" );
    writer->writeCharacters( s->end().date().toString( "yyyy-MM-dd" ) + "T" + s->end().time().toString( "hh:mm:ss" ) );
    writer->writeEndElement();
    writer->writeStartElement( "site" );
    writer->writeCDATA( s->site() );
    writer->writeEndElement();
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
void Comast::Log::writeScope( Comast::Scope *s ) {
    writer->writeStartElement( "scope" );
    writer->writeAttribute( "id", s->id() ); 
    writer->writeStartElement( "model" );
    writer->writeCDATA( s->model() );
    writer->writeEndElement();
    writer->writeStartElement( "type" );
    writer->writeCDATA( s->type() );
    writer->writeEndElement();
    writer->writeStartElement( "vendor" );
    writer->writeCDATA( s->vendor() );
    writer->writeEndElement();
    writer->writeStartElement( "focalLength" );
    writer->writeCharacters( QString::number( s->focalLength() ) );
    writer->writeEndElement();
    writer->writeEndElement();
}
void Comast::Log::writeEyepiece( Comast::Eyepiece *ep ) {
    writer->writeStartElement( "eyepiece" );
    writer->writeAttribute( "id", ep->id() ); 
    writer->writeStartElement( "model" );
    writer->writeCDATA( ep->model() );
    writer->writeEndElement();
    writer->writeStartElement( "vendor" );
    writer->writeCDATA( ep->vendor() );
    writer->writeEndElement();
    writer->writeStartElement( "focalLength" );
    writer->writeCharacters( QString::number( ep->focalLength() ) );
    writer->writeEndElement();
    writer->writeStartElement( "apparantFOV" );
    writer->writeAttribute( "unit", ep->fovUnit() );
    writer->writeCharacters( QString::number( ep->appFov() ) );
    writer->writeEndElement();
    writer->writeEndElement();
}
void Comast::Log::writeLens( Comast::Lens *l ) {
    writer->writeStartElement( "lens" );
    writer->writeAttribute( "id", l->id() );
    writer->writeStartElement( "model" );
    writer->writeCDATA( l->model() );
    writer->writeEndElement();
    writer->writeStartElement( "vendor" );
    writer->writeCDATA( l->vendor() );
    writer->writeEndElement();
    writer->writeStartElement( "factor" );
    writer->writeCharacters( QString::number( l->factor() ) );
    writer->writeEndElement();
}

void Comast::Log::writeFilter( Comast::Filter *f ) {
    writer->writeStartElement( "filter" );
    writer->writeAttribute( "id", f->id() );
    writer->writeStartElement( "model" );
    writer->writeCDATA( f->model() );
    writer->writeEndElement();
    writer->writeStartElement( "vendor" );
    writer->writeCDATA( f->vendor() );
    writer->writeEndElement();
    writer->writeStartElement( "type" );
    writer->writeCDATA( f->type() );
    writer->writeEndElement();
    writer->writeStartElement( "color" );
    writer->writeCDATA( f->color() );
    writer->writeEndElement();
    writer->writeEndElement();
}

void Comast::Log::writeObservation( Comast::Observation *o ) {
    writer->writeStartElement( "observation" );
    writer->writeStartElement( "observer" );
    writer->writeCharacters( o->observer() );
    writer->writeEndElement();
    writer->writeStartElement( "site" );
    writer->writeCharacters( o->site() );
    writer->writeEndElement();
    writer->writeStartElement( "session" );
    writer->writeCharacters( o->session() );
    writer->writeEndElement();
    writer->writeStartElement( "target" );
    writer->writeCharacters( o->target() );
    writer->writeEndElement();
    writer->writeStartElement( "begin" );
    writer->writeCharacters( o->begin().date().toString( "yyyy-MM-dd" ) + "T" + o->begin().time().toString( "hh:mm:ss" ) );
    writer->writeEndElement();
    writer->writeStartElement( "faintestStar" );
    writer->writeCharacters( QString::number( o->faintestStar() ) );
    writer->writeEndElement();
    writer->writeStartElement( "seeing" );
    writer->writeCharacters( QString::number( o->seeing() ) );
    writer->writeEndElement();
    writer->writeStartElement( "scope" );
    writer->writeCharacters( o->scope() );
    writer->writeEndElement();
    writer->writeStartElement( "eyepiece" );
    writer->writeCharacters( o->eyepiece() );
    writer->writeEndElement();
    writer->writeStartElement( "result" );
    writer->writeAttribute( "xsi:type", "oal:findingsType" );
    writer->writeAttribute( "lang", o->lang() );
    writer->writeCDATA( o->result() );
    writer->writeEndElement();
    writer->writeEndElement();
}
void Comast::Log::writeGeoDate() {
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
void Comast::Log::readBegin( QString input ) {
    reader = new QXmlStreamReader( input );
    ks = KStars::Instance();
    m_targetList.clear();
    while( ! reader->atEnd() ) {
        reader->readNext();
        if( reader->isStartElement() ) {
     //       if( reader->name() == "oal:observations" && reader->attributes().value("version") == "2.0" )
                readLog();
        }
    }
}

void Comast::Log::readUnknownElement() {
    while( ! reader->atEnd() ) {
        reader->readNext();
        
        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() )
            readUnknownElement();
    }
}

void Comast::Log::readLog() {
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "targets" )
                readTargets();
            if( reader->name() == "geodate" )
                readGeoDate();
            else
                readUnknownElement();
        }
    }
}

void Comast::Log::readTargets() {
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "target" )
                readTarget();
            else
                readUnknownElement();
        }
    }
}

void Comast::Log::readTarget() {
    SkyObject *o = NULL;
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
             if( reader->name() == "name" ) {
                QString Name = reader->readElementText();
                if( Name != "star" ) {
                    o = ks->data()->objectNamed( Name );
                    if( ! o ) o = ks->data()->skyComposite()->findStarByGenetiveName( Name );
                    if( o ) ks->observingList()->slotAddObject( o, true );
                }
            } else if( reader->name() == "time" ) {
                if( o )
                    ks->observingList()->setTime( o, QTime::fromString( reader->readElementText(), "h:mm:ss AP" ) );
          }
       //   else  if( reader->name() == "datasource" )
       //         kDebug() << reader->readElementText();
       //     else if( reader->name() == "position" )
       //         readPosition();
       //     else if( reader->name() == "constellation" )
       //         kDebug() << reader->readElementText();
            else
                readUnknownElement();
        }
    }
}

void Comast::Log::readPosition() {
    while( ! reader->atEnd() ) {
        reader->readNext();

        if( reader->isEndElement() )
            break;

        if( reader->isStartElement() ) {
            if( reader->name() == "ra" )
                kDebug() << reader->readElementText() << reader->attributes().value( "unit" );
            else if( reader->name() == "dec" )
                kDebug() << reader->readElementText() << reader->attributes().value( "unit" );
            else
                readUnknownElement();
        }
    }
}
void Comast::Log::readGeoDate() {
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
    ks->observingList()->setGeoDate( name, province, country, date );
}

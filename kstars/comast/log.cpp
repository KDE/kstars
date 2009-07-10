/***************************************************************************
                          log.cpp  -  description

                             -------------------
    begin                : Friday June 19, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prak902000@gmail.com
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
    writeEyePieces();
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
    writer->writeStartElement("observers");
    writer->writeEndElement();
}

void Comast::Log::writeSites() {
    writer->writeStartElement("sites");
    writer->writeEndElement();
}

void Comast::Log::writeSessions() {
    writer->writeStartElement("sessions");
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
    writer->writeEndElement();
}

void Comast::Log::writeEyePieces() {
    writer->writeStartElement("eyepieces");
    writer->writeEndElement();
}

void Comast::Log::writeLenses() {
    writer->writeStartElement("lenses");
    writer->writeEndElement();
}

void Comast::Log::writeFilters() {
    writer->writeStartElement("filters");
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

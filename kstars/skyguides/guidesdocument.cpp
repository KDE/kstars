/***************************************************************************
                          guidesdocument.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Oct 9 2011
    copyright            : (C) 2011 by Łukasz Jaśkiewicz
    email                : lucas.jaskiewicz@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "guidesdocument.h"
#include "guide.h"

using namespace SkyGuides;

GuidesDocument::GuidesDocument()
{}

GuidesDocument::~GuidesDocument()
{
    qDeleteAll(m_Guides);
}

void GuidesDocument::readBegin(const QString &contents)
{
    if(m_Reader) {
        delete m_Reader;
    }

    m_Reader = new QXmlStreamReader(contents);

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->name() == "skyguide:guides") {
            readGuides();
        } else {
            return;
        }
    }
}

bool GuidesDocument::isError() const
{
    if(m_Reader) {
        return m_Reader->hasError();
    } else {
        return false;
    }
}

QXmlStreamReader::Error GuidesDocument::error() const
{
    if(m_Reader) {
        return m_Reader->error();
    } else {
        return QXmlStreamReader::NoError;
    }
}

QString GuidesDocument::errorString() const
{
    if(m_Reader) {
        return m_Reader->errorString();
    } else {
        return QString();
    }
}

void GuidesDocument::readGuides()
{
    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->name() == "guide") {
            readGuide();
        }
    }
}

void GuidesDocument::readGuide()
{
    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "title") {

            } else if(m_Reader->name() == "description") {

            } else if(m_Reader->name() == "language") {

            } else if(m_Reader->name() == "authors") {

            } else if(m_Reader->name() == "thumbnailImage") {

            } else if(m_Reader->name() == "slides") {

            }
        }
    }
}

void GuidesDocument::readSlides()
{
    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->name() == "slides") {
            readGuide();
        }
    }
}

void GuidesDocument::readSlide()
{
    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "title") {

            } else if(m_Reader->name() == "subtitle") {

            } else if(m_Reader->name() == "text") {

            } else if(m_Reader->name() == "centerPoint") {

            } else if(m_Reader->name() == "images") {

            } else if(m_Reader->name() == "links") {

            }
        }
    }
}

void GuidesDocument::readImage()
{
    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "title") {

            } else if(m_Reader->name() == "description") {

            } else if(m_Reader->name() == "url") {

            }
        }
    }
}

void GuidesDocument::readLink()
{
    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "label") {

            } else if(m_Reader->name() == "description") {

            } else if(m_Reader->name() == "url") {

            }
        }
    }
}

void GuidesDocument::readAuthor()
{
    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "name") {

            } else if(m_Reader->name() == "surname") {

            } else if(m_Reader->name() == "contact") {

            }
        }
    }
}

void GuidesDocument::skipUnknownElement()
{
    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement())
            skipUnknownElement();
    }
}



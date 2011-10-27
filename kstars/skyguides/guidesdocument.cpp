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
#include "author.h"
#include "link.h"
#include "image.h"
#include "slide.h"

#include <QMessageBox>
#include <QStringList>

using namespace SkyGuides;

GuidesDocument::GuidesDocument() : m_Reader(0)
{}

GuidesDocument::~GuidesDocument()
{
    if(m_Reader)
        delete m_Reader;

    qDeleteAll(m_Guides);
}

void GuidesDocument::readBegin(const QString &contents)
{
    if(m_Reader) {
        delete m_Reader;
    }

    m_Reader = new QXmlStreamReader(contents);

    if(m_Reader->readNextStartElement()) {
        if(m_Reader->name() == "guides") {
            readGuides();
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

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "guide") {
                m_Guides.append(readGuide());
            } else {
                skipUnknownElement();
            }
        }
    }
}

Guide* GuidesDocument::readGuide()
{
    QString title, description, language, thumbnailImage, version;
    QList<Author*> authors;
    QList<Slide*> slides;
    QDate date;

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "title") {
                title = m_Reader->readElementText();
            } else if(m_Reader->name() == "description") {
                description = m_Reader->readElementText();
            } else if(m_Reader->name() == "language") {
                language = m_Reader->readElementText();
            } else if(m_Reader->name() == "authors") {
                authors = readAuthors();
            } else if(m_Reader->name() == "thumbnailImage") {
                thumbnailImage = m_Reader->readElementText();
            } else if(m_Reader->name() == "creationDate") {
                //date =
            } else if(m_Reader->name() == "version") {
                version = m_Reader->readElementText();
            } else if(m_Reader->name() == "slides") {
                slides = readSlides();
            } else {
                skipUnknownElement();
            }
        }
    }

    return new Guide(title, description, language, authors, thumbnailImage, date, version, slides);
}

QList<Slide*> GuidesDocument::readSlides()
{
    QList<Slide*> slides;

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "slide") {
                slides.append(readSlide());
            } else {
                skipUnknownElement();
            }
        }
    }

    return slides;
}

Slide* GuidesDocument::readSlide()
{
    QString title, subtitle, text;
    // Center point
    QList<Image*> images;
    QList<Link*> links;

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "title") {
                title = m_Reader->readElementText();
            } else if(m_Reader->name() == "subtitle") {
                subtitle = m_Reader->readElementText();
            } else if(m_Reader->name() == "text") {
                text = m_Reader->readElementText();
            } else if(m_Reader->name() == "centerPoint") {
                m_Reader->readElementText();
            } else if(m_Reader->name() == "images") {
                images = readImages();
            } else if(m_Reader->name() == "links") {
                links = readLinks();
            } else {
                skipUnknownElement();
            }
        }
    }

    return new Slide(title, subtitle, text, SkyPoint(), images, links);
}

QList<Image*> GuidesDocument::readImages()
{
    QList<Image*> images;

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "image") {
                images.append(readImage());
            } else {
                skipUnknownElement();
            }
        }
    }

    return images;
}

Image* GuidesDocument::readImage()
{
    QString title, description, url;

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "title") {
                title = m_Reader->readElementText();
            } else if(m_Reader->name() == "description") {
                description = m_Reader->readElementText();
            } else if(m_Reader->name() == "url") {
                url = m_Reader->readElementText();
            } else {
                skipUnknownElement();
            }
        }
    }

    return new Image(title, description, url);
}

QList<Link*> GuidesDocument::readLinks()
{
    QList<Link*> links;

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "link") {
                links.append(readLink());
            } else {
                skipUnknownElement();
            }
        }
    }

    return links;
}

Link* GuidesDocument::readLink()
{
    QString label, description, url;

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "label") {
                label = m_Reader->readElementText();
            } else if(m_Reader->name() == "description") {
                description = m_Reader->readElementText();
            } else if(m_Reader->name() == "url") {
                url = m_Reader->readElementText();
            } else {
                skipUnknownElement();
            }
        }
    }

    return new Link(label, description, url);
}

QList<Author*> GuidesDocument::readAuthors()
{
    QList<Author*> authors;

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "author") {
                authors.append(readAuthor());
            } else {
                skipUnknownElement();
            }
        }
    }

    return authors;
}

Author* GuidesDocument::readAuthor()
{
    QString name, surname;
    QStringList contacts;

    while(!m_Reader->atEnd()) {
        m_Reader->readNext();

        if(m_Reader->isEndElement())
            break;

        if(m_Reader->isStartElement()) {
            if(m_Reader->name() == "name") {
                name = m_Reader->readElementText();
            } else if(m_Reader->name() == "surname") {
                surname = m_Reader->readElementText();
            } else if(m_Reader->name() == "contact") {
                contacts.append(m_Reader->readElementText());
            } else {
                skipUnknownElement();
            }
        }
    }

    return new Author(name, surname, contacts);
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



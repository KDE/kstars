/***************************************************************************
                          astrobinapixml.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 8 2012
    copyright            : (C) 2012 by Lukasz Jaskiewicz and Rafal Kulaga
    email                : lucas.jaskiewicz@gmail.com, rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "astrobinapixml.h"

#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QDebug>


AstroBinApiXml::AstroBinApiXml(QNetworkAccessManager *manager, QObject *parent)
    : AstroBinApi(manager, parent)
{
    m_UrlApiTypeEnding = "&format=xml";
}

AstroBinApiXml::~AstroBinApiXml()
{}


void AstroBinApiXml::replyFinished(QNetworkReply *reply)
{
    if(reply->request().originatingObject() != static_cast<QObject*>(this)) {
        return;
    }

    m_XmlReader = new QXmlStreamReader(reply->readAll());
    bool result = false;

    while(!m_XmlReader->atEnd()) {
        // read begin element
        m_XmlReader->readNext();
        m_XmlReader->readNext();

        if(m_XmlReader->isStartElement()) {
            if(m_XmlReader->name() == "response") {
                result = readResponse();
            } else {
                skipUnknownElement();
            }
        } else {
            break;
        }
    }

    emit searchFinished(result);
}

bool AstroBinApiXml::readResponse()
{
    while(!m_XmlReader->atEnd()) {
        m_XmlReader->readNext();

        if(m_XmlReader->isEndElement()) {
            break;
        }

        if(m_XmlReader->isStartElement()) {
            if(m_XmlReader->name() == "objects") {
                readObjects();
            } else if(m_XmlReader->name() == "meta") {
                readMetadata();
            } else {
                skipUnknownElement();
            }
        }
    }

    return false;
}

bool AstroBinApiXml::readMetadata()
{
    while(!m_XmlReader->atEnd()) {
        m_XmlReader->readNext();

        if(m_XmlReader->isEndElement()) {
            break;
        }

        if(m_XmlReader->isStartElement()) {
            QStringRef elementName = m_XmlReader->name();
            if(elementName == "next") {
                m_LastSearchResult.m_Metadata.m_NextUrlPostfix = m_XmlReader->readElementText();
            } else if(elementName == "total_count") {
                m_LastSearchResult.m_Metadata.m_TotalResultCount = m_XmlReader->readElementText().toInt();
            } else if(elementName == "previous") {
                m_LastSearchResult.m_Metadata.m_PreviousUrlPostfix = m_XmlReader->readElementText();
            } else if(elementName == "limit") {
                m_LastSearchResult.m_Metadata.m_Limit = m_XmlReader->readElementText().toInt();
            } else if(elementName == "offset") {
                m_LastSearchResult.m_Metadata.m_Offset = m_XmlReader->readElementText().toInt();
            } else {
                skipUnknownElement();
            }
        }
    }

    return true;
}

bool AstroBinApiXml::readObjects()
{
    while(!m_XmlReader->atEnd()) {
        m_XmlReader->readNext();

        if(m_XmlReader->isEndElement()) {
            break;
        }

        if(m_XmlReader->isStartElement()) {
            if(m_XmlReader->name() == "object") {
                readObject();
            } else {
                skipUnknownElement();
            }
        }
    }

    return true;
}

bool AstroBinApiXml::readObject()
{
    AstroBinImage image;

    while(!m_XmlReader->atEnd()) {
        m_XmlReader->readNext();

        if(m_XmlReader->isEndElement()) {
            break;
        }

        if(m_XmlReader->isStartElement()) {
            QStringRef elementName = m_XmlReader->name();

            if(elementName == "imaging_telescopes") {
                image.m_ImagingTelescopes = readList();
            } else if(elementName == "orientation") {
                image.m_Orientation = m_XmlReader->readElementText().toDouble();
            } else if(elementName == "id") {
                image.m_Id = m_XmlReader->readElementText();
            } else if(elementName == "imaging_cameras") {
                image.m_ImagingCameras = readList();
            } else if(elementName == "title") {
                image.m_Title = m_XmlReader->readElementText();
            } else if(elementName == "comments") {
                image.m_Comments = readList();
            } else if(elementName == "filename") {
                image.m_FileName = m_XmlReader->readElementText();
            } else if(elementName == "subjects") {
                image.m_Subjects = readList();
            } else if(elementName == "dec_center_dms") {
                image.m_DecCenterDms = dms(m_XmlReader->readElementText());
            } else if(elementName == "original_ext") {
                image.m_OriginalFileExtension = m_XmlReader->readElementText();
            } else if(elementName == "uploaded") {
                image.m_DateUploaded = QDateTime::fromString(m_XmlReader->readElementText(), Qt::ISODate);
            } else if(elementName == "updated") {
                image.m_DateUpdated = QDateTime::fromString(m_XmlReader->readElementText(), Qt::ISODate);
            } else if(elementName == "description") {
                image.m_Description = m_XmlReader->readElementText();
            } else if(elementName == "user") {
                image.m_User = m_XmlReader->readElementText();
            } else if(elementName == "rating_score") {
                image.m_RatingScore = m_XmlReader->readElementText().toInt();
            } else if(elementName == "rating_votes") {
                image.m_VoteCount = m_XmlReader->readElementText().toInt();
            } else if(elementName == "fieldunits") {
                image.m_FovUnits = m_XmlReader->readElementText();
            } else if(elementName == "fieldw") {
                image.m_Fov.setWidth(m_XmlReader->readElementText().toDouble());
            } else if(elementName == "fieldh") {
                image.m_Fov.setHeight(m_XmlReader->readElementText().toDouble());
            } else if(elementName == "license") {
                image.m_License = m_XmlReader->readElementText().toInt();
            } else if(elementName == "ra_center_hms") {
                image.m_RaCenterHms = dms(m_XmlReader->readElementText());
            } else if(elementName == "w") {
                image.m_Size.setWidth(m_XmlReader->readElementText().toInt());
            } else if(elementName == "h") {
                image.m_Size.setHeight(m_XmlReader->readElementText().toInt());
            } else if(elementName == "animated") {
                image.m_Animated = !(bool)m_XmlReader->readElementText().compare("True", Qt::CaseInsensitive);
            } else if(elementName == "resource_uri") {
                image.m_ResourceUri = m_XmlReader->readElementText();
            } else {
                skipUnknownElement();
            }
        }
    }

    m_LastSearchResult.append(image);

    return true;
}

QStringList AstroBinApiXml::readList()
{
    QStringList values;

    while(!m_XmlReader->atEnd()) {
        m_XmlReader->readNext();

        if(m_XmlReader->isEndElement()) {
            break;
        }

        if(m_XmlReader->isStartElement()) {
            if(m_XmlReader->name() == "value") {
                values.append(m_XmlReader->readElementText());
            } else {
                skipUnknownElement();
            }
        }
    }

    return values;
}

void AstroBinApiXml::skipUnknownElement()
{
    while(!m_XmlReader->atEnd()) {
        m_XmlReader->readNext();

        if(m_XmlReader->isEndElement()) {
            break;
        }

        if(m_XmlReader->isStartElement()) {
            skipUnknownElement();
        }
    }
}

/***************************************************************************
                          astrobinapijson.cpp  -  K Desktop Planetarium
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

#include "astrobinapijson.h"

#include <QNetworkReply>
#include <QVariantMap>
#include <QVariant>
#include "qjson/parser.h"

void AstroBinApiJson::replyFinished(QNetworkReply *reply)
{
    if(reply->request().originatingObject() != static_cast<QObject*>(this)) {
        return;
    }

    m_LastSearchResult.clear();

    QJson::Parser parser;
    bool ok;

    QVariantMap result = parser.parse(reply->readAll(), &ok).toMap();
    if(!ok) {
        emit searchFinished(false);
    }

    // read metadata
    readMetadata(result.value("meta").toMap());

    // read objects
    readObjects(result.value("objects").toList());

    emit searchFinished(true);
}

void AstroBinApiJson::readMetadata(const QVariantMap &metadata)
{
    m_LastSearchResult.m_Metadata.m_Limit = metadata.value("limit").toInt();
    m_LastSearchResult.m_Metadata.m_NextUrlPostfix = metadata.value("next").toString();
    m_LastSearchResult.m_Metadata.m_Offset = metadata.value("offset").toInt();
    m_LastSearchResult.m_Metadata.m_PreviousUrlPostfix = metadata.value("previous").toString();
    m_LastSearchResult.m_Metadata.m_TotalResultCount = metadata.value("total_count").toInt();
}

void AstroBinApiJson::readObjects(const QVariantList &objects)
{
    foreach(QVariant object, objects) {
        AstroBinImage image;
        QVariantMap properties = object.toMap();

        // animated
        image.m_Animated = properties.value("animated").toBool();

        // comments
        foreach(QVariant comment, properties.value("comments").toList()) {
            image.m_Comments.append(comment.toString());
        }

        // DEC center DMS
        image.m_DecCenterDms = dms(properties.value("dec_center_dms").toString());

        // description
        image.m_Description = properties.value("description").toString();

        // FOV
        QSizeF fov(properties.value("fieldw").toString().toDouble(), properties.value("fieldh").toString().toDouble());
        image.m_Fov = fov;
        image.m_FovUnits = properties.value("fieldunits").toString();

        // filename
        image.m_FileName = properties.value("filename").toString();

        // image size
        QSize size(properties.value("w").toString().toInt(), properties.value("h").toString().toInt());
        image.m_Size = size;

        // image id
        image.m_Id = properties.value("id").toString();

        // imaging cameras
        foreach(QVariant camera, properties.value("imaging_cameras").toList()) {
            image.m_ImagingCameras.append(camera.toString());
        }

        // imaging telescopes
        foreach(QVariant telescope, properties.value("imaging_telescopes").toList()) {
            image.m_ImagingTelescopes.append(telescope.toString());
        }

        // license
        image.m_License = properties.value("license").toInt();

        // orientation
        image.m_Orientation = properties.value("orientation").toString().toDouble();

        // original ext
        image.m_OriginalFileExtension = properties.value("original_ext").toString();

        // RA center HMS
        image.m_RaCenterHms = dms(properties.value("ra_center_hms").toString());

        // rating score
        image.m_RatingScore = properties.value("rating_score").toInt();

        // rating votes
        image.m_VoteCount = properties.value("rating_votes").toInt();

        // resource uri
        image.m_ResourceUri = properties.value("resource_uri").toString();

        // subjects
        foreach(QVariant subject, properties.value("subjects").toList()) {
            image.m_Subjects.append(subject.toString());
        }

        // title
        image.m_Title = properties.value("title").toString();

        // date time uploaded
        image.m_DateUploaded = QDateTime::fromString(properties.value("uploaded").toString(), Qt::ISODate);

        // date time updated
        image.m_DateUpdated = QDateTime::fromString(properties.value("updated").toString(), Qt::ISODate);

        // user
        image.m_User = properties.value("user").toString();

        m_LastSearchResult.append(image);
    }
}

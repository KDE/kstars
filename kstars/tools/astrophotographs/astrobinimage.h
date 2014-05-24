/***************************************************************************
                          astrobinimage.h  -  K Desktop Planetarium
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

#ifndef ASTROBINIMAGE_H
#define ASTROBINIMAGE_H

#include "dms.h"

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSize>
#include <QDateTime>

class AstroBinImage
{
    friend class AstroBinApiJson;
    friend class AstroBinApiXml;

public:
    inline QString title() const { return m_Title; }
    inline QString description() const { return m_Description; }
    inline QStringList subjects() const { return m_Subjects; }
    inline QString user() const { return m_User; }
    inline QString fileName() const { return m_FileName; }
    inline QString id() const { return m_Id; }
    inline QString resourceUri() const { return m_ResourceUri; }
    inline QString originalFileExtension() const { return m_OriginalFileExtension; }
    inline QSize size() const { return m_Size; }
    inline QStringList imagingCameras() const { return m_ImagingCameras; }
    inline QStringList imagingTelescopes() const { return m_ImagingTelescopes; }
    inline qlonglong license() const { return m_License; }
    inline qreal orientation() const { return m_Orientation; }
    inline bool animated() const { return m_Animated; }

    inline QDateTime dateUploaded() const { return m_DateUploaded; }
    inline QDateTime dateUpdated() const { return m_DateUpdated; }

    inline QStringList comments() const { return m_Comments; }
    inline qlonglong ratingScore() const { return m_RatingScore; }
    inline qlonglong voteCount() const { return m_VoteCount; }

    inline dms decCenterDms() const { return m_DecCenterDms; }
    inline dms raCenterHms() const { return m_RaCenterHms; }

    inline QSizeF fov() const { return m_Fov; }
    inline QString fovUnit() const { return m_FovUnits; }

    QUrl downloadUrl() const;
    QUrl downloadResizedUrl() const;
    QUrl downloadThumbnailUrl() const;
    QUrl regularImageUrl() const;
    QUrl thumbImageUrl() const;
    QUrl hdUrl() const;

private:
    QString m_Title;
    QString m_Description;
    QStringList m_Subjects;
    QString m_User;
    QString m_FileName;
    QString m_Id;
    QString m_ResourceUri;
    QString m_OriginalFileExtension;
    QString m_RegularImageUrl;
    QString m_ThumbImageUrl;
    QString m_HDUrl;
    QSize m_Size;
    QStringList m_ImagingCameras;
    QStringList m_ImagingTelescopes;
    qlonglong m_License;
    qreal m_Orientation;
    bool m_Animated;

    QDateTime m_DateUploaded;
    QDateTime m_DateUpdated;

    QStringList m_Comments;
    qlonglong m_RatingScore;
    qlonglong m_VoteCount;

    dms m_DecCenterDms;
    dms m_RaCenterHms;

    QSizeF m_Fov;
    QString m_FovUnits;
};

#endif // ASTROBINIMAGE_H

/***************************************************************************
                          astrobinapijson.h  -  K Desktop Planetarium
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

#ifndef ASTROBINAPIJSON_H
#define ASTROBINAPIJSON_H

#include "astrobinapi.h"

#include <QVariantMap>
#include <QVariantList>

class AstroBinApiJson : public AstroBinApi
{
    Q_OBJECT

public:
    explicit AstroBinApiJson(QNetworkAccessManager *manager, QObject *parent = 0);

    virtual ~AstroBinApiJson();

protected slots:
    void replyFinished(QNetworkReply *reply);

private:
    void readMetadata(const QVariantMap &metadata);
    void readObjects(const QVariantList &objects);
};

#endif // ASTROBINAPIJSON_H

/***************************************************************************
                          astrobinapixml.h  -  K Desktop Planetarium
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

#ifndef ASTROBINAPIXML_H
#define ASTROBINAPIXML_H

#include "astrobinapi.h"

QT_BEGIN_NAMESPACE
class QXmlStreamReader;
QT_END_NAMESPACE

class AstroBinApiXml : public AstroBinApi
{
    Q_OBJECT

public:
    explicit AstroBinApiXml(QNetworkAccessManager *manager, QObject *parent = 0);

    virtual ~AstroBinApiXml();

protected slots:
    void replyFinished(QNetworkReply *reply);

private:
    bool readResponse();
    bool readMetadata();
    bool readObjects();
    bool readObject();
    QStringList readList();
    void skipUnknownElement();

    QXmlStreamReader *m_XmlReader;
};

#endif // ASTROBINAPIXML_H

/***************************************************************************
                          guidesdocument.h  -  K Desktop Planetarium
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

#ifndef GUIDESDOCUMENT_H
#define GUIDESDOCUMENT_H

#include "skyguides.h"

#include <QString>
#include <QList>
#include <QXmlStreamReader>

class Guide;

class SkyGuides::GuidesDocument
{
public:
    GuidesDocument();
    ~GuidesDocument();

    void readBegin(const QString &contents);

    QString schemaVersion() const { return m_SchemaVersion; }
    QList<Guide*>* guides() { return &m_Guides; }

    bool isError() const;
    QXmlStreamReader::Error error() const;
    QString errorString() const;

private:
    void readGuides();
    void readGuide();
    void readSlides();
    void readSlide();
    void readImage();
    void readLink();
    void readAuthor();




    void skipUnknownElement();

    QXmlStreamReader *m_Reader;
    QString m_SchemaVersion;
    QList<Guide*> m_Guides;
};

#endif // GUIDESDOCUMENT_H

/***************************************************************************
                          astrobinsearchresult.h  -  K Desktop Planetarium
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

#ifndef ASTROBINSEARCHRESULT_H
#define ASTROBINSEARCHRESULT_H

#include <QList>

#include "astrobinimage.h"
#include "astrobinmetadata.h"

class AstroBinSearchResult : public QList<AstroBinImage>
{
    friend class AstroBinApiJson;
    friend class AstroBinApiXml;

public:
    AstroBinMetadata metadata() const { return m_Metadata; }

private:
    AstroBinMetadata m_Metadata;
};

#endif // ASTROBINSEARCHRESULT_H

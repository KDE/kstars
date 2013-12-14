/***************************************************************************
                    auxinfo.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 20 2008
    copyright            : (C) 2008 by Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef AUXINFO_H_
#define AUXINFO_H_

#include <QString>
#include <QStringList>
#include <QSharedData>

/**
 *@struct AuxInfo
 *Stores Users' Logs and QStringLists of URLs for images 
 *and webpages regarding an object in the sky.
 *@short Auxiliary information associated with a SkyObject.
 *@author Akarsh Simha
 *@version 1.0
 */

class AuxInfo : public QSharedData
{
public:
    QStringList ImageList;
    QStringList ImageTitle;
    QStringList InfoList;
    QStringList InfoTitle;
    QString userLog;
    QString notes;
    QHash<QString, QVariant> otherData;
};

#endif


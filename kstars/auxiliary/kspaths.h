/** *************************************************************************
                          kspaths.h  -  K Desktop Planetarium
                             -------------------
    begin                : 20/05/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSPATHS_H_
#define KSPATHS_H_
#include <QStandardPaths>
#include <QDir>

/**
 *@class KSPaths
 *@short Wrapper for QStandardPaths with Android assets support
 *The purpose of this class is to search for resources on some platforms with paths that are not
 *provided by QStandardPaths (e.g. assets:/ on Android that).
 *@author Artem Fedoskin
 *@version 1.0
 */

class KSPaths
{

public:
    static QString locate(QStandardPaths::StandardLocation location, const QString &fileName,
                          QStandardPaths::LocateOptions options = QStandardPaths::LocateFile);
    static QStringList locateAll(QStandardPaths::StandardLocation, const QString &fileNames,
                             QStandardPaths::LocateOptions options = QStandardPaths::LocateFile);
    static inline QString writableLocation(QStandardPaths::StandardLocation type) {
        return QStandardPaths::writableLocation(type) + "/kstars/";
    }
};
#endif

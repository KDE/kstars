/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KSPATHS_H_
#define KSPATHS_H_
#include <QStandardPaths>
#include <QDir>

/**
 *@class KSPaths
 *@short Wrapper for QStandardPaths with Android assets support
 *The purpose of this class is to search for resources on some platforms with paths that are not
 *provided by QStandardPaths (e.g. assets:/ on Android).
 *@author Artem Fedoskin, Jasem Mutlaq
 *@version 1.0
 */

class KSPaths
{
  public:
    static QString locate(QStandardPaths::StandardLocation location, const QString &fileName,
                          QStandardPaths::LocateOptions options = QStandardPaths::LocateFile);
    static QStringList locateAll(QStandardPaths::StandardLocation, const QString &fileNames,
                                 QStandardPaths::LocateOptions options = QStandardPaths::LocateFile);
    static QString writableLocation(QStandardPaths::StandardLocation type);
};
#endif

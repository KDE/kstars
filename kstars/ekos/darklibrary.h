/*  Ekos Dark Library Handler
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef DARKLIBRARY_H
#define DARKLIBRARY_H

#include <QObject>

namespace Ekos
{

/**
 *@class DarkLibrary
 *@short Handles aquisition & loading of dark frames for cameras. If a suitable dark frame exists, it is loaded from disk, otherwise it gets captured and saved
 * for later use.
 *@author Jasem Mutlaq
 *@version 1.0
 */
class DarkLibrary : public QObject
{

    Q_OBJECT

public:

    static DarkLibrary *Instance();

private:
  DarkLibrary(QObject *parent);
  ~DarkLibrary();
  static DarkLibrary * _DarkLibrary;

  QList<QVariantMap> darkFrames;

};

}

#endif  // DARKLIBRARY_H

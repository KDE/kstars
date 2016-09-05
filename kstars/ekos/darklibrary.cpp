/*  Ekos Dark Library Handler
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "darklibrary.h"
#include "Options.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "auxiliary/ksuserdb.h"

namespace Ekos
{

DarkLibrary * DarkLibrary::_DarkLibrary = NULL;

DarkLibrary * DarkLibrary::Instance()
{
    if (_DarkLibrary == NULL)
        _DarkLibrary = new DarkLibrary(KStars::Instance());

    return _DarkLibrary;
}

DarkLibrary::DarkLibrary(QObject *parent) : QObject(parent)
{
    //if ccd TEXT NOT NULL, chip INTEGER DEFAULT 0, binX INTEGER, binY INTEGER, temperature REAL, duration REAL, filename TEXT NOT NULL, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)"))

    //KStarsData::Instance()->userdb()->AddDarkFrame(test);

    /*KStarsData::Instance()->userdb()->DeleteDarkFrame("/home/jasem/foo.fits");

    KStarsData::Instance()->userdb()->GetAllDarkFrames(darkFrames);

    foreach(QVariantMap map, darkFrames)
    {
        for(QVariantMap::const_iterator iter = map.begin(); iter != map.end(); ++iter)
            qDebug() << iter.key() << " --> " << iter.value();
    }*/


}

DarkLibrary::~DarkLibrary()
{
}
}



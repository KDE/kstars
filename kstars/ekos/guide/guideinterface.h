/*  Ekos guide class interface
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef GUIDEINTERFACE_H
#define GUIDEINTERFACE_H

#include <QObject>

namespace Ekos
{

/**
 *@class GuideInterface
 *@short Interface skeleton for implementation of different guiding applications and/or routines
 *@author Jasem Mutlaq
 *@version 1.0
 */
class GuideInterface : public QObject
{

    Q_OBJECT

public:
    GuideInterface();
    virtual ~GuideInterface() {}

};

}

#endif  // GUIDEINTERFACE_H

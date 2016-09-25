/*  Ekos Internal Guider Class
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>.

    Based on lin_guider

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef INTERNALGUIDER_H
#define INTERNALGUIDER_H

#include "../guideinterface.h"

class InternalGuider : public GuideInterface
{
    Q_OBJECT

public:
    InternalGuider();
    ~InternalGuider();
};

#endif // INTERNALGUIDER_H

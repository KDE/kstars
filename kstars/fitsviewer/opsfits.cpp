/*  FITS Options
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include "opsfits.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"

OpsFITS::OpsFITS() : QFrame(KStars::Instance())
{
    setupUi( this );

    connect(kcfg_LimitedResourcesMode, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (toggled)
        {
            kcfg_Auto3DCube->setChecked(false);
            kcfg_AutoDebayer->setChecked(false);
        }
    });
    connect(kcfg_Auto3DCube, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (toggled) kcfg_LimitedResourcesMode->setChecked(false);
    });
    connect(kcfg_AutoDebayer, &QCheckBox::toggled, this, [this](bool toggled)
    {
        if (toggled) kcfg_LimitedResourcesMode->setChecked(false);
    });
}

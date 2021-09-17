/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "skyobjectlite.h"

SkyObjectLite::SkyObjectLite() : SkyPointLite(), object(nullptr)
{
}

void SkyObjectLite::setObject(SkyObject *obj)
{
    object = obj;
    if (obj)
    {
        setPoint(obj);
        emit translatedNameChanged(obj->translatedName());
    }
}

QString SkyObjectLite::getTranslatedName()
{
    if (object)
    {
        return object->translatedName();
    }
    else
    {
        return "";
    }
}

/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skycomposite.h"

#include "skyobjects/skyobject.h"
#include <qdebug.h>

SkyComposite::SkyComposite(SkyComposite *parent) : SkyComponent(parent)
{
}

SkyComposite::~SkyComposite()
{
    qDeleteAll(components());
    m_Components.clear();
}

void SkyComposite::addComponent(SkyComponent *component, int priority)
{
    //qDebug() << "Adding sky component " << component << " of type " << typeid( *component ).name() << " with priority " << priority;
    m_Components.insertMulti(priority, component);
    /*
    foreach( SkyComponent *p, components() ) {
        qDebug() << "List now has: " << p << " of type " << typeid( *p ).name();
    }
    */
}

void SkyComposite::removeComponent(SkyComponent *const component)
{
    // qDebug() << "Removing sky component " << component << " of type " << typeid( *component ).name();
    QMap<int, SkyComponent *>::iterator it;
    for (it = m_Components.begin(); it != m_Components.end();)
    {
        if (it.value() == component)
            it = m_Components.erase(it);
        else
            ++it;
    }
    /*
    foreach( SkyComponent *p, components() ) {
        qDebug() << "List now has: " << p << " of type " << typeid( *p ).name();
    }
    */
}

void SkyComposite::draw(SkyPainter *skyp)
{
    if (selected())
        foreach (SkyComponent *component, components())
            component->draw(skyp);
}

void SkyComposite::update(KSNumbers *num)
{
    foreach (SkyComponent *component, components())
        component->update(num);
}

SkyObject *SkyComposite::findByName(const QString &name)
{
    foreach (SkyComponent *comp, components())
    {
        SkyObject *o = comp->findByName(name);
        if (o)
            return o;
    }
    return nullptr;
}

SkyObject *SkyComposite::objectNearest(SkyPoint *p, double &maxrad)
{
    if (!selected())
        return nullptr;
    SkyObject *oBest = nullptr;
    foreach (SkyComponent *comp, components())
    {
        //qDebug() << "Checking " << typeid( *comp ).name() <<" for oBest";
        SkyObject *oTry = comp->objectNearest(p, maxrad);
        if (oTry)
        {
            oBest = oTry;
            maxrad =
                p->angularDistanceTo(oBest).Degrees() *
                0.95; // Set a new maxrad, smaller than original to give priority to the earlier objects in the list.
            // qDebug() << "oBest = " << oBest << " of type " << typeid( *oTry ).name() << "; updated maxrad = " << maxrad;
        }
    }
    // qDebug() << "Returning best match: oBest = " << oBest;
    return oBest; //will be 0 if no object nearer than maxrad was found
}

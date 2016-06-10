/** *************************************************************************
                          constellationnamesitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 10/06/2016
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
#include "constellationnamesitem.h"
#include "constellationnamescomponent.h"

ConstellationNamesItem::ConstellationNamesItem(const QList<SkyObject*>& namesList, RootNode* parent)
    :SkyItem(LabelsItem::label_t::CONSTEL_NAME_LABEL, parent), m_namesList(namesList)
{
}

void ConstellationNamesItem::update() {
    /*if ( ! selected() )
        return;

    const Projector *proj = SkyMap::Instance()->projector();
    SkyLabeler* skyLabeler = SkyLabeler::Instance();
    skyLabeler->useStdFont();
    skyLabeler->setPen( QColor( KStarsData::Instance()->colorScheme()->colorNamed( "CNameColor" ) ) );

    QString name;
    foreach(SkyObject *p, m_ObjectList) {
        if( ! proj->checkVisibility( p ) )
            continue;

        bool visible = false;
        QPointF o = proj->toScreen( p, false, &visible );
        if( !visible || !proj->onScreen( o ) )
            continue;

        if( Options::useLatinConstellNames() || Options::useLocalConstellNames() )
            name = p->name();
        else
            name = p->name2();

        o.setX( o.x() - 5.0 * name.length() );
        skyLabeler->drawGuideLabel( o, name, 0.0 );
    }

    skyLabeler->resetFont();*/
}

void ConstellationNamesItem::recreateList() {
    removeAllChildNodes();
    /*foreach(SkyObject *comet, m_namesList) {
        appendChildNode(new PointSourceNode(com, rootNode(),labelType()));
    }*/
}


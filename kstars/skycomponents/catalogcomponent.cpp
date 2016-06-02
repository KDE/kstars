/***************************************************************************
                    catalogcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/17/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "catalogcomponent.h"

#include <QDebug>
#include <KLocalizedString>
#include <KMessageBox>
#include <QDir>
#include <QFile>
#include <QPixmap>
#include <QTextStream>

#include "Options.h"

#include "kstarsdata.h"
#include "skymap.h"
#include "skypainter.h"
#include "skyobjects/starobject.h"
#include "skyobjects/deepskyobject.h"


QStringList CatalogComponent::m_Columns
                            = QString( "ID RA Dc Tp Nm Mg Flux Mj Mn PA Ig" )
                              .split( ' ',QString::SkipEmptyParts );

CatalogComponent::CatalogComponent(SkyComposite *parent,
                                   const QString &catname,
                                   bool showerrs, int index)
                                 : ListComponent(parent), m_catName(catname),
                                   m_Showerrs(showerrs), m_ccIndex(index) {
    loadData();
}

CatalogComponent::~CatalogComponent() {
}

void CatalogComponent::loadData() {
    emitProgressText( i18n("Loading custom catalog: %1", m_catName ) );

    QList < QPair <int, QString> > names;

    KStarsData::Instance()->catalogdb()->GetAllObjects(m_catName,
                                                       m_ObjectList,
                                                       names,
                                                       this);
    for (int iter = 0; iter < names.size(); iter++) {
        if (names.at(iter).first <= SkyObject::TYPE_UNKNOWN) {
            //FIXME JM 2016-06-02: inefficient and costly check
            // Need better way around this
            //if (!objectNames(names.at(iter).first).contains(names.at(iter).second))
                objectNames(names.at(iter).first).append(names.at(iter).second);
        }
    }

    // Remove Duplicates
    foreach(QStringList list, objectNames())
        list.removeDuplicates();

    CatalogData loaded_catalog_data;
    KStarsData::Instance()->catalogdb()->GetCatalogData(m_catName, loaded_catalog_data);
    m_catPrefix = loaded_catalog_data.prefix;
    m_catColor = loaded_catalog_data.color;
    m_catFluxFreq = loaded_catalog_data.fluxfreq;
    m_catFluxUnit = loaded_catalog_data.fluxunit;
    m_catEpoch = loaded_catalog_data.epoch;
}

void CatalogComponent::update( KSNumbers * ) {
    if ( selected() ) {
        KStarsData *data = KStarsData::Instance();
        foreach ( SkyObject *obj, m_ObjectList ) {
            DeepSkyObject *dso  = dynamic_cast< DeepSkyObject * >( obj );
            StarObject *so = dynamic_cast< StarObject *>( obj );
            Q_ASSERT( dso || so ); // We either have stars, or deep sky objects
            if( dso ) {
                // Update the deep sky object if need be
                if ( dso->updateID != data->updateID() ) {
                    dso->updateID = data->updateID();
                    if ( dso->updateNumID != data->updateNumID() ) {
                        dso->updateCoords( data->updateNum() );

                    }
                    dso->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                }
            }
            else {
                // Do exactly the same thing for stars
                if ( so->updateID != data->updateID() ) {
                    so->updateID = data->updateID();
                    if ( so->updateNumID != data->updateNumID() ) {
                        so->updateCoords( data->updateNum() );
                    }
                    so->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                }
            }
        }
        this->updateID = data->updateID();
    }
}

void CatalogComponent::draw( SkyPainter *skyp ) {
    if ( ! selected() ) return;

    skyp->setBrush( Qt::NoBrush );
    skyp->setPen( QColor( m_catColor ) );

    // Check if the coordinates have been updated
    if( updateID != KStarsData::Instance()->updateID() )
        update( 0 );

    //Draw Custom Catalog objects
    foreach ( SkyObject *obj, m_ObjectList ) {
        if ( obj->type()==0 ) {
            StarObject *starobj = static_cast<StarObject*>(obj);
            // FIXME SKYPAINTER
            skyp->drawPointSource(starobj, starobj->mag(), starobj->spchar() );
        } else {
            // FIXME: this PA calc is totally different from the one that was
            // in DeepSkyComponent which is now in SkyPainter .... O_o
            //      --hdevalence
            // PA for Deep-Sky objects is 90 + PA because major axis is
            // horizontal at PA=0
            // double pa = 90. + map->findPA( dso, o.x(), o.y() );
            DeepSkyObject *dso = static_cast<DeepSkyObject*>(obj);
            skyp->drawDeepSkyObject(dso, true);
        }
    }
}

bool CatalogComponent::selected() {
    if ( Options::showCatalogNames().contains(m_catName) && Options::showDeepSky() ) // do not draw / update custom catalogs if show deep-sky is turned off, even if they are chosen.
      return true;
    return false;
}

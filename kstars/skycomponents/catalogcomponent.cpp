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

#include <QDir>
#include <QFile>
#include <QPixmap>
#include <QTextStream>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>

#include "Options.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/starobject.h"
#include "skyobjects/deepskyobject.h"
#include "skypainter.h"

QStringList CatalogComponent::m_Columns = QString( "ID RA Dc Tp Nm Mg Flux Mj Mn PA Ig" ).split( ' ', QString::SkipEmptyParts );

CatalogComponent::CatalogComponent(SkyComposite *parent, const QString &catname, bool showerrs, int index) :
    ListComponent(parent),
    m_catName( catname ),
    m_Showerrs( showerrs ),
    m_ccIndex(index)
{
    loadData();
}

CatalogComponent::~CatalogComponent()
{
}

//TODO(spacetime): Remove previous code

void CatalogComponent::loadData()
{
    emitProgressText( i18n("Loading custom catalog: %1", m_catName ) );

    /*
     * ******************************************
     *   READ FROM DB HERE
     * ******************************************
    */
    QMap <int, QString> names;

    KStars::Instance()->data()->catalogdb()->GetAllObjects(m_catName,
                                                           m_ObjectList,
                                                           names,
                                                           this);
    const int number_of_types = 10;
    for (int i=0; i < number_of_types; i++) {
      QList<QString> retrieved = names.values(i);
      if (retrieved.length()>0) {
          objectNames(i).append(retrieved);
      }
    }

    KStars::Instance()->data()->catalogdb()->GetCatalogData(m_catName, m_catPrefix,
                                                            m_catColor, m_catFluxFreq,
                                                            m_catFluxUnit, m_catEpoch);

}

void CatalogComponent::update( KSNumbers * )
{
    if ( selected() ) {
        KStarsData *data = KStarsData::Instance();
        foreach ( SkyObject *obj, m_ObjectList ) {
            DeepSkyObject *dso  = dynamic_cast< DeepSkyObject * >( obj );
            StarObject *so = dynamic_cast< StarObject *>( so );
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

void CatalogComponent::draw( SkyPainter *skyp )
{
    if ( ! selected() ) return;

    skyp->setBrush( Qt::NoBrush );
    skyp->setPen( QColor( m_catColor ) );

    // Check if the coordinates have been updated
    if( updateID != KStarsData::Instance()->updateID() )
        update( 0 );

    //Draw Custom Catalog objects
    foreach ( SkyObject *obj, m_ObjectList ) {
        if ( obj->type()==0 ) {
            StarObject *starobj = (StarObject*)obj;
            //FIXME_SKYPAINTER
            skyp->drawPointSource(starobj, starobj->mag(), starobj->spchar() );
        } else {
            //FIXME: this PA calc is totally different from the one that was in
            //DeepSkyComponent which is now in SkyPainter .... O_o
            //      --hdevalence
            //PA for Deep-Sky objects is 90 + PA because major axis is horizontal at PA=0
            //double pa = 90. + map->findPA( dso, o.x(), o.y() );
            DeepSkyObject *dso = (DeepSkyObject*)obj;
            skyp->drawDeepSkyObject(dso,true);
        }
    }
}

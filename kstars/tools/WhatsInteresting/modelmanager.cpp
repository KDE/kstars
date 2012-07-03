/***************************************************************************
                          modelmanager.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/26/05
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ksfilereader.h"
#include "modelmanager.h"
#include "kstarsdatetime.h"
#include "skymapcomposite.h"
#include "skyobject.h"

ModelManager::ModelManager()
{
    planetsModel = new SkyObjListModel();
    starsModel = new SkyObjListModel();
    galModel = new SkyObjListModel();
    conModel = new SkyObjListModel();
    starClustModel = new SkyObjListModel();
    nebModel = new SkyObjListModel();

    initobjects[SkyObject::STAR] = QList<SkyObject *>();
    initobjects[SkyObject::GALAXY] = QList<SkyObject *>();
    initobjects[SkyObject::CONSTELLATION] = QList<SkyObject *>();
    initobjects[SkyObject::OPEN_CLUSTER] = QList<SkyObject *>();
    initobjects[SkyObject::PLANETARY_NEBULA] = QList<SkyObject *>();
    updateModels();
}

void ModelManager::updateModels()
{
    KStarsData *data = KStarsData::Instance();

    baseCatList<<"Planetary Objects"<<"Stars"<<"Constellations"<<"Deep-sky Objects" ;
    planetaryList<<"Planets"<<"Satellites";
    deepSkyList<<"Galaxies"<<"Star Clusters"<<"Nebulae";

    KSFileReader fileReader;
    if ( !fileReader.open("Interesting.dat") ) return;

    while ( fileReader.hasMoreLines() )
    {
        QString line, soname;
        SkyObject::TYPE sotype;
        line = fileReader.readLine();

        SkyObject *o;
        if (o = data->skyComposite()->findByName( line ))
        {
            kDebug()<<"Here";
            kDebug() << o->name()<< o->typeName() ;
            switch (o->type())
            {
                case 0:
                    sotype = SkyObject::STAR;
                    break;
                case 3:
                    sotype = SkyObject::OPEN_CLUSTER;
                    break;
                case 6:
                    sotype = SkyObject::PLANETARY_NEBULA;
                    break;
                case 8:
                    sotype = SkyObject::GALAXY;
                    break;
                case 11:
                    sotype = SkyObject::CONSTELLATION;
                    break;
                default:
                    break;
            }
            QList<SkyObject *> solist = initobjects[sotype];
            solist.append(o);
            initobjects[sotype] = solist;
        }
//         soname = line.split(',')[0];
//         //sotype = line.split(',')[1].toInt();
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::STAR))
    {
        //kDebug()<<so->name()<<so->mag();
        if (isVisible(data->geo(), data->lst(), so))
        {
            starsModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::GALAXY))
    {
        //kDebug()<<so->name()<<so->mag();
        if (isVisible(data->geo(), data->lst(), so))
        {
            galModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::CONSTELLATION))
    {
        //kDebug()<<so->name()<<so->mag();
        if (isVisible(data->geo(), data->lst(), so))
        {
            conModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::OPEN_CLUSTER))
    {
        //kDebug()<<so->name()<<so->mag();
        if (isVisible(data->geo(), data->lst(), so))
        {
            starClustModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::PLANETARY_NEBULA))
    {
        //kDebug()<<so->name()<<so->mag();
        if (isVisible(data->geo(), data->lst(), so))
        {
            nebModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::PLANET ) ) 
    {
        SkyObject *so = data->skyComposite()->findByName( name );
        //kDebug()<<so->name()<<so->mag();
        if (so->mag() < 7 && isVisible(data->geo(), data->lst(), so))
        {
//             SkyObjectItem *planetItem = new SkyObjectItem(o);
//             planetItem->setText(o->name());
            planetsModel->addSkyObject(new SkyObjItem(so));
        }
    }
}

SkyObjListModel* ModelManager::returnModel(ModelManager::LIST_TYPE Type)
{
    if (Type == Planets)
        return planetsModel;
    else if (Type == Stars)
        return starsModel;
    else if (Type == Galaxies)
        return galModel;
    else if (Type == Constellations)
        return conModel;
    else if (Type == Star_Clusters)
        return starClustModel;
    else if (Type == Nebulae)
        return nebModel;

    return (new SkyObjListModel());
}

SkyObjListModel* ModelManager::returnModel(QString Type)
{
    if (Type == "Planet")
        return planetsModel;
    else if (Type == "Star")
        return starsModel;
    else if (Type == "Galaxy")
        return galModel;
    else if (Type == "Constellation")
        return conModel;
//     else if (Type == "Star_Clusters")
//         return starClustModel;
//     else if (Type == "Nebulae")
//         return nebModel;

    return (new SkyObjListModel());
}

QStringList ModelManager::returnCatListModel(ModelManager::LIST_TYPE Type)
{
    if (Type == BaseList)
        return baseCatList;
    else if ( Type == PlanetaryObjects )
        return planetaryList;
    else
        return deepSkyList;
}

bool ModelManager::isVisible(GeoLocation* geo, dms* lst, SkyObject* so)
{
    bool visible = false;
    KStarsDateTime ut = geo->LTtoUT( KStarsDateTime(KDateTime::currentLocalDateTime()) );
    SkyPoint sp = so->recomputeCoords( ut, geo );

    //check altitude of object at this time.
    sp.EquatorialToHorizontal( lst, geo->lat() );
    if ( sp.alt().Degrees() > 6.0 ) {
        visible = true;
    }
    return visible;
}

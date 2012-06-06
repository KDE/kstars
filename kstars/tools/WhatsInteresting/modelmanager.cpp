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
#include "kstarsdata.h"
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

//     int count = 0;

    baseCatList<<"Planetary Objects"<<"Deep-sky Objects" ;
    planetaryList<<"Planets"<<"Satellites";
    deepSkyList<<"Stars"<<"Galaxies"<<"Constellations"<<"Star Clusters"<<"Nebulae";

//     QFile file( KStandardDirs::locateLocal( "appdata", "initWIList.dat" ) );
//     file.open( QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text );
    KSFileReader fileReader;
    if ( !fileReader.open("initWIList.dat") ) return;

    while ( fileReader.hasMoreLines() )
    {
        QString line, soname;
        SkyObject::TYPE sotype;
        line = fileReader.readLine();
        soname = line.split(',')[0];
        //sotype = line.split(',')[1].toInt();
        switch (line.split(',')[1].toInt())
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
        }
        SkyObject *o = data->skyComposite()->findByName( soname );
        QList<SkyObject *> solist = initobjects[sotype];
        solist.append(o);
        initobjects[sotype] = solist;
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::STAR))
    {
        //kDebug()<<so->name()<<so->mag();
        starsModel->addSkyObject(so);
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::GALAXY))
    {
        //kDebug()<<so->name()<<so->mag();
        galModel->addSkyObject(so);
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::CONSTELLATION))
    {
        //kDebug()<<so->name()<<so->mag();
        conModel->addSkyObject(so);
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::OPEN_CLUSTER))
    {
        //kDebug()<<so->name()<<so->mag();
        starClustModel->addSkyObject(so);
    }

    foreach(SkyObject *so, initobjects.value(SkyObject::PLANETARY_NEBULA))
    {
        //kDebug()<<so->name()<<so->mag();
        nebModel->addSkyObject(so);
    }

    foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::PLANET ) ) 
    {
        SkyObject *so = data->skyComposite()->findByName( name );
        //kDebug()<<so->name()<<so->mag();
        if (so->mag() < 7)
        {
//             SkyObjectItem *planetItem = new SkyObjectItem(o);
//             planetItem->setText(o->name());
            planetsModel->addSkyObject(so);
        }
    }
// 
//     foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::STAR ) ) 
//     {
//         SkyObject *o = data->skyComposite()->findByName( name );
//         if (o->mag() < 4)
//         {
// //             SkyObjectItem *planetItem = new SkyObjectItem(o);
// //             planetItem->setText(o->name());
//             kDebug()<<o->name()<<o->mag();
//             QString line = o->name().append(",").append(QString::number(o->type())).append("\n");
//             file.write(line.toUtf8());
//             count ++;
//             starsModel->addSkyObject(o);
//         }
//     }
// 
//     foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::GALAXY ) ) 
//     {
//         SkyObject *o = data->skyComposite()->findByName( name );
//         //kDebug()<<o->name()<<o->mag();
//         if (o->mag() < 2)
//         {
// //             SkyObjectItem *planetItem = new SkyObjectItem(o);
// //             planetItem->setText(o->name());
//             kDebug()<<o->name()<<o->mag();
//             QString line = o->name().append(",").append(QString::number(o->type())).append("\n");
//             file.write(line.toUtf8());
//             count ++;
//             galModel->addSkyObject(o);
//         }
//     }
// 
//     foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::CONSTELLATION ) ) 
//     {
//         SkyObject *o = data->skyComposite()->findByName( name );
//         //kDebug()<<o->name()<<o->mag();
//         if (o->mag() < 2)
//         {
// //             SkyObjectItem *planetItem = new SkyObjectItem(o);
// //             planetItem->setText(o->name());
//             kDebug()<<o->name()<<o->mag();
//             QString line = o->name().append(",").append(QString::number(o->type())).append("\n");
//             file.write(line.toUtf8());
//             count ++;
//             conModel->addSkyObject(o);
//         }
//     }
// 
//     foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::OPEN_CLUSTER ) ) 
//     {
//         SkyObject *o = data->skyComposite()->findByName( name );
//         //kDebug()<<o->name()<<o->mag();
//         if (o->mag() < 2)
//         {
// //             SkyObjectItem *planetItem = new SkyObjectItem(o);
// //             planetItem->setText(o->name());
//             kDebug()<<o->name()<<o->mag();
//             QString line = o->name().append(",").append(QString::number(o->type())).append("\n");
//             file.write(line.toUtf8());
//             count ++;
//             starClustModel->addSkyObject(o);
//         }
//     }
// 
//     foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::PLANETARY_NEBULA ) ) 
//     {
//         SkyObject *o = data->skyComposite()->findByName( name );
//         //kDebug()<<o->name()<<o->mag();
//         if (o->mag() < 2)
//         {
// //             SkyObjectItem *planetItem = new SkyObjectItem(o);
// //             planetItem->setText(o->name());
// 
//             kDebug()<<o->name()<<o->mag();
//             QString line = o->name().append(",").append(QString::number(o->type())).append("\n");
//             file.write(line.toUtf8());
//             count ++;
//             nebModel->addSkyObject(o);
//         }
//     }
// 
//     file.close();
//     kDebug()<<"Count="<<count;
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

QStringList ModelManager::returnCatListModel(ModelManager::LIST_TYPE Type)
{
    if (Type == BaseList)
        return baseCatList;
    else if ( Type == PlanetaryObjects )
        return planetaryList;
    else
        return deepSkyList;
}

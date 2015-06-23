/***************************************************************************
                          ConstellationArtComponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2015-05-27
    copyright            : (C) 2015 by M.S.Adityan
    email                : msadityan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "constellationartcomponent.h"
#include "kstars/auxiliary/ksfilereader.h"
#include "kstars/skymap.h"
#include "kstars/projections/projector.h"

ConstellationArtComponent::ConstellationArtComponent( SkyComposite *parent ):SkyComponent(parent)
{
    loadData();
}

ConstellationArtComponent::~ConstellationArtComponent()
{    while( !m_ConstList.isEmpty() ) {
        ConstellationsArt *o = m_ConstList.takeFirst();
        delete o;
    }
}

void ConstellationArtComponent::loadData(){

    int i = 0;

    // Find constellation art file and open it. If it doesn't exist, output an error message.
    KSFileReader fileReader;
    if ( ! fileReader.open("constellationsart.txt" ) )
    {
        qDebug() << "No constellationsart.txt file found for sky culture";
        return;
    }

    while ( fileReader.hasMoreLines() ) {
        QString line;
        int rah1,ram1,ras1,dd1,dm1,ds1,rah2,ram2,ras2,dd2,dm2,ds2;
        QChar sign1,sign2;

        line = fileReader.readLine();
        if( line.isEmpty() )
            continue;
        QChar mode = line.at( 0 );
        //ignore lines beginning with "#":
        if( mode == '#' )
            continue;

        //reads the first column from constellationart.txt
        m_ConstList[i]->rank = line.mid(0,2).trimmed().toInt();

        //Read pixel coordinates and HD number of star 1
        m_ConstList[i]->x1 = line.mid( 3, 3 ).trimmed().toInt();
        m_ConstList[i]->y1 = line.mid( 7, 3 ).trimmed().toInt();
        m_ConstList[i]->hd1 = line.mid( 11, 6 ).trimmed().toInt();

        //Read pixel coordinates and HD number of star 2
        m_ConstList[i]->x2 = line.mid( 18, 3 ).trimmed().toInt();
        m_ConstList[i]->y2 = line.mid( 22, 3 ).trimmed().toInt();
        m_ConstList[i]->hd2 = line.mid( 26, 6 ).trimmed().toInt();

        //Read J2000 coordinates of Star 1
        rah1 = line.mid(33,2).trimmed().toInt();
        ram1 = line.mid(35,2).trimmed().toInt();
        ras1 = line.mid(37,2).trimmed().toInt();
        sign1 = line.at(39);
        dd1 = line.mid(40,2).trimmed().toInt();
        dm1 = line.mid(42,2).trimmed().toInt();
        ds1 = line.mid(44,2).trimmed().toInt();

        //Read J2000 coordinates of Star 2
        rah2 = line.mid(47,2).trimmed().toInt();
        ram2 = line.mid(49,2).trimmed().toInt();
        ras2 = line.mid(51,2).trimmed().toInt();
        sign2 = line.at(53);
        dd2 = line.mid(54,2).trimmed().toInt();
        dm2 = line.mid(56,2).trimmed().toInt();
        ds2 = line.mid(58,2).trimmed().toInt();

        m_ConstList[i]->ra1.setH(rah1,ram1,ras1);
        m_ConstList[i]->ra2.setH(rah2,ram2,ras2);
        m_ConstList[i]->dec1 = dms(dd1,dm1,ds1);
        m_ConstList[i]->dec2 = dms(dd2,dm2,ds2);

        if ( sign1 == '-' )
            m_ConstList[i]->dec1.setD( -1.0*m_ConstList[i]->dec1.Degrees() );

        if ( sign2 == '-' )
            m_ConstList[i]->dec2.setD( -1.0*m_ConstList[i]->dec2.Degrees() );

        m_ConstList[i]->star1->setRA0(m_ConstList[i]->ra1);
        m_ConstList[i]->star2->setRA0(m_ConstList[i]->ra2);
        m_ConstList[i]->star1->setDec0(m_ConstList[i]->dec1);
        m_ConstList[i]->star2->setDec0(m_ConstList[i]->dec2);

        //Read abbreviation and image file name
        m_ConstList[i]->abbrev = line.mid( 61, 3 );
        m_ConstList[i]->imageFileName  = line.mid( 65 ).trimmed();

        //Make a QImage object pointing to constellation image
        m_ConstList[i]->constart_image = QImage(m_ConstList[i]->imageFileName,0);
        i++;

        }
}

void ConstellationArtComponent::showList()
{
    int i = 0;
    for(i = 0; i < m_ConstList.size(); i++)
    {
        qDebug()<<m_ConstList[i]->rank<<m_ConstList[i]->getAbbrev()<<m_ConstList[i]->getImageFileName();
        qDebug()<<m_ConstList[i]->getx1()<<m_ConstList[i]->gety1()<<m_ConstList[i]->gethd1();
        qDebug()<<m_ConstList[i]->getx2()<<m_ConstList[i]->gety2()<<m_ConstList[i]->gethd2();
    }
}

void ConstellationArtComponent::draw(SkyPainter *skyp){

    int i = 0;
    //Loops through the QList containing all data required to draw western constellations.
    //There are 85 images, so m_ConstList.size() should return 85.
    for(i=0; i < m_ConstList.size(); i++){
            drawConstArtImage( skyp, m_ConstList[i]);
    }
}


void ConstellationArtComponent::drawConstArtImage(SkyPainter *skyp, ConstellationsArt *obj, bool drawFlag)
{
    if(drawFlag==false) return;

    SkyMap *map = SkyMap::Instance();
    //const Projector *proj = map->projector();

    KStarsData *data = KStarsData::Instance();
    //UpdateID updateID = data->updateID();

    skyp->setBrush( Qt::NoBrush );

    //if ( obj->updateID != updateID ) {
        //obj->updateID = updateID;

        int w = obj->imageWidth();
        int h = obj->imageHeight();

        QPointF position1,position2;
        SkyPoint s1 = obj->getStar1();
        SkyPoint s2 = obj->getStar2();

        position1 = map->projector()->toScreen(&s1);
        position2 = map->projector()->toScreen(&s2);

        QPainter painter;
        painter.save();
        //How do I define position to translate?
        painter.translate(position1);
        painter.drawImage( QRect(-0.5*w, -0.5*h, w, h), obj->image() );
        painter.restore();

}

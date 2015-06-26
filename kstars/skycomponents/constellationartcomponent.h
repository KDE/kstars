/***************************************************************************
                          ConstellationArtComponent.h  -  K Desktop Planetarium
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

#ifndef ConstellationArtComponent_H
#define ConstellationArtComponent_H

#include "kstars/skyobjects/constellationsart.h"
#include "skycomponent.h"
#include <QImage>
#include <QSqlDatabase>
#include "kstars/projections/projector.h"
#include "kstars/auxiliary/dms.h"
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
class ConstellationsArt;
class QColor;
class SkyMap;
class SkyPoint;
class SkyMesh;
class QImage;
class QPainter;
class dms;
class Projector;
class QPainter;
class QSqlDatabase;
class QGraphicsPixmapItem;
class QGraphicsView;

class ConstellationArtComponent : public SkyComponent
{
public:

    //Constructor
    explicit ConstellationArtComponent ( SkyComposite* );

    //Destructor
    ~ConstellationArtComponent();

    /*Read the file constellationsart.txt
    This catalog has the following columns.
    Constellation serial number,x1,y1,HD1,x2,y2,HD2,x3,y3,HD3,Abbrev,Image file*/
    void loadData();

    //Outputs details of the QList
    void showList();

    virtual void draw( SkyPainter *skyp );

    QList<ConstellationsArt*> m_ConstList;


private:

    void drawConstArtImage(SkyPainter *skyp, ConstellationsArt *obj, bool drawFlag = true);
};

#endif // ConstellationArtComponent_H

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
#include "kstars/Options.h"
#include "kstars/skyobjects/constellationsart.h"
#include "skycomponent.h"
#include <QImage>
#include <QSqlDatabase>
#include "kstars/projections/projector.h"
#include "kstars/auxiliary/dms.h"
#include "kstars/skymap.h"
#include "kstars/skyqpainter.h"
class ConstellationsArt;
class QColor;
class SkyMap;
class SkyPoint;
class SkyMesh;
class QImage;
class dms;
class Projector;
class QSqlDatabase;
class SkyMap;
class CultureList;
class SkyQPainter;

/**
 * @class ConstellationArtComponent
 * Represents the ConstellationsArt objects.
 * For each skyculture there is a separate table in skycultures.sqlite.
 * @author M.S.Adityan
 * @version 0.1
 */
class ConstellationArtComponent : public SkyComponent
{
public:

    /** Constructor*/
    explicit ConstellationArtComponent ( SkyComposite*, CultureList* cultures );

    /** Destructor*/
    ~ConstellationArtComponent();

    /**
     * @short Read the skycultures.sqlite database file.
     * Parse all the data from the skycultures database.Construct a ConstellationsArt object
     * from the data, and add it to a QList.
     * @return true if data file is successfully read.
     */
    void loadData();

    /**
     * @short Shows the details of the constellations
     * selected skyculture
     */
    void showList();

    virtual void draw( SkyPainter *skyp );

    QList<ConstellationsArt*> m_ConstList;

private:
    QString cultureName;
    int records;
};

#endif // ConstellationArtComponent_H

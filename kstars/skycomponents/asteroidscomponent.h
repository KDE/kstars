/***************************************************************************
                          asteroidscomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/30/08
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

#ifndef ASTEROIDSCOMPONENT_H
#define ASTEROIDSCOMPONENT_H

#include <QList>
#include <QPointer>

#include "binarylistcomponent.h"
#include "ksparser.h"
#include "typedef.h"
#include "skyobjects/ksasteroid.h"
#include "solarsystemlistcomponent.h"

class FileDownloader;

/** @class AsteroidsComponent
 * Represents the asteroids on the sky map.
 *
 * @author Thomas Kabelmann
 * @version 0.1
 */
class AsteroidsComponent : public QObject, public SolarSystemListComponent,
       virtual public BinaryListComponent<KSAsteroid, AsteroidsComponent>
{
    Q_OBJECT

    friend class BinaryListComponent<KSAsteroid, AsteroidsComponent>;
  public:
    /** @short Default constructor.
         * @p parent pointer to the parent SolarSystemComposite
         */
    explicit AsteroidsComponent(SolarSystemComposite *parent);

    ~AsteroidsComponent() override;
    void draw(SkyPainter *skyp) override;
    bool selected() override;
    SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

    void updateDataFile() override;

    QString ans();

  protected slots:
    void downloadReady();
    void downloadError(const QString &errorString);

  private:
    void loadDataFromText() override;
    FileDownloader *downloadJob;
};

#endif

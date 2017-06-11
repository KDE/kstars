/***************************************************************************
                          cometscomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/24/09
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef COMETSCOMPONENT_H
#define COMETSCOMPONENT_H

class SkyLabeler;

#include "solarsystemlistcomponent.h"
#include "ksparser.h"
#include <QList>

class FileDownloader;

/** @class CometsComponent
 * This class encapsulates the Comets
 *
 * @author Jason Harris
 * @version 0.1
 */
class CometsComponent : public QObject, public SolarSystemListComponent
{
    Q_OBJECT

  public:
    /** @short Default constructor.
         * @p parent pointer to the parent SolarSystemComposite
         */
    explicit CometsComponent(SolarSystemComposite *parent);

    virtual ~CometsComponent();
    bool selected() Q_DECL_OVERRIDE;
    void draw(SkyPainter *skyp) Q_DECL_OVERRIDE;
    void updateDataFile();

  protected slots:
    void downloadReady();
    void downloadError(const QString &errorString);

  private:
    void loadData();
    FileDownloader *downloadJob;
};

#endif

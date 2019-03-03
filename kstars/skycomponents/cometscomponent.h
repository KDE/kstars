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

#pragma once

#include "ksparser.h"
#include "solarsystemlistcomponent.h"

#include <QList>

class FileDownloader;
class SkyLabeler;

/**
 * @class CometsComponent
 *
 * This class encapsulates the Comets
 *
 * @author Jason Harris
 * @version 0.1
 */
class CometsComponent : public QObject, public SolarSystemListComponent
{
    Q_OBJECT

  public:
    /**
     * @short Default constructor.
     *
     * @p parent pointer to the parent SolarSystemComposite
     */
    explicit CometsComponent(SolarSystemComposite *parent);

    virtual ~CometsComponent() override = default;

    bool selected() override;
    void draw(SkyPainter *skyp) override;
    void updateDataFile(bool isAutoUpdate=false);

  protected slots:
    void downloadReady();
    void downloadError(const QString &errorString);

  private:
    void loadData();

    FileDownloader *downloadJob { nullptr };
};

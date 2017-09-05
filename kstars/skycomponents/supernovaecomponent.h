/*  Supernova Component
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on Samikshan Bairagya GSoC work.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef SUPERNOVAE_COMPONENT_H
#define SUPERNOVAE_COMPONENT_H

#include "listcomponent.h"
#include "skyobjects/supernova.h"

#include "ksnumbers.h"

#include <QList>

/**
 * @class SupernovaeComponent This class encapsulates Supernovae.
 *
 * @author Jasem Mutlaq, Samikshan Bairagya
 *
 * @version 0.2
 */

class Supernova;
class FileDownloader;

class SupernovaeComponent : public QObject, public ListComponent
{
    Q_OBJECT

  public:
    explicit SupernovaeComponent(SkyComposite *parent);
    ~SupernovaeComponent() override;
    bool selected() override;
    void update(KSNumbers *num = nullptr) override;
    SkyObject *findByName(const QString &name) override;
    SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

    /**
         * @note This should actually be implemented in a better manner.
         * Possibly by checking if the host galaxy for the supernova is drawn.
         */
    void draw(SkyPainter *skyp) override;

    //virtual void notifyNewSupernovae();
    /**
         * @note Basically copy pasted from StarComponent::zoomMagnitudeLimit()
         */
    static float zoomMagnitudeLimit();

  public slots:
    /**
         * @short This initiates updating of the data file
         */
    void slotTriggerDataFileUpdate();

  protected slots:
    void downloadReady();
    void downloadError(const QString &errorString);

  private:
    void loadData();

    FileDownloader *downloadJob = nullptr;
};

#endif

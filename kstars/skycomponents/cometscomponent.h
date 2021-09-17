/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksparser.h"
#include "solarsystemlistcomponent.h"
#include "filedownloader.h"

#include <QList>
#include <QPointer>

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
        void updateDataFile(bool isAutoUpdate = false);

    protected slots:
        void downloadReady();
        void downloadError(const QString &errorString);

    private:
        void loadData();

        QPointer<FileDownloader> downloadJob;
};

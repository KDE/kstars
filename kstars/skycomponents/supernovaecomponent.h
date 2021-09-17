/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on Samikshan Bairagya GSoC work.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksnumbers.h"
#include "listcomponent.h"
#include "skyobjects/supernova.h"
#include "filedownloader.h"

#include <QList>
#include <QPointer>

/**
 * @class SupernovaeComponent
 * @brief This class encapsulates Supernovae.
 *
 * @author Jasem Mutlaq, Samikshan Bairagya
 *
 * @version 0.2
 */

class Supernova;

class SupernovaeComponent : public QObject, public ListComponent
{
        Q_OBJECT

    public:
        explicit SupernovaeComponent(SkyComposite *parent);
        virtual ~SupernovaeComponent() override = default;

        bool selected() override;
        void update(KSNumbers *num = nullptr) override;
        SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

        /**
         * @note This should actually be implemented in a better manner.
         * Possibly by checking if the host galaxy for the supernova is drawn.
         */
        void draw(SkyPainter *skyp) override;

        //virtual void notifyNewSupernovae();
        /** @note Basically copy pasted from StarComponent::zoomMagnitudeLimit() */
        static float zoomMagnitudeLimit();

    public slots:
        /** @short This initiates updating of the data file */
        void slotTriggerDataFileUpdate();

    protected slots:
        void downloadReady();
        void downloadError(const QString &errorString);

    private:
        void loadData();
        bool m_DataLoaded { false }, m_DataLoading { false };
        QPointer<FileDownloader> downloadJob;
};

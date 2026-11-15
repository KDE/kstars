/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opships.h"
#include "ui_opshipsdisplay.h"
#include "ui_opshipscache.h"

class KConfigDialog;
class FileDownloader;

class OpsHIPSDisplay : public QFrame, public Ui::OpsHIPSDisplay
{
        Q_OBJECT

    public:
        explicit OpsHIPSDisplay();
};

class OpsHIPSCache : public QFrame, public Ui::OpsHIPSCache
{
        Q_OBJECT

    public:
        explicit OpsHIPSCache();
};

/**
 * @class OpsHIPS
 *
 * HIPS Settings including download of external sources and enabling/disabling them accordingly.
 *
 * @author Jasem Mutlaq
 */
class OpsHIPS : public QFrame, public Ui::OpsHIPS
{
        Q_OBJECT

    public:
        explicit OpsHIPS();
        virtual ~OpsHIPS() override = default;

    public Q_SLOTS:
        void slotRefresh();

    protected Q_SLOTS:
        void downloadReady();
        void downloadError(const QString &errorString);
        void previewReady();
        void slotItemUpdated(QListWidgetItem *item);
        void slotItemClicked(QListWidgetItem *item);
        void slotFilterChanged(const QString &text);

    private:

        void setPreview(const QString &id, const QString &url);
        /** @brief requestSources Download the HiPS source list from the current MocServer. */
        void requestSources();

        KConfigDialog *m_ConfigDialog { nullptr };
        FileDownloader *downloadJob { nullptr };
        FileDownloader *previewJob { nullptr };

        QList<QMap<QString, QString>> sources;
        // MocServer base URLs and the one being tried
        QStringList m_MocServerUrls;
        int m_MocServerIndex { 0 };
        bool dirty { false };
};

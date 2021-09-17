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

  public slots:
    void slotRefresh();    

  protected slots:
    void downloadReady();
    void downloadError(const QString &errorString);
    void previewReady();
    void slotItemUpdated(QListWidgetItem *item);
    void slotItemClicked(QListWidgetItem *item);

  private:

    void setPreview(const QString &id, const QString &url);

    KConfigDialog *m_ConfigDialog { nullptr };
    FileDownloader *downloadJob { nullptr };
    FileDownloader *previewJob { nullptr };

    QList<QMap<QString,QString>> sources;
    bool dirty { false };
};

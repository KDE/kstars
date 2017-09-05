/*  HiPS Options
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
    ~OpsHIPS() override;

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

    KConfigDialog *m_ConfigDialog = nullptr;
    FileDownloader *downloadJob = nullptr;
    FileDownloader *previewJob = nullptr;

    QList<QMap<QString,QString>> sources;
    bool dirty=false;
};

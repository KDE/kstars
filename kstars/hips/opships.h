/*  HiPS Options
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "ui_opships.h"

class KConfigDialog;
class FileDownloader;

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
    ~OpsHIPS();

  public slots:
    void slotApply();
    void slotRefresh();

  protected slots:
    void downloadReady();
    void downloadError(const QString &errorString);

  private:
    KConfigDialog *m_ConfigDialog = nullptr;
    FileDownloader *downloadJob = nullptr;

    QList<QVariantMap> sources;
    bool dirty=false;
};

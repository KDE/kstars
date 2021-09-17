/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_guidemanager.h"

#include "ekos/ekos.h"
#include "ekos/guide/guide.h"

#include <QWidget>
#include <QProgressIndicator.h>

class QProgressIndicator;

namespace Ekos
{

class GuideManager : public QWidget, public Ui::GuideManager
{
    Q_OBJECT
public:
    GuideManager(QWidget *parent = nullptr);
    void init(Guide *guideProcess);
    void updateGuideDetailView();
    void reset();

public slots:
    void updateGuideStatus(GuideState status);
    void updateGuideStarPixmap(QPixmap &starPix);
    void updateSigmas(double ra, double de);

private:
    QProgressIndicator *guidePI { nullptr };
    GuideStateWidget *guideStateWidget { nullptr };
    std::unique_ptr<QPixmap> guideStarPixmap;

};
}

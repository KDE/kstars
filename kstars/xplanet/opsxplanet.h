/*
    SPDX-FileCopyrightText: 2008 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsxplanet.h"

class KStars;

class OpsXplanet : public QFrame, public Ui::OpsXplanet
{
    Q_OBJECT

  public:
    explicit OpsXplanet(KStars *_ks);
    virtual ~OpsXplanet() override = default;

  private:
    KStars *ksw { nullptr };

    QString XPlanetShareDirectory();

  private slots:
    void showXPlanetMapsDirectory();
    void slotConfigFileWidgets(bool on);
    void slotStarmapFileWidgets(bool on);
    void slotArcFileWidgets(bool on);
    void slotLabelWidgets(bool on);
    void slotMarkerFileWidgets(bool on);
    void slotMarkerBoundsWidgets(bool on);
    void slotProjectionWidgets(int index);
    void slotBackgroundWidgets(bool on);
    void toggleXPlanetInternal();
    void slotSelectConfigFile();
    void slotSelectStarMapFile();
    void slotSelectArcFile();
};

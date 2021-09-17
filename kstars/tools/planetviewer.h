/*
    SPDX-FileCopyrightText: 2003 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kstarsdatetime.h"
#include "ui_planetviewer.h"

#include <QDialog>
#include <QTimer>

class QKeyEvent;
class QPaintEvent;

#define AUMAX 48

class KSPlanetBase;

class PlanetViewerUI : public QFrame, public Ui::PlanetViewer
{
    Q_OBJECT

  public:
    explicit PlanetViewerUI(QWidget *parent = nullptr);
};

/**
 * @class PlanetViewer
 * @short Display an overhead view of the solar system
 *
 * @version 1.0
 * @author Jason Harris
 */
class PlanetViewer : public QDialog
{
    Q_OBJECT

  public:
    explicit PlanetViewer(QWidget *parent = nullptr);

    inline QString centerPlanet() const { return CenterPlanet; }
    inline void setCenterPlanet(const QString &cp) { CenterPlanet = cp; }

    inline KPlotObject *planetObject(uint i) const { return planet[i]; }
    QString planetName(uint i) const;

  protected:
    void keyPressEvent(QKeyEvent *e) override;
    void paintEvent(QPaintEvent *) override;

  private slots:
    void initPlotObjects();
    void tick();
    void setTimeScale(float);
    void slotChangeDate();
    void slotRunClock();
    void slotToday();
    void slotCloseWindow();

  private:
    void updatePlanets();

    PlanetViewerUI *pw { nullptr };
    KStarsDateTime ut;
    double scale { 0 };
    bool isClockRunning { false };
    QTimer tmr;
    int UpdateInterval[9], LastUpdate[9];
    QString CenterPlanet;
    QList<KSPlanetBase *> PlanetList;
    KPlotObject *ksun { nullptr };
    KPlotObject *planet[9] { nullptr };
};

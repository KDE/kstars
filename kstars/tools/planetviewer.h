/***************************************************************************
                          planetviewer.h  -  Display overhead view of the solar system
                             -------------------
    begin                : Sun May 25 2003
    copyright            : (C) 2003 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef PLANETVIEWER_H_
#define PLANETVIEWER_H_

#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPaintEvent>

#include <kdialog.h>
#include <kpushbutton.h>

#include "pvplotwidget.h"
#include "ui_planetviewer.h"
#include "kstarsdatetime.h"

#define AUMAX 48

class KSPlanetBase;

class PlanetViewerUI : public QFrame, public Ui::PlanetViewer {
    Q_OBJECT
public:
    PlanetViewerUI(QWidget *parent = 0 );
};

/**@class PlanetViewer
	*@short Display an overhead view of the solar system
	*@version 1.0
	*@author Jason Harris
	*/
class PlanetViewer : public KDialog
{
    Q_OBJECT
public:
    PlanetViewer(QWidget *parent = 0);

    ~PlanetViewer();

    enum ViewType { Planets = 0, Astroids = 1, Both = 2 };

    inline QString centerPlanet() const { return CenterPlanet; }
    inline void setCenterPlanet( const QString &cp ) { CenterPlanet = cp; }

    inline KPlotObject* planetObject(uint i) const { return planet[i]; }
    QString planetName(uint i) const;

protected:
    virtual void keyPressEvent( QKeyEvent *e );
    virtual void paintEvent( QPaintEvent* );

private slots:
    void initPlotObjects();
    void tick();
    void setTimeScale(float);
    void slotChangeDate();
    void slotRunClock();
    void slotToday();
    void slotCloseWindow();
    void slotAstroidsVisibilityChanged();

private:
    void updatePlanets();

    PlanetViewerUI *pw;
    KStarsDateTime ut;
    double scale,astroidsVisibility;
    bool isClockRunning;
    QTimer tmr;
    int UpdateInterval[9], LastUpdate[9];
    QString CenterPlanet;

    QList<KSPlanetBase*> PlanetList;
    QList<KSPlanetBase*> AstroidList;
    QList<KSPlanetBase*> visibleAstroidList;

    KPlotObject *ksun;
    KPlotObject *planet[9];
    QList<KPlotObject*> astroid;
};

#endif

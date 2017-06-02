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

#include <QDialog>
#include <QPushButton>

#include "pvplotwidget.h"
#include "ui_planetviewer.h"
#include "kstarsdatetime.h"

#define AUMAX 48

class KSPlanetBase;

class PlanetViewerUI : public QFrame, public Ui::PlanetViewer
{
        Q_OBJECT
    public:
        PlanetViewerUI(QWidget * parent = 0 );
};

/** @class PlanetViewer
	*@short Display an overhead view of the solar system
	*@version 1.0
	*@author Jason Harris
	*/
class PlanetViewer : public QDialog
{
        Q_OBJECT
    public:
        PlanetViewer(QWidget * parent = 0);
        ~PlanetViewer();

        inline QString centerPlanet() const
        {
            return CenterPlanet;
        }
        inline void setCenterPlanet( const QString &cp )
        {
            CenterPlanet = cp;
        }

        inline KPlotObject * planetObject(uint i) const
        {
            return planet[i];
        }
        QString planetName(uint i) const;

    protected:
        void keyPressEvent( QKeyEvent * e ) Q_DECL_OVERRIDE;
        void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;

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

        PlanetViewerUI * pw;
        KStarsDateTime ut;
        double scale;
        bool isClockRunning;
        QTimer tmr;
        int UpdateInterval[9], LastUpdate[9];
        QString CenterPlanet;

        QList<KSPlanetBase *> PlanetList;

        KPlotObject * ksun;
        KPlotObject * planet[9];
};

#endif

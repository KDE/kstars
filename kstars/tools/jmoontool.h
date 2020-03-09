/***************************************************************************
                          jmoontool.h  -  Display overhead view of the solar system
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

#pragma once

#include <QDialog>

class QColor;

class KPlotWidget;
class KStars;

/**
 * @class JMoonTool
 * @short Display the positions of Jupiter's moons as a function of time
 *
 * @version 1.0
 * @author Jason Harris
 */
class JMoonTool : public QDialog
{
        Q_OBJECT

    public:
        explicit JMoonTool(QWidget *parent = nullptr);

    protected:
        virtual void keyPressEvent(QKeyEvent *e) override;

    private:
        void initPlotObjects();

        KPlotWidget *pw { nullptr };
        QColor colJp, colIo, colEu, colGn, colCa;
};

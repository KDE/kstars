/***************************************************************************
                      moonphasetool.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Jun 26 2010
    copyright            : (C) 2010 by Akarsh Simha
    email                : akarshsimha@gmail.com
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

//#include "widgets/genericcalendarwidget.h"
#include "widgets/moonphasecalendarwidget.h"

#include <QDialog>

#include <memory>

class QCalendarWidget;

class KSMoon;
class KSNumbers;
class KSSun;

/**
 * @class MoonPhaseTool
 * @short Shows a moon phase calendar for an entire month
 *
 * This tool shows a moon phase calendar for an entire month in the
 * same spirit as this website:
 * https://stardate.org/nightsky/moon
 *
 * It uses a clone of KSMoon in the backend to perform the phase
 * computation and uses the moon images for the various phases to
 * display a nice table
 *
 * @author Akarsh Simha
 * @version 1.0
 */
class MoonPhaseTool : public QDialog
{
    Q_OBJECT

  public:
    explicit MoonPhaseTool(QWidget *p);

    /*
        public slots:

        void slotUpdate();
        void slotSetMonth();
        */

  private:
    std::unique_ptr<KSMoon> m_Moon;
    std::unique_ptr<KSSun> m_Sun;
    KSNumbers *m_Num { nullptr };
    unsigned short month { 0 };
    unsigned int year { 0 };
    //GenericCalendarWidget *gcw { nullptr };
    QCalendarWidget *gcw { nullptr };
    std::unique_ptr<MoonPhaseCalendar> mpc;
};

/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

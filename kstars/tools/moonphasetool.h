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

#ifndef _MOONPHASETOOL_H_
#define _MOONPHASETOOL_H_

#include "widgets/genericcalendarwidget.h"
#include "widgets/moonphasecalendarwidget.h"

#include "skyobjects/ksmoon.h"
#include "ksnumbers.h"





/**
 *@class MoonPhaseTool
 *@author Akarsh Simha
 *@version 1.0
 *@short Shows a moon phase calendar for an entire month
 *
 * This tool shows a moon phase calendar for an entire month in the
 * same spirit as this website:
 * http://stardate.org/nightsky/moon/index.php
 *
 * It uses a clone of KSMoon in the backend to perform the phase
 * computation and uses the moon images for the various phases to
 * display a nice table
 */

class MoonPhaseTool : public QFrame {

    Q_OBJECT

public:
    MoonPhaseTool(QWidget *p);
    ~MoonPhaseTool();

    /*
public slots:

    void slotUpdate();
    void slotSetMonth();
    */

private:
    KSMoon *m_Moon;
    KSNumbers *m_Num;
    unsigned short month;
    unsigned int year;
    GenericCalendarWidget *gcw;
    MoonPhaseCalendar *mpc;
};

#endif

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

#ifndef _MOONPHASETOOL_H_
#define _MOONPHASETOOL_H_

#include "widgets/genericcalendarwidget.h"
#include "widgets/moonphasecalendarwidget.h"

#include "skyobjects/ksmoon.h"
#include "ksnumbers.h"



/**
 *@class MoonPhaseTool
 *@author Akarsh Simha
 *@version 1.0
 *@short Shows a moon phase calendar for an entire month
 *
 * This tool shows a moon phase calendar for an entire month in the
 * same spirit as this website:
 * http://stardate.org/nightsky/moon/index.php
 *
 * It uses a clone of KSMoon in the backend to perform the phase
 * computation and uses the moon images for the various phases to
 * display a nice table
 */

class MoonPhaseTool : public QFrame {

    Q_OBJECT

public:
    MoonPhaseTool(QWidget *p);
    ~MoonPhaseTool();

    /*
public slots:

    void slotUpdate();
    void slotSetMonth();
    */

private:
    KSMoon *m_Moon;
    KSNumbers *m_Num;
    unsigned short month;
    unsigned int year;
    GenericCalendarWidget *gcw;
    MoonPhaseCalendar *mpc;
};

#endif

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

#ifndef _MOONPHASETOOL_H_
#define _MOONPHASETOOL_H_

#include "widgets/genericcalendarwidget.h"
#include "widgets/moonphasecalendarwidget.h"

#include "skyobjects/ksmoon.h"
#include "ksnumbers.h"



/**
 *@class MoonPhaseTool
 *@author Akarsh Simha
 *@version 1.0
 *@short Shows a moon phase calendar for an entire month
 *
 * This tool shows a moon phase calendar for an entire month in the
 * same spirit as this website:
 * http://stardate.org/nightsky/moon/index.php
 *
 * It uses a clone of KSMoon in the backend to perform the phase
 * computation and uses the moon images for the various phases to
 * display a nice table
 */

class MoonPhaseTool : public QFrame {

    Q_OBJECT

public:
    MoonPhaseTool(QWidget *p);
    ~MoonPhaseTool();

    /*
public slots:

    void slotUpdate();
    void slotSetMonth();
    */

private:
    KSMoon *m_Moon;
    KSNumbers *m_Num;
    unsigned short month;
    unsigned int year;
    GenericCalendarWidget *gcw;
    MoonPhaseCalendar *mpc;
};

#endif


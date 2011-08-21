/***************************************************************************
                          pwizchartcontents.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 7 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef PWIZCHARTCONTENTS_H
#define PWIZCHARTCONTENTS_H

#include "ui_pwizchartcontents.h"

class PrintingWizard;

/**
  * \class PWizChartContentsUI
  * \brief User interface for "Configure chart contents" step of the Printing Wizard.
  * \author Rafał Kułaga
  */
class PWizChartContentsUI : public QFrame, public Ui::PWizChartContents
{
    Q_OBJECT
public:
    /**
      * \brief Constructor.
      */
    PWizChartContentsUI(PrintingWizard *wizard, QWidget *parent = 0);

    /**
      * \brief Enable or disable specific fields depending on the type of selected object.
      */
    void entered();

    /**
      * \brief Check if general details table is enabled.
      * \return True if general details table is enabled.
      */
    bool isGeneralTableChecked();

    /**
      * \brief Check if position details table is enabled.
      * \return True if position details table is enabled.
      */
    bool isPositionTableChecked();

    /**
      * \brief Check if Rise/Set/Transit details table is enabled.
      * \return True if Rise/Set/Transit details table is enabled.
      */
    bool isRSTTableChecked();

    /**
      * \brief Check if Asteroid/Comet details table is enabled.
      * \return True if Asteroid/Comet details table is enabled.
      */
    bool isAstComTableChecked();

    /**
      * \brief Check if logging form is enabled.
      * \return True if logging form is enabled.
      */
    bool isLoggingFormChecked();

private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZCHARTCONTENTS_H

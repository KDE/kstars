/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    explicit PWizChartContentsUI(PrintingWizard *wizard, QWidget *parent = nullptr);

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

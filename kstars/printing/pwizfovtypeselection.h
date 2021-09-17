/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PWIZFOVTYPESELECTION_H
#define PWIZFOVTYPESELECTION_H

#include "ui_pwizfovtypeselection.h"
#include "printingwizard.h"

/**
  * \class PWizFovTypeSelectionUI
  * \brief User interface for "Select FOV capture method" step of the Printing Wizard.
  * \author Rafał Kułaga
  */
class PWizFovTypeSelectionUI : public QFrame, public Ui::PWizFovTypeSelection
{
    Q_OBJECT
  public:
    /**
          * \brief Constructor.
          */
    explicit PWizFovTypeSelectionUI(PrintingWizard *wizard, QWidget *parent = nullptr);

    /**
          * \brief Get selected FOV export method.
          * \return Selected FOV export method.
          */
    PrintingWizard::FOV_TYPE getFovExportType();

  private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVTYPESELECTION_H

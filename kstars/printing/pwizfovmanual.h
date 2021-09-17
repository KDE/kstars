/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PWIZFOVMANUAL_H
#define PWIZFOVMANUAL_H

#include "ui_pwizfovmanual.h"

class PrintingWizard;

/**
  * \class PWizFovManualUI
  * \brief User interface for "Manual FOV capture" step of the Printing Wizard.
  * \author Rafał Kułaga
  */
class PWizFovManualUI : public QFrame, public Ui::PWizFovManual
{
    Q_OBJECT
  public:
    /**
          * \brief Constructor.
          */
    explicit PWizFovManualUI(PrintingWizard *wizard, QWidget *parent = nullptr);

  private slots:
    /**
          * \brief Slot: enter manual FOV capture mode.
          */
    void slotExportFov();

  private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVMANUAL_H

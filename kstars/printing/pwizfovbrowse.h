/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PWIZFOVBROWSER_H
#define PWIZFOVBROWSER_H

#include "ui_pwizfovbrowse.h"

class PrintingWizard;

/**
  * \class PWizFovBrowseUI
  * \brief User interface for "Browse captured FOV images" step of Printing Wizard.
  * \author Rafał Kułaga
  */
class PWizFovBrowseUI : public QFrame, public Ui::PWizFovBrowse
{
    Q_OBJECT
  public:
    /**
          * \brief Constructor.
          */
    explicit PWizFovBrowseUI(PrintingWizard *wizard, QWidget *parent = nullptr);

  private slots:
    /**
          * \brief Slot: open FOV editor window.
          */
    void slotOpenFovEditor();

  private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVBROWSER_H

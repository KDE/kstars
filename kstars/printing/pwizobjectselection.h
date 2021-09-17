/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PWIZOBJECTSELECTION_H
#define PWIZOBJECTSELECTION_H

#include "ui_pwizobjectselection.h"

class PrintingWizard;
class SkyObject;

/**
  * \class PWizObjectSelectionUI
  * \brief User interface for "Select observed object" step of the Printing Wizard.
  * \author Rafał Kułaga
  */
class PWizObjectSelectionUI : public QFrame, public Ui::PWizObjectSelection
{
    Q_OBJECT
  public:
    /**
          * \brief Constructor.
          */
    explicit PWizObjectSelectionUI(PrintingWizard *wizard, QWidget *parent = nullptr);

    /**
          * \brief Update UI elements for newly selected SkyObject.
          * \param obj Selected SkyObject.
          */
    void setSkyObject(SkyObject *obj);

    /**
          * \brief Static function: get QString with basic information about SkyObject.
          * \param obj Selected SkyObject.
          */
    static QString objectInfoString(SkyObject *obj);

  private slots:
    /**
          * \brief Slot: open "Find Object" dialog to select SkyObject.
          */
    void slotSelectFromList();

    /**
          * \brief Slot: enter object pointing mode to select SkyObject.
          */
    void slotPointObject();

    /**
          * \brief Slot: show "Details" window for selected object.
          */
    void slotShowDetails();

  private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZOBJECTSELECTION_H

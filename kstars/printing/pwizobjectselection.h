/***************************************************************************
                          pwizobjectselection.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 3 2011
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
    PWizObjectSelectionUI(PrintingWizard *wizard, QWidget *parent = 0);

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

/***************************************************************************
                          pwizfovsh.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Aug 15 2011
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

#ifndef PWIZFOVSH_H
#define PWIZFOVSH_H

#include "ui_pwizfovsh.h"

class PrintingWizard;
class SkyObject;

/**
  * \class PWizFovShUI
  * \brief User interface for "Star hopper FOV snapshot capture" step of the Printing Wizard.
  * \author Rafał Kułaga
  */
class PWizFovShUI : public QFrame, public Ui::PWizFovSh
{
    Q_OBJECT
  public:
    /**
          * \brief Constructor.
          */
    explicit PWizFovShUI(PrintingWizard *wizard, QWidget *parent = 0);

    /**
          * \brief Get magnitude limit set by user.
          * \return Magnitude limit set by user.
          */
    double getMaglim() { return maglimSpinBox->value(); }

    /**
          * \brief Get FOV name set by user.
          * \return FOV name set by user.
          */
    QString getFovName() { return fovCombo->currentText(); }

    /**
          * \brief Set object at which star hopper will begin.
          * \param obj Beginning object.
          */
    void setBeginObject(SkyObject *obj);

  private slots:
    /**
          * \brief Slot: select beginning object from list.
          */
    void slotSelectFromList();

    /**
          * \brief Slot: point beginning object on SkyMap.
          */
    void slotPointObject();

    /**
          * \brief Slot: open details window.
          */
    void slotDetails();

    /**
          * \brief Slot: begin capture.
          */
    void slotBeginCapture();

  private:
    /**
          * \brief Setup widgets.
          */
    void setupWidgets();

    /**
          * \brief Setup signal-slot connections.
          */
    void setupConnections();

    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVSH_H

/***************************************************************************
                          foveditordialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Fri Aug 12 2011
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

#ifndef FOVEDITORDIALOG_H
#define FOVEDITORDIALOG_H

#include "ui_foveditordialog.h"

#include <QDialog>

class PrintingWizard;

/**
  * \class FovEditorDialogUI
  * \brief User interface for FOV Editor Dialog.
  * \author Rafał Kułaga
  */
class FovEditorDialogUI : public QFrame, public Ui::FovEditorDialog
{
    Q_OBJECT
public:
    /**
      * \brief Constructor.
      */
    explicit FovEditorDialogUI(QWidget *parent = 0);
};

/**
  * \class FovEditorDialog
  * \brief Class representing FOV Editor Dialog which enables user to edit FOV snapshots.
  * \author Rafał Kułaga
  */
class FovEditorDialog : public QDialog
{
    Q_OBJECT
public:
    /**
      * \brief Constructor.
      */
    explicit FovEditorDialog(PrintingWizard *wizard, QWidget *parent = 0);

private slots:
    /**
      * \brief Slot: switch to next FOV snapshot.
      */
    void slotNextFov();

    /**
      * \brief Slot: switch to previous FOV snapshot.
      */
    void slotPreviousFov();

    /**
      * \brief Slot: recapture current FOV snapshot.
      */
    void slotCaptureAgain();

    /**
      * \brief Slot: delete current FOV snapshot.
      */
    void slotDelete();

    /**
      * \brief Slot: save description of the current FOV snapshot.
      */
    void slotSaveDescription();

    /**
      * \brief Slot: open "Save file" dialog to choose file name and format to save image.
      */
    void slotSaveImage();

private:
    /**
      * \brief Setup widget properties.
      */
    void setupWidgets();

    /**
      * \brief Setup signal-slot connections.
      */
    void setupConnections();

    /**
      * \brief Update buttons.
      */
    void updateButtons();

    /**
      * \brief Update image description.
      */
    void updateDescriptions();

    /**
      * \brief Update FOV image.
      */
    void updateFovImage();

    PrintingWizard *m_ParentWizard;
    FovEditorDialogUI *m_EditorUi;

    int m_CurrentIndex;
};

#endif // FOVEDITORDIALOG_H

/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_foveditordialog.h"

#include <QDialog>
#include <QFrame>

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
    explicit FovEditorDialogUI(QWidget *parent = nullptr);
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
    explicit FovEditorDialog(PrintingWizard *wizard, QWidget *parent = nullptr);

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

    PrintingWizard *m_ParentWizard { nullptr };
    FovEditorDialogUI *m_EditorUi { nullptr };

    int m_CurrentIndex { 0 };
};

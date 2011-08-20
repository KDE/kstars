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
#include "QList"

class PrintingWizard;

class FovEditorDialogUI : public QFrame, public Ui::FovEditorDialog
{
    Q_OBJECT
public:
    explicit FovEditorDialogUI(QWidget *parent = 0);
};

class FovEditorDialog : public KDialog
{
    Q_OBJECT
public:
    FovEditorDialog(PrintingWizard *wizard, QWidget *parent = 0);
    ~FovEditorDialog();

private slots:
    void slotNextFov();
    void slotPreviousFov();
    void slotCaptureAgain();
    void slotDelete();
    void slotSaveDescription();
    void slotSaveImage();

private:
    void setupWidgets();
    void setupConnections();

    void updateButtons();
    void updateDescriptions();
    void updateFovImage();

    PrintingWizard *m_ParentWizard;
    FovEditorDialogUI *m_EditorUi;

    int m_CurrentIndex;
};

#endif // FOVEDITORDIALOG_H

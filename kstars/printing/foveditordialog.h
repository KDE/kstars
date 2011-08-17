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

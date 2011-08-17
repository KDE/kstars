#ifndef PWIZFOVBROWSER_H
#define PWIZFOVBROWSER_H

#include "ui_pwizfovbrowse.h"

class PrintingWizard;

class PWizFovBrowseUI : public QFrame, Ui::PWizFovBrowse
{
    Q_OBJECT
public:
    PWizFovBrowseUI(PrintingWizard *wizard, QWidget *parent = 0);

private slots:
    void slotOpenFovEditor();

private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVBROWSER_H

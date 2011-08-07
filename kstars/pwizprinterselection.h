#ifndef PWIZPRINTERSELECTION_H
#define PWIZPRINTERSELECTION_H

#include "ui_pwizprinterselection.h"

class QPrinter;
class QPrintDialog;
class PrintingWizard;

// Printing Wizard printer selection screen
class PWizPrinterSelectionUI : public QFrame, public Ui::PWizPrinterSelection
{
    Q_OBJECT
public:
    PWizPrinterSelectionUI(PrintingWizard *wizard, QWidget *parent = 0);
    ~PWizPrinterSelectionUI();

private slots:
    void slotSelectPrinter();

private:
    void setupConnections();

    QPrinter *m_Printer;
    QPrintDialog *m_Dialog;
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZPRINTERSELECTION_H

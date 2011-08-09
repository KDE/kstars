#ifndef PWIZPRINT_H
#define PWIZPRINT_H

#include "ui_pwizprint.h"

class PrintingWizard;

class PWizPrintUI : public QFrame, public Ui::PWizPrint
{
    Q_OBJECT
public:
    PWizPrintUI(PrintingWizard *wizard, QWidget *parent = 0);

private slots:
    void slotPreview();
    void slotPrintPreview(QPrinter *printer);
    void slotPrint();

private:
    void printDocument(QPrinter *printer);

    PrintingWizard *m_ParentWizard;
};

#endif // PWIZPRINT_H

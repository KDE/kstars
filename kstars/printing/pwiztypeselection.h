#ifndef PWIZTYPESELECTION_H
#define PWIZTYPESELECTION_H

#include "ui_pwiztypeselection.h"
#include "printingwizard.h"

// Printing Wizard printout type selection screen
class PWizTypeSelectionUI : public QFrame, public Ui::PWizTypeSelection
{
    Q_OBJECT
public:
    PWizTypeSelectionUI(QWidget *parent = 0);

    PrintingWizard::PRINTOUT_TYPE getSelectedPrintoutType();
};


#endif // PWIZTYPESELECTION_H

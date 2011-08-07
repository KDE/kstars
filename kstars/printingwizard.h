#ifndef PRINTINGWIZARD_H
#define PRINTINGWIZARD_H

#include "ui_pwizwelcome.h"
#include "ui_pwiztypeselection.h"

#include "kdialog.h"
#include "QPrinter"

class KStars;
class PWizPrinterSelectionUI;
class QStackedWidget;

// Printing Wizard welcome screen
class PWizWelcomeUI : public QFrame, public Ui::PWizWelcome
{
    Q_OBJECT
public:
    PWizWelcomeUI(QWidget *parent = 0);
};

// Printing Wizard printout type selection screen
class PWizTypeSelectionUI : public QFrame, public Ui::PWizTypeSelection
{
    Q_OBJECT
public:
    PWizTypeSelectionUI(QWidget *parent = 0);
};

class PrintingWizard : public KDialog
{
    Q_OBJECT

public:
    PrintingWizard(QWidget *parent = 0);
    ~PrintingWizard();

    void setPrinter(QPrinter *printer) { m_Printer = printer; }

private slots:
    void slotPrevPage();
    void slotNextPage();

private:
    void setupWidgets();
    void setupConnections();
    void updateButtons();

    KStars *m_KStars;
    QStackedWidget *m_WizardStack;

    QPrinter *m_Printer;

    PWizWelcomeUI *m_WizWelcomeUI;
    PWizPrinterSelectionUI *m_WizPrinterSelectionUI;
    PWizTypeSelectionUI *m_WizTypeSelectionUI;
};

#endif // PRINTINGWIZARD_H

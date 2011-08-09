#ifndef PRINTINGWIZARD_H
#define PRINTINGWIZARD_H

#include "ui_pwizwelcome.h"

#include "kdialog.h"
#include "QPrinter"

class KStars;
class PWizPrinterSelectionUI;
class PWizTypeSelectionUI;
class PWizObjectSelectionUI;
class LoggingForm;
class FinderChart;
class SkyObject;
class QStackedWidget;

// Printing Wizard welcome screen
class PWizWelcomeUI : public QFrame, public Ui::PWizWelcome
{
    Q_OBJECT
public:
    PWizWelcomeUI(QWidget *parent = 0);
};

class PrintingWizard : public KDialog
{
    Q_OBJECT

public:
    enum PRINTOUT_TYPE
    {
        PT_FINDER_CHART,
        PT_LOGGING_FORM,
        PT_UNDEFINED
    };

    PrintingWizard(QWidget *parent = 0);
    ~PrintingWizard();

    PRINTOUT_TYPE getPrintoutType() { return m_PrintoutType; }
    QPrinter* getPrinter() { return m_Printer; }
    FinderChart* getFinderChart() { return m_FinderChart; }
    LoggingForm* getLoggingForm() { return m_LoggingForm; }
    SkyObject* getSkyObject() { return m_SkyObject; }

    void setPrinter(QPrinter *printer) { m_Printer = printer; }
    void setSkyObject(SkyObject *obj) { m_SkyObject = obj; }

    void restart();
    void pointingDone(SkyObject *obj);
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

    PRINTOUT_TYPE m_PrintoutType;
    FinderChart *m_FinderChart;
    LoggingForm *m_LoggingForm;

    SkyObject *m_SkyObject;

    PWizWelcomeUI *m_WizWelcomeUI;
    PWizPrinterSelectionUI *m_WizPrinterSelectionUI;
    PWizTypeSelectionUI *m_WizTypeSelectionUI;
    PWizObjectSelectionUI *m_WizObjectSelectionUI;
};

#endif // PRINTINGWIZARD_H

#include "pwizprinterselection.h"
#include "printingwizard.h"
#include "QPrinter"
#include "QPrintDialog"

PWizPrinterSelectionUI::PWizPrinterSelectionUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    setupUi(this);

    connect(SelectPrinterButton, SIGNAL(clicked()), this, SLOT(slotSelectPrinter()));

    m_Printer = new QPrinter(QPrinter::ScreenResolution);
    m_Dialog = new QPrintDialog(m_Printer, this);
    m_Dialog->setWindowTitle(i18n("Select Printer"));
}

PWizPrinterSelectionUI::~PWizPrinterSelectionUI()
{
    if(m_Printer)
    {
        delete m_Printer;
    }
}

void PWizPrinterSelectionUI::slotSelectPrinter()
{
    m_Dialog->exec();
    m_ParentWizard->setPrinter(m_Printer);
}

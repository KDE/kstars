#include "pwizprint.h"

#include "printingwizard.h"
#include "finderchart.h"
#include "loggingform.h"
#include "QPrintPreviewDialog"
#include "QPrinter"

PWizPrintUI::PWizPrintUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    connect(previewButton, SIGNAL(clicked()), this, SLOT(slotPreview()));
    connect(printButton, SIGNAL(clicked()), this, SLOT(slotPrint()));
}

void PWizPrintUI::slotPreview()
{
    QPrintPreviewDialog  previewDlg(m_ParentWizard->getPrinter(), this);
    connect(&previewDlg, SIGNAL(paintRequested(QPrinter*)), SLOT(printPreview(QPrinter*)));
    previewDlg.exec();
}

void PWizPrintUI::slotPrintPreview(QPrinter *printer)
{
    printDocument(printer);
}

void PWizPrintUI::slotPrint()
{
    printDocument(m_ParentWizard->getPrinter());
}

void PWizPrintUI::printDocument(QPrinter *printer)
{
    switch(m_ParentWizard->getPrintoutType())
    {
    case PrintingWizard::PT_FINDER_CHART:
        {
            m_ParentWizard->getFinderChart()->print(printer);
            break;
        }

    case PrintingWizard::PT_LOGGING_FORM:
        {
            m_ParentWizard->getLoggingForm()->print(printer);
            break;
        }

    default:
        {
            return;
        }
    }
}

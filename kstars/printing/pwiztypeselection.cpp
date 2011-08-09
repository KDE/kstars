#include "pwiztypeselection.h"

PWizTypeSelectionUI::PWizTypeSelectionUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

PrintingWizard::PRINTOUT_TYPE PWizTypeSelectionUI::getSelectedPrintoutType()
{
    if(finderChartRadio->isChecked())
    {
        return PrintingWizard::PT_FINDER_CHART;
    }

    else if(loggingFormRadio->isChecked())
    {
        return PrintingWizard::PT_LOGGING_FORM;
    }

    else
    {
        return PrintingWizard::PT_UNDEFINED;
    }

}



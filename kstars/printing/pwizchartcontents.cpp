#include "pwizchartcontents.h"

PWizChartContentsUI::PWizChartContentsUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    setupUi(this);
}

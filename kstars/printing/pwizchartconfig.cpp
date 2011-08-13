#include "pwizchartconfig.h"

PWizChartConfigUI::PWizChartConfigUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    setupUi(this);
}

QString PWizChartConfigUI::getChartTitle()
{
    return titleEdit->text();
}

QString PWizChartConfigUI::getChartSubtitle()
{
    return subtitleEdit->text();
}

QString PWizChartConfigUI::getChartDescription()
{
    return descriptionTextEdit->toPlainText();
}

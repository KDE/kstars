#ifndef PWIZCHARTCONFIG_H
#define PWIZCHARTCONFIG_H

#include "ui_pwizchartconfig.h"

class PrintingWizard;

class PWizChartConfigUI : public QFrame, public Ui::PWizChartConfig
{
    Q_OBJECT
public:
    PWizChartConfigUI(PrintingWizard *wizard, QWidget *parent = 0);

    QString getChartTitle();
    QString getChartSubtitle();
    QString getChartDescription();

    bool isGeoTimeChecked();
    bool isSymbolListChecked();

private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZCHARTCONFIG_H

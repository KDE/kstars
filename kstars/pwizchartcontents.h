#ifndef PWIZCHARTCONTENTS_H
#define PWIZCHARTCONTENTS_H

#include "ui_pwizchartcontents.h"

class PrintingWizard;

class PWizChartContentsUI : public QFrame, public Ui::PWizChartContents
{
    Q_OBJECT
public:
    PWizChartContentsUI(PrintingWizard *wizard, QWidget *parent = 0);

private:
    PrintingWizard *m_ParentWizard;
};

#endif // PWIZCHARTCONTENTS_H

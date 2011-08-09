#ifndef PWIZOBJECTSELECTION_H
#define PWIZOBJECTSELECTION_H

#include "ui_pwizobjectselection.h"

class PrintingWizard;
class SkyObject;

class PWizObjectSelectionUI : public QFrame, public Ui::PWizObjectSelection
{
    Q_OBJECT

public:
    PWizObjectSelectionUI(PrintingWizard *wizard, QWidget *parent = 0);
    void setSkyObject(SkyObject *obj);

private slots:
    void slotSelectFromList();
    void slotPointObject();
    void slotShowDetails();

private:
    QString objectInfoString(SkyObject *obj);

    PrintingWizard *m_ParentWizard;
};

#endif // PWIZOBJECTSELECTION_H

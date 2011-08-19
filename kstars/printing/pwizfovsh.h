#ifndef PWIZFOVSH_H
#define PWIZFOVSH_H

#include "ui_pwizfovsh.h"

class PrintingWizard;
class SkyObject;

class PWizFovShUI : public QFrame, public Ui::PWizFovSh
{
    Q_OBJECT
public:
    PWizFovShUI(PrintingWizard *wizard, QWidget *parent = 0);

    double getMaglim() { return maglimSpinBox->value(); }
    QString getFovName() { return fovCombo->currentText(); }
    void setBeginObject(SkyObject *obj);

private slots:
    void slotSelectFromList();
    void slotPointObject();
    void slotBeginCapture();

private:
    void setupWidgets();
    void setupConnections();

    PrintingWizard *m_ParentWizard;
};

#endif // PWIZFOVSH_H

#ifndef PWIZFOVCONFIG_H
#define PWIZFOVCONFIG_H

#include "ui_pwizfovconfig.h"

class PWizFovConfigUI : public QFrame, public Ui::PWizFovConfig
{
    Q_OBJECT
public:
    PWizFovConfigUI(QWidget *parent = 0);
    bool isSwitchColorsEnabled() { return switchColorsBox->isChecked(); }
    bool isFovShapeOverriden() { return overrideShapeBox->isChecked(); }

private:
    void setupWidgets();
    void setupConnections();
};

#endif // PWIZFOVCONFIG_H

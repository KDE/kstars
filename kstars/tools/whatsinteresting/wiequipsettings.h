
#ifndef WI_EQUIP_SETTINGS_H
#define WI_EQUIP_SETTINGS_H

#include "ui_wiequipsettings.h"

class KStars;

class WIEquipSettings : public QFrame, public Ui::WIEquipSettings
{
    Q_OBJECT
public:
    WIEquipSettings(KStars *ks);

private:
    KStars *m_Ks;
public slots:
    void slotTelescopeCheck(bool on);
    void slotBinocularsCheck(bool on);
    void slotNoEquipCheck(bool on);
    void slotSetTelType(int type);
};

#endif
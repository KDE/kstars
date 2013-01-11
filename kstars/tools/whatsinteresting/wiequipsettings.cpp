#include "wiequipsettings.h"
#include "kstars.h"
#include "Options.h"

WIEquipSettings::WIEquipSettings(KStars* ks): QFrame(ks), m_Ks(ks)
{
    setupUi(this);
    connect(kcfg_TelescopeCheck, SIGNAL(toggled(bool)), this, SLOT(slotTelescopeCheck(bool)));
    connect(kcfg_BinocularsCheck, SIGNAL(toggled(bool)), this, SLOT(slotBinocularsCheck(bool)));
    connect(kcfg_NoEquipCheck, SIGNAL(toggled(bool)), this, SLOT(slotNoEquipCheck(bool)));
    connect(telescopeType, SIGNAL(currentIndexChanged(int)), this, SLOT(slotSetTelType(int)));
}

void WIEquipSettings::slotTelescopeCheck(bool on)
{
    if (on)
    {
        kcfg_NoEquipCheck->setEnabled(false);
        telescopeType->setEnabled(true);
        telTypeText->setEnabled(true);
    }
    else
    {
        if (!kcfg_BinocularsCheck->isChecked())
            kcfg_NoEquipCheck->setEnabled(true);
        telescopeType->setEnabled(false);
        telTypeText->setEnabled(false);
    }
}

void WIEquipSettings::slotBinocularsCheck(bool on)
{
    if (on)
        kcfg_NoEquipCheck->setEnabled(false);
    else
    {
        if (!kcfg_TelescopeCheck->isChecked())
            kcfg_NoEquipCheck->setEnabled(true);
    }
}

void WIEquipSettings::slotNoEquipCheck(bool on)
{
    if (on)
    {
        kcfg_TelescopeCheck->setEnabled(false);
        kcfg_BinocularsCheck->setEnabled(false);
        kcfg_Aperture->setEnabled(false);
        apertureText->setEnabled(false);
        telescopeType->setEnabled(false);
        telTypeText->setEnabled(false);
    }
    else
    {
        kcfg_TelescopeCheck->setEnabled(true);
        kcfg_BinocularsCheck->setEnabled(true);
        kcfg_Aperture->setEnabled(true);
        apertureText->setEnabled(true);
        telescopeType->setEnabled(true);
        telTypeText->setEnabled(true);
    }
}

void WIEquipSettings::slotSetTelType(int type)
{
    Options::setTelescopeType(type);
}

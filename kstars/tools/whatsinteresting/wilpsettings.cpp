#include "wilpsettings.h"
#include "kstars.h"

WILPSettings::WILPSettings(KStars* ks): QFrame(ks), m_Ks(ks)
{
    setupUi(this);
    connect(kcfg_BortleClass, SIGNAL(valueChanged(int)), this, SLOT(bortleValueChanged(int)));
}

WILPSettings::~WILPSettings()
{}

void WILPSettings::bortleValueChanged(int value)
{
    kcfg_BortleClass->setValue(value);
}

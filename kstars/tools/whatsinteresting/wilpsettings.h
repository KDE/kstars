
#ifndef WI_LP_SETTINGS_H
#define WI_LP_SETTINGS_H

#include "ui_wilpsettings.h"

class KStars;

class WILPSettings : public QFrame, public Ui::WILPSettings
{
    Q_OBJECT
public:
    WILPSettings(KStars *ks);

private:
    KStars *m_Ks;

private slots:
    void bortleValueChanged(int value);
};

#endif
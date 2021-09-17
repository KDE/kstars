/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pwizobjectselection.h"
#include "printingwizard.h"
#include "kstars.h"
#include "skymap.h"
#include "dialogs/finddialog.h"
#include "dialogs/detaildialog.h"
#include "starobject.h"
#include "ksplanetbase.h"
#include "catalogobject.h"
#include <QPointer>
#include <KLocalizedString>

PWizObjectSelectionUI::PWizObjectSelectionUI(PrintingWizard *wizard, QWidget *parent)
    : QFrame(parent), m_ParentWizard(wizard)
{
    setupUi(this);

    detailsButton->setVisible(false);
    selectedObjLabel->setVisible(false);
    objInfoLabel->setVisible(false);

    connect(fromListButton, SIGNAL(clicked()), this, SLOT(slotSelectFromList()));
    connect(pointButton, SIGNAL(clicked()), this, SLOT(slotPointObject()));
    connect(detailsButton, SIGNAL(clicked()), this, SLOT(slotShowDetails()));
}

void PWizObjectSelectionUI::setSkyObject(SkyObject *obj)
{
    m_ParentWizard->setSkyObject(obj);
    m_ParentWizard->updateStepButtons();

    QString infoStr = objectInfoString(obj);

    objInfoLabel->setText(infoStr);

    detailsButton->setVisible(true);
    selectedObjLabel->setVisible(true);
    objInfoLabel->setVisible(true);
}

void PWizObjectSelectionUI::slotSelectFromList()
{
    if (FindDialog::Instance()->exec() == QDialog::Accepted)
    {
        SkyObject *obj = FindDialog::Instance()->targetObject();
        if (obj)
        {
            setSkyObject(obj);
            m_ParentWizard->updateStepButtons();
        }
    }
}

void PWizObjectSelectionUI::slotPointObject()
{
    m_ParentWizard->beginPointing();
}

void PWizObjectSelectionUI::slotShowDetails()
{
    if (m_ParentWizard->getSkyObject())
    {
        QPointer<DetailDialog> detailDlg(new DetailDialog(
                                             m_ParentWizard->getSkyObject(), KStars::Instance()->data()->ut(), KStars::Instance()->data()->geo(), this));
        detailDlg->exec();
        delete detailDlg;
    }
}

QString PWizObjectSelectionUI::objectInfoString(SkyObject *obj)
{
    QString retVal;

    switch (obj->type())
    {
        case SkyObject::STAR:
        {
            StarObject *s = (StarObject *)obj;

            retVal = s->longname();

            if (s->getHDIndex() != 0)
            {
                if (!s->longname().isEmpty())
                {
                    retVal += QString(" ● HD%1").arg(QString::number(s->getHDIndex()));
                }

                else
                {
                    retVal += QString(" ● HD%1").arg(QString::number(s->getHDIndex()));
                }
            }

            retVal += " ● " + s->sptype() + ' ' + i18n("star");
            retVal += " ● " + i18nc("number in magnitudes", "%1 mag", QLocale().toString(s->mag(), 1));

            break;
        }

        case SkyObject::ASTEROID: //[fall through to planets]
        case SkyObject::COMET:    //[fall through to planets]
        case SkyObject::MOON:     //[fall through to planets]

        case SkyObject::PLANET:
        {
            KSPlanetBase *ps = (KSPlanetBase *)obj;

            retVal = ps->longname();

            //Type is "G5 star" for Sun
            QString type;
            if (ps->name() == i18n("Sun"))
            {
                type = i18n("G5 star");
            }

            else if (ps->name() == i18n("Moon"))
            {
                type = ps->translatedName();
            }

            else if (ps->name() == i18nc("Asteroid name (optional)", "Pluto") || ps->name() == i18nc("Asteroid name (optional)", "Ceres") || ps->name() == i18nc("Asteroid name (optional)", "Eris"))
            {
                type = i18n("Dwarf planet");
            }
            else
            {
                type = ps->typeName();
            }

            retVal += " ● " + type;
            retVal += " ● " + i18nc("number in magnitudes", "%1 mag", QLocale().toString(ps->mag(), 1));

            break;
        }

        default: // deep-sky object
        {
            auto *dso = (CatalogObject *)obj;

            QString oname, pname;
            //Show all names recorded for the object
            if (!dso->longname().isEmpty() && dso->longname() != dso->name())
            {
                pname = dso->translatedLongName();
                oname = dso->translatedName();
            }

            else
            {
                pname = dso->translatedName();
            }

            if (!dso->translatedName2().isEmpty())
            {
                if (oname.isEmpty())
                {
                    oname = dso->translatedName2();
                }

                else
                {
                    oname += " ● " + dso->translatedName2();
                }
            }

            oname += " ● " + dso->catalogIdentifier();

            if (!oname.isEmpty())
                pname += " ● " + oname;

            retVal = pname;
            retVal += " ● " + dso->typeName();

            break;
        }
    }

    return retVal;
}

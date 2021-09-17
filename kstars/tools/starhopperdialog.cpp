/*
    SPDX-FileCopyrightText: 2014 Utkarsh Simha <utkarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "starhopperdialog.h"

#include "kstars.h"
#include "ksutils.h"
#include "skymap.h"
#include "ksnotification.h"
#include "skymapcomposite.h"
#include "starhopper.h"
#include "targetlistcomponent.h"
#include "dialogs/detaildialog.h"

StarHopperDialog::StarHopperDialog(QWidget *parent) : QDialog(parent), ui(new Ui::StarHopperDialog)
{
    ui->setupUi(this);
    m_lw       = ui->listWidget;
    m_Metadata = new QStringList();
    ui->directionsLabel->setWordWrap(true);
    m_sh.reset(new StarHopper());
    connect(ui->NextButton, SIGNAL(clicked()), this, SLOT(slotNext()));
    connect(ui->GotoButton, SIGNAL(clicked()), this, SLOT(slotGoto()));
    connect(ui->DetailsButton, SIGNAL(clicked()), this, SLOT(slotDetails()));
    connect(m_lw, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(slotGoto()));
    connect(m_lw, SIGNAL(itemSelectionChanged()), this, SLOT(slotRefreshMetadata()));
    connect(this, SIGNAL(finished(int)), this, SLOT(deleteLater()));
}

StarHopperDialog::~StarHopperDialog()
{
    TargetListComponent *t = getTargetListComponent();

    if (t->list)
        t->list->clear();
    SkyMap::Instance()->forceUpdate(true);
    delete ui;
}

void StarHopperDialog::starHop(const SkyPoint &startHop, const SkyPoint &stopHop, float fov, float maglim)
{
    QList<StarObject *> *starList = m_sh->computePath(startHop, stopHop, fov, maglim, m_Metadata);

    if (!starList->empty())
    {
        foreach (StarObject *so, *starList)
        {
            setData(so);
        }
        slotRefreshMetadata();
        m_skyObjList = KSUtils::castStarObjListToSkyObjList(starList);
        starList->clear();
        delete starList;
        TargetListComponent *t = getTargetListComponent();
        t->list.reset(m_skyObjList);
        SkyMap::Instance()->forceUpdate(true);
    }
    else
    {
        delete starList;
        KSNotification::error(i18n("Star-hopper algorithm failed. If you're trying a large star hop, try using a "
                                   "smaller FOV or changing the source point"));
    }
}

void StarHopperDialog::setData(StarObject *sobj)
{
    QListWidgetItem *item = new QListWidgetItem();
    QString starName;

    if (sobj->name() != "star")
    {
        starName = sobj->translatedLongName();
    }
    else if (sobj->getHDIndex())
    {
        starName = QString("HD%1").arg(QString::number(sobj->getHDIndex()));
    }
    else
    {
        starName = "";
        starName += sobj->spchar();
        starName += QString(" Star of mag %2").arg(QString::number(sobj->mag()));
    }
    item->setText(starName);
    QVariant qv;

    qv.setValue(sobj);
    item->setData(Qt::UserRole, qv);
    m_lw->addItem(item);
}

void StarHopperDialog::slotNext()
{
    m_lw->setCurrentRow(m_lw->currentRow() + 1);
    slotGoto();
}

void StarHopperDialog::slotGoto()
{
    slotRefreshMetadata();
    SkyObject *skyobj = getStarData(m_lw->currentItem());

    if (skyobj != nullptr)
    {
        KStars *ks = KStars::Instance();
        ks->map()->setClickedObject(skyobj);
        ks->map()->setClickedPoint(skyobj);
        ks->map()->slotCenter();
    }
}

void StarHopperDialog::slotDetails()
{
    SkyObject *skyobj = getStarData(m_lw->currentItem());

    if (skyobj != nullptr)
    {
        DetailDialog *detailDialog =
            new DetailDialog(skyobj, KStarsData::Instance()->ut(), KStarsData::Instance()->geo(), KStars::Instance());
        detailDialog->exec();
        delete detailDialog;
    }
}

SkyObject *StarHopperDialog::getStarData(QListWidgetItem *item)
{
    if (!item)
        return nullptr;
    else
    {
        QVariant v          = item->data(Qt::UserRole);
        StarObject *starobj = v.value<StarObject *>();
        return starobj;
    }
}

inline TargetListComponent *StarHopperDialog::getTargetListComponent()
{
    return KStarsData::Instance()->skyComposite()->getStarHopRouteList();
}

void StarHopperDialog::slotRefreshMetadata()
{
    int row = m_lw->currentRow();

    qDebug() << "Slot RefreshMetadata";
    if (row >= 0)
    {
        ui->directionsLabel->setText(m_Metadata->at(row));
    }
    else
    {
        ui->directionsLabel->setText(m_Metadata->at(0));
    }
    qDebug() << "Slot RefreshMetadata";
}

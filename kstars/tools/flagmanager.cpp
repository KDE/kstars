/*
    SPDX-FileCopyrightText: 2009 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "flagmanager.h"

#include "config-kstars.h"

#include "kspaths.h"
#include "ksnotification.h"
#include "kstars.h"
#include "kstars_debug.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"
#include "skycomponents/flagcomponent.h"
#include "skycomponents/skymapcomposite.h"

#ifdef HAVE_INDI
#include <basedevice.h>
#include "indi/indilistener.h"
#include "indi/indistd.h"
#include "indi/driverinfo.h"
#endif

#include <KMessageBox>

#include <QStandardItemModel>
#include <QSortFilterProxyModel>

FlagManagerUI::FlagManagerUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

FlagManager::FlagManager(QWidget *ks) : QDialog(ks)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    QList<QStandardItem *> itemList;
    QList<QImage> imageList;
    QStringList flagNames;
    int i;

    ui = new FlagManagerUI(this);

    setWindowTitle(i18nc("@title:window", "Flag Manager"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    m_Ks = KStars::Instance();

    ui->hintLabel->setText(i18n("To add custom icons, just add images in %1. File names must begin with flag. "
                                "For example, the file <i>flagSmall_red_cross.png</i> will be shown as <b>Small red "
                                "cross</b> in the combo box.",
                                KSPaths::writableLocation(QStandardPaths::AppDataLocation)));
    //Set up the Table Views
    m_Model = new QStandardItemModel(0, 5, this);
    m_Model->setHorizontalHeaderLabels(QStringList() << i18nc("Right Ascension", "RA") << i18nc("Declination", "Dec")
                                       << i18n("Epoch") << i18n("Icon") << i18n("Label"));
    m_SortModel = new QSortFilterProxyModel(this);
    m_SortModel->setSourceModel(m_Model);
    m_SortModel->setDynamicSortFilter(true);
    ui->flagList->setModel(m_SortModel);
    ui->flagList->horizontalHeader()->setStretchLastSection(true);

    ui->flagList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    ui->saveButton->setEnabled(false);

    //Fill the list
    imageList = m_Ks->data()->skyComposite()->flags()->imageList();
    flagNames = m_Ks->data()->skyComposite()->flags()->getNames();

    FlagComponent *flags = m_Ks->data()->skyComposite()->flags();
    QPixmap pixmap;

    for (i = 0; i < m_Ks->data()->skyComposite()->flags()->size(); ++i)
    {
        QStandardItem *labelItem = new QStandardItem(flags->label(i));
        labelItem->setForeground(QBrush(flags->labelColor(i)));

        itemList << new QStandardItem(flags->pointList().at(i)->ra0().toHMSString())
                 << new QStandardItem(flags->pointList().at(i)->dec0().toDMSString())
                 << new QStandardItem(flags->epoch(i))
                 << new QStandardItem(QIcon(pixmap.fromImage(flags->image(i))), flags->imageName(i)) << labelItem;
        m_Model->appendRow(itemList);
        itemList.clear();
    }

    //Fill the combobox
    for (i = 0; i < imageList.size(); ++i)
    {
        ui->flagCombobox->addItem(QIcon(pixmap.fromImage(flags->imageList(i))), flagNames.at(i), flagNames.at(i));
    }

    //Connect buttons
    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(slotAddFlag()));
    connect(ui->delButton, SIGNAL(clicked()), this, SLOT(slotDeleteFlag()));
    connect(ui->CenterButton, SIGNAL(clicked()), this, SLOT(slotCenterFlag()));
    connect(ui->ScopeButton, SIGNAL(clicked()), this, SLOT(slotCenterTelescope()));
    connect(ui->flagList, SIGNAL(clicked(QModelIndex)), this, SLOT(slotSetShownFlag(QModelIndex)));
    connect(ui->flagList, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(slotCenterFlag()));

    connect(ui->saveButton, SIGNAL(clicked()), this, SLOT(slotSaveChanges()));
}

void FlagManager::setRaDec(const dms &ra, const dms &dec)
{
    ui->raBox->show(ra, false);
    ui->decBox->show(dec, true);
}

void FlagManager::clearFields()
{
    ui->raBox->clear();
    ui->decBox->clear();

    ui->epochBox->setText("2000.0");
    ui->flagLabel->clear();
    ui->flagLabel->setFocus();

    //disable "Save Changes" button
    ui->saveButton->setEnabled(false);

    //unselect item from flagList
    ui->flagList->clearSelection();
}

void FlagManager::showFlag(int flagIdx)
{
    if (flagIdx < 0 || flagIdx >= m_Model->rowCount())
    {
        return;
    }

    else
    {
        ui->raBox->setText(m_Model->data(m_Model->index(flagIdx, 0)).toString());
        ui->decBox->setText(m_Model->data(m_Model->index(flagIdx, 1)).toString());
        ui->epochBox->setText(m_Model->data(m_Model->index(flagIdx, 2)).toString());

        //ui->flagCombobox->setCurrentItem( m_Model->data( m_Model->index( flagIdx, 3) ).toString() );
        ui->flagCombobox->setCurrentText(m_Model->data(m_Model->index(flagIdx, 3)).toString());
        ui->flagLabel->setText(m_Model->data(m_Model->index(flagIdx, 4)).toString());

        QColor labelColor = m_Model->item(flagIdx, 4)->foreground().color();
        ui->labelColorcombo->setColor(labelColor);
    }

    ui->flagList->selectRow(flagIdx);
    ui->saveButton->setEnabled(true);
}

bool FlagManager::validatePoint()
{
    bool raOk(false), decOk(false);
    dms ra(ui->raBox->createDms(false, &raOk)); //false means expressed in hours
    dms dec(ui->decBox->createDms(true, &decOk));

    QString message;

    //check if ra & dec values were successfully converted
    if (!raOk || !decOk)
    {
        KSNotification::error(i18n("Invalid coordinates."));
        return false;
    }

    //make sure values are in valid range
    if (ra.Hours() < 0.0 || ra.Degrees() > 360.0)
        message = i18n("The Right Ascension value must be between 0.0 and 24.0.");
    if (dec.Degrees() < -90.0 || dec.Degrees() > 90.0)
        message += '\n' + i18n("The Declination value must be between -90.0 and 90.0.");
    if (!message.isEmpty())
    {
        KSNotification::sorry(message, i18n("Invalid Coordinate Data"));
        return false;
    }

    //all checks passed
    return true;
}

void FlagManager::deleteFlagItem(int flagIdx)
{
    if (flagIdx < m_Model->rowCount())
    {
        m_Model->removeRow(flagIdx);
    }
}

void FlagManager::slotAddFlag()
{
    if (validatePoint() == false)
        return;

    dms ra(ui->raBox->createDms(false)); //false means expressed in hours
    dms dec(ui->decBox->createDms(true));

    insertFlag(true);

    FlagComponent *flags = m_Ks->data()->skyComposite()->flags();
    //Add flag in FlagComponent
    SkyPoint flagPoint(ra, dec);
    flags->add(flagPoint, ui->epochBox->text(), ui->flagCombobox->currentText(), ui->flagLabel->text(),
               ui->labelColorcombo->color());

    ui->flagList->selectRow(m_Model->rowCount() - 1);
    ui->saveButton->setEnabled(true);

    flags->saveToFile();

    //Redraw map
    m_Ks->map()->forceUpdate(false);
}

void FlagManager::slotDeleteFlag()
{
    int flag = ui->flagList->currentIndex().row();

    //Remove from FlagComponent
    m_Ks->data()->skyComposite()->flags()->remove(flag);

    //Remove from list
    m_Model->removeRow(flag);

    //Clear form fields
    clearFields();

    //Remove from file
    m_Ks->data()->skyComposite()->flags()->saveToFile();

    //Redraw map
    m_Ks->map()->forceUpdate(false);
}

void FlagManager::slotCenterFlag()
{
    if (ui->flagList->currentIndex().isValid())
    {
        m_Ks->map()->setClickedObject(nullptr);
        m_Ks->map()->setClickedPoint(
            m_Ks->data()->skyComposite()->flags()->pointList().at(ui->flagList->currentIndex().row()).get());
        m_Ks->map()->slotCenter();
    }
}

void FlagManager::slotCenterTelescope()
{
#ifdef HAVE_INDI

    if (INDIListener::Instance()->size() == 0)
    {
        KSNotification::sorry(i18n("KStars did not find any active telescopes."));
        return;
    }

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *bd = gd->getBaseDevice();

        if (gd->getType() != KSTARS_TELESCOPE)
            continue;

        if (bd == nullptr)
            continue;

        if (bd->isConnected() == false)
        {
            KSNotification::error(
                i18n("Telescope %1 is offline. Please connect and retry again.", gd->getDeviceName()));
            return;
        }

        ISD::GDSetCommand SlewCMD(INDI_SWITCH, "ON_COORD_SET", "TRACK", ISS_ON, this);

        gd->setProperty(&SlewCMD);
        gd->runCommand(INDI_SEND_COORDS,
                       m_Ks->data()->skyComposite()->flags()->pointList().at(ui->flagList->currentIndex().row()).get());

        return;
    }

    KSNotification::sorry(i18n("KStars did not find any active telescopes."));

#endif
}

void FlagManager::slotSaveChanges()
{
    int row = ui->flagList->currentIndex().row();

    if (validatePoint() == false)
        return;

    insertFlag(false, row);

    m_Ks->map()->forceUpdate();

    dms ra(ui->raBox->createDms(false)); //false means expressed in hours
    dms dec(ui->decBox->createDms(true));

    SkyPoint flagPoint(ra, dec);

    //Update FlagComponent
    m_Ks->data()->skyComposite()->flags()->updateFlag(row, flagPoint, ui->epochBox->text(),
            ui->flagCombobox->currentText(), ui->flagLabel->text(),
            ui->labelColorcombo->color());

    //Save changes to file
    m_Ks->data()->skyComposite()->flags()->saveToFile();

    ui->flagList->selectRow(row);
}

void FlagManager::slotSetShownFlag(QModelIndex idx)
{
    showFlag(idx.row());
}

void FlagManager::insertFlag(bool isNew, int row)
{
    dms ra(ui->raBox->createDms(false)); //false means expressed in hours
    dms dec(ui->decBox->createDms(true));
    SkyPoint flagPoint(ra, dec);

    // Add flag in the list
    QList<QStandardItem *> itemList;

    QStandardItem *labelItem = new QStandardItem(ui->flagLabel->text());
    labelItem->setForeground(QBrush(ui->labelColorcombo->color()));

    FlagComponent *flags = m_Ks->data()->skyComposite()->flags();

    QPixmap pixmap;
    itemList << new QStandardItem(flagPoint.ra0().toHMSString()) << new QStandardItem(flagPoint.dec0().toDMSString())
             << new QStandardItem(ui->epochBox->text())
             << new QStandardItem(QIcon(pixmap.fromImage(flags->imageList(ui->flagCombobox->currentIndex()))),
                                  ui->flagCombobox->currentText())
             << labelItem;

    if (isNew)
    {
        m_Model->appendRow(itemList);
    }

    else
    {
        for (int i = 0; i < m_Model->columnCount(); i++)
        {
            m_Model->setItem(row, i, itemList.at(i));
        }
    }
}

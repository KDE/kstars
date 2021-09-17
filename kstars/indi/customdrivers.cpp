/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "customdrivers.h"

#include "Options.h"

#include "kstars.h"
#include "driverinfo.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "ksnotification.h"


CustomDrivers::CustomDrivers(QWidget *parent, const QList<DriverInfo *> &driversList) : QDialog(parent), m_DriversList(driversList)
{
    setupUi(this);

    userdb = QSqlDatabase::cloneDatabase(KStarsData::Instance()->userdb()->GetDatabase(), "custom_drivers_db");
    userdb.open();
    model = new QSqlTableModel(this, userdb);
    model->setTable("customdrivers");

    driversView->setModel(model);
    driversView->hideColumn(0);

    refreshFromDB();

    tipLabel->setPixmap((QIcon::fromTheme("help-hint").pixmap(32, 32)));
    cautionLabel->setPixmap((QIcon::fromTheme("emblem-warning").pixmap(32, 32)));

    familyCombo->addItems(DeviceFamilyLabels.values());

    for (const DriverInfo *oneDriver : driversList)
    {
        // MDPD drivers CANNOT have aliases.
        if (oneDriver->getAuxInfo().value("mdpd", false).toBool() == true)
            continue;

        QString label = oneDriver->getLabel();
        if (label.isEmpty())
            continue;

        driverCombo->addItem(label);
    }

    syncDriver();

    connect(driverCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            this, &CustomDrivers::syncDriver);

    connect(labelIN, &QLineEdit::textChanged, [&]() {
        addDriverB->setEnabled(labelIN->text().isEmpty() == false);
    });

    connect(driversView, &QTableView::pressed, [&]() {
        removeDriverB->setEnabled(true);
    });

    connect(addDriverB, &QPushButton::clicked, this, &CustomDrivers::addDriver);
    connect(removeDriverB, &QPushButton::clicked, this, &CustomDrivers::removeDriver);
}

CustomDrivers::~CustomDrivers()
{
    userdb.close();
}

void CustomDrivers::refreshFromDB()
{
    KStarsData::Instance()->userdb()->GetAllCustomDrivers(m_CustomDrivers);
    model->select();
}

void CustomDrivers::syncDriver()
{
    const QString currentDriverLabel = driverCombo->currentText();

    for (const DriverInfo *oneDriver : m_DriversList)
    {
        if (currentDriverLabel == oneDriver->getLabel())
        {
            familyCombo->setCurrentIndex(oneDriver->getType());
            execIN->setText(oneDriver->getExecutable());
            nameIN->setText(oneDriver->getName());
            manufacturerIN->setText(oneDriver->manufacturer());
            break;
        }
    }
}

void CustomDrivers::addDriver()
{
    // Make sure label is unique in canonical drivers
    for (const DriverInfo *oneDriver : m_DriversList)
    {
        if (labelIN->text() == oneDriver->getLabel())
        {
            KSNotification::error(i18n("Label already exists. Label must be unique."));
            return;
        }
    }

    QVariantMap newDriver;

    newDriver["Label"] = labelIN->text();
    newDriver["Name"] = nameIN->text();
    newDriver["Family"] = familyCombo->currentText();
    newDriver["Manufacturer"] = manufacturerIN->text();
    newDriver["Exec"] = execIN->text();
    // TODO Try to make this editable as well
    newDriver["Version"] = "1.0";

    if (KStarsData::Instance()->userdb()->AddCustomDriver(newDriver) == false)
        KSNotification::error(i18n("Failed to add new driver. Is the label unique?"));

    refreshFromDB();

}

void CustomDrivers::removeDriver()
{
    int row = driversView->currentIndex().row();

    if (row < 0)
        return;

    QVariantMap oneDriver = m_CustomDrivers[row];

    KStarsData::Instance()->userdb()->DeleteCustomDriver(oneDriver["id"].toString());

    refreshFromDB();
}

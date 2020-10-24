/*  Ekos Options
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include "opsekos.h"

#include "manager.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "auxiliary/ksnotification.h"
#include "ekos/auxiliary/darklibrary.h"

#include <KConfigDialog>

#include <QSqlRecord>
#include <QSqlTableModel>

OpsEkos::OpsEkos() : QTabWidget(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("settings");

    // Our refresh lambda
    connect(this, &QTabWidget::currentChanged, this, [this](int index)
    {
        if (index == 4)
            refreshDarkData();
    });
    connect(darkTableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(loadDarkFITS(QModelIndex)));

    connect(openDarksFolderB, SIGNAL(clicked()), this, SLOT(openDarksFolder()));
    connect(clearAllB, SIGNAL(clicked()), this, SLOT(clearAll()));
    connect(clearRowB, SIGNAL(clicked()), this, SLOT(clearRow()));
    connect(clearExpiredB, SIGNAL(clicked()), this, SLOT(clearExpired()));
    connect(refreshB, SIGNAL(clicked()), this, SLOT(refreshDarkData()));

    connect(clearDSLRInfoB, &QPushButton::clicked, [ = ] ()
    {
        KStarsData::Instance()->userdb()->DeleteAllDSLRInfo();
    });

    refreshDarkData();

    connect(kcfg_EkosTopIcons, &QRadioButton::toggled, this, [this]()
    {
        if (Options::ekosTopIcons() != kcfg_EkosTopIcons->isChecked())
            KSNotification::info(i18n("You must restart KStars for this change to take effect."));
    });
}

void OpsEkos::clearExpired()
{
    if (darkFramesModel->rowCount() == 0)
        return;

    // Anything before this must go
    QDateTime expiredDate = QDateTime::currentDateTime().addDays(kcfg_DarkLibraryDuration->value() * -1);

    QString darkFilesPath = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "darks";

    QSqlDatabase userdb = QSqlDatabase::database("userdb");
    userdb.open();
    QSqlTableModel darkframe(nullptr, userdb);
    darkframe.setEditStrategy(QSqlTableModel::OnManualSubmit);
    darkframe.setTable("darkframe");
    // Select all those that already expired.
    darkframe.setFilter("timestamp < \'" + expiredDate.toString(Qt::ISODate) + "\'");

    darkframe.select();

    // Now remove all the expired files from disk
    for (int i = 0; i < darkframe.rowCount(); ++i)
    {
        QString oneFile = darkframe.record(i).value("filename").toString();
        QFile::remove(darkFilesPath + QDir::separator() + oneFile);
    }

    // And remove them from the database
    darkframe.removeRows(0, darkframe.rowCount());
    darkframe.submitAll();
    userdb.close();

    Ekos::DarkLibrary::Instance()->refreshFromDB();

    refreshDarkData();
}

void OpsEkos::clearAll()
{
    if (darkFramesModel->rowCount() == 0)
        return;

    if (KMessageBox::questionYesNo(KStars::Instance(),
                                   i18n("Are you sure you want to delete all dark frames images and data?")) ==
            KMessageBox::No)
        return;

    QString darkFilesPath = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "darks";

    QDir darkDir(darkFilesPath);
    darkDir.removeRecursively();
    darkDir.mkdir(darkFilesPath);

    QSqlDatabase userdb = QSqlDatabase::database("userdb");
    userdb.open();
    darkFramesModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    darkFramesModel->removeRows(0, darkFramesModel->rowCount());
    darkFramesModel->submitAll();
    userdb.close();

    Ekos::DarkLibrary::Instance()->refreshFromDB();

    refreshDarkData();
}

void OpsEkos::clearRow()
{
    QSqlDatabase userdb = QSqlDatabase::database("userdb");
    userdb.open();
    darkFramesModel->removeRow(darkTableView->currentIndex().row());
    darkFramesModel->submitAll();
    userdb.close();

    Ekos::DarkLibrary::Instance()->refreshFromDB();

    refreshDarkData();
}

void OpsEkos::openDarksFolder()
{
    QString darkFilesPath = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "darks";

    QDesktopServices::openUrl(QUrl::fromLocalFile(darkFilesPath));
}

void OpsEkos::refreshDarkData()
{
    QSqlDatabase userdb = QSqlDatabase::database("userdb");
    userdb.open();

    delete (darkFramesModel);
    delete (sortFilter);

    darkFramesModel = new QSqlTableModel(this, userdb);
    darkFramesModel->setTable("darkframe");
    darkFramesModel->select();

    sortFilter = new QSortFilterProxyModel(this);
    sortFilter->setSourceModel(darkFramesModel);
    sortFilter->sort (0);
    darkTableView->setModel (sortFilter);

    //darkTableView->setModel(darkFramesModel);
    // Hide ID
    darkTableView->hideColumn(0);
    // Hide Chip
    darkTableView->hideColumn(2);

    userdb.close();
}

void OpsEkos::loadDarkFITS(QModelIndex index)
{
    QSqlRecord record = darkFramesModel->record(index.row());

    QString filename = record.value("filename").toString();

    if (filename.isEmpty() == false)
    {
        QUrl url = QUrl::fromLocalFile(filename);
        KStars::Instance()->genericFITSViewer()->loadFile(url);
        KStars::Instance()->genericFITSViewer()->show();
    }
}

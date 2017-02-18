/*  Ekos Options
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QStringList>
#include <QComboBox>

#include <KConfigDialog>
#include <QSqlTableModel>
#include <QSqlDatabase>
#include <QDesktopServices>

#include "opsekos.h"
#include "Options.h"
#include "kstarsdata.h"
#include "ekosmanager.h"
#include "guide/guide.h"
#include "ksuserdb.h"
#include "ekos/auxiliary/darklibrary.h"
#include "kspaths.h"
#include "fov.h"

OpsEkos::OpsEkos()
        : QTabWidget( KStars::Instance() )
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "settings" );

#ifdef Q_OS_OSX
connect(kcfg_astrometrySolverIsInternal, SIGNAL(clicked()), this, SLOT(toggleSolverInternal()));
kcfg_astrometrySolverIsInternal->setToolTip(i18n("Internal or External Plate Solver?"));
if(Options::astrometrySolverIsInternal())
    kcfg_astrometrySolver->setEnabled(false);

connect(kcfg_astrometryConfFileIsInternal, SIGNAL(clicked()), this, SLOT(toggleConfigInternal()));
kcfg_astrometryConfFileIsInternal->setToolTip(i18n("Internal or External astrometry.cfg?"));
if(Options::astrometryConfFileIsInternal())
    kcfg_astrometryConfFile->setEnabled(false);

connect(kcfg_wcsIsInternal, SIGNAL(clicked()), this, SLOT(toggleWCSInternal()));
kcfg_wcsIsInternal->setToolTip(i18n("Internal or External wcsinfo?"));
if(Options::wcsIsInternal())
    kcfg_astrometryWCSInfo->setEnabled(false);
#else
kcfg_astrometrySolverIsInternal->setVisible(false);
kcfg_astrometryConfFileIsInternal->setVisible(false);
kcfg_wcsIsInternal->setVisible(false);
#endif

    connect( m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL( clicked() ), SLOT( slotApply() ) );

    // Our refresh lambda
    connect(this, &QTabWidget::currentChanged, this, [this](int index) { if (index == 4) refreshDarkData();});
    connect(darkTableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(loadDarkFITS(QModelIndex)));

    connect(openDarksFolderB, SIGNAL(clicked()), this, SLOT(openDarksFolder()));
    connect(clearAllB, SIGNAL(clicked()), this, SLOT(clearAll()));
    connect(clearRowB, SIGNAL(clicked()), this, SLOT(clearRow()));
    connect(refreshB, SIGNAL(clicked()), this, SLOT(refreshDarkData()));

    refreshDarkData();
}

OpsEkos::~OpsEkos() {}

void OpsEkos::toggleSolverInternal()
{
    kcfg_astrometrySolver->setEnabled(!kcfg_astrometrySolverIsInternal->isChecked());
    if(kcfg_astrometrySolverIsInternal->isChecked())
        kcfg_astrometrySolver->setText("*Internal Solver*");
    else
        kcfg_astrometrySolver->setText("/usr/local/bin/solve-field");
}

void OpsEkos::toggleConfigInternal()
{
    kcfg_astrometryConfFile->setEnabled(!kcfg_astrometryConfFileIsInternal->isChecked());
    if(kcfg_astrometryConfFileIsInternal->isChecked())
        kcfg_astrometryConfFile->setText("*Internal astrometry.cfg*");
    else
        kcfg_astrometryConfFile->setText("/etc/astrometry.cfg");
}

void OpsEkos::toggleWCSInternal()
{
    kcfg_astrometryWCSInfo->setEnabled(!kcfg_wcsIsInternal->isChecked());
    if(kcfg_wcsIsInternal->isChecked())
        kcfg_astrometryWCSInfo->setText("*Internal wcsinfo*");
    else
        kcfg_astrometryWCSInfo->setText("/usr/local/bin/wcsinfo");
}

void OpsEkos::slotApply()
{
    EkosManager *ekosManager = KStars::Instance()->ekosManager();

    if (ekosManager)
    {
        Ekos::Align *alignModule = ekosManager->alignModule();

        if (alignModule && alignModule->fov())
            alignModule->fov()->setImageDisplay(kcfg_SolverWCS->isChecked());
    }

}

void OpsEkos::clearAll()
{
    if (darkFramesModel->rowCount() == 0)
        return;

    if (KMessageBox::questionYesNo(KStars::Instance(), i18n("Are you sure you want to delete all dark frames images and data?")) == KMessageBox::No)
        return;

    QString darkFilesPath  = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "darks";

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
    QString darkFilesPath  = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "darks";

    QDesktopServices::openUrl(QUrl::fromLocalFile(darkFilesPath));
}

void OpsEkos::refreshDarkData()
{
    QSqlDatabase userdb = QSqlDatabase::database("userdb");
    userdb.open();

    delete (darkFramesModel);
    darkFramesModel = new QSqlTableModel(this, userdb);
    darkFramesModel->setTable("darkframe");
    darkFramesModel->select();
    darkTableView->setModel(darkFramesModel);
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
        KStars::Instance()->genericFITSViewer()->addFITS(&url);
        KStars::Instance()->genericFITSViewer()->show();
    }
}


#include "opsastrometrycfg.h"

#include "align.h"
#include "kstars.h"
#include "ksutils.h"
#include "Options.h"
#include "ksnotification.h"
#include "ui_opsastrometrycfg.h"
#include "kspaths.h"

#include <KConfigDialog>
#include <KMessageBox>
#include <QFileDialog>

namespace Ekos
{
OpsAstrometryCfg::OpsAstrometryCfg(Align *parent) : QDialog(KStars::Instance())
{
    setupUi(this);

    alignModule = parent;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("alignsettings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
    connect(astrometryCFGDisplay, SIGNAL(textChanged()), SLOT(slotCFGEditorUpdated()));

    connect(loadCFG, SIGNAL(clicked()), this, SLOT(slotLoadCFG()));
    connect(addIndexFilePath, SIGNAL(clicked()), this, SLOT(slotAddAstrometryIndexFileLocation()));
    connect(removeIndexFilePath, SIGNAL(clicked()), this, SLOT(slotRemoveAstrometryIndexFileLocation()));

    connect(AstrometryIndexFileLocations, &QListWidget::itemDoubleClicked, this, &OpsAstrometryCfg::slotClickAstrometryIndexFileLocation);
    connect(AstrometryIndexFileLocations, &QListWidget::itemActivated, this, [this]()
    {
        removeIndexFilePath->setEnabled(AstrometryIndexFileLocations->selectedItems().count() > 0);
    });

    slotLoadCFG();
}

void OpsAstrometryCfg::showEvent(QShowEvent *)
{
    slotLoadCFG();
}

void OpsAstrometryCfg::slotLoadCFG()
{
    QString confPath = KSUtils::getAstrometryConfFilePath();

    QFile confFile(confPath);

    astrometryCFGLocation->setText(confPath);

    if (confFile.open(QIODevice::ReadOnly) == false)
    {
        bool confFileExists = false;
        if(Options::astrometryConfFileIsInternal())
        {
            if(KSUtils::configureLocalAstrometryConfIfNecessary())
            {
                if (confFile.open(QIODevice::ReadOnly))
                    confFileExists = true;
            }
        }
        if(!confFileExists)
        {
            KSNotification::error(i18n("Astrometry configuration file corrupted or missing: %1\nPlease set the "
                                       "configuration file full path in INDI options.",
                                       confPath));
            return;
        }
    }

    QTextStream in(&confFile);

    currentCFGText = in.readAll();

    astrometryCFGDisplay->setPlainText(currentCFGText);

    confFile.close();
    AstrometryIndexFileLocations->clear();
    QStringList astrometryDataDirs = KSUtils::getAstrometryDataDirs();
    for(QString astrometryDataDir : astrometryDataDirs)
    {
        QListWidgetItem *item = new QListWidgetItem(astrometryDataDir);
        item->setIcon(QIcon::fromTheme("stock_folder"));
        AstrometryIndexFileLocations->addItem(item);
    }
}

void OpsAstrometryCfg::slotAddAstrometryIndexFileLocation()
{
    QString dir =
        QFileDialog::getExistingDirectory(KStars::Instance(), i18nc("@title:window", "Index File Directory"), QDir::homePath());

    if (dir.isEmpty())
        return;

    KSUtils::addAstrometryDataDir(dir);
    slotLoadCFG();
}

void OpsAstrometryCfg::slotRemoveAstrometryIndexFileLocation()
{
    const QString folder = AstrometryIndexFileLocations->selectedItems().first()->text();
    if (KMessageBox::questionYesNo(nullptr, i18n("Are you sure you want to remove %1 folder from astrometry configuration?", folder))
            == KMessageBox::No)
        return;

    KSUtils::removeAstrometryDataDir(folder);
    slotLoadCFG();
}

void OpsAstrometryCfg::slotClickAstrometryIndexFileLocation(QListWidgetItem *item)
{
    if(AstrometryIndexFileLocations->count() == 0)
        return;
    QUrl path = QUrl::fromLocalFile(item->text());
    QDesktopServices::openUrl(path);
}

void OpsAstrometryCfg::slotApply()
{
    if (currentCFGText != astrometryCFGDisplay->toPlainText())
    {
        QString confPath = KSUtils::getAstrometryConfFilePath();

        QFile confFile(confPath);
        if (confFile.open(QIODevice::WriteOnly) == false)
            KSNotification::error(i18n("Internal Astrometry configuration file write error."));
        else
        {
            QTextStream out(&confFile);
            out << astrometryCFGDisplay->toPlainText();
            confFile.close();
            KSNotification::info(i18n("Astrometry.cfg successfully saved."));
            currentCFGText = astrometryCFGDisplay->toPlainText();
            AstrometryIndexFileLocations->clear();
            AstrometryIndexFileLocations->addItems(KSUtils::getAstrometryDataDirs());
        }
    }
}
void OpsAstrometryCfg::slotCFGEditorUpdated()
{
    if (currentCFGText != astrometryCFGDisplay->toPlainText())
        m_ConfigDialog->button(QDialogButtonBox::Apply)->setEnabled(true);
}
}

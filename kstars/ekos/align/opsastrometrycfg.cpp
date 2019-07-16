
#include "opsastrometrycfg.h"

#include "align.h"
#include "kstars.h"
#include "ksutils.h"
#include "Options.h"
#include "ksnotification.h"
#include "ui_opsastrometrycfg.h"

#include <KConfigDialog>
#include <KMessageBox>

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
    connect(setIndexFileB, SIGNAL(clicked()), this, SLOT(slotSetAstrometryIndexFileLocation()));

    slotLoadCFG();
}

void OpsAstrometryCfg::slotLoadCFG()
{
    QString confPath;

    if (Options::astrometryConfFileIsInternal())
        confPath = QCoreApplication::applicationDirPath() + "/astrometry/bin/astrometry.cfg";
    else
        confPath = Options::astrometryConfFile();

    QFile confFile(confPath);

    astrometryCFGLocation->setText(confPath);

    if (confFile.open(QIODevice::ReadOnly) == false)
    {
        KSNotification::error(i18n("Astrometry configuration file corrupted or missing: %1\nPlease set the "
                                   "configuration file full path in INDI options.",
                                   Options::astrometryConfFile()));
        return;
    }

    QTextStream in(&confFile);

    currentCFGText = in.readAll();

    astrometryCFGDisplay->setPlainText(currentCFGText);

    confFile.close();
}

void OpsAstrometryCfg::slotSetAstrometryIndexFileLocation()
{
#ifdef Q_OS_OSX
    KSUtils::setAstrometryDataDir(kcfg_AstrometryIndexFileLocation->text());
#endif
    slotLoadCFG();
}

void OpsAstrometryCfg::slotApply()
{
    if (currentCFGText != astrometryCFGDisplay->toPlainText())
    {
        QString confPath;

        if (Options::astrometryConfFileIsInternal())
            confPath = QCoreApplication::applicationDirPath() + "/astrometry/bin/astrometry.cfg";
        else
            confPath = Options::astrometryConfFile();

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
            QString astrometryDataDir;
#ifdef Q_OS_OSX
            KSUtils::getAstrometryDataDir(astrometryDataDir);
#endif
            if(astrometryDataDir != kcfg_AstrometryIndexFileLocation->text())
                kcfg_AstrometryIndexFileLocation->setText(astrometryDataDir);
        }
    }
}
void OpsAstrometryCfg::slotCFGEditorUpdated()
{
    if (currentCFGText != astrometryCFGDisplay->toPlainText())
        m_ConfigDialog->button(QDialogButtonBox::Apply)->setEnabled(true);
}
}

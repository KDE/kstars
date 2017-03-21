#include "opsastrometryindexfiles.h"
#include "ui_opsastrometryindexfiles.h"

#include <KConfigDialog>
#include "kstars.h"
#include "align.h"
#include "Options.h"
#include <KMessageBox>

namespace Ekos
{

OpsAstrometryIndexFiles::OpsAstrometryIndexFiles(Align * parent)  : QDialog( KStars::Instance() )
{
    setupUi(this);

    alignModule = parent;

    //Get a pointer to the KConfigDialog
    // m_ConfigDialog = KConfigDialog::exists( "alignsettings" );
    connect(openIndexFileDirectory, SIGNAL(clicked()), this, SLOT(slotOpenIndexFileDirectory()));

    astrometryIndex[2.8] = "00";
    astrometryIndex[4.0] = "01";
    astrometryIndex[5.6] = "02";
    astrometryIndex[8] = "03";
    astrometryIndex[11] = "04";
    astrometryIndex[16] = "05";
    astrometryIndex[22] = "06";
    astrometryIndex[30] = "07";
    astrometryIndex[42] = "08";
    astrometryIndex[60] = "09";
    astrometryIndex[85] = "10";
    astrometryIndex[120] = "11";
    astrometryIndex[170] = "12";
    astrometryIndex[240] = "13";
    astrometryIndex[340] = "14";
    astrometryIndex[480] = "15";
    astrometryIndex[680] = "16";
    astrometryIndex[1000] = "17";
    astrometryIndex[1400] = "18";
    astrometryIndex[2000] = "19";
}

OpsAstrometryIndexFiles::~OpsAstrometryIndexFiles() {}

void OpsAstrometryIndexFiles::showEvent(QShowEvent *)
{
    slotUpdate();
}

void OpsAstrometryIndexFiles::slotUpdate()
{
    double fov_w, fov_h, fov_pixscale;

    // Values in arcmins. Scale in arcsec per pixel
    alignModule->getFOVScale(fov_w, fov_h, fov_pixscale);

    double fov_check = qMax(fov_w, fov_h);

    FOVOut->setText(QString("%1' x %2'").arg(QString::number(fov_w, 'f', 2)).arg(QString::number(fov_h, 'f', 2)));

    QString astrometryDataDir;

    if (getAstrometryDataDir(astrometryDataDir) == false)
        return;

    indexLocation->setText(astrometryDataDir);

    QStringList nameFilter("*.fits");
    QDir directory(astrometryDataDir);
    QStringList indexList = directory.entryList(nameFilter);

    foreach(QString indexName, indexList)
    {
        indexName=indexName.replace("-","_").left(10);
        QCheckBox * indexCheckBox = findChild<QCheckBox *>(indexName);
        if(indexCheckBox)
            indexCheckBox->setChecked(true);
    }

    QList<QCheckBox *> checkboxes = findChildren<QCheckBox *>();

    foreach(QCheckBox * checkBox, checkboxes)
    {
        checkBox->setIcon(QIcon(":/icons/breeze/default/security-low.svg"));
        checkBox->setToolTip(i18n("Optional"));
        checkBox->setEnabled(false); //This is for now, until we get a downloader set up.
    }

    float last_skymarksize = 2;
    foreach(float skymarksize, astrometryIndex.keys())
    {
        if(   (skymarksize >= 0.40 * fov_check && skymarksize <= 0.9 * fov_check)
                || (fov_check > last_skymarksize && fov_check < skymarksize))
        {
            QString indexName1="index_41" + astrometryIndex.value(skymarksize);
            QString indexName2="index_42" + astrometryIndex.value(skymarksize);
            QCheckBox * indexCheckBox1 = findChild<QCheckBox *>(indexName1);
            QCheckBox * indexCheckBox2 = findChild<QCheckBox *>(indexName2);
            if(indexCheckBox1)
            {
                indexCheckBox1->setIcon(QIcon(":/icons/breeze/default/security-high.svg"));
                indexCheckBox1->setToolTip(i18n("Required"));
            }
            if(indexCheckBox2)
            {
                indexCheckBox2->setIcon(QIcon(":/icons/breeze/default/security-high.svg"));
                indexCheckBox2->setToolTip(i18n("Required"));
            }

        }
        else if (skymarksize >= 0.10 * fov_check && skymarksize <= fov_check)
        {
            QString indexName1="index_41" + astrometryIndex.value(skymarksize);
            QString indexName2="index_42" + astrometryIndex.value(skymarksize);
            QCheckBox * indexCheckBox1 = findChild<QCheckBox *>(indexName1);
            QCheckBox * indexCheckBox2 = findChild<QCheckBox *>(indexName2);
            if(indexCheckBox1)
            {
                indexCheckBox1->setIcon(QIcon(":/icons/breeze/default/security-medium.svg"));
                indexCheckBox1->setToolTip(i18n("Recommended"));
            }
            if(indexCheckBox2)
            {
                indexCheckBox2->setIcon(QIcon(":/icons/breeze/default/security-medium.svg"));
                indexCheckBox2->setToolTip(i18n("Recommended"));
            }
        }

        last_skymarksize = skymarksize;
    }

}

void OpsAstrometryIndexFiles::slotOpenIndexFileDirectory()
{
    QString astrometryDataDir;
    if (getAstrometryDataDir(astrometryDataDir) == false)
        return;
    QUrl path = QUrl::fromLocalFile(astrometryDataDir);
    QDesktopServices::openUrl(path);
}

bool OpsAstrometryIndexFiles::getAstrometryDataDir(QString &dataDir)
{
    QString confPath;

    if(Options::astrometryConfFileIsInternal())
        confPath = QCoreApplication::applicationDirPath()+"/astrometry/bin/astrometry.cfg";
    else
        confPath = Options::astrometryConfFile();

    QFile confFile(confPath);

    if (confFile.open(QIODevice::ReadOnly) == false)
    {
        KMessageBox::error(0, i18n("Astrometry configuration file corrupted or missing: %1\nPlease set the configuration file full path in INDI options.", Options::astrometryConfFile()));
        return false;
    }

    QTextStream in(&confFile);
    QString line;
    while ( !in.atEnd() )
    {
        line = in.readLine();
        if (line.isEmpty() || line.startsWith("#"))
            continue;

        line = line.trimmed();
        if (line.startsWith("add_path"))
        {
            dataDir = line.mid(9).trimmed();
            return true;
        }
    }

    KMessageBox::error(0, i18n("Unable to find data dir in astrometry configuration file."));
    return false;
}


}

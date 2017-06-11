#include "opsastrometryindexfiles.h"
#include "ui_opsastrometryindexfiles.h"

#include <QProgressBar>
#include <KConfigDialog>
#include <KMessageBox>

#include "kstars.h"
#include "align.h"
#include "Options.h"

#include <kauthaction.h>
#include <kauthexecutejob.h>

namespace Ekos
{
OpsAstrometryIndexFiles::OpsAstrometryIndexFiles(Align *parent) : QDialog(KStars::Instance())
{
    setupUi(this);

    alignModule = parent;
    manager     = new QNetworkAccessManager();

    //Get a pointer to the KConfigDialog
    // m_ConfigDialog = KConfigDialog::exists( "alignsettings" );
    connect(openIndexFileDirectory, SIGNAL(clicked()), this, SLOT(slotOpenIndexFileDirectory()));

    astrometryIndex[2.8]  = "00";
    astrometryIndex[4.0]  = "01";
    astrometryIndex[5.6]  = "02";
    astrometryIndex[8]    = "03";
    astrometryIndex[11]   = "04";
    astrometryIndex[16]   = "05";
    astrometryIndex[22]   = "06";
    astrometryIndex[30]   = "07";
    astrometryIndex[42]   = "08";
    astrometryIndex[60]   = "09";
    astrometryIndex[85]   = "10";
    astrometryIndex[120]  = "11";
    astrometryIndex[170]  = "12";
    astrometryIndex[240]  = "13";
    astrometryIndex[340]  = "14";
    astrometryIndex[480]  = "15";
    astrometryIndex[680]  = "16";
    astrometryIndex[1000] = "17";
    astrometryIndex[1400] = "18";
    astrometryIndex[2000] = "19";

    QList<QCheckBox *> checkboxes = findChildren<QCheckBox *>();

    foreach (QCheckBox *checkBox, checkboxes)
    {
        connect(checkBox, SIGNAL(clicked(bool)), this, SLOT(downloadOrDeleteIndexFiles(bool)));
    }

    QList<QProgressBar *> progressBars = findChildren<QProgressBar *>();

    foreach (QProgressBar *bar, progressBars)
    {
        bar->setVisible(false);
    }
}

OpsAstrometryIndexFiles::~OpsAstrometryIndexFiles()
{
}

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

    foreach (QString indexName, indexList)
    {
        indexName                = indexName.replace("-", "_").left(10);
        QCheckBox *indexCheckBox = findChild<QCheckBox *>(indexName);
        if (indexCheckBox)
            indexCheckBox->setChecked(true);
    }

    QList<QCheckBox *> checkboxes = findChildren<QCheckBox *>();

    foreach (QCheckBox *checkBox, checkboxes)
    {
        checkBox->setIcon(QIcon(":/icons/breeze/default/security-low.svg"));
        checkBox->setToolTip(i18n("Optional"));
    }

    float last_skymarksize = 2;
    foreach (float skymarksize, astrometryIndex.keys())
    {
        if ((skymarksize >= 0.40 * fov_check && skymarksize <= 0.9 * fov_check) ||
            (fov_check > last_skymarksize && fov_check < skymarksize))
        {
            QString indexName1        = "index_41" + astrometryIndex.value(skymarksize);
            QString indexName2        = "index_42" + astrometryIndex.value(skymarksize);
            QCheckBox *indexCheckBox1 = findChild<QCheckBox *>(indexName1);
            QCheckBox *indexCheckBox2 = findChild<QCheckBox *>(indexName2);
            if (indexCheckBox1)
            {
                indexCheckBox1->setIcon(QIcon(":/icons/breeze/default/security-high.svg"));
                indexCheckBox1->setToolTip(i18n("Required"));
            }
            if (indexCheckBox2)
            {
                indexCheckBox2->setIcon(QIcon(":/icons/breeze/default/security-high.svg"));
                indexCheckBox2->setToolTip(i18n("Required"));
            }
        }
        else if (skymarksize >= 0.10 * fov_check && skymarksize <= fov_check)
        {
            QString indexName1        = "index_41" + astrometryIndex.value(skymarksize);
            QString indexName2        = "index_42" + astrometryIndex.value(skymarksize);
            QCheckBox *indexCheckBox1 = findChild<QCheckBox *>(indexName1);
            QCheckBox *indexCheckBox2 = findChild<QCheckBox *>(indexName2);
            if (indexCheckBox1)
            {
                indexCheckBox1->setIcon(QIcon(":/icons/breeze/default/security-medium.svg"));
                indexCheckBox1->setToolTip(i18n("Recommended"));
            }
            if (indexCheckBox2)
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

    if (Options::astrometryConfFileIsInternal())
        confPath = QCoreApplication::applicationDirPath() + "/astrometry/bin/astrometry.cfg";
    else
        confPath = Options::astrometryConfFile();

    QFile confFile(confPath);

    if (confFile.open(QIODevice::ReadOnly) == false)
    {
        KMessageBox::error(0, i18n("Astrometry configuration file corrupted or missing: %1\nPlease set the "
                                   "configuration file full path in INDI options.",
                                   Options::astrometryConfFile()));
        return false;
    }

    QTextStream in(&confFile);
    QString line;
    while (!in.atEnd())
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

bool OpsAstrometryIndexFiles::astrometryIndicesAreAvailable()
{
    QNetworkReply *response = manager->get(QNetworkRequest(QUrl("http://broiler.astrometry.net")));
    QTimer timeout(this);
    timeout.setInterval(5000);
    timeout.setSingleShot(true);
    timeout.start();
    while (!response->isFinished())
    {
        if (!timeout.isActive())
        {
            response->deleteLater();
            return false;
        }
        qApp->processEvents();
    }
    timeout.stop();
    bool wasSuccessful = (response->error() == QNetworkReply::NoError);
    response->deleteLater();
    return wasSuccessful;
}

void OpsAstrometryIndexFiles::downloadIndexFile(QString URL, QString fileN, QCheckBox *checkBox, int currentIndex,
                                                int maxIndex)
{
    QString indexString = QString::number(currentIndex);
    if (currentIndex < 10)
        indexString = "0" + indexString;

    QString indexSeriesName = checkBox->text().remove("&");
    QProgressBar *indexDownloadProgress =
        findChild<QProgressBar *>(indexSeriesName.replace("-", "_").left(10) + "_progress");
    if (indexDownloadProgress)
        indexDownloadProgress->setValue((int)(currentIndex * 100 / maxIndex));

    QString indexURL = URL;
    indexURL.replace("*", indexString);

    QNetworkReply *response = manager->get(QNetworkRequest(QUrl(indexURL)));

    QTimer::singleShot(60000, response, [response, checkBox, indexDownloadProgress] { //Shut it down after 60 sec.
        qDebug() << "Index File Download Timed out.";
        response->abort();
        response->deleteLater();
        if (checkBox)
            checkBox->setEnabled(true);
        if (indexDownloadProgress)
            indexDownloadProgress->setVisible(false);
    });
    connect(response, &QNetworkReply::finished, this,
            [URL, fileN, checkBox, currentIndex, maxIndex, this, response, indexString, indexDownloadProgress] {
                response->deleteLater();
                if (response->error() != QNetworkReply::NoError)
                    return;
                QByteArray responseData = response->readAll();
                QString indexFileN      = fileN;
                indexFileN.replace("*", indexString);

                QFile file(indexFileN);
                if (QFileInfo(QFileInfo(file).path()).isWritable())
                {
                    if (!file.open(QIODevice::WriteOnly))
                    {
                        KMessageBox::error(0, i18n("File Write Error"));
                        slotUpdate();
                        return;
                    }
                    else
                    {
                        file.write(responseData.data(), responseData.size());
                        file.close();
                    }
                }
                else
                {
#ifdef Q_OS_OSX
                    KMessageBox::error(0, i18n("Astrometry Folder Permissions Error"));
#else
                    KAuth::Action action(QStringLiteral("org.kde.kf5auth.kstars.saveindexfile"));
                    action.setHelperId(QStringLiteral("org.kde.kf5auth.kstars"));
                    action.setArguments(QVariantMap({ { "filename", indexFileN }, { "contents", responseData } }));
                    KAuth::ExecuteJob *job = action.execute();
                    if (!job->exec())
                    {
                        QMessageBox::information(
                            this, "Error",
                            QString("KAuth returned an error code: %1 %2").arg(job->error()).arg(job->errorString()));
                        slotUpdate();
                        return;
                    }
#endif
                }

                if (currentIndex == maxIndex)
                {
                    checkBox->setEnabled(true);
                    if (indexDownloadProgress)
                        indexDownloadProgress->setVisible(false);
                    slotUpdate();
                }
                else
                    downloadIndexFile(URL, fileN, checkBox, currentIndex + 1, maxIndex);
            });
}

void OpsAstrometryIndexFiles::downloadOrDeleteIndexFiles(bool checked)
{
    QCheckBox *checkBox = (QCheckBox *)QObject::sender();

    QString astrometryDataDir;
    if (getAstrometryDataDir(astrometryDataDir) == false)
        return;

    if (checkBox)
    {
        QString indexSetName                = checkBox->text().remove("&");
        QString progressBarName             = indexSetName;
        progressBarName                     = progressBarName.replace("-", "_").left(10) + "_progress";
        QProgressBar *indexDownloadProgress = findChild<QProgressBar *>(progressBarName);
        QString filePath                    = astrometryDataDir + "/" + indexSetName;
        int indexFileNum                    = indexSetName.mid(8, 2).toInt();
        if (checked)
        {
            checkBox->setChecked(!checked);
            if (astrometryIndicesAreAvailable())
            {
                if (indexDownloadProgress)
                    indexDownloadProgress->setVisible(true);
                checkBox->setEnabled(false);
                QString URL;
                if (indexSetName.startsWith("index-41"))
                    URL = "http://broiler.astrometry.net/~dstn/4100/" + indexSetName;
                else if (indexSetName.startsWith("index-42"))
                    URL = "http://broiler.astrometry.net/~dstn/4200/" + indexSetName;
                int maxIndex = 0;
                if (indexFileNum < 8 && URL.contains("*"))
                {
                    maxIndex = 11;
                    if (indexFileNum < 5)
                        maxIndex = 47;
                }
                downloadIndexFile(URL, filePath, checkBox, 0, maxIndex);
            }
            else
            {
                KMessageBox::sorry(0, i18n("Could not contact Astrometry Index Server: broiler.astrometry.net"));
            }
        }
        else
        {
            if (KMessageBox::Continue == KMessageBox::warningContinueCancel(
                                             NULL, "Are you sure you want to delete these index files? " + indexSetName,
                                             i18n("Delete File(s)"), KStandardGuiItem::cont(),
                                             KStandardGuiItem::cancel(), "delete_index_files_warning"))
            {
                if (QFileInfo(astrometryDataDir).isWritable())
                {
                    QStringList nameFilter("*.fits");
                    QDir directory(astrometryDataDir);
                    QStringList indexList = directory.entryList(nameFilter);
                    foreach (QString fileName, indexList)
                    {
                        if (fileName.contains(indexSetName.left(10)))
                        {
                            if (!directory.remove(fileName))
                            {
                                KMessageBox::error(0, i18n("File Delete Error"));
                                slotUpdate();
                                return;
                            }
                        }
                    }
                }
                else
                {
#ifdef Q_OS_OSX
                    KMessageBox::error(0, i18n("Astrometry Folder Permissions Error"));
                    slotUpdate();
#else
                    KAuth::Action action(QStringLiteral("org.kde.kf5auth.kstars.removeindexfileset"));
                    action.setHelperId(QStringLiteral("org.kde.kf5auth.kstars"));
                    action.setArguments(
                        QVariantMap({ { "indexSetName", indexSetName }, { "astrometryDataDir", astrometryDataDir } }));
                    KAuth::ExecuteJob *job = action.execute();
                    if (!job->exec())
                        QMessageBox::information(
                            this, "Error",
                            QString("KAuth returned an error code: %1 %2").arg(job->error()).arg(job->errorString()));
#endif
                }
            }
        }
    }
}
}

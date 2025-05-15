
#include "opsastrometryindexfiles.h"

#include "align.h"
#include "kstars.h"
#include "ksutils.h"
#include "Options.h"
#include "ksnotification.h"

#include <KConfigDialog>
#include <KMessageBox>
#include <QFileDialog>
#include <QDesktopServices>

namespace Ekos
{
OpsAstrometryIndexFiles::OpsAstrometryIndexFiles(Align *parent) : QDialog(KStars::Instance())
{
    setupUi(this);

    downloadSpeed       = 100;
    actualdownloadSpeed = downloadSpeed;
    alignModule         = parent;
    manager             = new QNetworkAccessManager();

    indexURL->setText("http://data.astrometry.net/");

    //Get a pointer to the KConfigDialog
    // m_ConfigDialog = KConfigDialog::exists( "alignsetFtings" );
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

    connect(indexLocations, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &OpsAstrometryIndexFiles::slotUpdate);

    for (auto &checkBox : checkboxes)
    {
        connect(checkBox, &QCheckBox::toggled, this, &OpsAstrometryIndexFiles::downloadOrDeleteIndexFiles);
    }

    QList<QProgressBar *> progressBars = findChildren<QProgressBar *>();
    QList<QLabel *> qLabels = findChildren<QLabel *>();
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();

    for (auto &bar : progressBars)
    {
        if(bar->objectName().contains("progress"))
        {
            bar->setVisible(false);
            bar->setTextVisible(false);
        }
    }

    for (auto &button : qButtons)
    {
        if(button->objectName().contains("cancel"))
        {
            button->setVisible(false);
        }
    }

    for (QLabel * label : qLabels)
    {
        if(label->text().contains("info") || label->text().contains("perc"))
        {
            label->setVisible(false);
        }
    }

    connect(addIndexFileDirectory, &QAbstractButton::clicked, this, [this]()
    {
        QString dir = QFileDialog::getExistingDirectory(this, "Load Index File Directory",
                      QDir::homePath(),
                      QFileDialog::ShowDirsOnly
                      | QFileDialog::DontResolveSymlinks);
        if (dir.isEmpty())
            return;
        addDirectoryToList(dir);
    });
    connect(removeIndexFileDirectory, &QAbstractButton::clicked, this, [this]()
    {
        if(indexLocations->currentIndex() != 0)
            removeDirectoryFromList(indexLocations->currentText());
    });


}

void OpsAstrometryIndexFiles::showEvent(QShowEvent *)
{
    updateIndexDirectoryList();

}

void OpsAstrometryIndexFiles::updateIndexDirectoryList()
{
    // This is needed because they might have directories stored in the config file.
    // So we can't just use the options folder list.
    QStringList astrometryDataDirs = KSUtils::getAstrometryDataDirs();

    indexLocations->clear();
    if(astrometryDataDirs.count() > 1)
        indexLocations->addItem("All Sources");
    indexLocations->addItems(astrometryDataDirs);
    slotUpdate();
}

void OpsAstrometryIndexFiles::addDirectoryToList(QString directory)
{
    QDir dir(directory);
    if(!dir.exists())
        return;
    QString directoryPath = dir.absolutePath();

    QStringList indexFileDirs = Options::astrometryIndexFolderList();
    if(indexFileDirs.contains(directoryPath))
        return;
    indexFileDirs.append(directoryPath);
    Options::setAstrometryIndexFolderList(indexFileDirs);
    updateIndexDirectoryList();
}

void OpsAstrometryIndexFiles::removeDirectoryFromList(QString directory)
{
    QStringList indexFileDirs = Options::astrometryIndexFolderList();
    if(indexFileDirs.contains(directory))
    {
        indexFileDirs.removeOne(directory);
        Options::setAstrometryIndexFolderList(indexFileDirs);
        updateIndexDirectoryList();
    }
}

void OpsAstrometryIndexFiles::downloadSingleIndexFile(const QString &indexFileName)
{
    QStringList astrometryDataDirs = Options::astrometryIndexFolderList();
    if (astrometryDataDirs.isEmpty())
    {
        KSNotification::sorry(i18n("No index file directories configured."), i18n("Error"), 10);
        return;
    }

    // Skip first directory if it's "All Sources"
    if (astrometryDataDirs.count() > 1)
        astrometryDataDirs.removeFirst();

    // Check if file exists in any directory
    for (const QString &dir : astrometryDataDirs)
    {
        QDir directory(dir);
        if (directory.exists())
        {
            if (fileCountMatches(directory, indexFileName))
            {
                return;
            }
        }
    }

    QString astrometryDataDir = findFirstWritableDir();
    if (astrometryDataDir.isEmpty())
    {
        KSNotification::sorry(i18n("No writable index file directory found."), i18n("Error"), 10);
        return;
    }

    if (!astrometryIndicesAreAvailable())
    {
        KSNotification::sorry(i18n("Could not contact Astrometry Index Server."), i18n("Error"), 10);
        return;
    }

    QString BASE_URL;
    if (indexURL->text().endsWith("/"))
        BASE_URL = indexURL->text();
    else
        BASE_URL = indexURL->text() + "/";

    QString URL;
    if (indexFileName.startsWith(QLatin1String("index-41")))
        URL = BASE_URL + "4100/" + indexFileName;
    else if (indexFileName.startsWith(QLatin1String("index-42")))
        URL = BASE_URL + "4200/" + indexFileName;
    else if (indexFileName.startsWith(QLatin1String("index-52")))
        URL = "https://portal.nersc.gov/project/cosmo/temp/dstn/index-5200/LITE/" + indexFileName;
    else
        return;

    QString filePath = astrometryDataDir + '/' + indexFileName;
    QString fileNumString = indexFileName.mid(8, 2);

    // Find the checkbox for this index series
    QString indexName = indexFileName;
    indexName.replace('-', '_');
    indexName = indexName.left(10);
    QCheckBox *indexCheckBox = findChild<QCheckBox *>(indexName);
    if (!indexCheckBox)
        return;

    double fileSize = 1E11 * qPow(astrometryIndex.key(fileNumString), -1.909);
    downloadIndexFile(URL, filePath, indexFileName, 0, 0, fileSize);

    // Update UI after download completes
    slotUpdate();
}

void OpsAstrometryIndexFiles::setCheckBoxStateProgrammatically(QCheckBox* checkbox, bool checked)
{
    checkbox->blockSignals(true);   // Temporarily block signals
    checkbox->setChecked(checked);  // Update checkbox state
    checkbox->blockSignals(false);  // Re-enable signals
}

void OpsAstrometryIndexFiles::slotUpdate()
{
    QList<QCheckBox *> checkboxes = findChildren<QCheckBox *>();

    for (auto &checkBox : checkboxes)
    {
        setCheckBoxStateProgrammatically(checkBox, false);
    }

    if(indexLocations->count() == 0)
        return;

    double fov_w, fov_h, fov_pixscale;

    // Values in arcmins. Scale in arcsec per pixel
    alignModule->getFOVScale(fov_w, fov_h, fov_pixscale);

    double fov_check = qMax(fov_w, fov_h);

    FOVOut->setText(QString("%1' x %2'").arg(QString::number(fov_w, 'f', 2), QString::number(fov_h, 'f', 2)));

    QStringList nameFilter("*.fits");

    QStringList astrometryDataDirs = Options::astrometryIndexFolderList();

    bool allDirsSelected = (indexLocations->currentIndex() == 0 && astrometryDataDirs.count() > 1);
    bool folderIsWriteable;

    QStringList astrometryDataDirsToIndex;

    if(allDirsSelected)
    {
        folderDetails->setText(i18n("Downloads Disabled, this is not a directory, it is a list of all index files."));
        folderIsWriteable = false;
        astrometryDataDirsToIndex = astrometryDataDirs;
        openIndexFileDirectory->setEnabled(false);
    }
    else
    {
        QString folderPath = indexLocations->currentText();
        folderIsWriteable = QFileInfo(folderPath).isWritable();
        if(folderIsWriteable)
            folderDetails->setText(i18n("Downloads Enabled, the directory exists and is writeable."));
        else
            folderDetails->setText(i18n("Downloads Disabled, directory permissions issue."));
        if(!QFileInfo::exists(folderPath))
            folderDetails->setText(i18n("Downloads Disabled, directory does not exist."));
        astrometryDataDirsToIndex << folderPath;
        openIndexFileDirectory->setEnabled(true);
    }
    folderDetails->setCursorPosition(0);

    //This loop checks all the folders that are supposed to be checked for the files
    //It checks the box if it finds them
    for(auto &astrometryDataDir : astrometryDataDirsToIndex)
    {
        QDir directory(astrometryDataDir);
        QStringList indexList = directory.entryList(nameFilter);

        for (auto &indexName : indexList)
        {
            if (fileCountMatches(directory, indexName))
            {
                indexName                = indexName.replace('-', '_').left(10);
                QCheckBox *indexCheckBox = findChild<QCheckBox *>(indexName);
                if (indexCheckBox)
                    setCheckBoxStateProgrammatically(indexCheckBox, true);
            }
        }
    }

    for (auto &checkBox : checkboxes)
    {
        checkBox->setEnabled(folderIsWriteable);
        checkBox->setIcon(QIcon(":/icons/astrometry-optional.svg"));
        checkBox->setToolTip(i18n("Optional"));
        checkBox->setStyleSheet("");
    }

    float last_skymarksize = 2;

    for (auto &skymarksize : astrometryIndex.keys())
    {
        QString indexName1        = "index_41" + astrometryIndex.value(skymarksize);
        QString indexName2        = "index_42" + astrometryIndex.value(skymarksize);
        QString indexName3        = "index_52" + astrometryIndex.value(skymarksize);
        QCheckBox *indexCheckBox1 = findChild<QCheckBox *>(indexName1);
        QCheckBox *indexCheckBox2 = findChild<QCheckBox *>(indexName2);
        QCheckBox *indexCheckBox3 = findChild<QCheckBox *>(indexName3);
        if ((skymarksize >= 0.40 * fov_check && skymarksize <= 0.9 * fov_check) ||
                (fov_check > last_skymarksize && fov_check < skymarksize))
        {
            if (indexCheckBox1)
            {
                indexCheckBox1->setIcon(QIcon(":/icons/astrometry-required.svg"));
                indexCheckBox1->setToolTip(i18n("Required"));
            }
            if (indexCheckBox2)
            {
                indexCheckBox2->setIcon(QIcon(":/icons/astrometry-required.svg"));
                indexCheckBox2->setToolTip(i18n("Required"));
            }
            if (indexCheckBox3)
            {
                indexCheckBox3->setIcon(QIcon(":/icons/astrometry-required.svg"));
                indexCheckBox3->setToolTip(i18n("Required"));
            }
        }
        else if (skymarksize >= 0.10 * fov_check && skymarksize <= fov_check)
        {
            if (indexCheckBox1)
            {
                indexCheckBox1->setIcon(QIcon(":/icons/astrometry-recommended.svg"));
                indexCheckBox1->setToolTip(i18n("Recommended"));
            }
            if (indexCheckBox2)
            {
                indexCheckBox2->setIcon(QIcon(":/icons/astrometry-recommended.svg"));
                indexCheckBox2->setToolTip(i18n("Recommended"));
            }
            if (indexCheckBox3)
            {
                indexCheckBox3->setIcon(QIcon(":/icons/astrometry-recommended.svg"));
                indexCheckBox3->setToolTip(i18n("Recommended"));
            }
        }

        last_skymarksize = skymarksize;
    }

    //This loop goes over all the directories and adds a stylesheet to change the look of the checkbox text
    //if the File is installed in any directory.  Note that this indicator is then used below in the
    //Index File download function to check if they really want to do install a file that is installed.
    for(QString astrometryDataDir : astrometryDataDirs)
    {
        QDir directory(astrometryDataDir);
        QStringList indexList = directory.entryList(nameFilter);

        for (auto &indexName : indexList)
        {
            if (fileCountMatches(directory, indexName))
            {
                indexName                = indexName.replace('-', '_').left(10);
                QCheckBox *indexCheckBox = findChild<QCheckBox *>(indexName);
                if (indexCheckBox)
                    indexCheckBox->setStyleSheet("QCheckBox{font-weight: bold; color:green}");
            }
        }
    }
}

int OpsAstrometryIndexFiles::indexFileCount(QString indexName)
{
    int count = 0;
    if(indexName.contains("4207") || indexName.contains("4206") || indexName.contains("4205"))
        count = 12;
    else if(indexName.contains("4204") || indexName.contains("4203") || indexName.contains("4202")
            || indexName.contains("4201") || indexName.contains("4200") || indexName.contains("5206")
            || indexName.contains("5205") || indexName.contains("5204") || indexName.contains("5203")
            || indexName.contains("5202") || indexName.contains("5201") || indexName.contains("5200"))
        count = 48;
    else
        count = 1;
    return count;
}

bool OpsAstrometryIndexFiles::fileCountMatches(QDir directory, QString indexName)
{
    QString indexNameMatch = indexName.left(10) + "*.fits";
    QStringList list = directory.entryList(QStringList(indexNameMatch));
    return list.count() == indexFileCount(indexName);
}

void OpsAstrometryIndexFiles::slotOpenIndexFileDirectory()
{
    if(indexLocations->count() == 0)
        return;
    QUrl path = QUrl::fromLocalFile(indexLocations->currentText());
    QDesktopServices::openUrl(path);
}

bool OpsAstrometryIndexFiles::astrometryIndicesAreAvailable()
{
    auto url = QUrl(indexURL->text());
    auto response = manager->get(QNetworkRequest(QUrl(url.url(QUrl::RemovePath))));
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

void OpsAstrometryIndexFiles::downloadIndexFile(const QString &URL, const QString &fileN, const QString &indexSeriesName,
        int currentIndex, int maxIndex, double fileSize)
{
    QElapsedTimer downloadTime;
    downloadTime.start();

    QString indexString = QString::number(currentIndex);
    if (currentIndex < 10)
        indexString = '0' + indexString;

    QString uiIndexName = indexSeriesName;
    uiIndexName.replace('-', '_');
    uiIndexName = uiIndexName.left(10);

    QProgressBar *indexDownloadProgress = findChild<QProgressBar *>(uiIndexName + "_progress");
    QLabel *indexDownloadInfo = findChild<QLabel *>(uiIndexName + "_info");
    QPushButton *indexDownloadCancel = findChild<QPushButton *>(uiIndexName + "_cancel");
    QLabel *indexDownloadPerc = findChild<QLabel *>(uiIndexName + "_perc");

    setDownloadInfoVisible(indexSeriesName, true);

    if(indexDownloadInfo)
    {
        if (indexDownloadProgress && maxIndex > 0)
            indexDownloadProgress->setValue(currentIndex * 100 / maxIndex);
        QString info = "(" + QString::number(currentIndex) + '/' + QString::number(maxIndex + 1) + ") ";
        indexDownloadInfo->setText(info);
        emit newDownloadProgress(info);
    }

    QString indexURL = URL;

    indexURL.replace('*', indexString);

    QNetworkReply *response = manager->get(QNetworkRequest(QUrl(indexURL)));

    //Shut it down after too much time elapses.
    //If the filesize is less  than 4 MB, it sets the timeout for 1 minute or 60000 ms.
    //If it's larger, it assumes a bad download rate of 1 Mbps (100 bytes/ms)
    //and the calculation estimates the time in milliseconds it would take to download.
    int timeout = 60000;
    if(fileSize > 4000000)
        timeout = fileSize / downloadSpeed;
    //qDebug()<<"Filesize: "<< fileSize << ", timeout: " << timeout;

    QMetaObject::Connection *cancelConnection = new QMetaObject::Connection();
    QMetaObject::Connection *replyConnection = new QMetaObject::Connection();
    QMetaObject::Connection *percentConnection = new QMetaObject::Connection();

    if(indexDownloadPerc)
    {
        *percentConnection = connect(response, &QNetworkReply::downloadProgress,
                                     [ = ](qint64 bytesReceived, qint64 bytesTotal)
        {
            if (indexDownloadProgress)
            {
                indexDownloadProgress->setValue(bytesReceived);
                indexDownloadProgress->setMaximum(bytesTotal);
            }
            QString info = QString::number(bytesReceived * 100 / bytesTotal) + '%';
            indexDownloadPerc->setText(info);
            emit newDownloadProgress(info);
        });

    }

    timeoutTimer.disconnect();
    connect(&timeoutTimer, &QTimer::timeout, this, [&]()
    {
        KSNotification::error(
            i18n("Download Timed out.  Either the network is not fast enough, the file is not accessible, or you are not connected."),
            i18n("Error"), 10);
        disconnectDownload(cancelConnection, replyConnection, percentConnection);
        if(response)
        {
            response->abort();
            response->deleteLater();
        }
        setDownloadInfoVisible(indexSeriesName, false);
    });
    timeoutTimer.start(timeout);

    *cancelConnection = connect(indexDownloadCancel, &QPushButton::clicked,
                                [ = ]()
    {
        qDebug() << Q_FUNC_INFO << "Download Cancelled.";
        timeoutTimer.stop();
        disconnectDownload(cancelConnection, replyConnection, percentConnection);
        emit newDownloadProgress(i18n("%1 download cancelled.", indexSeriesName));
        if(response)
        {
            response->abort();
            response->deleteLater();
        }
        setDownloadInfoVisible(indexSeriesName, false);
    });

    *replyConnection = connect(response, &QNetworkReply::finished, this,
                               [ = ]()
    {
        timeoutTimer.stop();
        if(response)
        {
            disconnectDownload(cancelConnection, replyConnection, percentConnection);
            setDownloadInfoVisible(indexSeriesName, false);
            response->deleteLater();
            if (response->error() != QNetworkReply::NoError)
            {
                emit newDownloadProgress(response->errorString());
                KSNotification::error(response->errorString(), i18n("Error"), 10);
                return;
            }

            QByteArray responseData = response->readAll();
            QString indexFileN      = fileN;

            indexFileN.replace('*', indexString);

            QFile file(indexFileN);
            if (QFileInfo(QFileInfo(file).path()).isWritable())
            {
                if (!file.open(QIODevice::WriteOnly))
                {
                    KSNotification::error(i18n("File Write Error"), i18n("Error"), 10);
                    slotUpdate();
                    return;
                }
                else
                {
                    file.write(responseData.data(), responseData.size());
                    file.close();
                    int downloadedFileSize = QFileInfo(file).size();
                    int dtime = downloadTime.elapsed();
                    actualdownloadSpeed = (actualdownloadSpeed + (downloadedFileSize / dtime)) / 2;
                    qDebug() << Q_FUNC_INFO << "Filesize: " << downloadedFileSize << ", time: " << dtime << ", inst speed: " <<
                             downloadedFileSize / dtime <<
                             ", averaged speed: " << actualdownloadSpeed;
                    emit newDownloadProgress(i18n("%1 download complete.", indexSeriesName));

                }
            }
            else
            {
                KSNotification::error(i18n("Astrometry Folder Permissions Error"), i18n("Error"), 10);
            }

            if (currentIndex == maxIndex)
            {
                slotUpdate();
            }
            else
                downloadIndexFile(URL, fileN, indexSeriesName, currentIndex + 1, maxIndex, fileSize);
        }
    });
}

void OpsAstrometryIndexFiles::setDownloadInfoVisible(const QString &indexSeriesName, bool set)
{
    QString uiIndexName = indexSeriesName;
    uiIndexName.replace('-', '_');
    uiIndexName = uiIndexName.left(10);

    QProgressBar *indexDownloadProgress = findChild<QProgressBar *>(uiIndexName + "_progress");
    QLabel *indexDownloadInfo = findChild<QLabel *>(uiIndexName + "_info");
    QPushButton *indexDownloadCancel = findChild<QPushButton *>(uiIndexName + "_cancel");
    QLabel *indexDownloadPerc = findChild<QLabel *>(uiIndexName + "_perc");
    if (indexDownloadProgress)
        indexDownloadProgress->setVisible(set);
    if (indexDownloadInfo)
        indexDownloadInfo->setVisible(set);
    if (indexDownloadCancel)
        indexDownloadCancel->setVisible(set);
    if (indexDownloadPerc)
        indexDownloadPerc->setVisible(set);
}
void OpsAstrometryIndexFiles::disconnectDownload(QMetaObject::Connection *cancelConnection,
        QMetaObject::Connection *replyConnection, QMetaObject::Connection *percentConnection)
{
    if(cancelConnection)
        disconnect(*cancelConnection);
    if(replyConnection)
        disconnect(*replyConnection);
    if(percentConnection)
        disconnect(*percentConnection);
}

void OpsAstrometryIndexFiles::downloadOrDeleteIndexFiles(bool checked)
{
    QCheckBox *checkBox = qobject_cast<QCheckBox *>(QObject::sender());

    // check to prevent unwanted triggers during initialization
    if (!isVisible() || !checkBox)
        return;

    if (indexLocations->count() == 0)
        return;

    QString astrometryDataDir = indexLocations->currentText();

    // If "All Sources" is selected, find first writable directory
    if (indexLocations->currentIndex() == 0 && indexLocations->count() > 1)
    {
        astrometryDataDir = findFirstWritableDir();
        if (astrometryDataDir.isEmpty())
        {
            KSNotification::sorry(i18n("No writable index file directory found."), i18n("Error"), 10);
            return;
        }
    }
    else if (!QFileInfo::exists(astrometryDataDir))
    {
        KSNotification::sorry(
            i18n("The selected Index File directory does not exist. Please either create it or choose another."), i18n("Sorry"), 10);
        return;
    }

    if (checkBox)
    {
        QString indexSeriesName = checkBox->text().remove('&');
        QString filePath        = astrometryDataDir + '/' + indexSeriesName;
        QString fileNumString   = indexSeriesName.mid(8, 2);

        if (checked)
        {
            if(!checkBox->styleSheet().isEmpty()) //This means that the checkbox has a stylesheet so the index file was installed someplace.
            {
                if (KMessageBox::Cancel == KMessageBox::warningContinueCancel(
                            nullptr, i18n("The file %1 already exists in another directory.  Are you sure you want to download it to this directory as well?",
                                          indexSeriesName),
                            i18n("Install File(s)"), KStandardGuiItem::cont(),
                            KStandardGuiItem::cancel(), "install_index_files_warning"))
                {
                    checkBox->blockSignals(true);
                    checkBox->setChecked(false);
                    checkBox->blockSignals(false);
                    slotUpdate();
                    return;
                }
            }
            if (astrometryIndicesAreAvailable())
            {
                QString BASE_URL;
                QString URL;

                if (this->indexURL->text().endsWith("/"))
                {
                    BASE_URL = this->indexURL->text();
                }
                else
                {
                    BASE_URL = this->indexURL->text() + "/";
                }

                if (indexSeriesName.startsWith(QLatin1String("index-41")))
                    URL = BASE_URL + "4100/" + indexSeriesName;
                else if (indexSeriesName.startsWith(QLatin1String("index-42")))
                    URL = BASE_URL + "4200/" + indexSeriesName;
                else if (indexSeriesName.startsWith(QLatin1String("index-52")))
                    URL = "https://portal.nersc.gov/project/cosmo/temp/dstn/index-5200/LITE/" + indexSeriesName;

                int maxIndex = indexFileCount(indexSeriesName) - 1;

                double fileSize = 1E11 * qPow(astrometryIndex.key(fileNumString),
                                              -1.909); //This estimates the file size based on skymark size obtained from the index number.
                if(maxIndex != 0)
                    fileSize /= maxIndex; //FileSize is divided between multiple files for some index series.
                downloadIndexFile(URL, filePath, indexSeriesName, 0, maxIndex, fileSize);
            }
            else
            {
                checkBox->blockSignals(true);
                checkBox->setChecked(false);
                checkBox->blockSignals(false);
                KSNotification::sorry(i18n("Could not contact Astrometry Index Server."), i18n("Error"), 10);
            }
        }
        else
        {
            if (KMessageBox::Continue == KMessageBox::warningContinueCancel(
                        nullptr, i18n("Are you sure you want to delete these index files? %1", indexSeriesName),
                        i18n("Delete File(s)"), KStandardGuiItem::cont(),
                        KStandardGuiItem::cancel(), "delete_index_files_warning"))
            {
                if (QFileInfo(astrometryDataDir).isWritable())
                {
                    QStringList nameFilter("*.fits");
                    QDir directory(astrometryDataDir);
                    QStringList indexList = directory.entryList(nameFilter);
                    for (auto &fileName : indexList)
                    {
                        if (fileName.contains(indexSeriesName.left(10)))
                        {
                            if (!directory.remove(fileName))
                            {
                                KSNotification::error(i18n("File Delete Error"), i18n("Error"), 10);
                                slotUpdate();
                                return;
                            }
                            slotUpdate();
                        }
                    }
                }
                else
                {
                    KSNotification::error(i18n("Astrometry Folder Permissions Error"), i18n("Error"), 10);
                    slotUpdate();
                }
            }
        }
    }
}

QString OpsAstrometryIndexFiles::findFirstWritableDir()
{
    QStringList astrometryDataDirs = Options::astrometryIndexFolderList();
    if (astrometryDataDirs.isEmpty())
        return QString();

    // Skip first directory if it's "All Sources"
    if (astrometryDataDirs.count() > 1)
        astrometryDataDirs.removeFirst();

    // Find first writable directory
    for (const QString &dir : astrometryDataDirs)
    {
        QFileInfo dirInfo(dir);
        if (dirInfo.exists() && dirInfo.isWritable())
            return dir;
    }

    return QString();
}
}

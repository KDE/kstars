/***************************************************************************
                          kswizard.cpp  -  description
                             -------------------
    begin                : Wed 28 Jan 2004
    copyright            : (C) 2004 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kswizard.h"

#include "geolocation.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "Options.h"
#include "widgets/dmsbox.h"

#include <kns3/downloaddialog.h>

#include <QDesktopServices>
#include <QFile>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>

namespace
{
bool hasPrefix(QString str, QString prefix)
{
    if (prefix.isEmpty())
        return true;
    return str.startsWith(prefix, Qt::CaseInsensitive);
}
}

WizWelcomeUI::WizWelcomeUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

WizLocationUI::WizLocationUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

WizDownloadUI::WizDownloadUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

#ifdef Q_OS_OSX
WizDataUI::WizDataUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}
WizAstrometryUI::WizAstrometryUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}
#endif

KSWizard::KSWizard(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
#endif

    wizardStack = new QStackedWidget(this);
    adjustSize();

    setWindowTitle(i18n("Setup Wizard"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(wizardStack);
    setLayout(mainLayout);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal);
    nextB     = new QPushButton(i18n("&Next >"));
    nextB->setDefault(true);
    backB = new QPushButton(i18n("< &Back"));
    backB->setEnabled(false);

    buttonBox->addButton(backB, QDialogButtonBox::ActionRole);
    buttonBox->addButton(nextB, QDialogButtonBox::ActionRole);

    mainLayout->addWidget(buttonBox);

    welcome = new WizWelcomeUI(wizardStack);
#ifdef Q_OS_OSX
    data       = new WizDataUI(wizardStack);
    astrometry = new WizAstrometryUI(wizardStack);
#endif
    location                = new WizLocationUI(wizardStack);
    WizDownloadUI *download = new WizDownloadUI(wizardStack);

    wizardStack->addWidget(welcome);
#ifdef Q_OS_OSX
    wizardStack->addWidget(data);
    wizardStack->addWidget(astrometry);
#endif
    wizardStack->addWidget(location);
    wizardStack->addWidget(download);
    wizardStack->setCurrentWidget(welcome);

    //Load images into banner frames.
    QPixmap im;
    if (im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzstars.png")))
        welcome->Banner->setPixmap(im);
    else if (im.load(QDir(QCoreApplication::applicationDirPath() + "/../Resources/data").absolutePath() +
                     "/wzstars.png"))
        welcome->Banner->setPixmap(im);
    if (im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzgeo.png")))
        location->Banner->setPixmap(im);
    else if (im.load(QDir(QCoreApplication::applicationDirPath() + "/../Resources/data").absolutePath() + "/wzgeo.png"))
        location->Banner->setPixmap(im);
    if (im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzdownload.png")))
        download->Banner->setPixmap(im);
    else if (im.load(QDir(QCoreApplication::applicationDirPath() + "/../Resources/data").absolutePath() +
                     "/wzdownload.png"))
        download->Banner->setPixmap(im);

#ifdef Q_OS_OSX
    if (im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzdownload.png")))
        data->Banner->setPixmap(im);
    else if (im.load(QDir(QCoreApplication::applicationDirPath() + "/../Resources/data").absolutePath() +
                     "/wzdownload.png"))
        data->Banner->setPixmap(im);
    if (im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzdownload.png")))
        astrometry->Banner->setPixmap(im);
    else if (im.load(QDir(QCoreApplication::applicationDirPath() + "/../Resources/data").absolutePath() +
                     "/wzdownload.png"))
        astrometry->Banner->setPixmap(im);

    data->dataPath->setText(
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString(), QStandardPaths::LocateDirectory) +
        "kstars");
    slotUpdateDataButtons();
    astrometry->astrometryPath->setText(
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString(), QStandardPaths::LocateDirectory) +
        "Astrometry");
    updateAstrometryButtons();

    connect(data->copyKStarsData, SIGNAL(clicked()), this, SLOT(slotOpenOrCopyKStarsDataDirectory()));
    connect(astrometry->astrometryButton, SIGNAL(clicked()), this, SLOT(slotOpenOrCreateAstrometryFolder()));
    connect(data->installGSC, SIGNAL(clicked()), this, SLOT(slotInstallGSC()));
    connect(this, SIGNAL(accepted()), this, SLOT(slotFinishWizard()));

    gscMonitor = new QProgressIndicator(data);
    data->GSCLayout->addWidget(gscMonitor);
    data->downloadProgress->setValue(0);
    data->downloadProgress->setEnabled(false);
    data->downloadProgress->setVisible(false);

#endif

    //connect signals/slots
    connect(nextB, SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(backB, SIGNAL(clicked()), this, SLOT(slotPrevPage()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    connect(location->CityListBox, SIGNAL(itemSelectionChanged()), this, SLOT(slotChangeCity()));
    connect(location->CityFilter, SIGNAL(textChanged(QString)), this, SLOT(slotFilterCities()));
    connect(location->ProvinceFilter, SIGNAL(textChanged(QString)), this, SLOT(slotFilterCities()));
    connect(location->CountryFilter, SIGNAL(textChanged(QString)), this, SLOT(slotFilterCities()));
    connect(download->DownloadButton, SIGNAL(clicked()), this, SLOT(slotDownload()));

    //Initialize Geographic Location page
    if (KStars::Instance())
        initGeoPage();
}

void KSWizard::setButtonsEnabled()
{
    nextB->setEnabled(wizardStack->currentIndex() < wizardStack->count() - 1);
    backB->setEnabled(wizardStack->currentIndex() > 0);

#ifdef Q_OS_OSX
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(dataDirExists());
    if ((wizardStack->currentWidget() == data) && (!dataDirExists()))
    {
        nextB->setEnabled(false);
    }
    if (wizardStack->currentWidget() == location)
    {
        if (KStars::Instance())
        {
            if (location->LongBox->isReadOnly() == false)
            {
                initGeoPage();
            }
        }
    }
#endif
}

void KSWizard::slotNextPage()
{
    wizardStack->setCurrentIndex(wizardStack->currentIndex() + 1);
    setButtonsEnabled();
}

void KSWizard::slotPrevPage()
{
    wizardStack->setCurrentIndex(wizardStack->currentIndex() - 1);
    setButtonsEnabled();
}

void KSWizard::initGeoPage()
{
    KStarsData *data = KStarsData::Instance();
    location->LongBox->setReadOnly(true);
    location->LatBox->setReadOnly(true);

    //Populate the CityListBox
    //flag the ID of the current City
    foreach (GeoLocation *loc, data->getGeoList())
    {
        location->CityListBox->addItem(loc->fullName());
        filteredCityList.append(loc);
        if (loc->fullName() == data->geo()->fullName())
        {
            Geo = loc;
        }
    }

    //Sort alphabetically
    location->CityListBox->sortItems();
    //preset to current city
    QList<QListWidgetItem*> locations = location->CityListBox->findItems(QString(data->geo()->fullName()), Qt::MatchExactly);
    if (locations.isEmpty() == false)
        location->CityListBox->setCurrentItem(locations[0]);
}

void KSWizard::slotChangeCity()
{
    if (location->CityListBox->currentItem())
    {
        for (auto &city : filteredCityList)
        {
            if (city->fullName() == location->CityListBox->currentItem()->text())
            {
                Geo = city;
                break;
            }
        }
        location->LongBox->showInDegrees(Geo->lng());
        location->LatBox->showInDegrees(Geo->lat());
    }
}

void KSWizard::slotFilterCities()
{
    location->CityListBox->clear();
    //Do NOT delete members of filteredCityList!
    filteredCityList.clear();

    foreach (GeoLocation *loc, KStarsData::Instance()->getGeoList())
    {
        if (hasPrefix(loc->translatedName(), location->CityFilter->text()) &&
            hasPrefix(loc->translatedCountry(), location->CountryFilter->text()) &&
            hasPrefix(loc->translatedProvince(), location->ProvinceFilter->text()))
        {
            location->CityListBox->addItem(loc->fullName());
            filteredCityList.append(loc);
        }
    }
    location->CityListBox->sortItems();

    if (location->CityListBox->count() > 0) // set first item in list as selected
        location->CityListBox->setCurrentItem(location->CityListBox->item(0));
}

void KSWizard::slotDownload()
{
    KNS3::DownloadDialog dlg;
    dlg.exec();
}

void KSWizard::slotFinishWizard()
{
    Options::setRunStartupWizard(false);
    if (KStars::Instance() && geo())
        KStars::Instance()->updateLocationFromWizard(*(geo()));
    delete this;
}

void KSWizard::slotOpenOrCopyKStarsDataDirectory()
{
#ifdef Q_OS_OSX
    QString dataLocation =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    if (dataLocation.isEmpty())
    {
        QDir dataSourceDir = QDir(QCoreApplication::applicationDirPath() + "/../Resources/data").absolutePath();
        if (! dataSourceDir.exists()) //If there is no default data directory in the app bundle
        {
            KMessageBox::sorry(0, i18n("Error! There was no default data directory found in the app bundle!"));
            return;
        }
        QDir writableDir;
        writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation));
        dataLocation =
            QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
        if (dataLocation.isEmpty()) //If there *still* is not a kstars data directory
        {
            KMessageBox::sorry(
                0, i18n("Error! There was a problem creating the data directory ~/Library/Application Support/ !"));
            return;
        }
        KSUtils::copyRecursively(dataSourceDir.absolutePath(), dataLocation);
        //This will update the next, ok, and copy kstars dir buttons.
        slotUpdateDataButtons();

        //This will let the program load after the data folder is copied
        hide();
        setModal(false);
        setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
        show();
    }
    else
    {
        QUrl path = QUrl::fromLocalFile(
            QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory));
        QDesktopServices::openUrl(path);
    }

#endif
}

void KSWizard::slotOpenOrCreateAstrometryFolder()
{
#ifdef Q_OS_OSX
    QString astrometryLocation =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory);
    if (astrometryLocation.isEmpty())
    {
        KSUtils::configureDefaultAstrometry();
        updateAstrometryButtons();
    }
    else
    {
        QUrl path = QUrl::fromLocalFile(
            QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory));
        QDesktopServices::openUrl(path);
    }
#endif
}

void KSWizard::slotInstallGSC()
{
#ifdef Q_OS_OSX
    QString location =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    gscZipPath            = location + "/gsc.zip";
    QProcess *downloadGSC = new QProcess();
    downloadGSC->setWorkingDirectory(location);
    connect(downloadGSC, SIGNAL(finished(int)), this, SLOT(slotExtractGSC()));
    connect(downloadGSC, SIGNAL(finished(int)), this, SLOT(downloadGSC.deleteLater()));
    downloadGSC->start("curl", QStringList() << "-L"
                                             << "-o"
                                             << "gsc.zip"
                                             << "http://www.indilib.org/jdownloads/Mac/gsc.zip");
    data->GSCFeedback->setText("downloading GSC . . .");

    downloadMonitor = new QTimer(this);
    connect(downloadMonitor, SIGNAL(timeout()), this, SLOT(slotCheckDownloadProgress()));
    downloadMonitor->start(1000);
    gscMonitor->startAnimation();
    data->downloadProgress->setVisible(true);
    data->downloadProgress->setEnabled(true);
    data->downloadProgress->setMaximum(240);
    data->downloadProgress->setValue(0);
#endif
}

void KSWizard::slotExtractGSC()
{
#ifdef Q_OS_OSX
    QString location =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    QProcess *gscExtractor = new QProcess();
    connect(gscExtractor, SIGNAL(finished(int)), this, SLOT(slotGSCInstallerFinished()));
    connect(gscExtractor, SIGNAL(finished(int)), this, SLOT(gscExtractor.deleteLater()));
    gscExtractor->setWorkingDirectory(location);
    gscExtractor->start("unzip", QStringList() << "-ao"
                                               << "gsc.zip");
    gscMonitor->startAnimation();
#endif
}

void KSWizard::slotCheckDownloadProgress()
{
#ifdef Q_OS_OSX
    if (QFileInfo(gscZipPath).exists())
        data->downloadProgress->setValue(QFileInfo(gscZipPath).size() / 1048576);
#endif
}

void KSWizard::slotGSCInstallerFinished()
{
#ifdef Q_OS_OSX
    if (downloadMonitor)
    {
        downloadMonitor->stop();
        delete downloadMonitor;
    }
    data->downloadProgress->setEnabled(false);
    data->downloadProgress->setValue(0);
    data->downloadProgress->setVisible(false);
    gscMonitor->stopAnimation();
    slotUpdateDataButtons();
    if (QFile(gscZipPath).exists())
        QFile(gscZipPath).remove();
#endif
}

#ifdef Q_OS_OSX
bool KSWizard::dataDirExists()
{
    QString dataLocation =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    return !dataLocation.isEmpty();
}

bool KSWizard::astrometryDirExists()
{
    QString astrometryLocation =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory);
    return !astrometryLocation.isEmpty();
}

bool KSWizard::pythonExists()
{
    return QFileInfo("/usr/local/bin/python").exists()||
            QFileInfo("/usr/bin/python").exists()||
            QFileInfo(QCoreApplication::applicationDirPath() + "/python/bin/python").exists();
}
bool KSWizard::GSCExists()
{
    QString GSCLocation =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars/gsc", QStandardPaths::LocateDirectory);
    return !GSCLocation.isEmpty();
}

bool KSWizard::pyfitsExists()
{
    return QFileInfo("/usr/local/lib/python2.7/site-packages/pyfits").exists()||
            QFileInfo("/usr/lib/python2.7/site-packages/pyfits").exists()||
            QFileInfo(QCoreApplication::applicationDirPath() + "/python/bin/site-packages/pyfits").exists();

}

bool KSWizard::netpbmExists()
{
    return QFileInfo("/usr/local/bin/jpegtopnm").exists()||
            QFileInfo("/usr/bin/jpegtopnm").exists()||
            QFileInfo(QCoreApplication::applicationDirPath() + "/netpbm/bin/jpegtopnm").exists();
}

void KSWizard::updateAstrometryButtons()
{
    astrometry->astrometryFound->setChecked(astrometryDirExists());
    astrometry->pythonFound->setChecked(pythonExists());
    astrometry->pyfitsFound->setChecked(pyfitsExists());
    astrometry->netpbmFound->setChecked(netpbmExists());
}
#endif

void KSWizard::slotUpdateDataButtons()
{
#ifdef Q_OS_OSX
    data->dataDirFound->setChecked(dataDirExists());
    if (dataDirExists())
    {
        data->copyKStarsData->setText("Open KStars Data Directory");
        data->foundFeedback1->setText("The KStars Data Directory called kstars is located at:");
        data->foundFeedback2->setText("Your data directory was found.  If you have any problems with it, you can "
                                      "always delete this data directory and KStars will give you a new data "
                                      "directory.  You can click this button to open the data directory, just be "
                                      "careful not to delete any important files.");
    }
    else
    {
        data->foundFeedback1->setText("The KStars Data Directory called kstars should be located at:");
        data->foundFeedback2->setText("<html><head/><body><p>Your data directory was not found. You can click the "
                                      "button below to copy a default KStars data directory to the correct location, "
                                      "or if you have a KStars directory already some place else, you can exit KStars "
                                      "and copy it to that location yourself.</p></body></html>");
    }
    bool ifGSCExists = GSCExists();
    data->GSCFound->setChecked(ifGSCExists);
    data->installGSC->setDisabled(ifGSCExists || !dataDirExists());
    if (ifGSCExists)
        data->GSCFeedback->setText("GSC was found on your system.  To use it, just take an image in the CCD simulator. "
                                   "To uninstall, just delete the gsc folder from your data directory above.");
    setButtonsEnabled();
#endif
}

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
#include "ksutils.h"
#include <QFile>
#include <QStackedWidget>
#include <QPixmap>
#include <QLineEdit>
#include <QPushButton>
#include <kns3/downloaddialog.h>
#include <QStandardPaths>
#include <QDesktopServices>

#include "kspaths.h"
#include "kstarsdata.h"
#include "geolocation.h"
#include "widgets/dmsbox.h"

namespace {
    bool hasPrefix(QString str, QString prefix) {
        if( prefix.isEmpty() )
            return true;
        return str.startsWith( prefix, Qt::CaseInsensitive );
    }
}

WizWelcomeUI::WizWelcomeUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

WizLocationUI::WizLocationUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

WizDownloadUI::WizDownloadUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

#ifdef Q_OS_OSX
WizDataUI::WizDataUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}
WizAstrometryUI::WizAstrometryUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}
#endif

KSWizard::KSWizard( QWidget *parent ) :
    QDialog( parent )
{
#ifdef Q_OS_OSX
        setWindowFlags(Qt::Dialog|Qt::WindowStaysOnTopHint);
#endif


    wizardStack = new QStackedWidget( this );
    adjustSize();


    setWindowTitle( i18n("Setup Wizard") );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(wizardStack);
    setLayout(mainLayout);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal);
    nextB = new QPushButton(i18n("&Next >"));
    nextB->setDefault(true);
    backB = new QPushButton(i18n("< &Back"));
    backB->setEnabled(false);

    buttonBox->addButton(backB, QDialogButtonBox::ActionRole);
    buttonBox->addButton(nextB, QDialogButtonBox::ActionRole);

    mainLayout->addWidget(buttonBox);


    welcome = new WizWelcomeUI( wizardStack );
#ifdef Q_OS_OSX
    data = new WizDataUI( wizardStack );
    astrometry = new WizAstrometryUI( wizardStack );
#endif
    location = new WizLocationUI( wizardStack );
    WizDownloadUI* download = new WizDownloadUI( wizardStack );


    wizardStack->addWidget( welcome );
#ifdef Q_OS_OSX
    wizardStack->addWidget( data );
    wizardStack->addWidget( astrometry );
#endif
    wizardStack->addWidget( location );
    wizardStack->addWidget( download );
    wizardStack->setCurrentWidget( welcome );

    //Load images into banner frames.
    QPixmap im;
    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzstars.png")) )
        welcome->Banner->setPixmap( im );
    else if( im.load(QDir(QCoreApplication::applicationDirPath()+"/../Resources/data").absolutePath()+"/wzstars.png"))
        welcome->Banner->setPixmap( im );
    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzgeo.png")) )
        location->Banner->setPixmap( im );
    else if( im.load(QDir(QCoreApplication::applicationDirPath()+"/../Resources/data").absolutePath()+"/wzgeo.png"))
        location->Banner->setPixmap( im );
    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzdownload.png")) )
        download->Banner->setPixmap( im );
    else if( im.load(QDir(QCoreApplication::applicationDirPath()+"/../Resources/data").absolutePath()+"/wzdownload.png"))
        download->Banner->setPixmap( im );

    #ifdef Q_OS_OSX
    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzdownload.png")) )
        data->Banner->setPixmap( im );
    else if( im.load(QDir(QCoreApplication::applicationDirPath()+"/../Resources/data").absolutePath()+"/wzdownload.png"))
        data->Banner->setPixmap( im );
    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzdownload.png")) )
        astrometry->Banner->setPixmap( im );
    else if( im.load(QDir(QCoreApplication::applicationDirPath()+"/../Resources/data").absolutePath()+"/wzdownload.png"))
        astrometry->Banner->setPixmap( im );


    data->dataPath->setText(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString(), QStandardPaths::LocateDirectory)+"kstars");
    slotUpdateDataButtons();
    astrometry->astrometryPath->setText(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString(), QStandardPaths::LocateDirectory)+"Astrometry");
    updateAstrometryButtons();

    connect(data->copyKStarsData, SIGNAL(clicked()), this, SLOT(slotOpenOrCopyKStarsDataDirectory()));
    connect(astrometry->astrometryButton, SIGNAL(clicked()), this, SLOT(slotOpenOrCreateAstrometryFolder()));
    connect(data->installGSC, SIGNAL(clicked()), this, SLOT(slotInstallGSC()));
    connect(astrometry->pipInstall, SIGNAL(clicked()), this, SLOT(slotInstallPip()));
    connect(astrometry->pyfitsInstall, SIGNAL(clicked()), this, SLOT(slotInstallPyfits()));
    connect(astrometry->netpbmInstall, SIGNAL(clicked()), this, SLOT(slotInstallNetpbm()));
    connect(this,SIGNAL(accepted()),this,SLOT(slotFinishWizard()));

    install = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path=env.value("PATH","");
    env.insert("PATH","/usr/local/bin:" + path);
    install->setProcessEnvironment(env);
    install->setProcessChannelMode ( QProcess::MergedChannels );
    connect(install, SIGNAL(readyReadStandardOutput()), this, SLOT(slotUpdateText()));
    connect(install, SIGNAL(readyReadStandardError()), this, SLOT(slotUpdateText()));
    connect(install, SIGNAL(finished(int)), this, SLOT(slotInstallerFinished()));

    gscMonitor = new QProgressIndicator(data);
    data->GSCLayout->addWidget(gscMonitor);
    data->downloadProgress->setValue(0);
    data->downloadProgress->setEnabled(false);
    data->downloadProgress->setVisible(false);
    installMonitor = new QProgressIndicator(astrometry);
    astrometry->installersLayout->addWidget(installMonitor);

    astrometry->programOutput->appendPlainText("Available Paths:");
    astrometry->programOutput->appendPlainText(env.value("PATH","No Paths!"));
    astrometry->programOutput->appendPlainText("🔭 Installer Ready!\n");

    #endif

    //connect signals/slots
    connect(nextB, SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(backB, SIGNAL(clicked()), this, SLOT(slotPrevPage()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    connect( location->CityListBox, SIGNAL( itemSelectionChanged () ), this, SLOT( slotChangeCity() ) );
    connect( location->CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
    connect( location->ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
    connect( location->CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
    connect( download->DownloadButton, SIGNAL( clicked() ), this, SLOT( slotDownload() ) );


    //Initialize Geographic Location page
    if(KStars::Instance())
        initGeoPage();
}

//Do NOT delete members of filteredCityList!  They are not created by KSWizard.
KSWizard::~KSWizard()
{}

void KSWizard::setButtonsEnabled() {
    nextB->setEnabled(wizardStack->currentIndex() < wizardStack->count()-1 );
    backB->setEnabled(wizardStack->currentIndex() > 0 );

     #ifdef Q_OS_OSX
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(dataDirExists());
    if((wizardStack->currentWidget()==data) &&(!dataDirExists())){
        nextB->setEnabled(false);
    }
    if(wizardStack->currentWidget()==location){
            if(KStars::Instance()){
                if(location->LongBox->isReadOnly()==false){
                    initGeoPage();

                }
            }
    }
       #endif
}

void KSWizard::slotNextPage() {
    wizardStack->setCurrentIndex( wizardStack->currentIndex() + 1 );
    setButtonsEnabled();
}

void KSWizard::slotPrevPage() {
    wizardStack->setCurrentIndex( wizardStack->currentIndex() - 1 );
    setButtonsEnabled();
}

void KSWizard::initGeoPage() {
    KStarsData* data = KStarsData::Instance();
    location->LongBox->setReadOnly( true );
    location->LatBox->setReadOnly( true );

    //Populate the CityListBox
    //flag the ID of the current City
    foreach ( GeoLocation *loc, data->getGeoList() ) {
        location->CityListBox->addItem( loc->fullName() );
        filteredCityList.append( loc );
        if ( loc->fullName() == data->geo()->fullName() ) {
            Geo = loc;
        }
    }

    //Sort alphabetically
    location->CityListBox->sortItems();
    //preset to current city
    location->CityListBox->setCurrentItem(location->CityListBox->findItems(QString(data->geo()->fullName()),
									   Qt::MatchExactly).at(0));
}

void KSWizard::slotChangeCity() {
    if ( location->CityListBox->currentItem() ) {
        for ( int i=0; i < filteredCityList.size(); ++i ) {
            if ( filteredCityList[i]->fullName() == location->CityListBox->currentItem()->text() ) {
                Geo = filteredCityList[i];
                break;
            }
        }
        location->LongBox->showInDegrees( Geo->lng() );
        location->LatBox->showInDegrees( Geo->lat() );
    }
}

void KSWizard::slotFilterCities() {
    location->CityListBox->clear();
    //Do NOT delete members of filteredCityList!
    filteredCityList.clear();

    foreach ( GeoLocation *loc, KStarsData::Instance()->getGeoList() ) {
        if( hasPrefix( loc->translatedName(),     location->CityFilter->text()     ) &&
            hasPrefix( loc->translatedCountry(),  location->CountryFilter->text() ) &&
            hasPrefix( loc->translatedProvince(), location->ProvinceFilter->text()  )
            )
        {
            location->CityListBox->addItem( loc->fullName() );
            filteredCityList.append( loc );
        }
    }
    location->CityListBox->sortItems();

    if ( location->CityListBox->count() > 0 )  // set first item in list as selected
        location->CityListBox->setCurrentItem( location->CityListBox->item(0) );
}

void KSWizard::slotDownload() {
    KNS3::DownloadDialog dlg;
    dlg.exec();
}


void KSWizard::slotFinishWizard(){
    if(KStars::Instance())
        KStars::Instance()->updateLocationFromWizard(*(geo()));
    delete this;
}


void KSWizard::slotOpenOrCopyKStarsDataDirectory(){
    #ifdef Q_OS_OSX
    QString dataLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    if(dataLocation.isEmpty()) {
        QString dataSourceLocation=QDir(QCoreApplication::applicationDirPath()+"/../Resources/data").absolutePath();
        if(dataSourceLocation.isEmpty()){ //If there is no default data directory in the app bundle
            KMessageBox::sorry(0, i18n("Error! There was no default data directory found in the app bundle!"));
            return;
        }
        QDir writableDir;
        writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation));
        dataLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
        if(dataLocation.isEmpty()){  //If there *still* is not a kstars data directory
            KMessageBox::sorry(0, i18n("Error! There was a problem creating the data directory ~/Library/Application Support/ !"));
            return;
        }
        KSUtils::copyRecursively(dataSourceLocation, dataLocation);
        //This will update the next, ok, and copy kstars dir buttons.
        slotUpdateDataButtons();

        //This will let the program load after the data folder is copied
        hide();
        setModal(false);
        setWindowFlags(Qt::Dialog|Qt::WindowStaysOnTopHint);
        show();
    } else{
        QUrl path = QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory));
        QDesktopServices::openUrl(path);
    }

#endif
}

void KSWizard::slotOpenOrCreateAstrometryFolder()
{
#ifdef Q_OS_OSX
    QString astrometryLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory);
    if(astrometryLocation.isEmpty()) {
        KSUtils::configureDefaultAstrometry();
        updateAstrometryButtons();
    } else{
        QUrl path = QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory));
        QDesktopServices::openUrl(path);
    }
#endif
}

void KSWizard::slotInstallGSC(){
    #ifdef Q_OS_OSX
    QString location=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    gscZipPath=location+"/gsc.zip";
    QProcess *downloadGSC=new QProcess();
    downloadGSC->setWorkingDirectory(location);
    connect(downloadGSC, SIGNAL(finished(int)), this, SLOT(slotExtractGSC()));
    connect(downloadGSC, SIGNAL(finished(int)), this, SLOT(downloadGSC.deleteLater()));
    downloadGSC->start("wget", QStringList() << "-O" << "gsc.zip" << "http://mactelescope.com/gsc.zip" );
    data->GSCFeedback->setText("downloading GSC . . .");

    downloadMonitor=new QTimer(this);
    connect(downloadMonitor, SIGNAL(timeout()), this, SLOT(slotCheckDownloadProgress()));
    downloadMonitor->start(1000);
    gscMonitor->startAnimation();
    data->downloadProgress->setVisible(true);
    data->downloadProgress->setEnabled(true);
    data->downloadProgress->setMaximum(240);
    data->downloadProgress->setValue(0);
   #endif
}

void KSWizard::slotExtractGSC(){
#ifdef Q_OS_OSX
    QString location=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    QProcess *gscExtractor=new QProcess();
    connect(gscExtractor, SIGNAL(finished(int)), this, SLOT(slotGSCInstallerFinished()));
    connect(gscExtractor, SIGNAL(finished(int)), this, SLOT(gscExtractor.deleteLater()));
    gscExtractor->setWorkingDirectory(location);
    gscExtractor->start("unzip", QStringList() << "-ao" << "gsc.zip");
    gscMonitor->startAnimation();
 #endif
}

void KSWizard::slotCheckDownloadProgress(){
#ifdef Q_OS_OSX
    if(QFileInfo(gscZipPath).exists());
        data->downloadProgress->setValue(QFileInfo(gscZipPath).size()/1048576);
#endif
}

void KSWizard::slotGSCInstallerFinished(){
#ifdef Q_OS_OSX
    if(downloadMonitor){
        downloadMonitor->stop();
        delete downloadMonitor;
    }
    data->downloadProgress->setEnabled(false);
    data->downloadProgress->setValue(0);
    data->downloadProgress->setVisible(false);
    gscMonitor->stopAnimation();
    slotUpdateDataButtons();
    if(QFile(gscZipPath).exists())
        QFile(gscZipPath).remove();
#endif
}



void KSWizard::slotInstallPip()
{
#ifdef Q_OS_OSX
    astrometry->programOutput->appendPlainText("🔭 INSTALLING PIP:\n");
    if(!pythonExists()){
        if(brewExists()){
            astrometry->programOutput->appendPlainText("/usr/local/bin/brew install python\n");
             install->start("/usr/local/bin/brew" , QStringList() << "install" << "python");
             installMonitor->startAnimation();
         } else
            KMessageBox::sorry(0,"Python is not installed.  Please install python to /usr/local/bin first, or <a href=http://brew.sh>install homebrew</a> and click again.", "", KMessageBox::AllowLink);
    }else{
        if(!pipExists()){
            if(brewExists()){
                astrometry->programOutput->appendPlainText("/usr/local/bin/brew install python\n");
                 install->start("/usr/local/bin/brew" , QStringList() << "install" << "python");
                 installMonitor->startAnimation();
             } else if(QProcess::execute("type /usr/local/bin/easy_install")==QProcess::NormalExit){
                astrometry->programOutput->appendPlainText("/usr/local/bin/easy_install pip\n");
                install->start("/usr/local/bin/easy_install" , QStringList() << "pip");
                installMonitor->startAnimation();
            } else{
                KMessageBox::sorry(0,"pip failed to install with homebrew and easy_install.  Try <a href=http://brew.sh>installing homebrew</a> and clicking again.", "", KMessageBox::AllowLink);
            }

        }
    }
#endif
}

void KSWizard::slotInstallPyfits()
{
#ifdef Q_OS_OSX
    astrometry->programOutput->appendPlainText("🔭 INSTALLING PYFITS:\n");

    if(!pythonExists()){
        KMessageBox::sorry(0,"/usr/local/bin/python is not installed.  Please install python first.");
    }else if(!pipExists()){
        KMessageBox::sorry(0,"/usr/local/bin/pip is not installed.  Please install pip first.");
    } else{
        astrometry->programOutput->appendPlainText("/usr/local/bin/pip install pyfits\n");
        install->start("/usr/local/bin/pip" , QStringList() << "install" << "pyfits");
        installMonitor->startAnimation();
    }
#endif
}

void KSWizard::slotInstallNetpbm()
{
#ifdef Q_OS_OSX
    astrometry->programOutput->appendPlainText("🔭 INSTALLING NETPBM:\n");

   if(brewExists()){
        install->start("/usr/local/bin/brew" , QStringList() << "install" << "netpbm");
        installMonitor->startAnimation();
    } else{
        //install->start("ruby", QStringList() << "-e" << "'$(curl -fsSL raw.githubusercontent.com/Homebrew/install/master/install)'" << "<" << "/dev/null" << "2>" << "/dev/null");
        //install->waitForFinished();
        //install->start("/usr/local/bin/brew" , QStringList() << "install" << "netpbm");
       KMessageBox::sorry(0,"homebrew is not installed.  Try <a href=http://brew.sh>installing homebrew</a> and clicking again.", "", KMessageBox::AllowLink);
    }
#endif
}


void KSWizard::slotUpdateText()
{
#ifdef Q_OS_OSX
   QByteArray data = install->readAllStandardOutput();
   astrometry->programOutput->appendPlainText(QString(data));
#endif
}

#ifdef Q_OS_OSX
bool KSWizard::dataDirExists(){
    QString dataLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    return !dataLocation.isEmpty();
}

bool KSWizard::astrometryDirExists(){
    QString astrometryLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory);
    return !astrometryLocation.isEmpty();
}

bool KSWizard::pythonExists(){
    return QProcess::execute("type /usr/local/bin/python")==QProcess::NormalExit;
}
bool KSWizard::GSCExists(){
    QString GSCLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars/gsc", QStandardPaths::LocateDirectory);
    return !GSCLocation.isEmpty();
}

bool KSWizard::pipExists(){
    return QProcess::execute("type /usr/local/bin/pip")==QProcess::NormalExit;
}

bool KSWizard::pyfitsExists(){
    QProcess testPyfits;
    testPyfits.start("/usr/local/bin/pip list");
    testPyfits.waitForFinished();
    QString listPip(testPyfits.readAllStandardOutput());
    qDebug()<<listPip;
    return listPip.contains("pyfits", Qt::CaseInsensitive);
}

bool KSWizard::netpbmExists(){
    return QProcess::execute("type /usr/local/bin/jpegtopnm")==QProcess::NormalExit;
}

bool KSWizard::brewExists(){
    return QProcess::execute("type /usr/local/bin/brew")==QProcess::NormalExit;
}

void KSWizard::updateAstrometryButtons(){
     astrometry->astrometryFound->setChecked(astrometryDirExists());
     if(astrometryDirExists()) {
         astrometry->astrometryButton->setText("Open Folder");
         astrometry->astrometryFeedback->setText("<html><head/><body><p>To plate solve, you need to put index files in the following folder. See the documentation at this link: <a href=http://astrometry.net/doc/readme.html>Astrometry Readme</a> for details on how to get files.</p></body></html>");
    } else{
        astrometry->astrometryButton->setText("Create Folder");
        astrometry->astrometryFeedback->setText("KStars needs to configure the astrometry.cfg file and create a folder for the index files. Please click the button below to complete this task.");
    }

    if(!pythonExists()){
        astrometry->pipFound->setChecked(false);
        astrometry->pipInstall->setDisabled(false);
        astrometry->pyfitsFound->setChecked(false);
        astrometry->pyfitsInstall->setDisabled(false);
    }
    else{
        bool ifPipExists=pipExists();
        astrometry->pipFound->setChecked(ifPipExists);
        astrometry->pipInstall->setDisabled(ifPipExists);
        if(ifPipExists){
            bool ifPyfitsExists=pyfitsExists();
            astrometry->pyfitsFound->setChecked(ifPyfitsExists);
            astrometry->pyfitsInstall->setDisabled(ifPyfitsExists);
        }
    }

    //Testing a random netpbm command.
    bool ifNetpbmExists=netpbmExists();
    astrometry->netpbmFound->setChecked(ifNetpbmExists);
    astrometry->netpbmInstall->setDisabled(ifNetpbmExists);
}
#endif

void KSWizard::slotUpdateDataButtons(){
#ifdef Q_OS_OSX
    data->dataDirFound->setChecked(dataDirExists());
    if(dataDirExists()) {
        data->copyKStarsData->setText("Open KStars Data Directory");
        data->foundFeedback1->setText("The KStars Data Directory called kstars is located at:");
        data->foundFeedback2->setText("Your data directory was found.  If you have any problems with it, you can always delete this data directory and KStars will give you a new data directory.  You can click this button to open the data directory, just be careful not to delete any important files.");
     } else{
        data->foundFeedback1->setText("The KStars Data Directory called kstars should be located at:");
        data->foundFeedback2->setText("<html><head/><body><p>Your data directory was not found. You can click the button below to copy a default KStars data directory to the correct location, or if you have a KStars directory already some place else, you can exit KStars and copy it to that location yourself.</p></body></html>");
    }
    bool ifGSCExists=GSCExists();
    data->GSCFound->setChecked(ifGSCExists);
    data->installGSC->setDisabled(ifGSCExists||!dataDirExists());
    if(ifGSCExists)
        data->GSCFeedback->setText("GSC was found on your system.  To use it, just take an image in the CCD simulator. To uninstall, just delete the gsc folder from your data directory above.");
    setButtonsEnabled();
#endif
}

void KSWizard::slotInstallerFinished(){
#ifdef Q_OS_OSX
    astrometry->programOutput->appendPlainText("🔭 Installer Finished!\n");
    updateAstrometryButtons();
    installMonitor->stopAnimation();
#endif
}


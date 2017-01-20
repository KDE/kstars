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
#include <QProcess>

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
        setWindowFlags(Qt::Tool| Qt::WindowStaysOnTopHint);
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


    WizWelcomeUI* welcome = new WizWelcomeUI( wizardStack );
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
    updateDataButtons();
    astrometry->astrometryPath->setText(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString(), QStandardPaths::LocateDirectory)+"Astrometry");
    updateAstrometryButtons();

    connect(data->copyKStarsData, SIGNAL(clicked()), this, SLOT(copyKStarsDataDirectory()));
    connect(astrometry->astrometryButton, SIGNAL(clicked()), this, SLOT(slotOpenOrCreateAstrometryFolder()));
    connect(astrometry->pipInstall, SIGNAL(clicked()), this, SLOT(slotInstallPip()));
    connect(astrometry->pyfitsInstall, SIGNAL(clicked()), this, SLOT(slotInstallPyfits()));
    connect(astrometry->netpbmInstall, SIGNAL(clicked()), this, SLOT(slotInstallNetpbm()));
    connect(this,SIGNAL(accepted()),this,SLOT(finishWizard()));

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
    if((wizardStack->currentWidget()==data) &&(!dataDirExists())){
        nextB->setEnabled(false);
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
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


void KSWizard::finishWizard(){
    KStars::Instance()->updateLocationFromWizard(*(geo()));
    delete this;
}

#ifdef Q_OS_OSX
void KSWizard::copyKStarsDataDirectory(){
    QString dataSourceLocation=QDir(QCoreApplication::applicationDirPath()+"/../Resources/data").absolutePath();
    if(dataSourceLocation.isEmpty()){ //If there is no default data directory in the app bundle
        KMessageBox::sorry(0, i18n("Error! There was no default data directory found in the app bundle!"));
        return;
    }
    QDir writableDir;
    writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation));
    QString dataLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    if(dataLocation.isEmpty()){  //If there *still* is not a kstars data directory
        KMessageBox::sorry(0, i18n("Error! There was a problem creating the data directory ~/Library/Application Support/ !"));
        return;
    }
    KSUtils::copyRecursively(dataSourceLocation, dataLocation);
    updateDataButtons();

    //This will let the program load after the data folder is copied
    hide();
    setModal(false);
    show();

    //Enable the buttons so you can go to the next page or click ok.
    nextB->setEnabled(true);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);


}





void KSWizard::slotOpenOrCreateAstrometryFolder()
{
    QString astrometryLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory);
    if(astrometryLocation.isEmpty()) {
        KSUtils::configureDefaultAstrometry();
        updateAstrometryButtons();
    } else{
        QUrl path = QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory));
        QDesktopServices::openUrl(path);
    }
}

void KSWizard::slotInstallPip()
{
    if(!pythonExists()){
        KMessageBox::sorry(0,"Python is not installed.  Please install python to /usr/local/bin first.");
    }else{
        if(!pipExists()){
            QProcess* install = new QProcess();
            connect(install, SIGNAL(finished(int)), this, SLOT(updateAstrometryButtons()));
            install->start("easy_install" , QStringList() << "pip");
        }
    }
}

void KSWizard::slotInstallPyfits()
{
    if(!pythonExists()){
        KMessageBox::sorry(0,"Python is not installed.  Please install python to /usr/local/bin first.");
    }else if(!pipExists()){
        KMessageBox::sorry(0,"Pip is not installed.  Please install pip first.");
    } else{
        QProcess* install = new QProcess();
        connect(install, SIGNAL(finished(int)), this, SLOT(updateAstrometryButtons()));
        install->start("pip" , QStringList() << "install" << "pyfits");
    }

}

void KSWizard::slotInstallNetpbm()
{
    QProcess* install = new QProcess();
    connect(install, SIGNAL(finished(int)), this, SLOT(updateAstrometryButtons()));

    if(brewExists()){
        install->start("brew" , QStringList() << "install" << "netpbm");
    } else{
        install->start("ruby", QStringList() << "-e" << "'$(curl -fsSL raw.githubusercontent.com/Homebrew/install/master/install)'" << "<" << "/dev/null" << "2>" << "/dev/null");
        install->waitForFinished();
        install->start("brew" , QStringList() << "install" << "netpbm");
    }
}

bool KSWizard::dataDirExists(){
    QString dataLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    return !dataLocation.isEmpty();
}

bool KSWizard::astrometryDirExists(){
    QString astrometryLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory);
    return !astrometryLocation.isEmpty();
}

bool KSWizard::pythonExists(){
    return QProcess::execute("type python")==QProcess::NormalExit;
}

bool KSWizard::pipExists(){
    return QProcess::execute("type pip")==QProcess::NormalExit;
}

bool KSWizard::pyfitsExists(){
    QProcess testPyfits;
    testPyfits.start("pip list");
    testPyfits.waitForFinished();
    QString listPip(testPyfits.readAllStandardOutput());
    qDebug()<<listPip;
    return listPip.contains("pyfits", Qt::CaseInsensitive);
}

bool KSWizard::netpbmExists(){
    return QProcess::execute("type jpegtopnm")==QProcess::NormalExit;
}

bool KSWizard::brewExists(){
    return QProcess::execute("type brew")==QProcess::NormalExit;
}

void KSWizard::updateDataButtons(){
    if(dataDirExists()) {
        data->dataDirFound->setChecked(true);
        data->copyKStarsData->setDisabled(true);
        data->foundFeedback1->setText("The KStars Data Directory called kstars is located at:");
        data->foundFeedback2->setText("Your data directory was found.  If you have any problems with it, you can always delete this data directory and KStars will give you a new data directory.");
     } else{
        data->dataDirFound->setChecked(false);
        data->copyKStarsData->setDisabled(false);
        data->foundFeedback1->setText("The KStars Data Directory called kstars should be located at:");
        data->foundFeedback2->setText("Your data directory was not found. You can click the button below to copy a default KStars data directory to the correct location, or if you have a KStars directory already some place else, you can exit KStars and copy it to that location yourself.");
    }
}

void KSWizard::updateAstrometryButtons(){
     if(astrometryDirExists()) {
         astrometry->astrometryFound->setChecked(true);
         astrometry->astrometryButton->setText("Open Folder");
         astrometry->astrometryFeedback->setText("");
    } else{
        astrometry->astrometryFound->setChecked(false);
        astrometry->astrometryButton->setText("Create Folder");
        astrometry->astrometryFeedback->setText("Note: Currently your Astrometry Folder does not exist, please click 'Create Folder' to make it.");
    }

    if(!pythonExists()){
        astrometry->pipFound->setChecked(false);
        astrometry->pipInstall->setDisabled(false);
        astrometry->pyfitsFound->setChecked(false);
        astrometry->pyfitsInstall->setDisabled(false);
    }
    else{
        if(pipExists()){
            astrometry->pipFound->setChecked(true);
            astrometry->pipInstall->setDisabled(true);
            if(pyfitsExists()){
                astrometry->pyfitsFound->setChecked(true);
                astrometry->pyfitsInstall->setDisabled(true);
            }else{
                astrometry->pyfitsFound->setChecked(false);
                astrometry->pyfitsInstall->setDisabled(false);
            }
        }else{
            astrometry->pipFound->setChecked(false);
            astrometry->pipInstall->setDisabled(false);
            astrometry->pyfitsFound->setChecked(false);
            astrometry->pyfitsInstall->setDisabled(false);
        }
    }

    //Testing a random netpbm command.
    if(netpbmExists()){
        astrometry->netpbmFound->setChecked(true);
        astrometry->netpbmInstall->setDisabled(true);
    }else{
        astrometry->netpbmFound->setChecked(false);
        astrometry->netpbmInstall->setDisabled(false);
    }
}
#endif

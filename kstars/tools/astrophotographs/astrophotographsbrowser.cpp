#include "astrophotographsbrowser.h"

#include "QDeclarativeView"
#include <QDebug>
#include <QDir>

#include <kio/copyjob.h>
#include <kio/netaccess.h>
#include <kio/job.h>
#include <KJob>
#include <KMessageBox>
#include <KFileDialog>

#include <QProgressDialog>
#include <QGraphicsObject>
#include <QDesktopWidget>
#include <QNetworkAccessManager>
#include <QListWidgetItem>
#include <QVariant>

#include "kstandarddirs.h"
#include "kdeclarative.h"
#include "imageviewer.h"

#include "searchresultitem.h"
#include "dialogs/imageviewerdialog.h"

AstrophotographsBrowser::AstrophotographsBrowser(QWidget *parent) : QWidget(parent),
    m_Offset(0), m_ImageCount(0), currentIndex(-1), m_ImageType(0), m_Lock(false), m_PreviousQuery("")
{
    m_BaseView = new QDeclarativeView();

    ///To use i18n() instead of qsTr() in qml/astrophotographsbrowser.qml for translation
    KDeclarative kd;
    kd.setDeclarativeEngine(m_BaseView->engine());
    kd.initialize();
    kd.setupBindings();

    m_NetworkManager = new QNetworkAccessManager(this);
    m_AstrobinApi = new AstroBinApiJson(m_NetworkManager, this);

    m_Ctxt = m_BaseView->rootContext();

    m_BaseView->setSource(KStandardDirs::locate("appdata","tools/astrophotographs/qml/astrophotographsbrowser.qml"));
    m_BaseObj = dynamic_cast<QObject *>(m_BaseView->rootObject());

    connect(m_AstrobinApi, SIGNAL(searchFinished(bool)), this, SLOT(slotAstrobinSearchCompleted(bool)));

    m_SearchContainerObj = m_BaseObj->findChild<QObject *>("searchContainerObj");
    connect(m_SearchContainerObj, SIGNAL(searchBarClicked()), this, SLOT(slotSearchBarClicked()));
    connect(m_SearchContainerObj, SIGNAL(searchButtonClicked()), this, SLOT(slotAstrobinSearch()));

    m_SearchBarObj = m_BaseObj->findChild<QObject *>("searchBarObj");
    m_ResultViewObj = m_BaseObj->findChild<QObject *>("resultViewObj");
    m_TitleOfAstroPhotographObj = m_BaseObj->findChild<QObject *>("titleOfAstroPhotographObj");
    m_AstroPhotographImageObj = m_BaseObj->findChild<QObject *>("astroPhotographImageObj");

    m_ResultListViewObj = m_BaseObj->findChild<QObject *>("resultListViewObj");
    connect(m_ResultListViewObj, SIGNAL(resultListItemClicked(int)), this, SLOT(onResultListItemClicked(int)));

    m_ButtonContainerObj = m_BaseObj->findChild<QObject *>("buttonContainerObj");
    connect(m_ButtonContainerObj, SIGNAL(saveButtonClicked()), this, SLOT(onSaveButtonClicked()));
    connect(m_ButtonContainerObj, SIGNAL(editButtonClicked()), this, SLOT(onEditButtonClicked()));

    m_BaseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    m_AstrobinApi->searchImageOfTheDay();
    m_BaseView->show();

}

AstrophotographsBrowser::~AstrophotographsBrowser(){
    delete m_BaseObj;
    delete m_BaseView;

    qDeleteAll(m_Jobs);
    qDeleteAll(m_ResultItemList);
    qDeleteAll(m_DetailItemList);
}

void AstrophotographsBrowser::slotSearchBarClicked(){
    m_SearchBarObj->setProperty("text", "");
}

void AstrophotographsBrowser::slotAstrobinSearch(){
    m_ImageType = 0;
    QVariant searchQuery = m_SearchBarObj->property("text");

    clearImagesList();

    if(m_PreviousQuery == searchQuery.toString()){
        m_Offset += 20;
        m_AstrobinApi->setOffset(m_Offset);
    }else{
        m_Offset = 0;
        m_AstrobinApi->setOffset(0);
        m_PreviousQuery = searchQuery.toString();
    }

    m_AstrobinApi->searchTitleContaining(searchQuery.toString());
}

void AstrophotographsBrowser::slotAstrobinSearchCompleted(bool ok)
{
    if(!ok) {
        KMessageBox::error(this, i18n("AstroBin.com search error: ", i18n("AstroBin.com search error")));
        return;
    }

    int progress = 0;
    // Show a progress dialog while processing
    QProgressDialog progressDlg( i18n( "Downloading Astrophotographs from internet... " ) , i18n( "Hide" ), 0, 0, this);
    progressDlg.setValue( progress );
    progressDlg.show();

    AstroBinSearchResult result = m_AstrobinApi->getResult();
    m_ImageCount = 0;
    m_Jobs.clear();
    m_Lock = true;
    foreach(AstroBinImage image, result) {
        KIO::StoredTransferJob *job = KIO::storedGet(image.thumbImageUrl(), KIO::Reload, KIO::HideProgressInfo);
        job->setUiDelegate(0);
        m_Jobs.append(job);
        connect(job, SIGNAL(result(KJob*)), SLOT(slotJobResult(KJob*)));
        m_AstroBinImageList.append(image);

        //FIXME: We need to wait till slotJobResult is not executed
        //So that images get downloaded and save in a order in which request for them has been sent.
        //using delay() function is worst possible solution, we need something like while(lock)
        //where lock is set close in the begining of this loop and set open in slotJobResult but plain
        //while loop is not working in qt.
        delay();
        progress += 5;
        progressDlg.setValue( progress );
    }
    m_Lock = false;
}

void AstrophotographsBrowser::onResultListItemClicked(int index){
    if(m_Lock || m_AstroBinImageList.count() <= index){
        //KMessageBox::error(this, i18n("Wait till search is complete", i18n("Could not load detail")));
        return;
    }

    currentIndex = index;
    AstroBinImage image = m_AstroBinImageList.at(currentIndex);

    m_ResultViewObj->setProperty("flipped", "true");
    m_TitleOfAstroPhotographObj->setProperty("text", image.title());

    //To remove previosly shown image source for image need to be set empty
    m_AstroPhotographImageObj->setProperty("source", "");

    m_DetailItemList.clear();

    if(m_AstrobinApi->isImageOfTheDay())
    {
        m_DetailItemList.append(new DetailItem("Every night, an algorithm looks at the images uploaded during the previous 7 days. The image with most Likes in that period is selected as Image of the Day. "));
    }

    if( !image.user().isEmpty() )
    {
        m_DetailItemList.append(new DetailItem("User : " + image.user()));
    }

    if( !image.dateUploaded().toString().isEmpty() )
    {
        m_DetailItemList.append(new DetailItem("Date : " + image.dateUploaded().toString()));
    }


    if( std::isfinite(image.decCenterDms().radians()) )
    {
        m_DetailItemList.append(new DetailItem("DEC : " + image.decCenterDms().toDMSString()));
        m_DetailItemList.append(new DetailItem("RA : " + image.raCenterHms().toHMSString()));
    }

    if( !image.description().isEmpty() )
    {
        m_DetailItemList.append(new DetailItem("Description : " + image.description()));
    }

    QString cameraList = "";
    foreach (QString camera, image.imagingCameras()) {
        cameraList += camera + ", ";
    }

    if( !cameraList.isEmpty() )
    {
        m_DetailItemList.append(new DetailItem("Cameras : " + cameraList.remove(cameraList.lastIndexOf(','),1)));
    }


    QString telescopeList = "";
    foreach (QString telescope, image.imagingTelescopes()) {
        telescopeList += telescope + ", ";
    }

    if( !telescopeList.isEmpty() )
    {
        m_DetailItemList.append(new DetailItem("Telescopes : " + telescopeList.remove(telescopeList.lastIndexOf(','),1)));
    }


    QString subjectList = "";
    foreach (QString subject, image.subjects()) {
        subjectList += subject + ", ";
    }

    if( !subjectList.isEmpty() )
    {
        m_DetailItemList.append(new DetailItem("Subjects : " + subjectList.remove(subjectList.lastIndexOf(','),1)));
    }

    if( image.voteCount() > 0 && std::isfinite(image.voteCount()))
    {
        m_DetailItemList.append(new DetailItem("Vote Count : " + QString::number(image.voteCount())));
    }

    m_Ctxt->setContextProperty( "detailModel", QVariant::fromValue(m_DetailItemList) );

    m_ImageType = 1;
    m_Lock = true;
    KIO::StoredTransferJob *job = KIO::storedGet(image.regularImageUrl(), KIO::NoReload, KIO::HideProgressInfo);
    job->setUiDelegate(0);
    m_Jobs.append(job);
    connect(job, SIGNAL(result(KJob*)), SLOT(slotJobResult(KJob*)));
    m_Lock = false;
}

void AstrophotographsBrowser::onSaveButtonClicked(){

    if(m_Lock)
    {
        return;
    }

    m_ImageType = 2;
    AstroBinImage image = m_AstroBinImageList.at(currentIndex);

    KIO::StoredTransferJob *job = KIO::storedGet(image.hdUrl(), KIO::NoReload, KIO::HideProgressInfo);
    job->setUiDelegate(0);
    m_Jobs.append(job);
    connect(job, SIGNAL(result(KJob*)), SLOT(slotJobResult(KJob*)));
}

void AstrophotographsBrowser::onEditButtonClicked(){
    ImageViewerDialog* ivd = 0;
    if( QFile::exists( KStandardDirs().locateLocal("data", "kstars/astrobinImages/regular/lastImage.png") ) )
    {
        ivd = new ImageViewerDialog( KStandardDirs().locateLocal("data", "kstars/astrobinImages/regular/lastImage.png") );
    }

    if( ivd )
        ivd->show();
}

void AstrophotographsBrowser::slotJobResult(KJob *job)
{
    KIO::StoredTransferJob *storedTranferJob = (KIO::StoredTransferJob*)job;
    m_Jobs.removeOne(storedTranferJob);

    //If there was a problem, just return silently without adding image to list.
    if(job->error()) {
        job->kill();
        return;
    }

    QPixmap *pm = new QPixmap();
    pm->loadFromData(storedTranferJob->data());
    QString path = scaleAndAddPixmap(pm);

    if(m_ImageCount < m_AstroBinImageList.count() && m_ImageType == AstrophotographsBrowser::Thumbnail)
    {
        m_ResultItemList.append(new SearchResultItem( path, m_AstroBinImageList.at(m_ImageCount).title().left(40), m_AstroBinImageList.at(m_ImageCount).dateUploaded().toString() ));
        m_Ctxt->setContextProperty( "resultModel", QVariant::fromValue(m_ResultItemList) );
        m_ImageCount += 1;
    }
    else if(m_ImageType == AstrophotographsBrowser::Detail)
    {
        QObject *astroPhotographImageObj = m_BaseObj->findChild<QObject *>("astroPhotographImageObj");
        astroPhotographImageObj->setProperty("source", path );
    }

    delete pm;

}

QString AstrophotographsBrowser::scaleAndAddPixmap(QPixmap *pixmap){
    uint w = pixmap->width();
    uint h = pixmap->height();
    uint pad = 0;
    uint hDesk = QApplication::desktop()->availableGeometry().height() - pad;

    if(h > hDesk) {
        *pixmap = pixmap->scaled(w * hDesk / h, hDesk, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    QString path;
    if(m_ImageType == AstrophotographsBrowser::Thumbnail)
    {
        path = KStandardDirs().locateLocal("data", "kstars/astrobinImages/thumb/image_"+ QString::number(m_ImageCount) + ".png");
    }
    else if(m_ImageType == AstrophotographsBrowser::Detail)
    {
        path = KStandardDirs().locateLocal("data", "kstars/astrobinImages/regular/lastImage.png");
    }
    else if(m_ImageType == AstrophotographsBrowser::HD)
    {
        KUrl fileURL = KFileDialog::getSaveUrl( QDir::homePath(), "*.png | *.jpg" );
        if ( fileURL.isValid() ) {
            path = fileURL.path();
        }else{
            KMessageBox::error(this, i18n("File path is not selected", i18n("Could not save image")));
        }
    }

    pixmap->save( path );

    return path;
}

void AstrophotographsBrowser::clearImagesList(){
    killAllRunningJobs();
    m_AstroBinImageList.clear();
    m_ResultItemList.clear();
}

void AstrophotographsBrowser::killAllRunningJobs(){
    foreach(KIO::StoredTransferJob *job, m_Jobs) {
        job->kill();
    }

    m_Jobs.clear();
}

void AstrophotographsBrowser::delay()
{
    QTime dieTime= QTime::currentTime().addMSecs(1000);
    while( QTime::currentTime() < dieTime )
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

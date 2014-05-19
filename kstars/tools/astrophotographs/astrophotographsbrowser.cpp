#include "astrophotographsbrowser.h"

#include "QDeclarativeView"
#include <QDebug>

#include <kio/copyjob.h>
#include <kio/netaccess.h>
#include <kio/job.h>
#include <KJob>
#include <KMessageBox>
#include <KFileDialog>

#include <QGraphicsObject>
#include <QDesktopWidget>
#include <QNetworkAccessManager>
#include <QListWidgetItem>
#include <QVariant>

#include "kstandarddirs.h"
#include "kdeclarative.h"

#include "searchresultitem.h"

AstrophotographsBrowser::AstrophotographsBrowser(QWidget *parent) : QWidget(parent),
    m_Offset(0), m_ImageCount(0), m_PreviousQuery("")
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

    m_BaseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    m_BaseView->show();

}

AstrophotographsBrowser::~AstrophotographsBrowser(){
    delete m_BaseObj;
    delete m_BaseView;

    qDeleteAll(m_AstrobinImages);
    qDeleteAll(m_Jobs);
    qDeleteAll(m_ResultItemList);
    qDeleteAll(m_AstrobinImages);
}

void AstrophotographsBrowser::slotSearchBarClicked(){
    m_SearchBarObj->setProperty("text", "");
}

void AstrophotographsBrowser::slotAstrobinSearch(){
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

    AstroBinSearchResult result = m_AstrobinApi->getResult();
    m_ImageCount = 0;
    foreach(AstroBinImage image, result) {
        KIO::StoredTransferJob *job = KIO::storedGet(image.thumbImageUrl(), KIO::NoReload, KIO::HideProgressInfo);
        job->setUiDelegate(0);
        m_Jobs.append(job);
        connect(job, SIGNAL(result(KJob*)), SLOT(slotJobResult(KJob*)));
        m_AstroBinImageList.append(image);
    }
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
    scaleAndAddPixmap(pm);

    if(m_ImageCount <= m_AstroBinImageList.count())
    {
        QString path = KStandardDirs().locateLocal("data", "kstars/astrobinImages/thumb/image_" + QString::number(m_ImageCount) + ".png");
        m_ResultItemList.append(new SearchResultItem( path, m_AstroBinImageList.at(m_ImageCount-1).title(), m_AstroBinImageList.at(m_ImageCount-1).dateUploaded().toString() ));
        m_Ctxt->setContextProperty( "resultModel", QVariant::fromValue(m_ResultItemList) );
    }

}

void AstrophotographsBrowser::scaleAndAddPixmap(QPixmap *pixmap){
    uint w = pixmap->width();
    uint h = pixmap->height();
    uint pad = 0;
    uint hDesk = QApplication::desktop()->availableGeometry().height() - pad;

    if(h > hDesk) {
        *pixmap = pixmap->scaled(w * hDesk / h, hDesk, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    m_ImageCount += 1;
    pixmap->save(KStandardDirs().locateLocal("data", "kstars/astrobinImages/thumb/image_"+ QString::number(m_ImageCount) + ".png"));

    m_AstrobinImages.append(pixmap);
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

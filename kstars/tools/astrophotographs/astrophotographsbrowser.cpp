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

#include "kstandarddirs.h"
#include "kdeclarative.h"

AstrophotographsBrowser::AstrophotographsBrowser(QWidget *parent) : QWidget(parent)
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
    connect(m_SearchContainerObj, SIGNAL(searchButtonClicked()), this, SLOT(slotAstrobinSearch()));

    m_BaseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    m_BaseView->show();

}

AstrophotographsBrowser::~AstrophotographsBrowser(){
    delete m_BaseObj;
    delete m_BaseView;
}

void AstrophotographsBrowser::slotAstrobinSearch(){

    readExistingImages();
    m_AstrobinApi->searchObjectImages("sun");
}

void AstrophotographsBrowser::slotAstrobinSearchCompleted(bool ok)
{
    if(!ok) {
        KMessageBox::error(this, i18n("AstroBin.com search error: ", i18n("AstroBin.com search error")));
        return;
    }

    AstroBinSearchResult result = m_AstrobinApi->getResult();
    foreach(AstroBinImage image, result) {
        KIO::StoredTransferJob *job = KIO::storedGet(image.downloadResizedUrl(), KIO::NoReload, KIO::HideProgressInfo);
        job->setUiDelegate(0);
        m_Jobs.append(job);
        connect(job, SIGNAL(result(KJob*)), SLOT(slotJobResult(KJob*)));
        qDebug() << "image" << image.title();
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
}

void AstrophotographsBrowser::readExistingImages(){
    foreach(QPixmap *pixmap, m_AstrobinImages) {
        scaleAndAddPixmap(pixmap);
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

    m_AstrobinImages.append(pixmap);
}

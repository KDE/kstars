#include "astrophotographsbrowser.h"

#include "QDeclarativeView"
#include <QGraphicsObject>
#include <QNetworkAccessManager>
#include <QDebug>

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

    qDebug() << "Brow created";
    m_BaseView->setSource(KStandardDirs::locate("appdata","tools/astrophotographs/qml/astrophotographsbrowser.qml"));
    m_BaseObj = dynamic_cast<QObject *>(m_BaseView->rootObject());

    qDebug() << "Base created " << m_BaseObj;

    m_SearchContainerObj = m_BaseObj->findChild<QObject *>("searchContainerObj");

    qDebug() << "SearchCont created " << m_SearchContainerObj;

    connect(m_SearchContainerObj, SIGNAL(searchButtonClicked()), this, SLOT(slotAstrobinSearch()));

    m_BaseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    m_BaseView->show();
}

AstrophotographsBrowser::~AstrophotographsBrowser(){
    delete m_BaseObj;
    delete m_BaseView;
}

void AstrophotographsBrowser::slotAstrobinSearch(){
    qDebug() << "Search Clicked";
}

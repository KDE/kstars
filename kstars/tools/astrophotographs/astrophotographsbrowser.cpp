#include "astrophotographsbrowser.h"

#include "kstandarddirs.h"
#include "kdeclarative.h"
#include <QGraphicsObject>
#include <QNetworkAccessManager>
#include <QDebug>

AstrophotographsBrowser::AstrophotographsBrowser(QWidget *parent) : QWidget(parent)
{
    m_BaseView = new QDeclarativeView();

    ///To use i18n() instead of qsTr() in qml/astrophotographsbrowser.qml for translation
    KDeclarative kd;
    kd.setDeclarativeEngine(m_BaseView->engine());
    kd.initialize();
    kd.setupBindings();

    m_Ctxt = m_BaseView->rootContext();

    m_BaseView->setSource(KStandardDirs::locate("appdata","tools/astrophotographs/qml/astrophotographsbrowser.qml"));

    m_BaseObj = dynamic_cast<QObject *>(m_BaseView->rootObject());

    m_BaseView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    m_BaseView->show();

    m_NetworkManager = new QNetworkAccessManager(this);
    m_AstrobinApi = new AstroBinApiJson(m_NetworkManager, this);
}

AstrophotographsBrowser::~AstrophotographsBrowser(){
    delete m_BaseObj;
    delete m_BaseView;
}

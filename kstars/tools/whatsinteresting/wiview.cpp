/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wiview.h"

#include "kspaths.h"
#include "kstars.h"
#include "ksnotification.h"
#include "modelmanager.h"
#include "obsconditions.h"
#include "Options.h"
#include "skymap.h"
#include "skymapcomposite.h"
#include "skyobjitem.h"
#include "skyobjlistmodel.h"
#include "starobject.h"
#include "wiequipsettings.h"
#include "dialogs/detaildialog.h"

#include <klocalizedcontext.h>

#include <QGraphicsObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <QStandardPaths>
#include <QtConcurrent>

#ifdef HAVE_INDI
#include <basedevice.h>
#include "indi/indilistener.h"
#endif

WIView::WIView(QWidget *parent) : QWidget(parent)
{
    //These settings are like this just to get it started.
    int bortle                           = Options::bortleClass();
    int aperture                         = 100;
    ObsConditions::Equipment equip       = ObsConditions::Telescope;
    ObsConditions::TelescopeType telType = ObsConditions::Reflector;

    m_Obs = new ObsConditions(bortle, aperture, equip, telType);

    m_ModManager.reset(new ModelManager(m_Obs));

    m_BaseView = new QQuickView();

    ///To use i18n() instead of qsTr() in qml/wiview.qml for translation
    //KDeclarative kd;
    // kd.setDeclarativeEngine(m_BaseView->engine());
    //kd.initialize();
    //kd.setupBindings();

    m_Ctxt = m_BaseView->rootContext();

    m_Ctxt->setContextProperty(
        "soListModel",
        m_ModManager
        ->getTempModel()); // This is to avoid an error saying it doesn't exist.

    ///Use instead of KDeclarative
    m_Ctxt->setContextObject(new KLocalizedContext(m_BaseView));

#if 0
    QString WI_Location;
#if defined(Q_OS_OSX)
    WI_Location = QCoreApplication::applicationDirPath() + "/../Resources/kstars/tools/whatsinteresting/qml/wiview.qml";
    if (!QFileInfo(WI_Location).exists())
        WI_Location = KSPaths::locate(QStandardPaths::AppDataLocation, "tools/whatsinteresting/qml/wiview.qml");
#elif defined(Q_OS_WIN)
    WI_Location = KSPaths::locate(QStandardPaths::GenericDataLocation, "tools/whatsinteresting/qml/wiview.qml");
#else
    WI_Location = KSPaths::locate(QStandardPaths::AppDataLocation, "tools/whatsinteresting/qml/wiview.qml");
#endif

    m_BaseView->setSource(QUrl::fromLocalFile(WI_Location));
#endif

    m_BaseView->setSource(QUrl("qrc:/qml/whatisinteresting/wiview.qml"));

    m_BaseObj = m_BaseView->rootObject();

    m_ProgressBar = m_BaseObj->findChild<QQuickItem *>("progressBar");

    m_loadingMessage = m_BaseObj->findChild<QQuickItem *>("loadingMessage");

    m_CategoryTitle = m_BaseObj->findChild<QQuickItem *>(QString("categoryTitle"));

    m_ViewsRowObj = m_BaseObj->findChild<QQuickItem *>(QString("viewsRowObj"));
    connect(m_ViewsRowObj, SIGNAL(categorySelected(QString)), this,
            SLOT(onCategorySelected(QString)));
    connect(m_ViewsRowObj, SIGNAL(inspectSkyObject(QString)), this,
            SLOT(inspectSkyObject(QString)));

    m_SoListObj = m_BaseObj->findChild<QQuickItem *>("soListObj");
    connect(m_SoListObj, SIGNAL(soListItemClicked(int)), this,
            SLOT(onSoListItemClicked(int)));

    m_DetailsViewObj = m_BaseObj->findChild<QQuickItem *>("detailsViewObj");

    descTextObj = m_DetailsViewObj->findChild<QObject *>("descTextObj");
    infoBoxText = m_DetailsViewObj->findChild<QObject *>("infoBoxText");

    m_NextObj = m_BaseObj->findChild<QQuickItem *>("nextObj");
    connect(m_NextObj, SIGNAL(nextObjClicked()), this, SLOT(onNextObjClicked()));
    m_PrevObj = m_BaseObj->findChild<QQuickItem *>("prevObj");
    connect(m_PrevObj, SIGNAL(prevObjClicked()), this, SLOT(onPrevObjClicked()));

    m_CenterButtonObj = m_BaseObj->findChild<QQuickItem *>("centerButtonObj");
    connect(m_CenterButtonObj, SIGNAL(centerButtonClicked()), this,
            SLOT(onCenterButtonClicked()));

    autoCenterCheckbox = m_DetailsViewObj->findChild<QObject *>("autoCenterCheckbox");
    autoTrackCheckbox  = m_DetailsViewObj->findChild<QObject *>("autoTrackCheckbox");

    m_SlewTelescopeButtonObj =
        m_BaseObj->findChild<QQuickItem *>("slewTelescopeButtonObj");
    connect(m_SlewTelescopeButtonObj, SIGNAL(slewTelescopeButtonClicked()), this,
            SLOT(onSlewTelescopeButtonClicked()));

    m_DetailsButtonObj = m_BaseObj->findChild<QQuickItem *>("detailsButtonObj");
    connect(m_DetailsButtonObj, SIGNAL(detailsButtonClicked()), this,
            SLOT(onDetailsButtonClicked()));

    QObject *settingsIconObj = m_BaseObj->findChild<QQuickItem *>("settingsIconObj");
    connect(settingsIconObj, SIGNAL(settingsIconClicked()), this,
            SLOT(onSettingsIconClicked()));

    inspectIconObj = m_BaseObj->findChild<QQuickItem *>("inspectIconObj");
    connect(inspectIconObj, SIGNAL(inspectIconClicked(bool)), this,
            SLOT(onInspectIconClicked(bool)));

    visibleIconObj = m_BaseObj->findChild<QQuickItem *>("visibleIconObj");
    connect(visibleIconObj, SIGNAL(visibleIconClicked(bool)), this,
            SLOT(onVisibleIconClicked(bool)));

    favoriteIconObj = m_BaseObj->findChild<QQuickItem *>("favoriteIconObj");
    connect(favoriteIconObj, SIGNAL(favoriteIconClicked(bool)), this,
            SLOT(onFavoriteIconClicked(bool)));

    QObject *reloadIconObj = m_BaseObj->findChild<QQuickItem *>("reloadIconObj");
    connect(reloadIconObj, SIGNAL(reloadIconClicked()), this,
            SLOT(onReloadIconClicked()));

    QObject *downloadIconObj = m_BaseObj->findChild<QQuickItem *>("downloadIconObj");
    connect(downloadIconObj, SIGNAL(downloadIconClicked()), this,
            SLOT(onUpdateIconClicked()));

    m_BaseView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_BaseView->show();

    // Fix some weird issue with what's interesting panel view under Windows
    // In Qt 5.9 it content is messed up and there is no way to close the panel
#ifdef Q_OS_WIN
    m_BaseView->setFlags(Qt::WindowCloseButtonHint);
#endif

    connect(KStars::Instance()->map(), SIGNAL(objectClicked(SkyObject *)), this,
            SLOT(inspectSkyObjectOnClick(SkyObject *)));

    manager.reset(new QNetworkAccessManager());

    setProgressBarVisible(true);
    connect(m_ModManager.get(), SIGNAL(loadProgressUpdated(double)), this,
            SLOT(updateProgress(double)));
    connect(m_ModManager.get(), SIGNAL(modelUpdated()), this, SLOT(refreshListView()));
    m_ViewsRowObj->setProperty("enabled", false);

    inspectOnClick = false;

    nightVision = m_BaseObj->findChild<QObject *>("nightVision");
    //if (Options::darkAppColors())
    //    nightVision->setProperty("state", "active");
}

void WIView::setNightVisionOn(bool on)
{
    if (on)
        nightVision->setProperty("state", "active");
    else
        nightVision->setProperty("state", "");

    if (m_CurSoItem != nullptr)
        loadDetailsView(m_CurSoItem, m_CurIndex);
}

void WIView::setProgressBarVisible(bool visible)
{
    m_ProgressBar->setProperty("visible", visible);
}

void WIView::updateProgress(double value)
{
    m_ProgressBar->setProperty("value", value);

    if (value == 1)
    {
        setProgressBarVisible(false);
        m_ViewsRowObj->setProperty("enabled", true);
        m_loadingMessage->setProperty("state", "");
    }
    else
    {
        setProgressBarVisible(true);
        m_loadingMessage->setProperty("state", "loading");
    }
}

void WIView::updateObservingConditions()
{
    int bortle = Options::bortleClass();

    /**
    NOTE This part of the code dealing with equipment type is presently not required
    as WI does not differentiate between Telescope and Binoculars. It only needs the
     aperture of the equipment whichever available. However this is kept as a part of
    the code as support to be utilised in the future.
    **/
    ObsConditions::Equipment equip = ObsConditions::None;

    if (Options::telescopeCheck() && Options::binocularsCheck())
        equip = ObsConditions::Both;
    else if (Options::telescopeCheck())
        equip = ObsConditions::Telescope;
    else if (Options::binocularsCheck())
        equip = ObsConditions::Binoculars;

    ObsConditions::TelescopeType telType;

    if (KStars::Instance()->getWIEquipSettings())
        telType = (equip == ObsConditions::Telescope) ? KStars::Instance()->getWIEquipSettings()->getTelType() :
                  ObsConditions::Invalid;
    else
        telType = ObsConditions::Invalid;

    int aperture = 100;

    //This doesn't work correctly, FIXME!!
    // if(KStars::Instance()->getWIEquipSettings())
    //    aperture = KStars::Instance()->getWIEquipSettings()->getAperture();

    if (!m_Obs)
        m_Obs = new ObsConditions(bortle, aperture, equip, telType);
    else
        m_Obs->setObsConditions(bortle, aperture, equip, telType);
}

void WIView::onCategorySelected(QString model)
{
    m_CurrentObjectListName = model;
    m_Ctxt->setContextProperty("soListModel",
                               m_ModManager->returnModel(m_CurrentObjectListName));
    m_CurIndex = -2;
    if (!m_ModManager->showOnlyVisibleObjects())
        visibleIconObj->setProperty("state", "unchecked");
    if (!m_ModManager->showOnlyFavoriteObjects())
        favoriteIconObj->setProperty("state", "unchecked");

    if ((QStringList() << "ngc"
            << "ic"
            << "messier"
            << "sharpless")
            .contains(model))
    {
        QtConcurrent::run(m_ModManager.get(), &ModelManager::loadCatalog, model);
        return;
    }

    updateModel(*m_Obs);
}

void WIView::onSoListItemClicked(int index)
{
    SkyObjItem *soitem = m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem(index);
    if (soitem)
        loadDetailsView(soitem, index);
}

void WIView::onNextObjClicked()
{
    if (!m_CurrentObjectListName.isEmpty())
    {
        int modelSize = m_ModManager->returnModel(m_CurrentObjectListName)->rowCount();
        if (modelSize > 0)
        {
            SkyObjItem *nextItem =
                m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem((m_CurIndex + 1) % modelSize);
            loadDetailsView(nextItem, (m_CurIndex + 1) % modelSize);
        }
    }
}

void WIView::onPrevObjClicked()
{
    if (!m_CurrentObjectListName.isEmpty())
    {
        int modelSize = m_ModManager->returnModel(m_CurrentObjectListName)->rowCount();
        if (modelSize > 0)
        {
            SkyObjItem *prevItem =
                m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem((m_CurIndex - 1 + modelSize) % modelSize);
            loadDetailsView(prevItem, (m_CurIndex - 1 + modelSize) % modelSize);
        }
    }
}

void WIView::onCenterButtonClicked()
{
    ///Center map on selected sky-object
    SkyObject *so  = m_CurSoItem->getSkyObject();
    KStars *kstars = KStars::Instance();

    if (so)
    {
        kstars->map()->setFocusPoint(so);
        kstars->map()->setFocusObject(so);
        kstars->map()->setDestination(*kstars->map()->focusPoint());
        Options::setIsTracking(autoTrackCheckbox->property("checked") == true);
    }
}

void WIView::onSlewTelescopeButtonClicked()
{
    if (KMessageBox::Continue ==
            KMessageBox::warningContinueCancel(nullptr, "Are you sure you want your telescope to slew to this object?",
                    i18n("Continue Slew"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                    "continue_wi_slew_warning"))
    {
#ifdef HAVE_INDI

        if (INDIListener::Instance()->size() == 0)
        {
            KSNotification::sorry(i18n("KStars did not find any active telescopes."));
            return;
        }

        foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
        {
            INDI::BaseDevice *bd = gd->getBaseDevice();

            if (gd->getType() != KSTARS_TELESCOPE)
                continue;

            if (bd == nullptr)
                continue;

            if (bd->isConnected() == false)
            {
                KSNotification::error(i18n("Telescope %1 is offline. Please connect and retry again.", gd->getDeviceName()));
                return;
            }

            ISD::GDSetCommand SlewCMD(INDI_SWITCH, "ON_COORD_SET", "TRACK", ISS_ON, this);

            gd->setProperty(&SlewCMD);
            gd->runCommand(INDI_SEND_COORDS, m_CurSoItem->getSkyObject());

            /// Slew map to selected sky-object
            onCenterButtonClicked();

            return;
        }

        KSNotification::sorry(i18n("KStars did not find any active telescopes."));

#endif
    }
}

void WIView::onDetailsButtonClicked()
{
    ///Code taken from WUTDialog::slotDetails()
    KStars *kstars = KStars::Instance();
    SkyObject *so  = m_CurSoItem->getSkyObject();
    if (so)
    {
        DetailDialog *detail = new DetailDialog(so, kstars->data()->lt(), kstars->data()->geo(), kstars);
        detail->exec();
        delete detail;
    }
}

void WIView::onSettingsIconClicked()
{
    KStars *kstars = KStars::Instance();
    kstars->showWISettingsUI();
}

void WIView::onReloadIconClicked()
{
    if (!m_CurrentObjectListName.isEmpty())
    {
        updateModel(*m_Obs);
        m_CurIndex = m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjIndex(m_CurSoItem);
    }
    loadDetailsView(m_CurSoItem, m_CurIndex);
}

void WIView::onVisibleIconClicked(bool visible)
{
    m_ModManager->setShowOnlyVisibleObjects(visible);
    onReloadIconClicked();
}

void WIView::onFavoriteIconClicked(bool favorites)
{
    m_ModManager->setShowOnlyFavoriteObjects(favorites);
    onReloadIconClicked();
}

void WIView::onUpdateIconClicked()
{
    QMessageBox mbox;
    QPushButton *currentObject = mbox.addButton("Current Object", QMessageBox::AcceptRole);
    QPushButton *missingObjects = nullptr;
    QPushButton *allObjects = nullptr;

    mbox.setText("Please choose which object(s) to try to update with Wikipedia data.");
    if (!m_CurrentObjectListName.isEmpty())
    {
        missingObjects = mbox.addButton("Objects with no data", QMessageBox::AcceptRole);
        allObjects     = mbox.addButton("Entire List", QMessageBox::AcceptRole);
    }
    QPushButton *cancel = mbox.addButton("Cancel", QMessageBox::AcceptRole);
    mbox.setDefaultButton(cancel);

    mbox.exec();
    if (mbox.clickedButton() == currentObject)
    {
        if (m_CurSoItem != nullptr)
        {
            tryToUpdateWikipediaInfo(m_CurSoItem, getWikipediaName(m_CurSoItem));
        }
    }
    else if (mbox.clickedButton() == allObjects || mbox.clickedButton() == missingObjects)
    {
        SkyObjListModel *model = m_ModManager->returnModel(m_CurrentObjectListName);
        if (model->rowCount() > 0)
        {
            tryToUpdateWikipediaInfoInModel(mbox.clickedButton() == missingObjects);
        }
        else
        {
            qDebug() << "No Objects in List!";
        }
    }
}

void WIView::refreshListView()
{
    m_Ctxt->setContextProperty("soListModel", nullptr);
    if (!m_CurrentObjectListName.isEmpty())
        m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(m_CurrentObjectListName));
    if (m_CurIndex == -2)
        onSoListItemClicked(0);
    if (m_CurIndex != -1)
        m_SoListObj->setProperty("currentIndex", m_CurIndex);
}

void WIView::updateModel(ObsConditions &obs)
{
    if (!m_CurrentObjectListName.isEmpty())
    {
        m_Obs = &obs;
        m_ModManager->updateModel(m_Obs, m_CurrentObjectListName);
    }
}

void WIView::inspectSkyObject(const QString &name)
{
    if (!name.isEmpty() && name != "star")
    {
        SkyObject *obj = KStarsData::Instance()->skyComposite()->findByName(name);

        if (obj)
            inspectSkyObject(obj);
    }
}

void WIView::inspectSkyObjectOnClick(SkyObject *obj)
{
    if (inspectOnClick && KStars::Instance()->isWIVisible())
        inspectSkyObject(obj);
}

void WIView::inspectSkyObject(SkyObject *obj)
{
    if (!obj)
        return;

    if (obj->name() != "star")
    {
        m_CurrentObjectListName = "";
        trackedItem.reset(new SkyObjItem(obj));
        loadDetailsView(trackedItem.get(), -1);
        m_BaseObj->setProperty("state", "singleItemSelected");
        m_CategoryTitle->setProperty("text", "Selected Object");
    }
}

void WIView::loadDetailsView(SkyObjItem *soitem, int index)
{
    if (soitem == nullptr)
        return;

    int modelSize = -1;

    if (index != -1)
        modelSize = m_ModManager->returnModel(m_CurrentObjectListName)->rowCount();

    if (soitem != m_CurSoItem)
        m_CurSoItem = soitem;

    m_CurIndex  = index;
    if (modelSize <= 1)
    {
        m_NextObj->setProperty("visible", "false");
        m_PrevObj->setProperty("visible", "false");
    }
    else
    {
        SkyObjItem *nextItem =
            m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem((m_CurIndex + 1) % modelSize);
        SkyObjItem *prevItem =
            m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem((m_CurIndex - 1 + modelSize) % modelSize);

        m_NextObj->setProperty("visible", "true");
        m_PrevObj->setProperty("visible", "true");
        QObject *nextTextObj = m_NextObj->findChild<QObject *>("nextTextObj");

        nextTextObj->setProperty("text", nextItem->getName());
        QObject *prevTextObj = m_PrevObj->findChild<QObject *>("prevTextObj");

        prevTextObj->setProperty("text", prevItem->getName());
    }

    QObject *sonameObj      = m_DetailsViewObj->findChild<QObject *>("sonameObj");
    QObject *posTextObj     = m_DetailsViewObj->findChild<QObject *>("posTextObj");
    QObject *detailImage    = m_DetailsViewObj->findChild<QObject *>("detailImage");
    QObject *detailsTextObj = m_DetailsViewObj->findChild<QObject *>("detailsTextObj");

    sonameObj->setProperty("text", soitem->getDescName());
    posTextObj->setProperty("text", soitem->getPosition());
    detailImage->setProperty("refreshableSource", soitem->getImageURL(false));

    loadObjectDescription(soitem);

    infoBoxText->setProperty(
        "text",
        "<BR><BR>No Wikipedia information. <BR>  Please try to download it using the orange download button below.");
    loadObjectInfoBox(soitem);

    QString summary = soitem->getSummary(false);

    QString magText;
    if (soitem->getType() == SkyObjItem::Constellation)
        magText = xi18n("Magnitude:  --");
    else
        magText = xi18n("Magnitude: %1", QLocale().toString(soitem->getMagnitude(), 'f', 2));

    QString sbText = xi18n("Surface Brightness: %1", soitem->getSurfaceBrightness());

    QString sizeText = xi18n("Size: %1", soitem->getSize());

    QString details = summary + "<BR>" + sbText + "<BR>" + magText + "<BR>" + sizeText;
    detailsTextObj->setProperty("text", details);

    if (autoCenterCheckbox->property("checked") == true)
    {
        QTimer::singleShot(500, this, SLOT(onCenterButtonClicked()));
    }

    if (m_CurIndex != -1)
        m_SoListObj->setProperty("currentIndex", m_CurIndex);
}

QString WIView::getWikipediaName(SkyObjItem *soitem)
{
    if (!soitem)
        return "";

    QString name;

    if (soitem->getName().toLower().startsWith(QLatin1String("m ")))
        name = soitem->getName().replace("M ", "Messier_").remove(' ');
    else if (soitem->getName().toLower().startsWith(QLatin1String("ngc")))
        name = soitem->getName().toLower().replace("ngc", "NGC_").remove(' ');
    else if (soitem->getName().toLower().startsWith(QLatin1String("ic")))
        name = soitem->getName().toLower().replace("ic", "IC_").remove(' ');
    else if (soitem->getType() == SkyObjItem::Constellation)
    {
        QStringList words = soitem->getName().split(' ');

        for (int i = 0; i < words.size(); i++)
        {
            QString temp = words.at(i).toLower();
            temp[0]      = temp[0].toUpper();
            words.replace(i, temp);
        }
        name = words.join("_") + "_(constellation)";
        if (name.contains("Serpens"))
            name = "Serpens_(constellation)";
    }
    else if (soitem->getTypeName() == i18n("Asteroid"))
        name = soitem->getName().remove(' ') + "_(asteroid)";
    else if (soitem->getTypeName() == i18n("Comet"))
        name = soitem->getLongName();
    else if (soitem->getType() == SkyObjItem::Planet && soitem->getName() != i18n("Sun") && soitem->getName() != i18n("Moon"))
        name = soitem->getName().remove(' ') + "_(planet)";
    else if (soitem->getType() == SkyObjItem::Star)
    {
        StarObject *star = dynamic_cast<StarObject *>(soitem->getSkyObject());

        // The greek name seems to give the most consistent search results for opensearch.
        name             = star->gname(false).replace(' ', '_');
        if (name.isEmpty())
            name = soitem->getName().replace(' ', '_') + "_(star)";
        name.remove('[').remove(']');
    }
    else
        name = soitem->getName().remove(' ');

    return name;
}

void WIView::updateWikipediaDescription(SkyObjItem *soitem)
{
    if (!soitem)
        return;

    QString name = getWikipediaName(soitem);

    QUrl url("https://en.wikipedia.org/w/api.php?format=xml&action=query&prop=extracts&exintro&explaintext&redirects=1&titles=" + name);

    QNetworkReply *response = manager->get(QNetworkRequest(url));
    QTimer::singleShot(30000, response, [response]   //Shut it down after 30 sec.
    {
        response->abort();
        response->deleteLater();
        qDebug() << "Wikipedia Download Timed out.";
    });
    connect(response, &QNetworkReply::finished, this, [soitem, this, response, name]
    {
        response->deleteLater();
        if (response->error() != QNetworkReply::NoError)
            return;
        QString contentType = response->header(QNetworkRequest::ContentTypeHeader).toString();
        if (!contentType.contains("charset=utf-8"))
        {
            qWarning() << "Content charsets other than utf-8 are not implemented yet.";
            return;
        }
        QString result = QString::fromUtf8(response->readAll());
        int leftPos    = result.indexOf("<extract xml:space=\"preserve\">") + 30;
        if (leftPos < 30)
            return;
        int rightPos = result.indexOf("</extract>") - leftPos;

        QString srchtml =
        "\n<p style=text-align:right>Source: (<a href='" + QString("https://en.wikipedia.org/wiki/") + name + "'>" +
        "Wikipedia</a>)"; //Note the \n is so that the description is put on another line in the file.  Doesn't affect the display but allows the source to be loaded in the details but not the list.
        QString html = "<HTML>" + result.mid(leftPos, rightPos) + srchtml + "</HTML>";

        saveObjectInfoBoxText(soitem, "description", html);

        //TODO is this explicitly needed now with themes?
#if 0
        QString color     = (Options::darkAppColors()) ? "red" : "white";
        QString linkColor = (Options::darkAppColors()) ? "red" : "yellow";
        html              = "<HTML><HEAD><style type=text/css>body {color:" + color +
        ";} a {text-decoration: none;color:" + linkColor + ";}</style></HEAD><BODY>" + html + "</BODY></HTML>";
#endif

        if (soitem == m_CurSoItem)
            descTextObj->setProperty("text", html);
        refreshListView();
    });
}

void WIView::loadObjectDescription(SkyObjItem *soitem)
{
    QFile file;
    QString fname = "description-" + soitem->getName().toLower().remove(' ') + ".html";
    //determine filename in local user KDE directory tree.
    file.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("descriptions/" + fname));

    if (file.exists())
    {
        if (file.open(QIODevice::ReadOnly))
        {
            QTextStream in(&file);
            bool isDarkTheme = (Options::currentTheme() == "Night Vision");
            QString color     = (isDarkTheme) ? "red" : "white";
            QString linkColor = (isDarkTheme) ? "red" : "yellow";

            QString line      = "<HTML><HEAD><style type=text/css>body {color:" + color +
                                ";} a {text-decoration: none;color:" + linkColor + ";}</style></HEAD><BODY><BR>" +
                                in.readAll() + "</BODY></HTML>";
            descTextObj->setProperty("text", line);
            file.close();
        }
    }
    else
    {
        descTextObj->setProperty("text", soitem->getTypeName());
    }
}

void WIView::loadObjectInfoBox(SkyObjItem *soitem)
{
    if (!soitem)
        return;
    QFile file;
    QString fname = "infoText-" + soitem->getName().toLower().remove(' ') + ".html";
    //determine filename in local user KDE directory tree.
    file.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("descriptions/" + fname));

    if (file.exists())
    {
        if (file.open(QIODevice::ReadOnly))
        {
            QTextStream in(&file);
            QString infoBoxHTML;
            while (!in.atEnd())
            {
                infoBoxHTML = in.readAll();
                QString wikiImageName =
                    QUrl::fromLocalFile(
                        KSPaths::locate(QStandardPaths::AppDataLocation,
                                        "descriptions/wikiImage-" + soitem->getName().toLower().remove(' ') + ".png"))
                    .url();
                if (!wikiImageName.isEmpty())
                {
                    int captionEnd = infoBoxHTML.indexOf(
                                         "</caption>"); //Start looking for the image AFTER the caption.  Planets have images in their caption.
                    if (captionEnd == -1)
                        captionEnd = 0;
                    int leftImg    = infoBoxHTML.indexOf("src=\"", captionEnd) + 5;
                    int rightImg   = infoBoxHTML.indexOf("\"", leftImg) - leftImg;
                    QString imgURL = infoBoxHTML.mid(leftImg, rightImg);
                    infoBoxHTML.replace(imgURL, wikiImageName);
                }
                bool isDarkTheme = (Options::currentTheme() == "Night Vision");
                QString color     = (isDarkTheme) ? "red" : "white";
                QString linkColor = (isDarkTheme) ? "red" : "yellow";
                if (isDarkTheme)
                    infoBoxHTML.replace("color: white", "color: " + color);
                infoBoxHTML = "<HTML><HEAD><style type=text/css>body {color:" + color +
                              ";} a {text-decoration: none;color:" + linkColor + ";}</style></HEAD><BODY>" +
                              infoBoxHTML + "</BODY></HTML>";

                infoBoxText->setProperty("text", infoBoxHTML);
            }
            file.close();
        }
    }
}

void WIView::tryToUpdateWikipediaInfoInModel(bool onlyMissing)
{
    SkyObjListModel *model = m_ModManager->returnModel(m_CurrentObjectListName);
    int objectNum          = model->rowCount();
    for (int i = 0; i < objectNum; i++)
    {
        SkyObjItem *soitem = model->getSkyObjItem(i);
        QFile file;
        QString fname = "infoText-" + soitem->getName().toLower().remove(' ') + ".html";
        //determine filename in local user KDE directory tree.
        file.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("descriptions/" + fname));

        if (file.exists() && onlyMissing)
            continue;

        tryToUpdateWikipediaInfo(soitem, getWikipediaName(soitem));
    }
}

void WIView::tryToUpdateWikipediaInfo(SkyObjItem *soitem, QString name)
{
    if (name.isEmpty() || !soitem)
        return;

    QUrl url("https://en.wikipedia.org/w/index.php?action=render&title=" + name + "&redirects");
    QNetworkReply *response = manager->get(QNetworkRequest(url));

    QTimer::singleShot(30000, response, [response]   //Shut it down after 30 sec.
    {
        response->abort();
        response->deleteLater();
        qDebug() << "Wikipedia Download Timed out.";
    });
    connect(response, &QNetworkReply::finished, this, [name, response, soitem, this]
    {
        response->deleteLater();
        if (response->error() == QNetworkReply::ContentNotFoundError)
        {
            QString html = "<BR>Sorry, No Wikipedia article with this object name seems to exist.  It is possible that "
            "one does exist but does not match the namimg scheme.";
            saveObjectInfoBoxText(soitem, "infoText", html);
            infoBoxText->setProperty("text", html);
            return;
        }
        if (response->error() != QNetworkReply::NoError)
            return;
        QString result = QString::fromUtf8(response->readAll());
        int leftPos    = result.indexOf("<table class=\"infobox");
        int rightPos   = result.indexOf("</table>", leftPos) - leftPos;

        if (leftPos == -1)
        {
            //No InfoBox is Found
            if (soitem->getType() == SkyObjItem::Star &&
                    name != soitem->getName().replace(' ', '_')) //For stars, the regular name rather than gname
            {
                tryToUpdateWikipediaInfo(soitem, soitem->getName().replace(' ', '_'));
                return;
            }
            QString html = "<BR>Sorry, no Information Box in the object's Wikipedia article was found.";
            saveObjectInfoBoxText(soitem, "infoText", html);
            infoBoxText->setProperty("text", html);
            return;
        }

        updateWikipediaDescription(soitem);

        QString infoText = result.mid(leftPos, rightPos);

        //This if statement should correct for a situation like for the planets where there is a single internal table inside the infoText Box.
        if (infoText.indexOf("<table", leftPos + 6) != -1)
        {
            rightPos = result.indexOf("</table>", leftPos + rightPos + 6) - leftPos;
            infoText = result.mid(leftPos, rightPos);
        }

        //This next section is for the headers in the colored boxes. It turns them black instead of white because they are more visible that way.
        infoText.replace("background: #", "color:black;background: #")
        .replace("background-color: #", "color:black;background: #")
        .replace("background:#", "color:black;background:#")
        .replace("background-color:#", "color:black;background:#")
        .replace("background: pink", "color:black;background: pink");
        infoText.replace("//", "http://"); //This is to fix links on wikipedia which are missing http from the url
        infoText.replace("https:http:", "https:")
        .replace("http:http:", "http:"); //Just in case it was done to an actual complete url

        //This section is intended to remove links from the object name header at the top.  The links break up the header.
        int thLeft = infoText.indexOf("<th ");
        if (thLeft != -1)
        {
            int thRight = infoText.indexOf("</th>", thLeft);
            int firstA  = infoText.indexOf("<a ", thLeft);
            if (firstA != -1 && firstA < thRight)
            {
                int rightA = infoText.indexOf(">", firstA) - firstA + 1;
                infoText.remove(firstA, rightA);
                int endA = infoText.indexOf("</a>", firstA);
                infoText.remove(endA, 4);
            }
        }

        int annotationLeft  = infoText.indexOf("<annotation");
        int annotationRight = infoText.indexOf("</annotation>", annotationLeft) + 13 - annotationLeft;
        infoText.remove(annotationLeft,
                        annotationRight); //This removes the annotation that does not render correctly for some DSOs.

        int mathLeft  = infoText.indexOf("<img src=\"https://wikimedia.org/api/rest_v1/media/math");
        int mathRight = infoText.indexOf(">", mathLeft) + 1 - mathLeft;
        infoText.remove(mathLeft, mathRight); //This removes an image that doesn't render properly for some DSOs.

        infoText.replace("style=\"width:22em\"", "style=\"width:100%;background-color: black;color: white;\"");
        infoText = infoText + "<BR>(Source: <a href='" + "https://en.wikipedia.org/w/index.php?title=" + name +
                   "&redirects" + "'>Wikipedia</a>)";
        saveInfoURL(soitem, "https://en.wikipedia.org/w/index.php?title=" + name + "&redirects");

        int captionEnd = infoText.indexOf(
                             "</caption>"); //Start looking for the image AFTER the caption.  Planets have images in their caption.
        if (captionEnd == -1)
            captionEnd = 0;
        int leftImg = infoText.indexOf("src=\"", captionEnd) + 5;
        if (leftImg > captionEnd + 5)
        {
            int rightImg   = infoText.indexOf("\"", leftImg) - leftImg;
            QString imgURL = infoText.mid(leftImg, rightImg);
            imgURL.replace(
                "http://upload.wikimedia.org",
                "https://upload.wikimedia.org"); //Although they will display, the images apparently don't download properly unless they are https.
            saveImageURL(soitem, imgURL);
            downloadWikipediaImage(soitem, imgURL);
        }

        QString html = "<CENTER>" + infoText + "</table></CENTER>";

        saveObjectInfoBoxText(soitem, "infoText", html);
        bool isDarkTheme = (Options::currentTheme() == "Night Vision");
        QString color     = (isDarkTheme) ? "red" : "white";
        QString linkColor = (isDarkTheme) ? "red" : "yellow";
        if (isDarkTheme)
            html.replace("color: white", "color: " + color);
        html = "<HTML><HEAD><style type=text/css>body {color:" + color +
               ";} a {text-decoration: none;color:" + linkColor + ";}</style></HEAD><BODY>" + html + "</BODY></HTML>";
        if (soitem == m_CurSoItem)
            infoBoxText->setProperty("text", html);
    });
}

void WIView::saveObjectInfoBoxText(SkyObjItem *soitem, QString type, QString text)
{
    QFile file;
    QString fname = type + '-' + soitem->getName().toLower().remove(' ') + ".html";

    QDir filePath(KSPaths::writableLocation(QStandardPaths::AppDataLocation) + "/descriptions");
    filePath.mkpath(".");

    //determine filename in local user KDE directory tree.
    file.setFileName(filePath.filePath(fname));

    if (file.open(QIODevice::WriteOnly) == false)
    {
        qDebug() << "Image text cannot be saved for later.  file save error";
        return;
    }
    else
    {
        QTextStream stream(&file);
        stream << text;
        file.close();
    }
}

void WIView::saveImageURL(SkyObjItem *soitem, QString imageURL)
{
    QFile file;
    //determine filename in local user KDE directory tree.
    file.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("image_url.dat"));
    QString entry = soitem->getName() + ':' + "Show Wikipedia Image" + ':' + imageURL;

    if (file.open(QIODevice::ReadOnly))
    {
        QTextStream in(&file);
        QString line;
        while (!in.atEnd())
        {
            line = in.readLine();
            if (line == entry)
            {
                file.close();
                return;
            }
        }
        file.close();
    }

    if (!file.open(QIODevice::ReadWrite | QIODevice::Append))
    {
        qDebug() << "Image URL cannot be saved for later.  image_url.dat error";
        return;
    }
    else
    {
        QTextStream stream(&file);
        stream << entry << '\n';
        file.close();
    }
}

void WIView::saveInfoURL(SkyObjItem *soitem, QString infoURL)
{
    QFile file;
    //determine filename in local user KDE directory tree.
    file.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("info_url.dat"));
    QString entry = soitem->getName() + ':' + "Wikipedia Page" + ':' + infoURL;

    if (file.open(QIODevice::ReadOnly))
    {
        QTextStream in(&file);
        QString line;
        while (!in.atEnd())
        {
            line = in.readLine();
            if (line == entry)
            {
                file.close();
                return;
            }
        }
        file.close();
    }

    if (!file.open(QIODevice::ReadWrite | QIODevice::Append))
    {
        qDebug() << "Info URL cannot be saved for later.  info_url.dat error";
        return;
    }
    else
    {
        QTextStream stream(&file);
        stream << entry << '\n';
        file.close();
    }
}

void WIView::downloadWikipediaImage(SkyObjItem *soitem, QString imageURL)
{
    QString fname = "wikiImage-" + soitem->getName().toLower().remove(' ') + ".png";

    QDir filePath(KSPaths::writableLocation(QStandardPaths::AppDataLocation) + "/descriptions");
    filePath.mkpath(".");

    QString fileN = filePath.filePath(fname);

    QNetworkReply *response = manager->get(QNetworkRequest(QUrl(imageURL)));
    QTimer::singleShot(60000, response, [response]   //Shut it down after 60 sec.
    {
        response->abort();
        response->deleteLater();
        qDebug() << "Image Download Timed out.";
    });
    connect(response, &QNetworkReply::finished, this, [fileN, response, this]
    {
        response->deleteLater();
        if (response->error() != QNetworkReply::NoError)
            return;
        QImage *image           = new QImage();
        QByteArray responseData = response->readAll();
        if (image->loadFromData(responseData))
        {
            image->save(fileN);
            refreshListView(); //This is to update the images displayed with the new image.
        }
        else
            qDebug() << "image not downloaded";
    });
}

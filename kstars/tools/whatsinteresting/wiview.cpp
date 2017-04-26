/***************************************************************************
                          wiview.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/26/05
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QStandardPaths>
#include <QGraphicsObject>
#include "wiview.h"
#include "skymap.h"
#include "dialogs/detaildialog.h"
#include <klocalizedcontext.h>
#include "kspaths.h"
#include "starobject.h"
#include "skymap.h"
#include "Options.h"
#include "wiequipsettings.h"
#include "skymapcomposite.h"


#ifdef HAVE_INDI
#include <basedevice.h>
#include "indi/indilistener.h"
#include "indi/indistd.h"
#include "indi/driverinfo.h"
#endif


WIView::WIView(QWidget * parent) : QWidget(parent), m_CurrentObjectListName(-1)
{

    //These settings are like this just to get it started.
    int bortle = Options::bortleClass();
    int aperture = 100;
    ObsConditions::Equipment equip = ObsConditions::Telescope;
    ObsConditions::TelescopeType telType = ObsConditions::Reflector;

    m_Obs = new ObsConditions(bortle, aperture, equip, telType);

    m_ModManager = new ModelManager(m_Obs);

    m_BaseView = new QQuickView();

    ///To use i18n() instead of qsTr() in qml/wiview.qml for translation
    //KDeclarative kd;
    // kd.setDeclarativeEngine(m_BaseView->engine());
    //kd.initialize();
    //kd.setupBindings();

    m_Ctxt = m_BaseView->rootContext();

    m_Ctxt->setContextProperty("soListModel", m_ModManager->getTempModel()); // This is to avoid an error saying it doesn't exist.

    ///Use instead of KDeclarative
    m_Ctxt->setContextObject(new KLocalizedContext(m_BaseView));

    m_BaseView->setSource(QUrl::fromLocalFile(KSPaths::locate(QStandardPaths::AppDataLocation, "tools/whatsinteresting/qml/wiview.qml")));

    m_BaseObj = m_BaseView->rootObject();

    m_CategoryTitle = m_BaseObj->findChild<QQuickItem *>(QString("categoryTitle"));

    m_ViewsRowObj = m_BaseObj->findChild<QQuickItem *>(QString("viewsRowObj"));
    connect(m_ViewsRowObj, SIGNAL(categorySelected(QString)), this, SLOT(onCategorySelected(QString)));
    connect(m_ViewsRowObj, SIGNAL(inspectSkyObject(QString)), this, SLOT(inspectSkyObject(QString)));

    m_SoListObj = m_BaseObj->findChild<QQuickItem *>("soListObj");
    connect(m_SoListObj, SIGNAL(soListItemClicked(int)), this, SLOT(onSoListItemClicked(int)));

    m_DetailsViewObj = m_BaseObj->findChild<QQuickItem *>("detailsViewObj");

    m_skyObjView = m_BaseObj->findChild<QQuickItem *>("skyObjView");
    m_ContainerObj = m_BaseObj->findChild<QQuickItem *>("containerObj");

    m_NextObj = m_BaseObj->findChild<QQuickItem *>("nextObj");
    connect(m_NextObj, SIGNAL(nextObjClicked()), this, SLOT(onNextObjClicked()));
    m_PrevObj = m_BaseObj->findChild<QQuickItem *>("prevObj");
    connect(m_PrevObj, SIGNAL(prevObjClicked()), this, SLOT(onPrevObjClicked()));

    m_CenterButtonObj = m_BaseObj->findChild<QQuickItem *>("centerButtonObj");
    connect(m_CenterButtonObj, SIGNAL(centerButtonClicked()), this, SLOT(onCenterButtonClicked()));

    m_SlewTelescopeButtonObj = m_BaseObj->findChild<QQuickItem *>("slewTelescopeButtonObj");
    connect(m_SlewTelescopeButtonObj, SIGNAL(slewTelescopeButtonClicked()), this, SLOT(onSlewTelescopeButtonClicked()));

    m_DetailsButtonObj = m_BaseObj->findChild<QQuickItem *>("detailsButtonObj");
    connect(m_DetailsButtonObj, SIGNAL(detailsButtonClicked()), this, SLOT(onDetailsButtonClicked()));

    QObject * settingsIconObj = m_BaseObj->findChild<QQuickItem *>("settingsIconObj");
    connect(settingsIconObj, SIGNAL(settingsIconClicked()), this, SLOT(onSettingsIconClicked()));

    visibleIconObj = m_BaseObj->findChild<QQuickItem *>("visibleIconObj");
    connect(visibleIconObj, SIGNAL(visibleIconClicked(bool)), this, SLOT(onVisibleIconClicked(bool)));

    favoriteIconObj = m_BaseObj->findChild<QQuickItem *>("favoriteIconObj");
    connect(favoriteIconObj, SIGNAL(favoriteIconClicked(bool)), this, SLOT(onFavoriteIconClicked(bool)));

    QObject * reloadIconObj = m_BaseObj->findChild<QQuickItem *>("reloadIconObj");
    connect(reloadIconObj, SIGNAL(reloadIconClicked()), this, SLOT(onReloadIconClicked()));

    QObject * downloadIconObj = m_BaseObj->findChild<QQuickItem *>("downloadIconObj");
    connect(downloadIconObj, SIGNAL(downloadIconClicked()), this, SLOT(onDownloadIconClicked()));

    m_BaseView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_BaseView->show();

    connect(KStars::Instance()->map(),SIGNAL(objectClicked(SkyObject*)),this, SLOT(inspectSkyObject(SkyObject*)));

    manager = new QNetworkAccessManager();
}

WIView::~WIView()
{
    delete m_ModManager;
    delete m_CurSoItem;
    delete manager;
}

void WIView::updateObservingConditions(){
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

    if(KStars::Instance()->getWIEquipSettings())
        telType = (equip == ObsConditions::Telescope) ? KStars::Instance()->getWIEquipSettings()->getTelType() : ObsConditions::Invalid;
    else
        telType = ObsConditions::Invalid;

   int aperture=100;

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
    updateModel(m_Obs);
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(model));
    if(!m_ModManager->showOnlyVisibleObjects())
        visibleIconObj->setProperty("state","unchecked");
    if(!m_ModManager->showOnlyFavoriteObjects())
        favoriteIconObj->setProperty("state","unchecked");
    onSoListItemClicked(0);
}

void WIView::onSoListItemClicked(int index)
{
    SkyObjItem * soitem = m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem(index);
    if(soitem)
        loadDetailsView(soitem, index);
}

void WIView::onNextObjClicked()
{
    int modelSize = m_ModManager->returnModel(m_CurrentObjectListName)->rowCount();
    SkyObjItem * nextItem = m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem((m_CurIndex+1)%modelSize);
    loadDetailsView(nextItem, (m_CurIndex+1)%modelSize);
}

void WIView::onPrevObjClicked()
{
    int modelSize = m_ModManager->returnModel(m_CurrentObjectListName)->rowCount();
    SkyObjItem * prevItem = m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem((m_CurIndex-1+modelSize)%modelSize);
    loadDetailsView(prevItem, (m_CurIndex-1+modelSize)%modelSize);
}

void WIView::onCenterButtonClicked()
{
    ///Center map on selected sky-object
    SkyObject * so = m_CurSoItem->getSkyObject();
    KStars * kstars = KStars::Instance();
    if (so != 0)
    {
        kstars->map()->setFocusPoint(so);
        kstars->map()->setFocusObject(so);
        kstars->map()->setDestination(*kstars->map()->focusPoint());
    }
}

void WIView::onSlewTelescopeButtonClicked()
{

    if(KMessageBox::Continue==KMessageBox::warningContinueCancel(NULL, "Are you sure you want your telescope to slew to this object?",
            i18n("Continue Slew"),  KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "continue_wi_slew_warning"))
    {

#ifdef HAVE_INDI

    if (INDIListener::Instance()->size() == 0)
    {
        KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));
        return;
    }

    foreach(ISD::GDInterface * gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice * bd = gd->getBaseDevice();

        if (gd->getType() != KSTARS_TELESCOPE)
            continue;

        if (bd == NULL)
            continue;

        if (bd->isConnected() == false)
        {
            KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.", gd->getDeviceName()));
            return;
        }


        ISD::GDSetCommand SlewCMD(INDI_SWITCH, "ON_COORD_SET", "TRACK", ISS_ON, this);

        gd->setProperty(&SlewCMD);
        gd->runCommand(INDI_SEND_COORDS, m_CurSoItem->getSkyObject());

        ///Slew map to selected sky-object
        onCenterButtonClicked();

        return;

    }

    KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));

#endif
    }


}

void WIView::onDetailsButtonClicked()
{
    ///Code taken from WUTDialog::slotDetails()
    KStars * kstars = KStars::Instance();
    SkyObject * so = m_CurSoItem->getSkyObject();
    DetailDialog * detail = new DetailDialog(so, kstars->data()->lt(), kstars->data()->geo(), kstars);
    detail->exec();
    delete detail;
}

void WIView::onSettingsIconClicked()
{
    KStars * kstars = KStars::Instance();
    kstars->showWISettingsUI();
}

void WIView::onReloadIconClicked()
{
    updateModel(m_Obs);
    m_CurIndex=m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjIndex(m_CurSoItem);
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

void WIView::onDownloadIconClicked(){
    QObject * descTextObj = m_DetailsViewObj->findChild<QObject *>("descTextObj");
    QObject * infoBoxTextObj = m_DetailsViewObj->findChild<QObject *>("infoBoxText");
    if(m_CurSoItem){
        tryToLoadDescFromWikipedia(descTextObj, m_CurSoItem);
        tryToLoadInfoBoxFromWikipedia(infoBoxTextObj, m_CurSoItem, getWikipediaName(m_CurSoItem));
    }
}

void WIView::updateModel(ObsConditions * obs)
{
    m_Ctxt->setContextProperty("soListModel",0); //This is needed so that when the model updates, it will properly update the ListView.

    m_Obs = obs;
    m_ModManager->updateModel(m_Obs,m_CurrentObjectListName);

    if (m_CurrentObjectListName !="")
        m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(m_CurrentObjectListName));
}

void WIView::inspectSkyObject(QString name){
        SkyObject *obj=KStarsData::Instance()->skyComposite()->findByName(name);
        if(obj)
            inspectSkyObject(obj);
}

void WIView::inspectSkyObject(SkyObject *obj){
    if(obj){
        if(obj->name()!="star"){
            loadDetailsView(new SkyObjItem(obj),-1);
            m_BaseObj->setProperty("state", "singleItemSelected");
            m_CategoryTitle->setProperty("text", "Selected Object");
        }
    }
}

void WIView::loadDetailsView(SkyObjItem * soitem, int index)
{
    m_CurSoItem = soitem;
    m_CurIndex = index;
    int modelSize;
    if(index==-1)
        modelSize=-1;
    else
        modelSize = m_ModManager->returnModel(m_CurrentObjectListName)->rowCount();

    if(modelSize <= 1)
    {
        m_NextObj->setProperty("visible", "false");
        m_PrevObj->setProperty("visible", "false");
    }
    else
    {
        SkyObjItem * nextItem = m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem((m_CurIndex+1)%modelSize);
        SkyObjItem * prevItem = m_ModManager->returnModel(m_CurrentObjectListName)->getSkyObjItem((m_CurIndex-1+modelSize)%modelSize);
        m_NextObj->setProperty("visible", "true");
        m_PrevObj->setProperty("visible", "true");
        QObject * nextTextObj = m_NextObj->findChild<QObject *>("nextTextObj");
        nextTextObj->setProperty("text", nextItem->getName());
        QObject * prevTextObj = m_PrevObj->findChild<QObject *>("prevTextObj");
        prevTextObj->setProperty("text", prevItem->getName());
    }

    QObject * sonameObj = m_DetailsViewObj->findChild<QObject *>("sonameObj");
    QObject * posTextObj = m_DetailsViewObj->findChild<QObject *>("posTextObj");
    QObject * detailImage = m_DetailsViewObj->findChild<QObject *>("detailImage");
    QObject * descTextObj = m_DetailsViewObj->findChild<QObject *>("descTextObj");
    QObject * infoBoxTextObj = m_DetailsViewObj->findChild<QObject *>("infoBoxText");
    QObject * detailsTextObj = m_DetailsViewObj->findChild<QObject *>("detailsTextObj");
    QObject * autoCenterCheckBox = m_DetailsViewObj->findChild<QObject *>("autoCenterCheckbox");

    sonameObj->setProperty("text", soitem->getDescName());
    posTextObj->setProperty("text", soitem->getPosition());
    detailImage->setProperty("refreshableSource", soitem->getImageURL(false));
    //if(soitem->getType() != SkyObjItem::Planet)
    loadObjectDescription(descTextObj, soitem);

    infoBoxTextObj->setProperty("text", "trying to Load infoText box from Wikipedia. . .");
    loadObjectInfoBox(infoBoxTextObj, soitem);

    QString summary=soitem->getSummary(false);

    QString magText;
    if (soitem->getType() == SkyObjItem::Constellation)
        magText = xi18n("Magnitude:  --");
    else
        magText = xi18n("Magnitude: %1", QLocale().toString(soitem->getMagnitude(),'f', 2));

    QString sbText = xi18n("Surface Brightness: %1", soitem->getSurfaceBrightness());

    QString sizeText = xi18n("Size: %1", soitem->getSize());

    QString details = summary + "<BR>" + sbText + "<BR>" + magText + "<BR>" + sizeText;
    detailsTextObj->setProperty("text", details);

    if(autoCenterCheckBox->property("checked")==true){
        QTimer::singleShot(500, this, SLOT(onCenterButtonClicked()));
    }

    if(m_CurIndex!=-1)
        m_SoListObj->setProperty("currentIndex", m_CurIndex);

}

QString WIView::getWikipediaName(SkyObjItem *soitem){
    QString name;
    if(soitem->getName().toLower().startsWith("m "))
        name = soitem->getName().replace("M ","Messier_").remove( ' ' );
    else if(soitem->getName().toLower().startsWith("ngc"))
        name = soitem->getName().toLower().replace("ngc","NGC_").remove( ' ' );
    else if(soitem->getName().toLower().startsWith("ic"))
        name = soitem->getName().toLower().replace("ic","IC_").remove( ' ' );
    else if(soitem->getType() == SkyObjItem::Constellation){
        QStringList words = soitem->getName().split(" ");
        for(int i=0;i<words.size();i++){
            QString temp=words.at(i).toLower();
            temp[0]=temp[0].toUpper();
            words.replace(i,temp);
        }
        name = words.join("_")+ "_(constellation)";
    }
    else if(soitem->getTypeName() == "Asteroid")
        name = soitem->getName().remove( ' ' ) + "_(asteroid)";
    else if(soitem->getTypeName() == "Comet")
        name = soitem->getLongName();
    else if(soitem->getType() == SkyObjItem::Planet&&soitem->getName()!="Sun"&&soitem->getName()!="Moon")
        name = soitem->getName().remove( ' ' ) + "_(planet)";
    else if(soitem->getType() == SkyObjItem::Star){
        name = soitem->getName().remove( ' ' );
    }else
        name = soitem->getName().remove( ' ' );
    return name;
}

void WIView::tryToLoadDescFromWikipedia(QObject * descTextObj, SkyObjItem * soitem)
{
    QString name=getWikipediaName(soitem);
    if(soitem->getType() == SkyObjItem::Star){
    StarObject *star=dynamic_cast<StarObject*>(soitem->getSkyObject());
        if(star)
            name = star->gname().toLower().remove( ' ' ); //the greek name seems to give the most consistent search results for opensearch.
        else
            name = soitem->getName().toLower().remove( ' ' );
    }

    QUrl url("https://en.wikipedia.org/w/api.php?action=opensearch&search=" + name + "&format=xml");

    QNetworkReply * response = manager->get(QNetworkRequest(url));
    QTimer::singleShot(30000, response, [response]
    { //Shut it down after 30 sec.
        response->abort();
        response->deleteLater();
        qDebug()<<"Wikipedia Download Timed out.";
    });
    connect(response, &QNetworkReply::finished, this, [soitem, this, descTextObj, response]{
      response->deleteLater();
      if (response->error() != QNetworkReply::NoError) return;
      QString contentType =
        response->header(QNetworkRequest::ContentTypeHeader).toString();
      if (!contentType.contains("charset=utf-8")) {
        qWarning() << "Content charsets other than utf-8 are not implemented yet.";
        return;
      }
    QString result = QString::fromUtf8(response->readAll());
    int leftPos=result.indexOf("<Description")+34;
    if(leftPos<34)
        return;
    int rightPos=result.indexOf("</Description>")-leftPos;

    int leftURL=result.indexOf("<Url xml:space=\"preserve\">")+26;
    int rightURL=result.indexOf("</Url>")-leftURL;

    QString srchtml="\n<p style=text-align:right>Source: (<a href='" + result.mid(leftURL,rightURL) + "'>" + "Wikipedia</a>)";  //Note the \n is so that the description is put on another line in the file.  Doesn't affect the display but allows the source to be loaded in the details but not the list.
    QString html="<HTML>" + result.mid(leftPos,rightPos) + srchtml + "</HTML>";

      saveObjectText( soitem, "description", html);
      descTextObj->setProperty("text", html);
    });


}

void WIView::loadObjectDescription(QObject * descTextObj, SkyObjItem * soitem){
    QFile file;
    QString fname = "description-" + soitem->getName().toLower().remove( ' ' ) + ".html";
    file.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "descriptions/" + fname ) ; //determine filename in local user KDE directory tree.

    if(file.exists()){

        if(file.open(QIODevice::ReadOnly))
        {
            QTextStream in(&file);
            QString line = in.readAll();
            descTextObj->setProperty("text", line);
            file.close();
        }
    }
    else{
        tryToLoadDescFromWikipedia(descTextObj, soitem);
    }
}

void WIView::loadObjectInfoBox(QObject * infoBoxText, SkyObjItem * soitem)
{
    QFile file;
    QString fname = "infoText-" + soitem->getName().toLower().remove( ' ' ) + ".html";
    file.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "descriptions/" + fname ) ; //determine filename in local user KDE directory tree.

    if(file.exists())
    {
        if(file.open(QIODevice::ReadOnly))
        {
            QTextStream in(&file);
            QString infoBoxHTML;
            while ( !in.atEnd() )
            {
                infoBoxHTML = in.readAll();
                QString wikiImageName=KSPaths::locate(QStandardPaths::GenericDataLocation, "descriptions/wikiImage-" + soitem->getName().toLower().remove( ' ' ) + ".png" );
                if(wikiImageName!=""){
                    int captionEnd = infoBoxHTML.indexOf("</caption>"); //Start looking for the image AFTER the caption.  Planets have images in their caption.
                    if(captionEnd == -1)
                        captionEnd = 0;
                    int leftImg=infoBoxHTML.indexOf("src=\"", captionEnd)+5;
                    int rightImg=infoBoxHTML.indexOf("\"",leftImg)-leftImg;
                    QString imgURL=infoBoxHTML.mid(leftImg,rightImg);
                    infoBoxHTML.replace(imgURL,wikiImageName);
                }
                infoBoxText->setProperty("text", infoBoxHTML);
            }
            file.close();
        }
    }
    else
    {
        tryToLoadInfoBoxFromWikipedia(infoBoxText, soitem, getWikipediaName(soitem));
    }
}

void WIView::tryToLoadInfoBoxFromWikipedia(QObject * infoBoxText, SkyObjItem * soitem, QString name)
{
    QUrl url("https://en.wikipedia.org/w/index.php?action=render&title="+ name + "&redirects");

    QNetworkReply * response = manager->get(QNetworkRequest(url));
    QTimer::singleShot(30000, response, [response]
    { //Shut it down after 30 sec.
        response->abort();
        response->deleteLater();
        qDebug()<<"Wikipedia Download Timed out.";
    });
    connect(response, &QNetworkReply::finished, this, [infoBoxText, name, response, soitem, this]{
        response->deleteLater();
        if (response->error() != QNetworkReply::NoError) return;

        QString result = QString::fromUtf8(response->readAll());
        int leftPos=result.indexOf("<table class=\"infobox");
        int rightPos=result.indexOf("</table>", leftPos)-leftPos;

        if(leftPos==-1){
            if(soitem->getType()==SkyObjItem::Star&&!name.contains("_(star)"))
                tryToLoadInfoBoxFromWikipedia(infoBoxText, soitem, name+"_(star)");
            return;
        }

        QString infoText=result.mid(leftPos,rightPos);

        //This if statement should correct for a situation like for the planets where there is a single internal table inside the infoText Box.
        if(infoText.indexOf("<table",leftPos+6)!=-1){
            rightPos=result.indexOf("</table>", leftPos+rightPos+6)-leftPos;
            infoText=result.mid(leftPos,rightPos);
        }

        //This next line is for the headers in the colored boxes. It turns them black instead of white because they are more visible that way.
        infoText.replace("background: #","color:black;background: #").replace("background-color: #","color:black;background: #").replace("background:#","color:black;background:#").replace("background-color:#","color:black;background:#").replace("background: pink","color:black;background: pink");
        infoText.replace("//","http://"); //This is to fix links on wikipedia which are missing http from the url
        infoText.replace("https:http:","https:").replace("http:http:","http:");//Just in case it was done to an actual complete url

        int annotationLeft=infoText.indexOf("<annotation");
        int annotationRight=infoText.indexOf("</annotation>" , annotationLeft) + 13 - annotationLeft;
        infoText.remove(annotationLeft,annotationRight); //This removes the annotation that does not render correctly for some DSOs.

        int mathLeft=infoText.indexOf("<img src=\"https://wikimedia.org/api/rest_v1/media/math");
        int mathRight=infoText.indexOf(">" , mathLeft) + 1 - mathLeft ;
        infoText.remove(mathLeft, mathRight);//This removes an image that doesn't render properly for some DSOs.

        infoText.replace("style=\"width:22em\"","style=\"width:100%;background-color: black;color: white;\"");
        infoText=infoText + "<BR>(Source: <a href='" + "https://en.wikipedia.org/w/index.php?title="+ name + "&redirects" + "'>Wikipedia</a>)";

        int captionEnd = infoText.indexOf("</caption>"); //Start looking for the image AFTER the caption.  Planets have images in their caption.
        if(captionEnd == -1)
            captionEnd = 0;
        int leftImg=infoText.indexOf("src=\"", captionEnd)+5;
        int rightImg=infoText.indexOf("\"",leftImg)-leftImg;
        QString imgURL=infoText.mid(leftImg,rightImg);
        imgURL.replace("http://upload.wikimedia.org","https://upload.wikimedia.org"); //Although they will display, the images apparently don't download properly unless they are https.
        saveImageURL( soitem, imgURL);
        downloadWikipediaImage(soitem, imgURL);

        QString html="<HTML><style type=text/css>a {text-decoration: none;color: yellow}</style><body><CENTER>" +  infoText + "</CENTER></body></HTML>";
        infoBoxText->setProperty("text", html);
        saveObjectText( soitem, "infoText", html);
    });
}

void WIView::saveObjectText(SkyObjItem * soitem, QString type, QString text){

    QFile file;
    QString fname = type + "-" + soitem->getName().toLower().remove( ' ' ) + ".html";

    QDir writableDir;
    QString filePath=KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "descriptions";
    writableDir.mkpath(filePath);

    file.setFileName( filePath + "/" + fname ) ; //determine filename in local user KDE directory tree.

    if(file.exists())
        return;

    if (file.open(QIODevice::WriteOnly) == false)
    {
        qDebug()<<"Image text cannot be saved for later.  file save error";
        return;
    }
    else
    {
        QTextStream stream( &file );
        stream << text;
        file.close();
    }
}



void WIView::saveImageURL(SkyObjItem * soitem, QString imageURL){

    QFile file;
    file.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "image_url.dat" ) ; //determine filename in local user KDE directory tree.
    QString entry = soitem->getName() + ':' + "Show Wikipedia Image" + ':' + imageURL;

    if(file.open(QIODevice::ReadOnly)){
        QTextStream in(&file);
        QString line;
        while ( !in.atEnd() )
        {
            line = in.readLine();
            if (line==entry){
                file.close();
                return;
            }
        }
        file.close();
    }

    if ( !file.open( QIODevice::ReadWrite | QIODevice::Append ) )
    {
        qDebug()<<"Image URL cannot be saved for later.  image_url.dat error";
        return;
    }
    else
    {
        QTextStream stream( &file );
        stream << entry << endl;
        file.close();
    }
}


void WIView::downloadWikipediaImage(SkyObjItem * soitem, QString imageURL)
{
    QString fname = "wikiImage-" + soitem->getName().toLower().remove( ' ' ) + ".png";

    QDir writableDir;
    QString filePath=KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "descriptions";
    writableDir.mkpath(filePath);

    QString fileN =filePath + "/" + fname;

    QNetworkReply * response = manager->get(QNetworkRequest(QUrl(imageURL)));
    QTimer::singleShot(60000, response, [response]
    { //Shut it down after 60 sec.
        response->abort();
        response->deleteLater();
        qDebug()<<"Image Download Timed out.";
    });
    connect(response, &QNetworkReply::finished, this, [fileN, response, this]{
      response->deleteLater();
      if (response->error() != QNetworkReply::NoError) return;
        QImage* image = new QImage();
        QByteArray responseData=response->readAll();
        if(image->loadFromData(responseData)){
            image->save(fileN);
            onReloadIconClicked(); //This is to update the images displayed with the new image.
        }
        else
            qDebug()<<"image not downloaded";
    });


}


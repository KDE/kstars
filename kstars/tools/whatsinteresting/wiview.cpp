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


#ifdef HAVE_INDI
#include <basedevice.h>
#include "indi/indilistener.h"
#include "indi/indistd.h"
#include "indi/driverinfo.h"
#endif


WIView::WIView(QWidget * parent, ObsConditions * obs) : QWidget(parent), m_Obs(obs), m_CurCategorySelected(-1)
{


    m_ModManager = new ModelManager(m_Obs);

    m_BaseView = new QQuickView();

    ///To use i18n() instead of qsTr() in qml/wiview.qml for translation
    //KDeclarative kd;
    // kd.setDeclarativeEngine(m_BaseView->engine());
    //kd.initialize();
    //kd.setupBindings();

    m_Ctxt = m_BaseView->rootContext();

    ///Use instead of KDeclarative
    m_Ctxt->setContextObject(new KLocalizedContext(m_BaseView));

    m_BaseView->setSource(QUrl::fromLocalFile(KSPaths::locate(QStandardPaths::AppDataLocation, "tools/whatsinteresting/qml/wiview.qml")));

    m_BaseObj = m_BaseView->rootObject();

    //soTypeTextObj = m_BaseObj->findChild<QObject *>("soTypeTextObj");

    m_ViewsRowObj = m_BaseObj->findChild<QQuickItem *>(QString("viewsRowObj"));
    connect(m_ViewsRowObj, SIGNAL(categorySelected(int)), this, SLOT(onCategorySelected(int)));

    m_SoListObj = m_BaseObj->findChild<QQuickItem *>("soListObj");
    connect(m_SoListObj, SIGNAL(soListItemClicked(int, QString, int)), this, SLOT(onSoListItemClicked(int, QString, int)));

    m_DetailsViewObj = m_BaseObj->findChild<QQuickItem *>("detailsViewObj");

    m_NextObj = m_BaseObj->findChild<QQuickItem *>("nextObj");
    connect(m_NextObj, SIGNAL(nextObjClicked()), this, SLOT(onNextObjClicked()));
    m_PrevObj = m_BaseObj->findChild<QQuickItem *>("prevObj");
    connect(m_PrevObj, SIGNAL(prevObjClicked()), this, SLOT(onPrevObjClicked()));

    m_SlewButtonObj = m_BaseObj->findChild<QQuickItem *>("slewButtonObj");
    connect(m_SlewButtonObj, SIGNAL(slewButtonClicked()), this, SLOT(onSlewButtonClicked()));

    m_SlewTelescopeButtonObj = m_BaseObj->findChild<QQuickItem *>("slewTelescopeButtonObj");
    connect(m_SlewTelescopeButtonObj, SIGNAL(slewTelescopeButtonClicked()), this, SLOT(onSlewTelescopeButtonClicked()));

    m_DetailsButtonObj = m_BaseObj->findChild<QQuickItem *>("detailsButtonObj");
    connect(m_DetailsButtonObj, SIGNAL(detailsButtonClicked()), this, SLOT(onDetailsButtonClicked()));

    QObject * settingsIconObj = m_BaseObj->findChild<QQuickItem *>("settingsIconObj");
    connect(settingsIconObj, SIGNAL(settingsIconClicked()), this, SLOT(onSettingsIconClicked()));

    QObject * reloadIconObj = m_BaseObj->findChild<QQuickItem *>("reloadIconObj");
    connect(reloadIconObj, SIGNAL(reloadIconClicked()), this, SLOT(onReloadIconClicked()));

    m_BaseView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_BaseView->show();
}

WIView::~WIView()
{
    delete m_ModManager;
    delete m_CurSoItem;
}

void WIView::onCategorySelected(int type)
{
    m_CurCategorySelected = type;
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(type));
}

void WIView::onSoListItemClicked(int type, QString typeName, int index)
{
    Q_UNUSED(typeName)
    Q_UNUSED(type)

    SkyObjItem * soitem = m_ModManager->returnModel(m_CurCategorySelected)->getSkyObjItem(index);

//    soTypeTextObj->setProperty("text", typeName);
//    soTypeTextObj->setProperty("visible", true);

//    soListObj->setProperty("visible", false);

    loadDetailsView(soitem, index);
}

void WIView::onNextObjClicked()
{
    int modelSize = m_ModManager->returnModel(m_CurCategorySelected)->rowCount();
    SkyObjItem * nextItem = m_ModManager->returnModel(m_CurCategorySelected)->getSkyObjItem((m_CurIndex+1)%modelSize);
    loadDetailsView(nextItem, (m_CurIndex+1)%modelSize);
}

void WIView::onPrevObjClicked()
{
    int modelSize = m_ModManager->returnModel(m_CurCategorySelected)->rowCount();
    SkyObjItem * prevItem = m_ModManager->returnModel(m_CurCategorySelected)->getSkyObjItem((m_CurIndex-1+modelSize)%modelSize);
    loadDetailsView(prevItem, (m_CurIndex-1+modelSize)%modelSize);
}

void WIView::onSlewButtonClicked()
{
    ///Slew map to selected sky-object
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
        onSlewButtonClicked();

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
    updateModels(m_Obs);
}

void WIView::updateModels(ObsConditions * obs)
{
    m_Obs = obs;
    m_ModManager->updateModels(m_Obs);
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(0));
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(1));
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(2));
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(3));
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(4));
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(5));
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(6));
    m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(7));

    if (m_CurCategorySelected >=0 && m_CurCategorySelected <= 7)
        m_Ctxt->setContextProperty("soListModel", m_ModManager->returnModel(m_CurCategorySelected));
}

void WIView::loadDetailsView(SkyObjItem * soitem, int index)
{
    m_CurSoItem = soitem;
    m_CurIndex = index;

    int modelSize = m_ModManager->returnModel(m_CurCategorySelected)->rowCount();
    SkyObjItem * nextItem = m_ModManager->returnModel(m_CurCategorySelected)->getSkyObjItem((m_CurIndex+1)%modelSize);
    SkyObjItem * prevItem = m_ModManager->returnModel(m_CurCategorySelected)->getSkyObjItem((m_CurIndex-1+modelSize)%modelSize);

    if(modelSize <= 1)
    {
        m_NextObj->setProperty("visible", "false");
        m_PrevObj->setProperty("visible", "false");
    }
    else
    {
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

    sonameObj->setProperty("text", soitem->getLongName());
    posTextObj->setProperty("text", soitem->getPosition());
    detailImage->setProperty("refreshableSource", soitem->getImageURL(false));
    descTextObj->setProperty("text", soitem->getDesc());
    if(soitem->getType() != SkyObjItem::Planet)
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
        onSlewButtonClicked();
    }

}


void WIView::tryToLoadDescFromWikipedia(QObject * descTextObj, SkyObjItem * soitem)
{
    QString name;
    if(soitem->getName().startsWith("M "))
        name = soitem->getName().replace("M ","messier_").toLower().remove( ' ' );
    else if(soitem->getType() == SkyObjItem::Constellation)
        name = soitem->getName().toLower().remove( ' ' ) + "_constellation";
    else if(soitem->getType() == SkyObjItem::Star){
        StarObject *star=dynamic_cast<StarObject*>(soitem->getSkyObject());
        if(star)
            name = star->gname().toLower().remove( ' ' ); //the greek name seems to give the most consistent search results.
        else
            name = soitem->getName().toLower().remove( ' ' );
    }else
        name = soitem->getName().toLower().remove( ' ' );

    QUrl url("https://en.wikipedia.org/w/api.php?action=opensearch&search=" + name + "&format=xml");

    QNetworkAccessManager * manager = new QNetworkAccessManager();
    QNetworkReply * response = manager->get(QNetworkRequest(url));
    connect(manager, &QNetworkAccessManager::finished, this, [soitem, this, descTextObj, manager, response]{
      response->deleteLater();
      manager->deleteLater();
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
            QString line;
            while ( !in.atEnd() )
            {
                line = in.readAll();
                infoBoxText->setProperty("text", line);
                QTimer::singleShot(500, infoBoxText, SLOT(doLayout()));
            }
            file.close();
        }
    }
    else
    {
        tryToLoadInfoBoxFromWikipedia(infoBoxText, soitem);
    }
}

void WIView::tryToLoadInfoBoxFromWikipedia(QObject * infoBoxText, SkyObjItem * soitem)
{
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
    else if(soitem->getType() == SkyObjItem::Planet)
        name = soitem->getName().remove( ' ' ) + "_(planet)";
    else if(soitem->getType() == SkyObjItem::Star){
        StarObject *star=dynamic_cast<StarObject*>(soitem->getSkyObject());
        if(star)
            name = star->gname().toLower().remove( ' ' ); //the greek name seems to give the most consistent search results.
        else
            name = soitem->getName().toLower().remove( ' ' );
    }else
        name = soitem->getName().toLower().remove( ' ' );
    qDebug()<<name;

    QUrl url("https://en.wikipedia.org/w/index.php?action=render&title="+ name + "&redirects");

    QNetworkAccessManager * manager = new QNetworkAccessManager();
    QNetworkReply * response = manager->get(QNetworkRequest(url));
    connect(manager, &QNetworkAccessManager::finished, this, [infoBoxText, name, manager, response, soitem, this]{
      if (response->error() != QNetworkReply::NoError) return;
    QString result = QString::fromUtf8(response->readAll());
    int leftPos=result.indexOf("<table class=\"infobox");
    int rightPos=result.indexOf("</table>")-leftPos;

    QString infoText=result.mid(leftPos,rightPos).replace("href=\"//","href=\"http://").replace("src=\"//","src=\"http://").replace("background:","color:black;background:").replace("background-color:","color:black;background:");
    infoText.replace("style=\"width:22em\"","style=\"width:100%;border: 1px solid #a2a9b1;border-spacing: 3px;background-color: black;color: white;margin: 0.5em 0 0.5em 1em;padding: 0.2em;line-height: 1.5em;\"");
    infoText=infoText + "<BR>(Source: <a href='" + "https://en.wikipedia.org/w/index.php?title="+ name + "&redirects" + "'>Wikipedia</a>)";
    int leftImg=infoText.indexOf("src=\"")+5;
    int rightImg=infoText.indexOf("\"",leftImg)-leftImg;
    QString imgURL=infoText.mid(leftImg,rightImg);
    saveImageURL( soitem, imgURL);
    downloadWikipediaImage(soitem, imgURL);

    QString html="<HTML><style type=text/css>a {text-decoration: none;color: yellow}</style><body><CENTER>" +  infoText + "</CENTER></body></HTML>";
    infoBoxText->setProperty("text", html);
    saveObjectText( soitem, "infoText", html);

      QTimer::singleShot(500, infoBoxText, SLOT(doLayout())); //This should make the image load after the html loads

      response->deleteLater();
      manager->deleteLater();
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

    //qDebug()<<fileN;
    //qDebug()<<imageURL;

    QUrl url(imageURL);

    QNetworkAccessManager * manager = new QNetworkAccessManager();
    QNetworkReply * response = manager->get(QNetworkRequest(url));
    connect(manager, &QNetworkAccessManager::finished, this, [fileN, manager, response, this]{
      response->deleteLater();
      manager->deleteLater();
      if (response->error() != QNetworkReply::NoError) return;

        QImage* image = new QImage();
        QByteArray responseData=response->readAll();
        image->loadFromData(responseData);
        image->save(fileN);


    });


}


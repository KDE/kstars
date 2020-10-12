/***************************************************************************
                          observinglist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 29 Nov 2004
    copyright            : (C) 2004-2020 by Jeff Woods, Jason Harris,
                           Prakash Mohan, Akarsh Simha
    email                : jcwoods@bellsouth.net, jharris@30doradus.org,
                           prakash.mohan@kdemail.net, akarsh@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "observinglist.h"

#include "config-kstars.h"

#include "constellationboundarylines.h"
#include "fov.h"
#include "imageviewer.h"
#include "ksalmanac.h"
#include "ksnotification.h"
#include "ksdssdownloader.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "obslistpopupmenu.h"
#include "obslistwizard.h"
#include "Options.h"
#include "sessionsortfilterproxymodel.h"
#include "skymap.h"
#include "thumbnailpicker.h"
#include "dialogs/detaildialog.h"
#include "dialogs/finddialog.h"
#include "dialogs/locationdialog.h"
#include "oal/execute.h"
#include "skycomponents/skymapcomposite.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "tools/altvstime.h"
#include "tools/eyepiecefield.h"
#include "tools/wutdialog.h"

#ifdef HAVE_INDI
#include <basedevice.h>
#include "indi/indilistener.h"
#include "indi/drivermanager.h"
#include "indi/driverinfo.h"
#include "ekos/manager.h"
#endif

#include <KPlotting/KPlotAxis>
#include <KPlotting/KPlotObject>
#include <KMessageBox>
#include <QMessageBox>

#include <kstars_debug.h>

//
// ObservingListUI
// ---------------------------------
ObservingListUI::ObservingListUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

//
// ObservingList
// ---------------------------------
ObservingList::ObservingList()
    : QDialog((QWidget *)KStars::Instance()), LogObject(nullptr), m_CurrentObject(nullptr), isModified(false), m_dl(nullptr),
      m_manager{ CatalogsDB::dso_db_path() }
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    ui                      = new ObservingListUI(this);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setWindowTitle(i18nc("@title:window", "Observation Planner"));

    setLayout(mainLayout);

    dt = KStarsDateTime::currentDateTime();
    setFocusPolicy(Qt::StrongFocus);
    geo            = KStarsData::Instance()->geo();
    sessionView    = false;
    m_listFileName = QString();
    pmenu.reset(new ObsListPopupMenu());
    //Set up the Table Views
    m_WishListModel.reset(new QStandardItemModel(0, 5, this));
    m_SessionModel.reset(new QStandardItemModel(0, 5));

    m_WishListModel->setHorizontalHeaderLabels(
        QStringList() << i18n("Name") << i18n("Alternate Name") << i18nc("Right Ascension", "RA (J2000)")
        << i18nc("Declination", "Dec (J2000)") << i18nc("Magnitude", "Mag") << i18n("Type")
        << i18n("Current Altitude"));
    m_SessionModel->setHorizontalHeaderLabels(
        QStringList() << i18n("Name") << i18n("Alternate Name") << i18nc("Right Ascension", "RA (J2000)")
        << i18nc("Declination", "Dec (J2000)") << i18nc("Magnitude", "Mag") << i18n("Type")
        << i18nc("Constellation", "Constell.") << i18n("Time") << i18nc("Altitude", "Alt")
        << i18nc("Azimuth", "Az"));

    m_WishListSortModel.reset(new QSortFilterProxyModel(this));
    m_WishListSortModel->setSourceModel(m_WishListModel.get());
    m_WishListSortModel->setDynamicSortFilter(true);
    m_WishListSortModel->setSortRole(Qt::UserRole);
    ui->WishListView->setModel(m_WishListSortModel.get());
    ui->WishListView->horizontalHeader()->setStretchLastSection(true);

    ui->WishListView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_SessionSortModel.reset(new SessionSortFilterProxyModel());
    m_SessionSortModel->setSourceModel(m_SessionModel.get());
    m_SessionSortModel->setDynamicSortFilter(true);
    ui->SessionView->setModel(m_SessionSortModel.get());
    ui->SessionView->horizontalHeader()->setStretchLastSection(true);
    ui->SessionView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ksal.reset(new KSAlmanac);
    ksal->setLocation(geo);
    ui->avt->setGeoLocation(geo);
    ui->avt->setSunRiseSetTimes(ksal->getSunRise(), ksal->getSunSet());
    ui->avt->setLimits(-12.0, 12.0, -90.0, 90.0);
    ui->avt->axis(KPlotWidget::BottomAxis)->setTickLabelFormat('t');
    ui->avt->axis(KPlotWidget::BottomAxis)->setLabel(i18n("Local Time"));
    ui->avt->axis(KPlotWidget::TopAxis)->setTickLabelFormat('t');
    ui->avt->axis(KPlotWidget::TopAxis)->setTickLabelsShown(true);
    ui->DateEdit->setDate(dt.date());
    ui->SetLocation->setText(geo->fullName());
    ui->ImagePreview->installEventFilter(this);
    ui->WishListView->viewport()->installEventFilter(this);
    ui->WishListView->installEventFilter(this);
    ui->SessionView->viewport()->installEventFilter(this);
    ui->SessionView->installEventFilter(this);
    // setDefaultImage();
    //Connections
    connect(ui->WishListView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(slotCenterObject()));
    connect(ui->WishListView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(slotNewSelection()));
    connect(ui->SessionView->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            this, SLOT(slotNewSelection()));
    connect(ui->WUTButton, SIGNAL(clicked()), this, SLOT(slotWUT()));
    connect(ui->FindButton, SIGNAL(clicked()), this, SLOT(slotFind()));
    connect(ui->OpenButton, SIGNAL(clicked()), this, SLOT(slotOpenList()));
    connect(ui->SaveButton, SIGNAL(clicked()), this, SLOT(slotSaveSession()));
    connect(ui->SaveAsButton, SIGNAL(clicked()), this, SLOT(slotSaveSessionAs()));
    connect(ui->WizardButton, SIGNAL(clicked()), this, SLOT(slotWizard()));
    connect(ui->batchAddButton, SIGNAL(clicked()), this, SLOT(slotBatchAdd()));
    connect(ui->SetLocation, SIGNAL(clicked()), this, SLOT(slotLocation()));
    connect(ui->Update, SIGNAL(clicked()), this, SLOT(slotUpdate()));
    connect(ui->DeleteImage, SIGNAL(clicked()), this, SLOT(slotDeleteCurrentImage()));
    connect(ui->SearchImage, SIGNAL(clicked()), this, SLOT(slotSearchImage()));
    connect(ui->SetTime, SIGNAL(clicked()), this, SLOT(slotSetTime()));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(slotChangeTab(int)));
    connect(ui->saveImages, SIGNAL(clicked()), this, SLOT(slotSaveAllImages()));
    connect(ui->DeleteAllImages, SIGNAL(clicked()), this, SLOT(slotDeleteAllImages()));
    connect(ui->OALExport, SIGNAL(clicked()), this, SLOT(slotOALExport()));
    connect(ui->clearListB, SIGNAL(clicked()), this, SLOT(slotClearList()));
    //Add icons to Push Buttons
    ui->OpenButton->setIcon(QIcon::fromTheme("document-open"));
    ui->OpenButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ui->SaveButton->setIcon(QIcon::fromTheme("document-save"));
    ui->SaveButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ui->SaveAsButton->setIcon(
        QIcon::fromTheme("document-save-as"));
    ui->SaveAsButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ui->WizardButton->setIcon(QIcon::fromTheme("tools-wizard"));
    ui->WizardButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    noSelection = true;
    showScope   = false;
    ui->NotesEdit->setEnabled(false);
    ui->SetTime->setEnabled(false);
    ui->TimeEdit->setEnabled(false);
    ui->SearchImage->setEnabled(false);
    ui->saveImages->setEnabled(false);
    ui->DeleteImage->setEnabled(false);
    ui->OALExport->setEnabled(false);

    m_NoImagePixmap =
        QPixmap(":/images/noimage.png")
        .scaled(ui->ImagePreview->width(), ui->ImagePreview->height(), Qt::KeepAspectRatio, Qt::FastTransformation);
    m_altCostHelper = [this](const SkyPoint & p) -> QStandardItem *
    {
        const double inf = std::numeric_limits<double>::infinity();
        double altCost   = 0.;
        QString itemText;
        double maxAlt = p.maxAlt(*(geo->lat()));
        if (Options::obsListDemoteHole() && maxAlt > 90. - Options::obsListHoleSize())
            maxAlt = 90. - Options::obsListHoleSize();
        if (maxAlt <= 0.)
        {
            altCost  = -inf;
            itemText = i18n("Never rises");
        }
        else
        {
            altCost = (p.alt().Degrees() / maxAlt) * 100.;
            if (altCost < 0)
                itemText = i18nc("Short text to describe that object has not risen yet", "Not risen");
            else
            {
                if (altCost > 100.)
                {
                    altCost  = -inf;
                    itemText = i18nc("Object is in the Dobsonian hole", "In hole");
                }
                else
                    itemText = QString::number(altCost, 'f', 0) + '%';
            }
        }

        QStandardItem *altItem = new QStandardItem(itemText);
        altItem->setData(altCost, Qt::UserRole);
        //        qCDebug(KSTARS) << "Updating altitude for " << p.ra().toHMSString() << " " << p.dec().toDMSString() << " alt = " << p.alt().toDMSString() << " info to " << itemText;
        return altItem;
    };

    // Needed to fix weird bug on Windows that started with Qt 5.9 that makes the title bar
    // not visible and therefore dialog not movable.
#ifdef Q_OS_WIN
    move(100, 100);
#endif
}

void ObservingList::showEvent(QShowEvent *)
{
    // ONLY run for first ever load

    if (m_initialWishlistLoad == false)
    {
        m_initialWishlistLoad = true;

        slotLoadWishList(); //Load the wishlist from disk if present
        m_CurrentObject = nullptr;
        setSaveImagesButton();

        slotUpdateAltitudes();
        m_altitudeUpdater = new QTimer(this);
        connect(m_altitudeUpdater, SIGNAL(timeout()), this, SLOT(slotUpdateAltitudes()));
        m_altitudeUpdater->start(120000); // update altitudes every 2 minutes
    }
}

//SLOTS

void ObservingList::slotAddObject(const SkyObject *_obj, bool session, bool update)
{
    bool addToWishList = true;
    if (!_obj)
        _obj = SkyMap::Instance()->clickedObject(); // Eh? Why? Weird default behavior.

    if (!_obj)
    {
        qCWarning(KSTARS) << "Trying to add null object to observing list! Ignoring.";
        return;
    }

    QString finalObjectName = getObjectName(_obj);

    if (finalObjectName.isEmpty())
    {
        KSNotification::sorry(i18n("Unnamed stars are not supported in the observing lists"));
        return;
    }

    //First, make sure object is not already in the list
    QSharedPointer<SkyObject> obj = findObject(_obj);
    if (obj)
    {
        addToWishList = false;
        if (!session)
        {
            KStars::Instance()->statusBar()->showMessage(
                i18n("%1 is already in your wishlist.", finalObjectName),
                0); // FIXME: This message is too inconspicuous if using the Find dialog to add
            return;
        }
    }
    else
    {
        assert(!findObject(_obj, session));
        qCDebug(KSTARS) << "Cloned object " << finalObjectName << " to add to observing list.";
        obj = QSharedPointer<SkyObject>(
                  _obj->clone()); // Use a clone in case the original SkyObject is deleted due to change in catalog configuration.
    }

    if (session && sessionList().contains(obj))
    {
        KStars::Instance()->statusBar()->showMessage(i18n("%1 is already in the session plan.", finalObjectName), 0);
        return;
    }

    // JM: If we are loading observing list from disk, solar system objects magnitudes are not calculated until later
    // Therefore, we manual invoke updateCoords to force computation of magnitude.
    if ((obj->type() == SkyObject::COMET || obj->type() == SkyObject::ASTEROID || obj->type() == SkyObject::MOON ||
            obj->type() == SkyObject::PLANET) &&
            obj->mag() == 0)
    {
        KSNumbers num(dt.djd());
        CachingDms LST = geo->GSTtoLST(dt.gst());
        obj->updateCoords(&num, true, geo->lat(), &LST, true);
    }

    QString smag = "--";
    if (-30.0 < obj->mag() && obj->mag() < 90.0)
        smag = QString::number(obj->mag(), 'f', 2); // The lower limit to avoid display of unrealistic comet magnitudes

    SkyPoint p = obj->recomputeHorizontalCoords(dt, geo);

    QList<QStandardItem *> itemList;

    auto getItemWithUserRole = [](const QString & itemText) -> QStandardItem *
    {
        QStandardItem *ret = new QStandardItem(itemText);
        ret->setData(itemText, Qt::UserRole);
        return ret;
    };

    // Fill itemlist with items that are common to both wishlist additions and session plan additions
    auto populateItemList = [&getItemWithUserRole, &itemList, &finalObjectName, obj, &p, &smag]()
    {
        itemList.clear();
        QStandardItem *keyItem = getItemWithUserRole(finalObjectName);
        keyItem->setData(QVariant::fromValue<void *>(static_cast<void *>(obj.data())), Qt::UserRole + 1);
        itemList
                << keyItem // NOTE: The rest of the methods assume that the SkyObject pointer is available in the first column!
                << getItemWithUserRole(obj->translatedLongName()) << getItemWithUserRole(p.ra0().toHMSString())
                << getItemWithUserRole(p.dec0().toDMSString()) << getItemWithUserRole(smag)
                << getItemWithUserRole(obj->typeName());
    };

    //Insert object in the Wish List
    if (addToWishList)
    {
        m_WishList.append(obj);
        m_CurrentObject = obj.data();

        //QString ra, dec;
        //ra = "";//p.ra().toHMSString();
        //dec = p.dec().toDMSString();

        populateItemList();
        // FIXME: Instead sort by a "clever" observability score, calculated as follows:
        //     - First sort by (max altitude) - (current altitude) rounded off to the nearest
        //     - Weight by declination - latitude (in the northern hemisphere, southern objects get higher precedence)
        //     - Demote objects in the hole
        SkyPoint p = obj->recomputeHorizontalCoords(KStarsDateTime::currentDateTimeUtc(), geo); // Current => now
        itemList << m_altCostHelper(p);
        m_WishListModel->appendRow(itemList);

        //Note addition in statusbar
        KStars::Instance()->statusBar()->showMessage(i18n("Added %1 to observing list.", finalObjectName), 0);
        ui->WishListView->resizeColumnsToContents();
        if (!update)
            slotSaveList();
    }
    //Insert object in the Session List
    if (session)
    {
        m_SessionList.append(obj);
        dt.setTime(TimeHash.value(finalObjectName, obj->transitTime(dt, geo)));
        dms lst(geo->GSTtoLST(dt.gst()));
        p.EquatorialToHorizontal(&lst, geo->lat());

        QString alt = "--", az = "--";

        QStandardItem *BestTime = new QStandardItem();
        /* QString ra, dec;
         if(obj->name() == "star" ) {
            ra = obj->ra0().toHMSString();
            dec = obj->dec0().toDMSString();
            BestTime->setData( QString( "--" ), Qt::DisplayRole );
        }
        else {*/
        BestTime->setData(TimeHash.value(finalObjectName, obj->transitTime(dt, geo)), Qt::DisplayRole);
        alt = p.alt().toDMSString();
        az  = p.az().toDMSString();
        //}
        // TODO: Change the rest of the parameters to their appropriate datatypes.
        populateItemList();
        itemList << getItemWithUserRole(KSUtils::constNameToAbbrev(
                                            KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName(obj.data())))
                 << BestTime << getItemWithUserRole(alt) << getItemWithUserRole(az);

        m_SessionModel->appendRow(itemList);
        //Adding an object should trigger the modified flag
        isModified = true;
        ui->SessionView->resizeColumnsToContents();
        //Note addition in statusbar
        KStars::Instance()->statusBar()->showMessage(i18n("Added %1 to session list.", finalObjectName), 0);
        SkyMap::Instance()->forceUpdate();
    }
    setSaveImagesButton();
}

void ObservingList::slotRemoveObject(const SkyObject *_o, bool session, bool update)
{
    if (!update) // EH?!
    {
        if (!_o)
            _o = SkyMap::Instance()->clickedObject();
        else if (sessionView) //else if is needed as clickedObject should not be removed from the session list.
            session = true;
    }

    // Is the pointer supplied in our own lists?
    const QList<QSharedPointer<SkyObject>> &list = (session ? sessionList() : obsList());
    QStandardItemModel *currentModel             = (session ? m_SessionModel.get() : m_WishListModel.get());

    QSharedPointer<SkyObject> o = findObject(_o, session);
    if (!o)
    {
        qWarning() << "Object (name: " << getObjectName(o.data())
                   << ") supplied to ObservingList::slotRemoveObject() was not found in the "
                   << QString(session ? "session" : "observing") << " list!";
        return;
    }

    int k = list.indexOf(o);
    assert(k >= 0);

    // Remove from hash
    ImagePreviewHash.remove(o.data());

    if (o.data() == LogObject)
        saveCurrentUserLog();

    //Remove row from the TableView model
    // FIXME: Is there no faster way?
    for (int irow = 0; irow < currentModel->rowCount(); ++irow)
    {
        QString name = currentModel->item(irow, 0)->text();
        if (getObjectName(o.data()) == name)
        {
            currentModel->removeRow(irow);
            break;
        }
    }

    if (!session)
    {
        obsList().removeAt(k);
        ui->avt->removeAllPlotObjects();
        ui->WishListView->resizeColumnsToContents();
        if (!update)
            slotSaveList();
    }
    else
    {
        if (!update)
            TimeHash.remove(o->name());
        sessionList().removeAt(k); //Remove from the session list
        isModified = true;         //Removing an object should trigger the modified flag
        ui->avt->removeAllPlotObjects();
        ui->SessionView->resizeColumnsToContents();
        SkyMap::Instance()->forceUpdate();
    }
}

void ObservingList::slotRemoveSelectedObjects()
{
    //Find each object by name in the session list, and remove it
    //Go backwards so item alignment doesn't get screwed up as rows are removed.
    for (int irow = getActiveModel()->rowCount() - 1; irow >= 0; --irow)
    {
        bool rowSelected;
        if (sessionView)
            rowSelected = ui->SessionView->selectionModel()->isRowSelected(irow, QModelIndex());
        else
            rowSelected = ui->WishListView->selectionModel()->isRowSelected(irow, QModelIndex());

        if (rowSelected)
        {
            QModelIndex sortIndex, index;
            sortIndex    = getActiveSortModel()->index(irow, 0);
            index        = getActiveSortModel()->mapToSource(sortIndex);
            SkyObject *o = static_cast<SkyObject *>(index.data(Qt::UserRole + 1).value<void *>());
            Q_ASSERT(o);
            slotRemoveObject(o, sessionView);
        }
    }

    if (sessionView)
    {
        //we've removed all selected objects, so clear the selection
        ui->SessionView->selectionModel()->clear();
        //Update the lists in the Execute window as well
        KStarsData::Instance()->executeSession()->init();
    }

    setSaveImagesButton();
    ui->ImagePreview->setCursor(Qt::ArrowCursor);
}

void ObservingList::slotNewSelection()
{
    bool found      = false;
    singleSelection = false;
    noSelection     = false;
    showScope       = false;
    //ui->ImagePreview->clearPreview();
    //ui->ImagePreview->setPixmap(QPixmap());
    ui->ImagePreview->setCursor(Qt::ArrowCursor);
    QModelIndexList selectedItems;
    QString newName;
    QSharedPointer<SkyObject> o;
    QString labelText;
    ui->DeleteImage->setEnabled(false);

    selectedItems =
        getActiveSortModel()->mapSelectionToSource(getActiveView()->selectionModel()->selection()).indexes();

    if (selectedItems.size() == getActiveModel()->columnCount())
    {
        newName         = selectedItems[0].data().toString();
        singleSelection = true;
        //Find the selected object in the SessionList,
        //then break the loop.  Now SessionList.current()
        //points to the new selected object (until now it was the previous object)
        for (auto &o_temp : getActiveList())
        {
            if (getObjectName(o_temp.data()) == newName)
            {
                o = o_temp;
                found = true;
                break;
            }
        }
    }

    if (singleSelection)
    {
        //Enable buttons
        ui->ImagePreview->setCursor(Qt::PointingHandCursor);
#ifdef HAVE_INDI
        showScope = true;
#endif
        if (found)
        {
            m_CurrentObject = o.data();
            //QPoint pos(0,0);
            plot(o.data());
            //Change the m_currentImageFileName, DSS/SDSS Url to correspond to the new object
            setCurrentImage(o.data());
            ui->SearchImage->setEnabled(true);
            if (newName != i18n("star"))
            {
                //Display the current object's user notes in the NotesEdit
                //First, save the last object's user log to disk, if necessary
                saveCurrentUserLog(); //uses LogObject, which is still the previous obj.
                //set LogObject to the new selected object
                LogObject = currentObject();
                ui->NotesEdit->setEnabled(true);

                const auto &userLog =
                  KStarsData::Instance()->getUserData(LogObject->name()).userLog;

                if (userLog.isEmpty())
                {
                    ui->NotesEdit->setPlainText(
                        i18n("Record here observation logs and/or data on %1.", getObjectName(LogObject)));
                }
                else
                {
                    ui->NotesEdit->setPlainText(userLog);
                }
                if (sessionView)
                {
                    ui->TimeEdit->setEnabled(true);
                    ui->SetTime->setEnabled(true);
                    ui->TimeEdit->setTime(TimeHash.value(o->name(), o->transitTime(dt, geo)));
                }
            }
            else //selected object is named "star"
            {
                //clear the log text box
                saveCurrentUserLog();
                ui->NotesEdit->clear();
                ui->NotesEdit->setEnabled(false);
                ui->SearchImage->setEnabled(false);
            }
            QString ImagePath = KSPaths::locate(QStandardPaths::AppDataLocation, m_currentImageFileName);
            if (!ImagePath.isEmpty())
            {
                //If the image is present, show it!
                KSDssImage ksdi(ImagePath);
                KSDssImage::Metadata md = ksdi.getMetadata();
                //ui->ImagePreview->showPreview( QUrl::fromLocalFile( ksdi.getFileName() ) );
                if (ImagePreviewHash.contains(o.data()) == false)
                    ImagePreviewHash[o.data()] = QPixmap(ksdi.getFileName()).scaledToHeight(ui->ImagePreview->width());

                //ui->ImagePreview->setPixmap(QPixmap(ksdi.getFileName()).scaledToHeight(ui->ImagePreview->width()));
                ui->ImagePreview->setPixmap(ImagePreviewHash[o.data()]);
                if (md.isValid())
                {
                    ui->dssMetadataLabel->setText(
                        i18n("DSS Image metadata: \n Size: %1\' x %2\' \n Photometric band: %3 \n Version: %4",
                             QString::number(md.width), QString::number(md.height), QString() + md.band, md.version));
                }
                else
                    ui->dssMetadataLabel->setText(i18n("No image info available."));
                ui->ImagePreview->show();
                ui->DeleteImage->setEnabled(true);
            }
            else
            {
                setDefaultImage();
                ui->dssMetadataLabel->setText(
                    i18n("No image available. Click on the placeholder image to download one."));
            }
            QString cname =
                KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName(o.data());
            if (o->type() != SkyObject::CONSTELLATION)
            {
                labelText = "<b>";
                if (o->type() == SkyObject::PLANET)
                    labelText += o->translatedName();
                else
                    labelText += o->name();
                if (std::isfinite(o->mag()) && o->mag() <= 30.)
                    labelText += ":</b> " + i18nc("%1 magnitude of object, %2 type of sky object (planet, asteroid "
                                                  "etc), %3 name of a constellation",
                                                  "%1 mag %2 in %3", o->mag(), o->typeName().toLower(), cname);
                else
                    labelText +=
                        ":</b> " + i18nc("%1 type of sky object (planet, asteroid etc), %2 name of a constellation",
                                         "%1 in %2", o->typeName(), cname);
            }
        }
        else
        {
            setDefaultImage();
            qCWarning(KSTARS) << "Object " << newName << " not found in list.";
        }
        ui->quickInfoLabel->setText(labelText);
    }
    else
    {
        if (selectedItems.isEmpty()) //Nothing selected
        {
            //Disable buttons
            noSelection = true;
            ui->NotesEdit->setEnabled(false);
            m_CurrentObject = nullptr;
            ui->TimeEdit->setEnabled(false);
            ui->SetTime->setEnabled(false);
            ui->SearchImage->setEnabled(false);
            //Clear the user log text box.
            saveCurrentUserLog();
            ui->NotesEdit->setPlainText("");
            //Clear the plot in the AVTPlotwidget
            ui->avt->removeAllPlotObjects();
        }
        else //more than one object selected.
        {
            ui->NotesEdit->setEnabled(false);
            ui->TimeEdit->setEnabled(false);
            ui->SetTime->setEnabled(false);
            ui->SearchImage->setEnabled(false);
            m_CurrentObject = nullptr;
            //Clear the plot in the AVTPlotwidget
            ui->avt->removeAllPlotObjects();
            //Clear the user log text box.
            saveCurrentUserLog();
            ui->NotesEdit->setPlainText("");
            ui->quickInfoLabel->setText(QString());
        }
    }
}

void ObservingList::slotCenterObject()
{
    if (getSelectedItems().size() == 1)
    {
        SkyMap::Instance()->setClickedObject(currentObject());
        SkyMap::Instance()->setClickedPoint(currentObject());
        SkyMap::Instance()->slotCenter();
    }
}

void ObservingList::slotSlewToObject()
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
            KSNotification::error(
                i18n("Telescope %1 is offline. Please connect and retry again.", gd->getDeviceName()));
            return;
        }

        ISD::GDSetCommand SlewCMD(INDI_SWITCH, "ON_COORD_SET", "TRACK", ISS_ON, this);

        gd->setProperty(&SlewCMD);
        gd->runCommand(INDI_SEND_COORDS, currentObject());

        return;
    }

    KSNotification::sorry(i18n("KStars did not find any active telescopes."));

#endif
}

void ObservingList::slotAddToEkosScheduler()
{
#ifdef HAVE_INDI
    Ekos::Manager::Instance()->addObjectToScheduler(currentObject());
#endif
}

//FIXME: This will open multiple Detail windows for each object;
//Should have one window whose target object changes with selection
void ObservingList::slotDetails()
{
    if (currentObject())
    {
        QPointer<DetailDialog> dd =
            new DetailDialog(currentObject(), KStarsData::Instance()->ut(), geo, KStars::Instance());
        dd->exec();
        delete dd;
    }
}

void ObservingList::slotWUT()
{
    KStarsDateTime lt = dt;
    lt.setTime(QTime(8, 0, 0));
    QPointer<WUTDialog> w = new WUTDialog(KStars::Instance(), sessionView, geo, lt);
    w->exec();
    delete w;
}

void ObservingList::slotAddToSession()
{
    Q_ASSERT(!sessionView);
    if (getSelectedItems().size())
    {
        foreach (const QModelIndex &i, getSelectedItems())
        {
            foreach (QSharedPointer<SkyObject> o, obsList())
                if (getObjectName(o.data()) == i.data().toString())
                    slotAddObject(
                        o.data(),
                        true); // FIXME: Would be good to have a wrapper that accepts QSharedPointer<SkyObject>
        }
    }
}

void ObservingList::slotFind()
{
    if (FindDialog::Instance()->exec() == QDialog::Accepted)
    {
        SkyObject *o = FindDialog::Instance()->targetObject();
        if (o != nullptr)
        {
            slotAddObject(o, sessionView);
        }
    }
}

void ObservingList::slotBatchAdd()
{
    bool accepted = false;
    QString items = QInputDialog::getMultiLineText(this,
                    sessionView ? i18n("Batch add to observing session") : i18n("Batch add to observing wishlist"),
                    i18n("Specify a list of objects with one object on each line to add. The names must be understood to KStars, or if the internet resolver is enabled in settings, to the CDS Sesame resolver. Objects that are internet resolved will be added to the database."),
                    QString(),
                    &accepted);
    bool resolve = Options::resolveNamesOnline();

    if (accepted && !items.isEmpty())
    {
        QStringList failedObjects;
        QStringList objectNames = items.split("\n");
        for (QString objectName : objectNames)
        {
            objectName = FindDialog::processSearchText(objectName);
            SkyObject *object = KStarsData::Instance()->objectNamed(objectName);
            if (!object && resolve)
            {
                object = FindDialog::resolveAndAdd(m_manager, objectName);
            }
            if (!object)
            {
                failedObjects.append(objectName);
            }
            else
            {
                slotAddObject(object, sessionView);
            }
        }

        if (!failedObjects.isEmpty())
        {
            QMessageBox msgBox =
            {
                QMessageBox::Icon::Warning,
                i18np("Batch add: %1 object not found", "Batch add: %1 objects not found", failedObjects.size()),
                i18np("%1 object could not be found in the database or resolved, and hence could not be added. See the details for more.",
                      "%1 objects could not be found in the database or resolved, and hence could not be added. See the details for more.",
                      failedObjects.size()),
                QMessageBox::Ok,
                this
            };
            msgBox.setDetailedText(failedObjects.join("\n"));
            msgBox.exec();
        }
    }
    Q_ASSERT(false); // Not implemented
}

void ObservingList::slotEyepieceView()
{
    KStars::Instance()->slotEyepieceView(currentObject(), getCurrentImagePath());
}

void ObservingList::slotAVT()
{
    QModelIndexList selectedItems;
    // TODO: Think and see if there's a more efficient way to do this. I can't seem to think of any, but this code looks like it could be improved. - Akarsh
    selectedItems =
        (sessionView ?
         m_SessionSortModel->mapSelectionToSource(ui->SessionView->selectionModel()->selection()).indexes() :
         m_WishListSortModel->mapSelectionToSource(ui->WishListView->selectionModel()->selection()).indexes());

    if (selectedItems.size())
    {
        QPointer<AltVsTime> avt = new AltVsTime(KStars::Instance());
        foreach (const QModelIndex &i, selectedItems)
        {
            if (i.column() == 0)
            {
                SkyObject *o = static_cast<SkyObject *>(i.data(Qt::UserRole + 1).value<void *>());
                Q_ASSERT(o);
                avt->processObject(o);
            }
        }
        avt->exec();
        delete avt;
    }
}

//FIXME: On close, we will need to close any open Details/AVT windows
void ObservingList::slotClose()
{
    //Save the current User log text
    saveCurrentUserLog();
    ui->avt->removeAllPlotObjects();
    slotNewSelection();
    saveCurrentList();
    hide();
}

void ObservingList::saveCurrentUserLog()
{
    if (LogObject && !ui->NotesEdit->toPlainText().isEmpty() &&
            ui->NotesEdit->toPlainText() !=
            i18n("Record here observation logs and/or data on %1.", getObjectName(LogObject)))
    {
        const auto &success = KStarsData::Instance()->updateUserLog(
            LogObject->name(), ui->NotesEdit->toPlainText());

        if (!success.first)
            KSNotification::sorry(success.second, i18n("Could not update the user log."));

        ui->NotesEdit->clear();
        LogObject = nullptr;
    }
}

void ObservingList::slotOpenList()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(KStars::Instance(), i18nc("@title:window", "Open Observing List"), QUrl(),
                   "KStars Observing List (*.obslist)");
    QFile f;

    if (fileURL.isValid())
    {
        f.setFileName(fileURL.toLocalFile());
        //FIXME do we still need to do this?
        /*
        if ( ! fileURL.isLocalFile() ) {
            //Save remote list to a temporary local file
            QTemporaryFile tmpfile;
            tmpfile.setAutoRemove(false);
            tmpfile.open();
            m_listFileName = tmpfile.fileName();
            if( KIO::NetAccess::download( fileURL, m_listFileName, this ) )
                f.setFileName( m_listFileName );

        } else {
            m_listFileName = fileURL.toLocalFile();
            f.setFileName( m_listFileName );
        }
        */

        if (!f.open(QIODevice::ReadOnly))
        {
            QString message = i18n("Could not open file %1", f.fileName());
            KSNotification::sorry(message, i18n("Could Not Open File"));
            return;
        }
        saveCurrentList(); //See if the current list needs to be saved before opening the new one
        ui->tabWidget->setCurrentIndex(1); // FIXME: This is not robust -- asimha
        slotChangeTab(1);

        sessionList().clear();
        TimeHash.clear();
        m_CurrentObject = nullptr;
        m_SessionModel->removeRows(0, m_SessionModel->rowCount());
        SkyMap::Instance()->forceUpdate();
        //First line is the name of the list. The rest of the file is
        //object names, one per line. With the TimeHash value if present
        QTextStream istream(&f);
        QString input;
        input = istream.readAll();
        OAL::Log logObject;
        logObject.readBegin(input);
        //Set the New TimeHash
        TimeHash = logObject.timeHash();
        GeoLocation *geo_new = logObject.geoLocation();
        if (!geo_new)
        {
            // FIXME: This is a very hackish solution -- if we
            // encounter an invalid XML file, we know we won't read a
            // GeoLocation successfully. It does not detect partially
            // corrupt files. -- asimha
            KSNotification::sorry(i18n("The specified file is invalid. We expect an XML file based on the OpenAstronomyLog schema."));
            f.close();
            return;
        }
        dt = logObject.dateTime();
        //foreach (SkyObject *o, *(logObject.targetList()))
        for (auto &o : logObject.targetList())
            slotAddObject(o.data(), true);
        //Update the location and user set times from file
        slotUpdate();
        //Newly-opened list should not trigger isModified flag
        isModified = false;
        f.close();
    }
    else if (!fileURL.toLocalFile().isEmpty())
    {
        KSNotification::sorry(i18n("The specified file is invalid"));
    }
}

void ObservingList::slotClearList()
{
    if ((ui->tabWidget->currentIndex() == 0 && obsList().isEmpty()) ||
            (ui->tabWidget->currentIndex() == 1 && sessionList().isEmpty()))
        return;

    QString message = i18n("Are you sure you want to clear all objects?");
    if (KMessageBox::questionYesNo(this, message, i18n("Clear all?")) == KMessageBox::Yes)
    {
        // Did I forget anything else to remove?
        ui->avt->removeAllPlotObjects();
        m_CurrentObject = LogObject = nullptr;

        if (ui->tabWidget->currentIndex() == 0)
        {
            // IMPORTANT: Is this enough or we will have dangling pointers in memory?
            ImagePreviewHash.clear();
            obsList().clear();
            m_WishListModel->setRowCount(0);
        }
        else
        {
            // IMPORTANT: Is this enough or we will have dangling pointers in memory?
            sessionList().clear();
            TimeHash.clear();
            isModified = true; //Removing an object should trigger the modified flag
            m_SessionModel->setRowCount(0);
            SkyMap::Instance()->forceUpdate();
        }
    }
}

void ObservingList::saveCurrentList()
{
    //Before loading a new list, do we need to save the current one?
    //Assume that if the list is empty, then there's no need to save
    if (sessionList().size())
    {
        if (isModified)
        {
            QString message = i18n("Do you want to save the current session?");
            if (KMessageBox::questionYesNo(this, message, i18n("Save Current session?"), KStandardGuiItem::save(),
                                           KStandardGuiItem::discard()) == KMessageBox::Yes)
                slotSaveSession();
        }
    }
}

void ObservingList::slotSaveSessionAs(bool nativeSave)
{
    if (sessionList().isEmpty())
        return;

    QUrl fileURL = QFileDialog::getSaveFileUrl(KStars::Instance(), i18nc("@title:window", "Save Observing List"), QUrl(),
                   "KStars Observing List (*.obslist)");
    if (fileURL.isValid())
    {
        m_listFileName = fileURL.toLocalFile();
        slotSaveSession(nativeSave);
    }
}

void ObservingList::slotSaveList()
{
    QFile f;
    // FIXME: Move wishlist into a database.
    // TODO: Support multiple wishlists.

    QString fileContents;
    QTextStream ostream(
        &fileContents); // We first write to a QString to prevent truncating the file in case there is a crash.
    foreach (const QSharedPointer<SkyObject> o, obsList())
    {
        if (!o)
        {
            qWarning() << "Null entry in observing wishlist! Skipping!";
            continue;
        }
        if (o->name() == "star")
        {
            //ostream << o->name() << "  " << o->ra0().Hours() << "  " << o->dec0().Degrees() << endl;
            ostream << getObjectName(o.data(), false) << '\n';
        }
        else if (o->type() == SkyObject::STAR)
        {
            Q_ASSERT(dynamic_cast<const StarObject *>(o.data()));
            const QSharedPointer<StarObject> s = qSharedPointerCast<StarObject>(o);
            if (s->name() == s->gname())
                ostream << s->name2() << '\n';
            else
                ostream << s->name() << '\n';
        }
        else
        {
            ostream << o->name() << '\n';
        }
    }
    f.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("wishlist.obslist"));
    if (!f.open(QIODevice::WriteOnly))
    {
        qWarning() << "Cannot save wish list to file!"; // TODO: This should be presented as a message box to the user
        KMessageBox::error(this, i18n("Could not open the observing wishlist file %1 for writing. Your wishlist changes will not be saved. Check if the location is writable and not full.", f.fileName()), i18n("Could not save observing wishlist"));
        return;
    }
    QTextStream writeemall(&f);
    writeemall << fileContents;
    f.close();
}

void ObservingList::slotLoadWishList()
{
    QFile f;
    f.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("wishlist.obslist"));
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning(KSTARS) << "No WishList Saved yet";
        return;
    }
    QTextStream istream(&f);
    QString line;

    QPointer<QProgressDialog> addingObjectsProgress = new QProgressDialog();
    addingObjectsProgress->setWindowTitle(i18nc("@title:window", "Observing List Wizard"));
    addingObjectsProgress->setLabelText(i18n("Please wait while loading objects..."));
    addingObjectsProgress->setMaximum(0);
    addingObjectsProgress->setMinimum(0);
    addingObjectsProgress->show();

    QStringList failedObjects;

    while (!istream.atEnd())
    {
        line = istream.readLine();
        //If the object is named "star", add it by coordinates
        SkyObject *o;
        /*if ( line.startsWith( QLatin1String( "star" ) ) ) {
            QStringList fields = line.split( ' ', QString::SkipEmptyParts );
            dms ra = dms::fromString( fields[1], false ); //false = hours
            dms dc = dms::fromString( fields[2], true );  //true  = degrees
            SkyPoint p( ra, dc );
            double maxrad = 1000.0/Options::zoomFactor();
            o = ks->data()->skyComposite()->starNearest( &p, maxrad );
        }
        else {*/
        o = KStarsData::Instance()->objectNamed(line);
        //}
        //If we haven't identified the object, try interpreting the
        //name as a star's genetive name (with ascii letters)
        if (!o)
            o = KStarsData::Instance()->skyComposite()->findStarByGenetiveName(line);
        if (o)
            slotAddObject(o, false, true);
        else
            failedObjects.append(line);

        if (addingObjectsProgress->wasCanceled())
            break;
        qApp->processEvents();
    }
    delete (addingObjectsProgress);
    f.close();

    if (!failedObjects.isEmpty())
    {
        QMessageBox msgBox = {QMessageBox::Icon::Warning,
                              i18np("Observing wishlist truncated: %1 object not found", "Observing wishlist truncated: %1 objects not found", failedObjects.size()),
                              i18np("%1 object could not be found in the database, and will be removed from the observing wish list. We recommend that you copy the detailed list as a backup.", "%1 objects could not be found in the database, and will be removed from the observing wish list. We recommend that you copy the detailed list as a backup.", failedObjects.size()),
                              QMessageBox::Ok,
                              this
                             };
        msgBox.setDetailedText(failedObjects.join("\n"));
        msgBox.exec();
    }
}

void ObservingList::slotSaveSession(bool nativeSave)
{
    if (sessionList().isEmpty())
    {
        KSNotification::error(i18n("Cannot save an empty session list."));
        return;
    }

    if (m_listFileName.isEmpty())
    {
        slotSaveSessionAs(nativeSave);
        return;
    }
    QFile f(m_listFileName);
    if (!f.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Could not open file %1.  Try a different filename?", f.fileName());
        if (KMessageBox::warningYesNo(nullptr, message, i18n("Could Not Open File"), KGuiItem(i18n("Try Different")),
                                      KGuiItem(i18n("Do Not Try"))) == KMessageBox::Yes)
        {
            m_listFileName.clear();
            slotSaveSessionAs(nativeSave);
        }
        return;
    }
    QTextStream ostream(&f);
    OAL::Log log;
    ostream << log.writeLog(nativeSave);
    f.close();
    isModified = false; //We've saved the session, so reset the modified flag.
}

void ObservingList::slotWizard()
{
    QPointer<ObsListWizard> wizard = new ObsListWizard(KStars::Instance());
    if (wizard->exec() == QDialog::Accepted)
    {
        QPointer<QProgressDialog> addingObjectsProgress = new QProgressDialog();
        addingObjectsProgress->setWindowTitle(i18nc("@title:window", "Observing List Wizard"));
        addingObjectsProgress->setLabelText(i18n("Please wait while adding objects..."));
        addingObjectsProgress->setMaximum(wizard->obsList().size());
        addingObjectsProgress->setMinimum(0);
        addingObjectsProgress->setValue(0);
        addingObjectsProgress->show();
        int counter = 1;
        foreach (SkyObject *o, wizard->obsList())
        {
            slotAddObject(o);
            addingObjectsProgress->setValue(counter++);
            if (addingObjectsProgress->wasCanceled())
                break;
            qApp->processEvents();
        }
        delete addingObjectsProgress;
    }

    delete wizard;
}

void ObservingList::plot(SkyObject *o)
{
    if (!o)
        return;
    float DayOffset = 0;
    if (TimeHash.value(o->name(), o->transitTime(dt, geo)).hour() > 12)
        DayOffset = 1;

    QDateTime midnight = QDateTime(dt.date(), QTime());
    KStarsDateTime ut  = geo->LTtoUT(KStarsDateTime(midnight));
    double h1          = geo->GSTtoLST(ut.gst()).Hours();
    if (h1 > 12.0)
        h1 -= 24.0;

    ui->avt->setSecondaryLimits(h1, h1 + 24.0, -90.0, 90.0);
    ksal->setLocation(geo);
    ksal->setDate(ut);
    ui->avt->setGeoLocation(geo);
    ui->avt->setSunRiseSetTimes(ksal->getSunRise(), ksal->getSunSet());
    ui->avt->setDawnDuskTimes(ksal->getDawnAstronomicalTwilight(), ksal->getDuskAstronomicalTwilight());
    ui->avt->setMinMaxSunAlt(ksal->getSunMinAlt(), ksal->getSunMaxAlt());
    ui->avt->setMoonRiseSetTimes(ksal->getMoonRise(), ksal->getMoonSet());
    ui->avt->setMoonIllum(ksal->getMoonIllum());
    ui->avt->update();
    KPlotObject *po = new KPlotObject(Qt::white, KPlotObject::Lines, 2.0);
    for (double h = -12.0; h <= 12.0; h += 0.5)
    {
        po->addPoint(h, findAltitude(o, (h + DayOffset * 24.0)));
    }
    ui->avt->removeAllPlotObjects();
    ui->avt->addPlotObject(po);
}

double ObservingList::findAltitude(SkyPoint *p, double hour)
{
    // Jasem 2015-09-05 Using correct procedure to find altitude
    SkyPoint sp                   = *p; // make a copy
    QDateTime midnight            = QDateTime(dt.date(), QTime());
    KStarsDateTime ut             = geo->LTtoUT(KStarsDateTime(midnight));
    KStarsDateTime targetDateTime = ut.addSecs(hour * 3600.0);
    dms LST                       = geo->GSTtoLST(targetDateTime.gst());
    sp.EquatorialToHorizontal(&LST, geo->lat());
    return sp.alt().Degrees();
}

void ObservingList::slotChangeTab(int index)
{
    noSelection = true;
    saveCurrentUserLog();
    ui->NotesEdit->setEnabled(false);
    ui->TimeEdit->setEnabled(false);
    ui->SetTime->setEnabled(false);
    ui->SearchImage->setEnabled(false);
    ui->DeleteImage->setEnabled(false);
    m_CurrentObject = nullptr;
    sessionView     = index != 0;
    setSaveImagesButton();
    ui->WizardButton->setEnabled(!sessionView); //wizard adds only to the Wish List
    ui->OALExport->setEnabled(sessionView);
    //Clear the selection in the Tables
    ui->WishListView->clearSelection();
    ui->SessionView->clearSelection();
    //Clear the user log text box.
    saveCurrentUserLog();
    ui->NotesEdit->setPlainText("");
    ui->avt->removeAllPlotObjects();
}

void ObservingList::slotLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);
    if (ld->exec() == QDialog::Accepted)
    {
        geo = ld->selectedCity();
        ui->SetLocation->setText(geo->fullName());
    }
    delete ld;
}

void ObservingList::slotUpdate()
{
    dt.setDate(ui->DateEdit->date());
    ui->avt->removeAllPlotObjects();
    //Creating a copy of the lists, we can't use the original lists as they'll keep getting modified as the loop iterates
    QList<QSharedPointer<SkyObject>> _obsList = m_WishList, _SessionList = m_SessionList;

    for (QSharedPointer<SkyObject> &o : _obsList)
    {
        if (o->name() != "star")
        {
            slotRemoveObject(o.data(), false, true);
            slotAddObject(o.data(), false, true);
        }
    }
    for (QSharedPointer<SkyObject> &obj : _SessionList)
    {
        if (obj->name() != "star")
        {
            slotRemoveObject(obj.data(), true, true);
            slotAddObject(obj.data(), true, true);
        }
    }
    SkyMap::Instance()->forceUpdate();
}

void ObservingList::slotSetTime()
{
    SkyObject *o = currentObject();
    slotRemoveObject(o, true);
    TimeHash[o->name()] = ui->TimeEdit->time();
    slotAddObject(o, true, true);
}

void ObservingList::slotCustomDSS()
{
    ui->SearchImage->setEnabled(false);
    //ui->ImagePreview->clearPreview();
    ui->ImagePreview->setPixmap(QPixmap());

    KSDssImage::Metadata md;
    bool ok = true;

    int width  = QInputDialog::getInt(this, i18n("Customized DSS Download"), i18n("Specify image width (arcminutes): "),
                                      15, 15, 75, 1, &ok);
    int height = QInputDialog::getInt(this, i18n("Customized DSS Download"),
                                      i18n("Specify image height (arcminutes): "), 15, 15, 75, 1, &ok);
    QStringList strList = (QStringList() << "poss2ukstu_blue"
                           << "poss2ukstu_red"
                           << "poss2ukstu_ir"
                           << "poss1_blue"
                           << "poss1_red"
                           << "quickv"
                           << "all");
    QString version =
        QInputDialog::getItem(this, i18n("Customized DSS Download"), i18n("Specify version: "), strList, 0, false, &ok);

    QUrl srcUrl(KSDssDownloader::getDSSURL(currentObject()->ra0(), currentObject()->dec0(), width, height, "gif",
                                           version, &md));

    delete m_dl;
    m_dl = new KSDssDownloader();
    connect(m_dl, SIGNAL(downloadComplete(bool)), SLOT(downloadReady(bool)));
    m_dl->startSingleDownload(srcUrl, getCurrentImagePath(), md);
}

void ObservingList::slotGetImage(bool _dss, const SkyObject *o)
{
    dss = _dss;
    Q_ASSERT(
        !o ||
        o == currentObject()); // FIXME: Meaningless to operate on m_currentImageFileName unless o == currentObject()!
    if (!o)
        o = currentObject();
    ui->SearchImage->setEnabled(false);
    //ui->ImagePreview->clearPreview();
    //ui->ImagePreview->setPixmap(QPixmap());
    setCurrentImage(o);
    QString currentImagePath = getCurrentImagePath();
    if (QFile::exists(currentImagePath))
        QFile::remove(currentImagePath);
    //QUrl url;
    dss = true;
    qWarning() << "FIXME: Removed support for SDSS. Until reintroduction, we will supply a DSS image";
    std::function<void(bool)> slot = std::bind(&ObservingList::downloadReady, this, std::placeholders::_1);
    new KSDssDownloader(o, currentImagePath, slot, this);
}

void ObservingList::downloadReady(bool success)
{
    // set downloadJob to 0, but don't delete it - the job will be deleted automatically
    //    downloadJob = 0;

    delete m_dl;
    m_dl = nullptr; // required if we came from slotCustomDSS; does nothing otherwise

    if (!success)
    {
        KSNotification::sorry(i18n("Failed to download DSS/SDSS image."));
    }
    else
    {
        /*
          if( QFile( QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(m_currentImageFileName) ).size() > 13000)
          //The default image is around 8689 bytes
        */
        //ui->ImagePreview->showPreview( QUrl::fromLocalFile( getCurrentImagePath() ) );
        ui->ImagePreview->setPixmap(QPixmap(getCurrentImagePath()).scaledToHeight(ui->ImagePreview->width()));
        saveThumbImage();
        ui->ImagePreview->show();
        ui->ImagePreview->setCursor(Qt::PointingHandCursor);
        ui->DeleteImage->setEnabled(true);
    }
    /*
    // FIXME: Implement a priority order SDSS > DSS in the DSS downloader
    else if( ! dss )
        slotGetImage( true );
    */
}

void ObservingList::setCurrentImage(const SkyObject *o)
{
    QString sanitizedName = o->name().remove(' ').remove('\'').remove('\"').toLower();

    // JM: Always use .png across all platforms. No JPGs at all?
    m_currentImageFileName = "image-" + sanitizedName + ".png";

    m_currentThumbImageFileName = "thumb-" + sanitizedName + ".png";

    // Does full image exists in the path?
    QString currentImagePath = KSPaths::locate(QStandardPaths::AppDataLocation, m_currentImageFileName);

    // Let's try to fallback to thumb-* images if they exist
    if (currentImagePath.isEmpty())
    {
        currentImagePath = KSPaths::locate(QStandardPaths::AppDataLocation, m_currentThumbImageFileName);

        // If thumb image exists, let's use it
        if (currentImagePath.isEmpty() == false)
            m_currentImageFileName = m_currentThumbImageFileName;
    }

    // 2017-04-14: Unnamed stars already unsupported in observing list
    /*
    if( o->name() == "star" )
    {
        QString RAString( o->ra0().toHMSString() );
        QString DecString( o->dec0().toDMSString() );
        m_currentImageFileName = "Image_J" + RAString.remove(' ').remove( ':' ) + DecString.remove(' ').remove( ':' ); // Note: Changed naming convention to standard 2016-08-25 asimha; old images shall have to be re-downloaded.
        // Unnecessary complication below:
        // QChar decsgn = ( (o->dec0().Degrees() < 0.0 ) ? '-' : '+' );
        // m_currentImageFileName = m_currentImageFileName.remove('+').remove('-') + decsgn;
    }
    */

    // 2017-04-14 JM: If we use .png always, let us use it across all platforms.
    /*
    QString imagePath = getCurrentImagePath();
    if ( QFile::exists( imagePath))   // New convention -- append filename extension so file is usable on Windows etc.
    {
        QFile::rename( imagePath, imagePath + ".png" );
    }
    m_currentImageFileName += ".png";
    */
}

QString ObservingList::getCurrentImagePath()
{
    QString currentImagePath = KSPaths::locate(QStandardPaths::AppDataLocation, m_currentImageFileName);
    if (QFile::exists(currentImagePath))
    {
        return currentImagePath;
    }
    else
        return QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(m_currentImageFileName);
}

void ObservingList::slotSaveAllImages()
{
    ui->SearchImage->setEnabled(false);
    ui->DeleteImage->setEnabled(false);
    m_CurrentObject = nullptr;
    //Clear the selection in the Tables
    ui->WishListView->clearSelection();
    ui->SessionView->clearSelection();

    foreach (QSharedPointer<SkyObject> o, getActiveList())
    {
        if (!o)
            continue; // FIXME: Why would we have null objects? But appears that we do.
        setCurrentImage(o.data());
        QString img(getCurrentImagePath());
        //        QUrl url( ( Options::obsListPreferDSS() ) ? DSSUrl : SDSSUrl ); // FIXME: We have removed SDSS support!
        QUrl url(KSDssDownloader::getDSSURL(o.data()));
        if (!o->isSolarSystem()) //TODO find a way for adding support for solar system images
            saveImage(url, img, o.data());
    }
}

void ObservingList::saveImage(QUrl /*url*/, QString /*filename*/, const SkyObject *o)
{
    if (!o)
        o = currentObject();
    Q_ASSERT(o);
    if (!QFile::exists(getCurrentImagePath()))
    {
        // Call the DSS downloader
        slotGetImage(true, o);
    }
}

void ObservingList::slotImageViewer()
{
    QPointer<ImageViewer> iv;
    QString currentImagePath = getCurrentImagePath();
    if (QFile::exists(currentImagePath))
    {
        QUrl url = QUrl::fromLocalFile(currentImagePath);
        iv       = new ImageViewer(url);
    }

    if (iv)
        iv->show();
}

void ObservingList::slotDeleteAllImages()
{
    if (KMessageBox::warningYesNo(nullptr, i18n("This will delete all saved images. Are you sure you want to do this?"),
                                  i18n("Delete All Images")) == KMessageBox::No)
        return;
    ui->ImagePreview->setCursor(Qt::ArrowCursor);
    ui->SearchImage->setEnabled(false);
    ui->DeleteImage->setEnabled(false);
    m_CurrentObject = nullptr;
    //Clear the selection in the Tables
    ui->WishListView->clearSelection();
    ui->SessionView->clearSelection();
    //ui->ImagePreview->clearPreview();
    ui->ImagePreview->setPixmap(QPixmap());
    QDirIterator iterator(KSPaths::writableLocation(QStandardPaths::AppDataLocation));
    while (iterator.hasNext())
    {
        // TODO: Probably, there should be a different directory for cached images in the observing list.
        if (iterator.fileName().contains("Image") && (!iterator.fileName().contains("dat")) &&
                (!iterator.fileName().contains("obslist")))
        {
            QFile file(iterator.filePath());
            file.remove();
        }
        iterator.next();
    }
}

void ObservingList::setSaveImagesButton()
{
    ui->saveImages->setEnabled(!getActiveList().isEmpty());
}

// FIXME: Is there a reason to implement these as an event filter,
// instead of as a signal-slot connection? Shouldn't we just use slots
// to subscribe to various events from the Table / Session view?
//
// NOTE: ui->ImagePreview is a QLabel, which has no clicked() event or
// public mouseReleaseEvent(), so eventFilter makes sense.
bool ObservingList::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->ImagePreview)
    {
        if (event->type() == QEvent::MouseButtonRelease)
        {
            if (currentObject())
            {
                if (!QFile::exists(getCurrentImagePath()))
                {
                    if (!currentObject()->isSolarSystem())
                        slotGetImage(Options::obsListPreferDSS());
                    else
                        slotSearchImage();
                }
                else
                    slotImageViewer();
            }
            return true;
        }
    }
    if (obj == ui->WishListView->viewport() || obj == ui->SessionView->viewport())
    {
        bool sessionViewEvent = (obj == ui->SessionView->viewport());

        if (event->type() == QEvent::MouseButtonRelease) // Mouse button release event
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            QPoint pos(mouseEvent->globalX(), mouseEvent->globalY());

            if (mouseEvent->button() == Qt::RightButton)
            {
                if (!noSelection)
                {
                    pmenu->initPopupMenu(sessionViewEvent, !singleSelection, showScope);
                    pmenu->popup(pos);
                }
                return true;
            }
        }
    }

    if (obj == ui->WishListView || obj == ui->SessionView)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Delete)
            {
                slotRemoveSelectedObjects();
                return true;
            }
        }
    }

    return false;
}

void ObservingList::slotSearchImage()
{
    QPixmap *pm                  = new QPixmap(":/images/noimage.png");
    QPointer<ThumbnailPicker> tp = new ThumbnailPicker(currentObject(), *pm, this, 200, 200, i18n("Image Chooser"));
    if (tp->exec() == QDialog::Accepted)
    {
        QString currentImagePath = getCurrentImagePath();
        QFile f(currentImagePath);

        //If a real image was set, save it.
        if (tp->imageFound())
        {
            const auto image = *tp->image();
            image.save(f.fileName(), "PNG");
            //ui->ImagePreview->showPreview( QUrl::fromLocalFile( f.fileName() ) );
            saveThumbImage();
            slotNewSelection();
            ui->ImagePreview->setPixmap(image.scaledToHeight(ui->ImagePreview->width()));
            ui->ImagePreview->repaint();
        }
    }
    delete pm;
    delete tp;
}

void ObservingList::slotDeleteCurrentImage()
{
    QFile::remove(getCurrentImagePath());
    ImagePreviewHash.remove(m_CurrentObject);
    slotNewSelection();
}

void ObservingList::saveThumbImage()
{
    QFileInfo const f(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(m_currentThumbImageFileName));
    if (!f.exists())
    {
        QImage img(getCurrentImagePath());
        img = img.scaled(200, 200, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        img.save(f.filePath());
    }
}

QString ObservingList::getTime(const SkyObject *o) const
{
    return TimeHash.value(o->name(), QTime(30, 0, 0)).toString("h:mm:ss AP");
}

QTime ObservingList::scheduledTime(SkyObject *o) const
{
    return TimeHash.value(o->name(), o->transitTime(dt, geo));
}

void ObservingList::setTime(const SkyObject *o, QTime t)
{
    TimeHash.insert(o->name(), t);
}

void ObservingList::slotOALExport()
{
    slotSaveSessionAs(false);
}

void ObservingList::slotAddVisibleObj()
{
    KStarsDateTime lt = dt;
    lt.setTime(QTime(8, 0, 0));
    QPointer<WUTDialog> w = new WUTDialog(KStars::Instance(), sessionView, geo, lt);
    w->init();
    QModelIndexList selectedItems;
    selectedItems =
        m_WishListSortModel->mapSelectionToSource(ui->WishListView->selectionModel()->selection()).indexes();
    if (selectedItems.size())
    {
        foreach (const QModelIndex &i, selectedItems)
        {
            foreach (QSharedPointer<SkyObject> o, obsList())
                if (getObjectName(o.data()) == i.data().toString() && w->checkVisibility(o.data()))
                    slotAddObject(
                        o.data(),
                        true); // FIXME: Better if there is a QSharedPointer override for this, although the check will ensure that we don't duplicate.
        }
    }
    delete w;
}

SkyObject *ObservingList::findObjectByName(QString name)
{
    foreach (QSharedPointer<SkyObject> o, sessionList())
    {
        if (getObjectName(o.data(), false) == name)
            return o.data();
    }
    return nullptr;
}

void ObservingList::selectObject(const SkyObject *o)
{
    ui->tabWidget->setCurrentIndex(1);
    ui->SessionView->selectionModel()->clear();
    for (int irow = m_SessionModel->rowCount() - 1; irow >= 0; --irow)
    {
        QModelIndex mSortIndex = m_SessionSortModel->index(irow, 0);
        QModelIndex mIndex     = m_SessionSortModel->mapToSource(mSortIndex);
        int idxrow             = mIndex.row();
        if (m_SessionModel->item(idxrow, 0)->text() == getObjectName(o))
            ui->SessionView->selectRow(idxrow);
        slotNewSelection();
    }
}

void ObservingList::setDefaultImage()
{
    ui->ImagePreview->setPixmap(m_NoImagePixmap);
    ui->ImagePreview->update();
}

QString ObservingList::getObjectName(const SkyObject *o, bool translated)
{
    QString finalObjectName;
    if (o->name() == "star")
    {
        const StarObject *s = dynamic_cast<const StarObject *>(o);

        // JM: Enable HD Index stars to be added to the observing list.
        if (s != nullptr && s->getHDIndex() != 0)
            finalObjectName = QString("HD %1").arg(QString::number(s->getHDIndex()));
    }
    else
        finalObjectName = translated ? o->translatedName() : o->name();

    return finalObjectName;
}

void ObservingList::slotUpdateAltitudes()
{
    // FIXME: Update upon gaining visibility, do not update when not visible
    KStarsDateTime now = KStarsDateTime::currentDateTimeUtc();
    //    qCDebug(KSTARS) << "Updating altitudes in observation planner @ JD - J2000 = " << double( now.djd() - J2000 );
    for (int irow = m_WishListModel->rowCount() - 1; irow >= 0; --irow)
    {
        QModelIndex idx = m_WishListSortModel->mapToSource(m_WishListSortModel->index(irow, 0));
        SkyObject *o    = static_cast<SkyObject *>(idx.data(Qt::UserRole + 1).value<void *>());
        Q_ASSERT(o);
        SkyPoint p = o->recomputeHorizontalCoords(now, geo);
        idx =
            m_WishListSortModel->mapToSource(m_WishListSortModel->index(irow, m_WishListSortModel->columnCount() - 1));
        QStandardItem *replacement = m_altCostHelper(p);
        m_WishListModel->setData(idx, replacement->data(Qt::DisplayRole), Qt::DisplayRole);
        m_WishListModel->setData(idx, replacement->data(Qt::UserRole), Qt::UserRole);
        delete replacement;
    }
    emit m_WishListModel->dataChanged(
        m_WishListModel->index(0, m_WishListModel->columnCount() - 1),
        m_WishListModel->index(m_WishListModel->rowCount() - 1, m_WishListModel->columnCount() - 1));
}

QSharedPointer<SkyObject> ObservingList::findObject(const SkyObject *o, bool session)
{
    const QList<QSharedPointer<SkyObject>> &list = (session ? sessionList() : obsList());
    const QString &target                        = getObjectName(o);
    foreach (QSharedPointer<SkyObject> obj, list)
    {
        if (getObjectName(obj.data()) == target)
            return obj;
    }
    return QSharedPointer<SkyObject>(); // null pointer
}

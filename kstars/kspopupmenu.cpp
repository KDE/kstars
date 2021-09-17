/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kspopupmenu.h"

#include "config-kstars.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skyobjects/trailobject.h"
#include "skyobjects/catalogobject.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/satellite.h"
#include "skyobjects/supernova.h"
#include "skycomponents/constellationboundarylines.h"
#include "skycomponents/flagcomponent.h"
#include "skycomponents/skymapcomposite.h"
#include "skyobjectuserdata.h"
#include "tools/whatsinteresting/wiview.h"
#include "catalogsdb.h"
#include "observinglist.h"

#ifdef HAVE_INDI
#include "indi/indilistener.h"
#include "indi/guimanager.h"
#include "indi/driverinfo.h"
#include "indi/indistd.h"
#include "indi/indidevice.h"
#include "indi/indigroup.h"
#include "indi/indiproperty.h"
#include "indi/indielement.h"
#include "indi/inditelescope.h"
#include <basedevice.h>
#endif

#include <KLocalizedString>

#include <QWidgetAction>

namespace
{
// Convert magnitude to string representation for QLabel
QString magToStr(double m)
{
    if (m > -30 && m < 90)
        return QString("%1<sup>m</sup>").arg(m, 0, 'f', 2);
    else
        return QString();
}

// Return object name
QString getObjectName(SkyObject *obj)
{
    // FIXME: make logic less convoluted.
    if (obj->longname() != obj->name()) // Object has proper name
    {
        return obj->translatedLongName() + ", " + obj->translatedName();
    }
    else
    {
        if (!obj->translatedName2().isEmpty())
            return obj->translatedName() + ", " + obj->translatedName2();
        else
            return obj->translatedName();
    }
}

// String representation for rise/set time of object. If object
// doesn't rise/set returns descriptive string.
//
// Second parameter choose between raise and set. 'true' for
// raise, 'false' for set.
QString riseSetTimeLabel(SkyObject *o, bool isRaise)
{
    KStarsData *data = KStarsData::Instance();
    QTime t          = o->riseSetTime(data->ut(), data->geo(), isRaise);
    if (t.isValid())
    {
        //We can round to the nearest minute by simply adding 30 seconds to the time.
        QString time = QLocale().toString(t.addSecs(30), QLocale::ShortFormat);
        return isRaise ? i18n("Rise time: %1", time) :
                         i18nc("the time at which an object falls below the horizon",
                               "Set time: %1", time);
    }
    if (o->alt().Degrees() > 0)
        return isRaise ? i18n("No rise time: Circumpolar") :
                         i18n("No set time: Circumpolar");
    else
        return isRaise ? i18n("No rise time: Never rises") :
                         i18n("No set time: Never rises");
}

// String representation for transit time for object
QString transitTimeLabel(SkyObject *o)
{
    KStarsData *data = KStarsData::Instance();
    QTime t          = o->transitTime(data->ut(), data->geo());
    if (t.isValid())
        //We can round to the nearest minute by simply adding 30 seconds to the time.
        return i18n("Transit time: %1",
                    QLocale().toString(t.addSecs(30), QLocale::ShortFormat));
    else
        return "--:--";
}
} // namespace

KSPopupMenu::KSPopupMenu()
    : QMenu(KStars::Instance()), m_CurrentFlagIdx(-1), m_EditActionMapping(nullptr),
      m_DeleteActionMapping(nullptr)
{
}

KSPopupMenu::~KSPopupMenu()
{
    if (m_EditActionMapping)
    {
        delete m_EditActionMapping;
    }

    if (m_DeleteActionMapping)
    {
        delete m_DeleteActionMapping;
    }
}

void KSPopupMenu::createEmptyMenu(SkyPoint *nullObj)
{
    KStars *ks = KStars::Instance();
    SkyObject o(SkyObject::TYPE_UNKNOWN, nullObj->ra(), nullObj->dec());
    o.setAlt(nullObj->alt());
    o.setAz(nullObj->az());
    initPopupMenu(&o, i18n("Empty sky"), QString(), QString(), false, false);
    addAction(i18nc("Sloan Digital Sky Survey", "Show SDSS Image"), ks->map(),
              SLOT(slotSDSS()));
    addAction(i18nc("Digitized Sky Survey", "Show DSS Image"), ks->map(),
              SLOT(slotDSS()));
}

void KSPopupMenu::slotEditFlag()
{
    if (m_CurrentFlagIdx != -1)
    {
        KStars::Instance()->map()->slotEditFlag(m_CurrentFlagIdx);
    }
}

void KSPopupMenu::slotDeleteFlag()
{
    if (m_CurrentFlagIdx != -1)
    {
        KStars::Instance()->map()->slotDeleteFlag(m_CurrentFlagIdx);
    }
}

void KSPopupMenu::slotEditFlag(QAction *action)
{
    int idx = m_EditActionMapping->value(action, -1);

    if (idx == -1)
    {
        return;
    }

    else
    {
        KStars::Instance()->map()->slotEditFlag(idx);
    }
}

void KSPopupMenu::slotDeleteFlag(QAction *action)
{
    int idx = m_DeleteActionMapping->value(action, -1);

    if (idx == -1)
    {
        return;
    }

    else
    {
        KStars::Instance()->map()->slotDeleteFlag(idx);
    }
}

void KSPopupMenu::createStarMenu(StarObject *star)
{
    KStars *ks = KStars::Instance();
    //Add name, rise/set time, center/track, and detail-window items
    QString name;
    if (star->name() != "star")
    {
        name = star->translatedLongName();
    }
    else
    {
        if (star->getHDIndex())
        {
            name = QString("HD%1").arg(QString::number(star->getHDIndex()));
        }
        else
        {
            // FIXME: this should be some catalog name too
            name = "Star";
        }
    }
    initPopupMenu(star, name, i18n("star"),
                  i18n("%1<sup>m</sup>, %2", star->mag(), star->sptype()));
    //If the star is named, add custom items to popup menu based on object's ImageList and InfoList
    if (star->name() != "star")
    {
        addLinksToMenu(star);
    }
    else
    {
        addAction(i18nc("Sloan Digital Sky Survey", "Show SDSS Image"), ks->map(),
                  SLOT(slotSDSS()));
        addAction(i18nc("Digitized Sky Survey", "Show DSS Image"), ks->map(),
                  SLOT(slotDSS()));
    }
}

void KSPopupMenu::createCatalogObjectMenu(CatalogObject *obj)
{
    QString name     = getObjectName(obj);
    QString typeName = obj->typeName();

    QString info = QString("%1<br>Catalog: %2")
                       .arg(magToStr(obj->mag()))
                       .arg(obj->getCatalog().name);

    if (obj->a() > 0)
        info += QString("<br>[a=%1′, b=%2′]").arg(obj->a()).arg(obj->b());

    initPopupMenu(obj, name, typeName, info);
    addLinksToMenu(obj);
}

void KSPopupMenu::createPlanetMenu(SkyObject *p)
{
    QString info = magToStr(p->mag());
    QString type = i18n("Solar system object");
    initPopupMenu(p, p->translatedName(), type, info);
    addLinksToMenu(p, false); //don't offer DSS images for planets
}

void KSPopupMenu::createMoonMenu(KSMoon *moon)
{
    QString info = QString("%1, %2").arg(magToStr(moon->mag()), moon->phaseName());
    initPopupMenu(moon, moon->translatedName(), QString(), info);
    addLinksToMenu(moon, false); //don't offer DSS images for planets
}

void KSPopupMenu::createSatelliteMenu(Satellite *satellite)
{
    KStars *ks = KStars::Instance();
    QString velocity, altitude, range;
    velocity.setNum(satellite->velocity());
    altitude.setNum(satellite->altitude());
    range.setNum(satellite->range());

    clear();

    addFancyLabel(satellite->name());
    addFancyLabel(satellite->id());
    addFancyLabel(i18n("satellite"));
    addFancyLabel(KStarsData::Instance()
                      ->skyComposite()
                      ->constellationBoundary()
                      ->constellationName(satellite));

    addSeparator();

    addFancyLabel(i18n("Velocity: %1 km/s", velocity), -2);
    addFancyLabel(i18n("Altitude: %1 km", altitude), -2);
    addFancyLabel(i18n("Range: %1 km", range), -2);

    addSeparator();

    //Insert item for centering on object
    addAction(QIcon::fromTheme("snap-nodes-center"), i18n("Center && Track"), ks->map(),
              SLOT(slotCenter()));
    //Insert item for measuring distances
    //FIXME: add key shortcut to menu items properly!
    addAction(QIcon::fromTheme("kruler-east"),
              i18n("Angular Distance To...            ["), ks->map(),
              SLOT(slotBeginAngularDistance()));
    addAction(QIcon::fromTheme("show-path-outline"),
              i18n("Starhop from here to...            "), ks->map(),
              SLOT(slotBeginStarHop()));
    addAction(QIcon::fromTheme("edit-copy"), i18n("Copy TLE to Clipboard"), ks->map(),
              SLOT(slotCopyTLE()));

    //Insert "Add/Remove Label" item
    if (ks->map()->isObjectLabeled(satellite))
        addAction(QIcon::fromTheme("list-remove"), i18n("Remove Label"), ks->map(),
                  SLOT(slotRemoveObjectLabel()));
    else
        addAction(QIcon::fromTheme("label"), i18n("Attach Label"), ks->map(),
                  SLOT(slotAddObjectLabel()));

    addSeparator();
    addINDI();
}

void KSPopupMenu::createSupernovaMenu(Supernova *supernova)
{
    QString name = supernova->name();
    float mag    = supernova->mag();
    float z      = supernova->getRedShift();
    QString type = supernova->getType();

    QString info;

    if (mag < 99)
        info += QString("%1<sup>m</sup> ").arg(mag);
    info += type;
    if (z < 99)
        info += QString(" z: %1").arg(QString::number(z, 'f', 2));

    initPopupMenu(supernova, name, i18n("supernova"), info);
}

void KSPopupMenu::initPopupMenu(SkyObject *obj, const QString &name, const QString &type,
                                QString info, bool showDetails, bool showObsList,
                                bool showFlags)
{
    KStarsData *data = KStarsData::Instance();
    SkyMap *map      = SkyMap::Instance();

    clear();
    bool showLabel = (name != i18n("star") && !name.isEmpty());
    QString Name   = name;

    if (Name.isEmpty())
        Name = i18n("Empty sky");

    addFancyLabel(Name);
    addFancyLabel(type);
    addFancyLabel(info);
    addFancyLabel(KStarsData::Instance()
                      ->skyComposite()
                      ->constellationBoundary()
                      ->constellationName(obj));

    //Insert Rise/Set/Transit labels
    SkyObject *o = obj->clone();
    addSeparator();
    addFancyLabel(riseSetTimeLabel(o, true), -2);
    addFancyLabel(riseSetTimeLabel(o, false), -2);
    addFancyLabel(transitTimeLabel(o), -2);
    addSeparator();
    delete o;

    // Show 'Select this object' item when in object pointing mode and when obj is not empty sky
    if (map->isInObjectPointingMode() && obj->type() != 21)
    {
        addAction(i18n("Select this object"), map, SLOT(slotObjectSelected()));
    }

    //Insert item for centering on object
    addAction(QIcon::fromTheme("snap-nodes-center"), i18n("Center && Track"), map,
              SLOT(slotCenter()));

    if (showFlags)
    {
        //Insert actions for flag operations
        initFlagActions(obj);
    }

    //Insert item for measuring distances
    //FIXME: add key shortcut to menu items properly!
    addAction(QIcon::fromTheme("kruler-east"),
              i18n("Angular Distance To...            ["), map,
              SLOT(slotBeginAngularDistance()));
    addAction(QIcon::fromTheme("show-path-outline"),
              i18n("Starhop from here to...            "), map, SLOT(slotBeginStarHop()));

    //Insert item for Showing details dialog
    if (showDetails)
        addAction(QIcon::fromTheme("view-list-details"),
                  i18nc("Show Detailed Information Dialog", "Details"), map,
                  SLOT(slotDetail()));

    addAction(QIcon::fromTheme("edit-copy"), i18n("Copy Coordinates"), map,
              SLOT(slotCopyCoordinates()));

    //Insert "Add/Remove Label" item
    if (showLabel)
    {
        if (map->isObjectLabeled(obj))
        {
            addAction(QIcon::fromTheme("list-remove"), i18n("Remove Label"), map,
                      SLOT(slotRemoveObjectLabel()));
        }
        else
        {
            addAction(QIcon::fromTheme("label"), i18n("Attach Label"), map,
                      SLOT(slotAddObjectLabel()));
        }
    }
    // Should show observing list
    if (showObsList)
    {
        if (data->observingList()->contains(obj))
            addAction(QIcon::fromTheme("list-remove"),
                      i18n("Remove From Observing WishList"), data->observingList(),
                      SLOT(slotRemoveObject()));
        else
            addAction(QIcon::fromTheme("bookmarks"), i18n("Add to Observing WishList"),
                      data->observingList(), SLOT(slotAddObject()));
    }
    // Should we show trail actions
    TrailObject *t = dynamic_cast<TrailObject *>(obj);
    if (t)
    {
        if (t->hasTrail())
            addAction(i18n("Remove Trail"), map, SLOT(slotRemovePlanetTrail()));
        else
            addAction(i18n("Add Trail"), map, SLOT(slotAddPlanetTrail()));
    }

    addAction(QIcon::fromTheme("redeyes"), i18n("Simulate Eyepiece View"), map,
              SLOT(slotEyepieceView()));

    addSeparator();
    if (obj->isSolarSystem() &&
        obj->type() !=
            SkyObject::
                COMET) // FIXME: We now have asteroids -- so should this not be isMajorPlanet() || Pluto?
    {
        addAction(i18n("View in XPlanet"), map, SLOT(slotStartXplanetViewer()));
    }
    addSeparator();
    addINDI();

    addAction(QIcon::fromTheme("view-list-details"), i18n("View in What's Interesting"),
              this, SLOT(slotViewInWI()));
}

void KSPopupMenu::initFlagActions(SkyObject *obj)
{
    KStars *ks = KStars::Instance();

    QList<int> flags = ks->data()->skyComposite()->flags()->getFlagsNearPix(obj, 5);

    if (flags.isEmpty())
    {
        // There is no flag around clicked SkyObject
        addAction(QIcon::fromTheme("flag"), i18n("Add Flag..."), ks->map(),
                  SLOT(slotAddFlag()));
    }

    else if (flags.size() == 1)
    {
        // There is only one flag around clicked SkyObject
        addAction(QIcon::fromTheme("document-edit"), i18n("Edit Flag"), this,
                  SLOT(slotEditFlag()));
        addAction(QIcon::fromTheme("delete"), i18n("Delete Flag"), this,
                  SLOT(slotDeleteFlag()));

        m_CurrentFlagIdx = flags.first();
    }

    else
    {
        // There are more than one flags around clicked SkyObject - we need to create submenus
        QMenu *editMenu = new QMenu(i18n("Edit Flag..."), KStars::Instance());
        editMenu->setIcon(QIcon::fromTheme("document-edit"));
        QMenu *deleteMenu = new QMenu(i18n("Delete Flag..."), KStars::Instance());
        deleteMenu->setIcon(QIcon::fromTheme("delete"));

        connect(editMenu, SIGNAL(triggered(QAction *)), this,
                SLOT(slotEditFlag(QAction *)));
        connect(deleteMenu, SIGNAL(triggered(QAction *)), this,
                SLOT(slotDeleteFlag(QAction *)));

        if (m_EditActionMapping)
        {
            delete m_EditActionMapping;
        }

        if (m_DeleteActionMapping)
        {
            delete m_DeleteActionMapping;
        }

        m_EditActionMapping   = new QHash<QAction *, int>;
        m_DeleteActionMapping = new QHash<QAction *, int>;

        foreach (int idx, flags)
        {
            QIcon flagIcon(
                QPixmap::fromImage(ks->data()->skyComposite()->flags()->image(idx)));

            QString flagLabel = ks->data()->skyComposite()->flags()->label(idx);
            if (flagLabel.size() > 35)
            {
                flagLabel = flagLabel.left(35);
                flagLabel.append("...");
            }

            QAction *editAction = new QAction(flagIcon, flagLabel, ks);
            editAction->setIconVisibleInMenu(true);
            editMenu->addAction(editAction);
            m_EditActionMapping->insert(editAction, idx);

            QAction *deleteAction = new QAction(flagIcon, flagLabel, ks);
            deleteAction->setIconVisibleInMenu(true);
            deleteMenu->addAction(deleteAction);
            m_DeleteActionMapping->insert(deleteAction, idx);
        }

        addMenu(editMenu);
        addMenu(deleteMenu);
    }
}

void KSPopupMenu::addLinksToMenu(SkyObject *obj, bool showDSS)
{
    KStars *ks = KStars::Instance();

    const auto &user_data    = KStarsData::Instance()->getUserData(obj->name());
    const auto &image_list   = user_data.images();
    const auto &website_list = user_data.websites();
    for (const auto &res : std::list<
             std::tuple<QString, SkyObjectUserdata::LinkList, SkyObjectUserdata::Type>>{
             { i18n("Image Resources"), image_list, SkyObjectUserdata::Type::image },
             { i18n("Web Links"), website_list, SkyObjectUserdata::Type::website } })
    {
        const auto &title = std::get<0>(res);
        const auto &list  = std::get<1>(res);
        const auto &type  = std::get<2>(res);

        if (!list.empty())
        {
            QMenu *LinkSubMenu = new QMenu();
            LinkSubMenu->setTitle(title);
            for (const auto &entry : list)
            {
                QString t = QString(entry.title);
                QAction *action;
                if (type == SkyObjectUserdata::Type::website)
                    action = LinkSubMenu->addAction(
                        i18nc("Image/info menu item (should be translated)",
                              t.toLocal8Bit()),
                        ks->map(), SLOT(slotInfo()));
                else
                    action = LinkSubMenu->addAction(
                        i18nc("Image/info menu item (should be translated)",
                              t.toLocal8Bit()),
                        ks->map(), SLOT(slotImage()));

                action->setData(entry.url);
            }
            addMenu(LinkSubMenu);
        }
    }

    // Look for a custom object
    {
        auto *object = dynamic_cast<CatalogObject *>(obj);
        if (object)
        {
            CatalogsDB::DBManager manager{ CatalogsDB::dso_db_path() };

            if (object->getCatalog().mut &&
                manager.get_object(object->getObjectId()).first)
            {
                addAction(i18n("Remove From Local Catalog"), ks->map(),
                          SLOT(slotRemoveCustomObject()));
            }
        }
    }

    if (showDSS)
    {
        addAction(i18nc("Sloan Digital Sky Survey", "Show SDSS Image"), ks->map(),
                  SLOT(slotSDSS()));
        addAction(i18nc("Digitized Sky Survey", "Show DSS Image"), ks->map(),
                  SLOT(slotDSS()));
    }

    if (showDSS)
        addSeparator();
}

#if 0
void KSPopupMenu::addINDI()
{
#ifdef HAVE_INDI

    if (INDIListener::Instance()->size() == 0)
        return;

    foreach (ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *bd = gd->getBaseDevice();

        if (bd == nullptr)
            continue;

        //if (bd->isConnected() == false)
        //    continue;

        QMenu *menuDevice           = nullptr;
        ISD::GDInterface *telescope = nullptr;

        foreach (INDI::Property *pp, gd->getProperties())
        {
            if (pp->getType() != INDI_SWITCH || INDIListener::Instance()->isStandardProperty(pp->getName()) == false
                    || QString(pp->getName()).startsWith("TELESCOPE_MOTION"))
                continue;

            ISwitchVectorProperty *svp = pp->getSwitch();

            for (int i = 0; i < svp->nsp; i++)
            {
                if (menuDevice == nullptr)
                {
                    menuDevice = new QMenu(gd->getDeviceName());
                    addMenu(menuDevice);
                }

                QAction *a = menuDevice->addAction(svp->sp[i].label);

                ISD::GDSetCommand *cmd =
                    new ISD::GDSetCommand(INDI_SWITCH, pp->getName(), svp->sp[i].name, ISS_ON, this);

                connect(a, &QAction::triggered, gd, [gd, cmd] { gd->setProperty(cmd); });

                if (!strcmp(svp->sp[i].name, "SLEW") || !strcmp(svp->sp[i].name, "SYNC") ||
                        !strcmp(svp->sp[i].name, "TRACK"))
                {
                    telescope = INDIListener::Instance()->getDevice(gd->getDeviceName());

                    if (telescope)
                    {
                        connect(a, &QAction::triggered, this, [this, telescope] { telescope->runCommand(INDI_SEND_COORDS); });;
                    }
                }
            }

            if (menuDevice != nullptr)
                menuDevice->addSeparator();
        }

        if (telescope != nullptr && menuDevice != nullptr)
        {
            if (dynamic_cast<ISD::Telescope*>(telescope)->canCustomPark())
            {
                menuDevice->addSeparator();
                QAction *a = menuDevice->addAction(i18n("Slew && Set As Parking Position"));

                QSignalMapper *scopeMapper = new QSignalMapper(this);
                scopeMapper->setMapping(a, INDI_CUSTOM_PARKING);
                connect(a, SIGNAL(triggered()), scopeMapper, SLOT(map()));
                connect(scopeMapper, SIGNAL(mapped(int)), telescope, SLOT(runCommand(int)));
            }

            menuDevice->addSeparator();

            QAction *a = menuDevice->addAction(i18n("Center Crosshair"));

            QSignalMapper *scopeMapper = new QSignalMapper(this);
            scopeMapper->setMapping(a, INDI_FIND_TELESCOPE);
            connect(a, SIGNAL(triggered()), scopeMapper, SLOT(map()));
            connect(scopeMapper, SIGNAL(mapped(int)), telescope, SLOT(runCommand(int)));
        }
    }

#endif
}
#endif

void KSPopupMenu::addINDI()
{
#ifdef HAVE_INDI
    if (INDIListener::Instance()->size() == 0)
        return;

    for (auto device : INDIListener::Instance()->getDevices())
    {
        ISD::Telescope *mount = dynamic_cast<ISD::Telescope *>(device);
        if (!mount)
            continue;

        QMenu *mountMenu = new QMenu(device->getDeviceName());
        mountMenu->setIcon(QIcon::fromTheme("kstars"));
        addMenu(mountMenu);

        if (mount->canGoto() || mount->canSync())
        {
            if (mount->canGoto())
            {
                QAction *a = mountMenu->addAction(QIcon::fromTheme("object-rotate-right"),
                                                  i18nc("Move mount to target", "Goto"));
                a->setEnabled(!mount->isParked());
                connect(a, &QAction::triggered,
                        [mount] { mount->Slew(SkyMap::Instance()->clickedPoint()); });
            }
            if (mount->canSync())
            {
                QAction *a =
                    mountMenu->addAction(QIcon::fromTheme("media-record"),
                                         i18nc("Synchronize mount to target", "Sync"));
                a->setEnabled(!mount->isParked());
                connect(a, &QAction::triggered,
                        [mount] { mount->Sync(SkyMap::Instance()->clickedPoint()); });
            }

            mountMenu->addSeparator();
        }

        if (mount->canAbort())
        {
            QAction *a =
                mountMenu->addAction(QIcon::fromTheme("process-stop"), i18n("Abort"));
            a->setEnabled(!mount->isParked());
            connect(a, &QAction::triggered, [mount] { mount->Abort(); });
            mountMenu->addSeparator();
        }

        if (mount->canPark())
        {
            QAction *park =
                mountMenu->addAction(QIcon::fromTheme("flag-red"), i18n("Park"));
            park->setEnabled(!mount->isParked());
            connect(park, &QAction::triggered, [mount] { mount->Park(); });

            QAction *unpark =
                mountMenu->addAction(QIcon::fromTheme("flag-green"), i18n("UnPark"));
            unpark->setEnabled(mount->isParked());
            connect(unpark, &QAction::triggered, [mount] { mount->UnPark(); });

            mountMenu->addSeparator();
        }

        const SkyObject *clickedObject = KStars::Instance()->map()->clickedObject();
        if (clickedObject && clickedObject->type() == SkyObject::SATELLITE &&
            (mount->canTrackSatellite()))
        {
            const Satellite *sat = dynamic_cast<const Satellite *>(clickedObject);
            const KStarsDateTime currentTime        = KStarsData::Instance()->ut();
            const KStarsDateTime currentTimePlusOne = currentTime.addSecs(60);
            QAction *a =
                mountMenu->addAction(QIcon::fromTheme("arrow"), i18n("Track satellite"));
            a->setEnabled(!mount->isParked());
            connect(a, &QAction::triggered,
                    [mount, sat, currentTime, currentTimePlusOne] {
                        mount->setSatelliteTLEandTrack(sat->tle(), currentTime,
                                                       currentTimePlusOne);
                    });
            mountMenu->addSeparator();
        }

        if (mount->canCustomPark())
        {
            QAction *a = mountMenu->addAction(QIcon::fromTheme("go-jump-declaration"),
                                              i18n("Goto && Set As Parking Position"));
            a->setEnabled(!mount->isParked());
            connect(a, &QAction::triggered,
                    [mount] { mount->runCommand(INDI_CUSTOM_PARKING); });
        }

        QAction *a =
            mountMenu->addAction(QIcon::fromTheme("edit-find"), i18n("Find Telescope"));
        connect(a, &QAction::triggered,
                [mount] { mount->runCommand(INDI_FIND_TELESCOPE); });
    }
#endif
}

void KSPopupMenu::addFancyLabel(const QString &name, int deltaFontSize)
{
    if (name.isEmpty())
        return;
    QLabel *label = new QLabel("<b>" + name + "</b>", this);
    label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    if (deltaFontSize != 0)
    {
        QFont font = label->font();
        font.setPointSize(font.pointSize() + deltaFontSize);
        label->setFont(font);
    }
    QWidgetAction *act = new QWidgetAction(this);
    act->setDefaultWidget(label);
    addAction(act);
}

void KSPopupMenu::slotViewInWI()
{
    if (!KStars::Instance()->map()->clickedObject())
        return;
    if (!KStars::Instance()->isWIVisible())
        KStars::Instance()->slotToggleWIView();
    KStars::Instance()->wiView()->inspectSkyObject(
        KStars::Instance()->map()->clickedObject());
}

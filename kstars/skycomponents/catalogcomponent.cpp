/***************************************************************************
                    catalogcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/17/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "catalogcomponent.h"

#include "catalogdata.h"
#include "kstarsdata.h"
#include "skypainter.h"
#include "skyobjects/starobject.h"
#include "skyobjects/deepskyobject.h"
#include "skycomponents/deepskycomponent.h"

CatalogComponent::CatalogComponent(SkyComposite *parent, const QString &catname, bool showerrs, int index,
                                   bool callLoadData)
    : ListComponent(parent), m_catName(catname), m_Showerrs(showerrs), m_ccIndex(index)
{
    if (callLoadData)
        loadData();
}

CatalogComponent::~CatalogComponent()
{
    // EH? WHY IS THIS EMPTY? -- AS

    // FIXME: Check this and implement it properly when you're not as
    // desperate to sleep as I am right now. -- AS

    /*
    auto removeFromContainer = []( auto &thingToRemove, auto &container ) {
        container.remove( container.indexOf( thingToRemove ) );
    };

    foreach ( SkyObject *obj, m_ObjectList ) {
        // FIXME: Should the name removal be done here!? What part of this giant edifice of code is supposed to take care of this? SkyMapComposite? -- AS
        // FIXME: Removing from any of these containers is VERY SLOW (QList will be O(N) to find and O(1) to remove, whereas QVector will be O(N) to find and O(N) to remove...) -- AS
        removeFromComtainer( objectNames( obj->type() ), obj->name() );
        removeFromContainer( objectLists(obj->type()), QPair<QString, const SkyObject *>(obj->name(), obj) );
        removeFromContainer( objectLists(obj->type()), QPair<QString, const SkyObject *>(obj->longname(), obj) );

        delete obj;
    }
    */
}

void CatalogComponent::_loadData(bool includeCatalogDesignation)
{
    if (includeCatalogDesignation)
        emitProgressText(i18n("Loading custom catalog: %1", m_catName));
    else
        emitProgressText(i18n("Loading internal catalog: %1", m_catName));

    QList<QPair<int, QString>> names;

    KStarsData::Instance()->catalogdb()->GetAllObjects(m_catName, m_ObjectList, names, this, includeCatalogDesignation);

    for (const auto &name : names)
    {
        if (name.first <= SkyObject::TYPE_UNKNOWN)
        {
            //FIXME JM 2016-06-02: inefficient and costly check
            // Need better way around this
            //if (!objectNames(names.at(iter).first).contains(names.at(iter).second))

            // AS: FIXME -- after Artem Fedoskin introduced the
            // objectLists, which is definitely better, we should
            // really work towards doing away with this.

            // N.B. It might be better to use a std::multiset instead
            // of QVector<QPair<>>, because it allows for inexpensive
            // O(log N) lookups, but the filter and find will be very
            // quick because of the binary search tree. HOWEVER, it
            // will come at the cost of a lot of code complexity,
            // since std:: containers may not work well out of the box
            // with Qt's MVC system, so this would be desirable only
            // if N (number of named objects in KStars) becomes very
            // very large such that the filtering in Find Dialog takes
            // too long. -- AS

            objectNames(name.first).append(name.second);
        }
    }

    //FIXME - get rid of objectNames completely. For now only KStars Lite uses objectLists
    for (auto obj : m_ObjectList)
    {
        Q_ASSERT(obj);
        if (obj->type() <= SkyObject::TYPE_UNKNOWN)
        {
            QVector<QPair<QString, const SkyObject *>> &objects = objectLists(obj->type());
            bool dupName                                        = false;
            bool dupLongname                                    = false;

            QString name     = obj->name();
            QString longname = obj->longname();

            //FIXME - find a better way to check for duplicates

            // FIXME: AS: There is an argument for why we may not want
            // to remove duplicates -- when the object is removed, all
            // names seem to be removed, so if there are two objects
            // in KStars with the same name in two different catalogs
            // (e.g. Arp 148 from Arp catalog, and Arp 148 from some
            // miscellaneous catalog), then disabling one catalog
            // removes the name entirely from the list.

            for (const auto &object : objects)
            {
                if (name == object.first)
                    dupName = true;
                if (longname == object.first)
                    dupLongname = true;
            }

            if (!dupName)
            {
                objectLists(obj->type()).append(QPair<QString, const SkyObject *>(name, obj));
            }

            if (!longname.isEmpty() && !dupLongname && name != longname)
            {
                objectLists(obj->type()).append(QPair<QString, const SkyObject *>(longname, obj));
            }
        }
    }

    // Remove Duplicates (see FIXME by AS above)
    for (auto &list : objectNames())
        list.removeDuplicates();

    CatalogData loaded_catalog_data;
    KStarsData::Instance()->catalogdb()->GetCatalogData(m_catName, loaded_catalog_data);
    m_catColor    = loaded_catalog_data.color;
    m_catFluxFreq = loaded_catalog_data.fluxfreq;
    m_catFluxUnit = loaded_catalog_data.fluxunit;
}

void CatalogComponent::update(KSNumbers *)
{
    if (selected())
    {
        KStarsData *data = KStarsData::Instance();
        foreach (SkyObject *obj, m_ObjectList)
        {
            DeepSkyObject *dso = dynamic_cast<DeepSkyObject *>(obj);
            StarObject *so     = dynamic_cast<StarObject *>(obj);
            Q_ASSERT(dso || so); // We either have stars, or deep sky objects
            if (dso)
            {
                // Update the deep sky object if need be
                if (dso->updateID != data->updateID())
                {
                    dso->updateID = data->updateID();
                    if (dso->updateNumID != data->updateNumID())
                    {
                        dso->updateCoords(data->updateNum());
                    }
                    dso->EquatorialToHorizontal(data->lst(), data->geo()->lat());
                }
            }
            else
            {
                // Do exactly the same thing for stars
                if (so->updateID != data->updateID())
                {
                    so->updateID = data->updateID();
                    if (so->updateNumID != data->updateNumID())
                    {
                        so->updateCoords(data->updateNum());
                    }
                    so->EquatorialToHorizontal(data->lst(), data->geo()->lat());
                }
            }
        }
        this->updateID = data->updateID();
    }
}

void CatalogComponent::draw(SkyPainter *skyp)
{
    if (!selected())
        return;

    skyp->setBrush(Qt::NoBrush);
    skyp->setPen(QColor(m_catColor));

    // Check if the coordinates have been updated
    if (updateID != KStarsData::Instance()->updateID())
        update(nullptr);

    //Draw Custom Catalog objects
    // FIXME: Improve using HTM!

    // N.B. Calls to Options::foo() don't might not get optimized to
    // inlining and so we should call them outside the loop for speed.
    float maglim = float(DeepSkyComponent::determineDeepSkyMagnitudeLimit());
    auto zoomFactor = Options::zoomFactor();
    auto sizeRescaling = dms::PI * zoomFactor / 10800.0;
    bool showUnknownMagObjects = Options::showUnknownMagObjects();

    foreach (SkyObject *obj, m_ObjectList)
    {
        if (obj->type() == 0)
        {
            StarObject *starobj = static_cast<StarObject *>(obj);
            // FIXME SKYPAINTER
            skyp->drawPointSource(starobj, starobj->mag(), starobj->spchar());
        }
        else
        {
            // FIXME: this PA calc is totally different from the one that was
            // in DeepSkyComponent which is now in SkyPainter .... O_o
            //      --hdevalence
            // PA for Deep-Sky objects is 90 + PA because major axis is
            // horizontal at PA=0
            // double pa = 90. + map->findPA( dso, o.x(), o.y() );
            //
            // ^ Not sure if above is still valid -- asimha 2016/08/16
            DeepSkyObject *dso = static_cast<DeepSkyObject *>(obj);

            // N.B. Code duplicated from DeepSkyComponent::draw()
            float mag = dso->mag();
            float size = dso->a() * sizeRescaling;
            bool sizeCriterion = (size > 1.0 || zoomFactor > 2000.);
            bool magCriterion  = (mag < maglim) || (showUnknownMagObjects && (std::isnan(mag) || mag > 36.0));
            if (sizeCriterion && magCriterion)
                skyp->drawDeepSkyObject(dso, true);
        }
    }
}

bool CatalogComponent::getVisibility()
{
    return (Options::showCatalog().at(m_ccIndex) > 0) ? true : false;
}

bool CatalogComponent::selected()
{
    // Do not draw / update custom catalogs if show deep-sky is turned off, even if they are chosen.
    if (Options::showCatalogNames().contains(m_catName) && Options::showDeepSky())
        return true;

    return false;
}

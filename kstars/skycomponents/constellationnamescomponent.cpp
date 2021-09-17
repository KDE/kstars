/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "constellationnamescomponent.h"

#include "ksfilereader.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skylabeler.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif
#include "projections/projector.h"
#include "skycomponents/culturelist.h"

#include <QtConcurrent>

ConstellationNamesComponent::ConstellationNamesComponent(SkyComposite *parent, CultureList *cultures)
    : ListComponent(parent)
{
    QtConcurrent::run(this, &ConstellationNamesComponent::loadData, cultures);
}

void ConstellationNamesComponent::loadData(CultureList *cultures)
{
    uint i       = 0;
    bool culture = false;
    KSFileReader fileReader;
    QString cultureName;

    if (!fileReader.open("cnames.dat"))
        return;

    emitProgressText(i18n("Loading constellation names"));

    localCNames = Options::useLocalConstellNames();

    while (fileReader.hasMoreLines())
    {
        QString line, name, abbrev;
        int rah, ram, ras, dd, dm, ds;
        QChar sgn, mode;

        line = fileReader.readLine();

        mode = line.at(0);
        if (mode == 'C')
        {
            cultureName = line.mid(2).trimmed();
            culture     = cultureName == cultures->current();
            i++;
            continue;
        }

        if (culture)
        {
            rah = line.midRef(0, 2).toInt();
            ram = line.midRef(2, 2).toInt();
            ras = line.midRef(4, 2).toInt();

            sgn = line.at(6);
            dd  = line.midRef(7, 2).toInt();
            dm  = line.midRef(9, 2).toInt();
            ds  = line.midRef(11, 2).toInt();

            abbrev = line.mid(13, 3);
            name   = line.mid(17).trimmed();

            if (Options::useLocalConstellNames())
                name = i18nc("Constellation name (optional)", name.toLocal8Bit().data());

            dms r;
            r.setH(rah, ram, ras);
            dms d(dd, dm, ds);

            if (sgn == '-')
                d.setD(-1.0 * d.Degrees());

            SkyObject *o = new SkyObject(SkyObject::CONSTELLATION, r, d, 0.0, name, abbrev);
            o->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            appendListObject(o);

            //Add name to the list of object names
            objectNames(SkyObject::CONSTELLATION).append(name);
            objectLists(SkyObject::CONSTELLATION).append(QPair<QString, const SkyObject *>(name, o));
        }
    }
}

bool ConstellationNamesComponent::selected()
{
#ifndef KSTARS_LITE
    return Options::showCNames() && !(Options::hideOnSlew() && Options::hideCNames() && SkyMap::IsSlewing());
#else
    return Options::showCNames() && !(Options::hideOnSlew() && Options::hideCNames() && SkyMapLite::IsSlewing());
#endif
}

// Don't precess the location of the names
void ConstellationNamesComponent::update(KSNumbers *)
{
    if (!selected())
        return;
    KStarsData *data = KStarsData::Instance();
    foreach (SkyObject *o, m_ObjectList)
        o->EquatorialToHorizontal(data->lst(), data->geo()->lat());
}

void ConstellationNamesComponent::draw(SkyPainter *skyp)
{
    Q_UNUSED(skyp);
#ifndef KSTARS_LITE
    if (!selected())
        return;

    const Projector *proj  = SkyMap::Instance()->projector();
    SkyLabeler *skyLabeler = SkyLabeler::Instance();
    //skyLabeler->useStdFont();
    // Subjective change, but constellation names really need to stand out against everything else
    skyLabeler->setFont(QFont("Arial", 14));
    skyLabeler->setPen(QColor(KStarsData::Instance()->colorScheme()->colorNamed("CNameColor")));

    QString name;
    foreach (SkyObject *p, m_ObjectList)
    {
        if (!proj->checkVisibility(p))
            continue;

        bool visible = false;
        QPointF o    = proj->toScreen(p, false, &visible);
        if (!visible || !proj->onScreen(o))
            continue;

        if (Options::useLatinConstellNames() || Options::useLocalConstellNames())
            name = p->name();
        else
            name = p->name2();

        o.setX(o.x() - 5.0 * name.length());
        skyLabeler->drawGuideLabel(o, name, 0.0);
    }

    skyLabeler->resetFont();
#endif
}

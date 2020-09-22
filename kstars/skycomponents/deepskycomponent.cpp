/***************************************************************************
                          deepskycomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/15/08
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

#include "deepskycomponent.h"

#include "ksfilereader.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "kstars_debug.h"
#include "Options.h"
#include "skylabeler.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif
#include "skymesh.h"
#include "skypainter.h"
#include "htmesh/MeshIterator.h"
#include "projections/projector.h"
#include "skyobjects/deepskyobject.h"

DeepSkyComponent::DeepSkyComponent(SkyComposite *parent) : SkyComponent(parent)
{
    m_skyMesh = SkyMesh::Instance();
    // Add labels
    for (int i = 0; i <= MAX_LINENUMBER_MAG; i++)
        m_labelList[i] = new LabelList;
    loadData();
}

DeepSkyComponent::~DeepSkyComponent()
{
    clearList(m_MessierList);
    clearList(m_NGCList);
    clearList(m_ICList);
    clearList(m_OtherList);
    qDeleteAll(m_DeepSkyIndex);
    m_DeepSkyIndex.clear();
    qDeleteAll(m_MessierIndex);
    m_MessierIndex.clear();
    qDeleteAll(m_NGCIndex);
    m_NGCIndex.clear();
    qDeleteAll(m_ICIndex);
    m_ICIndex.clear();
    qDeleteAll(m_OtherIndex);
    m_OtherIndex.clear();
    for (int i = 0; i <= MAX_LINENUMBER_MAG; i++)
        delete m_labelList[i];
}

bool DeepSkyComponent::selected()
{
    return Options::showDeepSky();
}

void DeepSkyComponent::update(KSNumbers *)
{
}

void DeepSkyComponent::loadData()
{
    KStarsData *data = KStarsData::Instance();
    //Check whether we need to concatenate a split NGC/IC catalog
    //(i.e., if user has downloaded the Steinicke catalog)
    mergeSplitFiles();

    QList<QPair<QString, KSParser::DataTypes>> sequence;
    QList<int> widths;
    sequence.append(qMakePair(QString("Flag"), KSParser::D_QSTRING));
    widths.append(1);

    sequence.append(qMakePair(QString("ID"), KSParser::D_INT));
    widths.append(4);

    sequence.append(qMakePair(QString("suffix"), KSParser::D_QSTRING));
    widths.append(1);

    sequence.append(qMakePair(QString("RA_H"), KSParser::D_INT));
    widths.append(2);

    sequence.append(qMakePair(QString("RA_M"), KSParser::D_INT));
    widths.append(2);

    sequence.append(qMakePair(QString("RA_S"), KSParser::D_FLOAT));
    widths.append(4);

    sequence.append(qMakePair(QString("D_Sign"), KSParser::D_QSTRING));
    widths.append(2);

    sequence.append(qMakePair(QString("Dec_d"), KSParser::D_INT));
    widths.append(2);

    sequence.append(qMakePair(QString("Dec_m"), KSParser::D_INT));
    widths.append(2);

    sequence.append(qMakePair(QString("Dec_s"), KSParser::D_INT));
    widths.append(2);

    sequence.append(qMakePair(QString("BMag"), KSParser::D_QSTRING));
    widths.append(6);

    sequence.append(qMakePair(QString("type"), KSParser::D_INT));
    widths.append(2);

    sequence.append(qMakePair(QString("a"), KSParser::D_FLOAT));
    widths.append(6);

    sequence.append(qMakePair(QString("b"), KSParser::D_FLOAT));
    widths.append(6);

    sequence.append(qMakePair(QString("pa"), KSParser::D_QSTRING));
    widths.append(4);

    sequence.append(qMakePair(QString("PGC"), KSParser::D_INT));
    widths.append(7);

    sequence.append(qMakePair(QString("other cat"), KSParser::D_QSTRING));
    widths.append(4);

    sequence.append(qMakePair(QString("other1"), KSParser::D_QSTRING));
    widths.append(6);

    sequence.append(qMakePair(QString("other2"), KSParser::D_QSTRING));
    widths.append(6);

    sequence.append(qMakePair(QString("Messr"), KSParser::D_QSTRING));
    widths.append(2);

    sequence.append(qMakePair(QString("MessrNum"), KSParser::D_INT));
    widths.append(4);

    sequence.append(qMakePair(QString("Longname"), KSParser::D_QSTRING));
    //No width to be appended for last sequence object

    QString file_name = KSPaths::locate(QStandardPaths::GenericDataLocation, QString("ngcic.dat"));
    KSParser deep_sky_parser(file_name, '#', sequence, widths);

    deep_sky_parser.SetProgress(i18n("Loading NGC/IC objects"), 13444, 10);
    qCInfo(KSTARS) << "Loading NGC/IC objects";

    QHash<QString, QVariant> row_content;
    while (deep_sky_parser.HasNextRow())
    {
        row_content = deep_sky_parser.ReadNextRow();

        QString iflag;
        QString cat;
        iflag = row_content["Flag"].toString().mid(0, 1); //check for NGC/IC catalog flag
        /*
        Q_ASSERT(iflag == "I" || iflag == "N" || iflag == " ");
        // (spacetime): ^ Why an assert? Change in implementation of ksparser
        //  might result in crash for no reason.
        // n.b. We also allow non-NGC/IC objects which have a blank iflag
        */

        float mag(1000.0);
        int type, ingc, imess(-1), pa;
        int pgc, ugc;
        QString ss, name, name2, longname;
        QString cat2;

        // Designation
        if (iflag == "I")
            cat = "IC";
        else if (iflag == "N")
            cat = "NGC";

        ingc = row_content["ID"].toInt(); // NGC/IC catalog number
        if (ingc == 0)
            cat.clear(); //object is not in NGC or IC catalogs

        QString suffix = row_content["suffix"].toString(); // multipliticity suffixes, eg: the 'A' in NGC 4945A

        //Q_ASSERT(suffix.isEmpty() || (suffix.isEmpty() == false && suffix.at(0) > 0x40 && suffix.at(0) < 0x7B));

        //coordinates
        int rah     = row_content["RA_H"].toInt();
        int ram     = row_content["RA_M"].toInt();
        double ras  = row_content["RA_S"].toDouble();
        QString sgn = row_content["D_Sign"].toString();
        int dd      = row_content["Dec_d"].toInt();
        int dm      = row_content["Dec_m"].toInt();
        int ds      = row_content["Dec_s"].toInt();

        if (!((0.0 <= rah && rah < 24.0) || (0.0 <= ram && ram < 60.0) || (0.0 <= ras && ras < 60.0) ||
              (0.0 <= dd && dd <= 90.0) || (0.0 <= dm && dm < 60.0) || (0.0 <= ds && ds < 60.0)))
        {
            qCWarning(KSTARS) << "Bad coordinates while processing NGC/IC object: " << cat << ingc;
            qCWarning(KSTARS) << "RA H:M:S = " << rah << ":" << ram << ":" << ras << "; Dec D:M:S = " << dd << ":" << dm << ":"
                     << ds;
        }

        //Ignore lines with no coordinate values if not debugging
        if (rah == 0 && ram == 0 && ras == 0)
            continue;

        //B magnitude
        ss = row_content["BMag"].toString();
        if (ss.isEmpty())
        {
            mag = 99.9f;
        }
        else
        {
            mag = ss.toFloat();
        }

        //object type
        type = row_content["type"].toInt();

        //major and minor axes
        float a = row_content["a"].toFloat();
        float b = row_content["b"].toFloat();

        //position angle.  The catalog PA is zero when the Major axis
        //is horizontal.  But we want the angle measured from North, so
        //we set PA = 90 - pa.
        ss = row_content["pa"].toString();
        if (ss.isEmpty())
        {
            pa = 90;
        }
        else
        {
            pa = 90 - ss.toInt();
        }

        //PGC number
        pgc = row_content["PGC"].toInt();

        //UGC number
        if (row_content["other cat"].toString().trimmed() == "UGC")
        {
            ugc = row_content["other1"].toString().toInt();
        }
        else
        {
            ugc = 0;
        }

        //Messier number
        if (row_content["Messr"].toString().trimmed() == "M")
        {
            cat2 = cat;
            if (ingc == 0)
                cat2.clear();
            cat   = 'M';
            imess = row_content["MessrNum"].toInt();
        }

        longname = row_content["Longname"].toString();

        dms r;
        //r.setH(rah, ram, int(ras));
        r.setH(rah+ram/60.0+ras/3600.0);
        dms d(dd, dm, ds);

        if (sgn == "-")
        {
            d.setD(-1.0 * d.Degrees());
        }

        bool hasName = true;
        QString snum;
        if (cat == "IC" || cat == "NGC")
        {
            snum.setNum(ingc);
            name = cat + ' ' + ((suffix.isEmpty()) ? snum : (snum + suffix));
        }
        else if (cat == "M")
        {
            snum.setNum(imess);
            name = cat + ' ' + snum; // Note: Messier has no suffixes
            if (cat2 == "NGC" || cat2 == "IC")
            {
                snum.setNum(ingc);
                name2 = cat2 + ' ' + ((suffix.isEmpty()) ? snum : (snum + suffix));
            }
            else
            {
                name2.clear();
            }
        }
        else
        {
            if (!longname.isEmpty())
                name = longname;
            else
            {
                hasName = false;
                name    = i18n("Unnamed Object");
            }
        }

        name = i18nc("object name (optional)", name.toLatin1().constData());
        if (!longname.isEmpty())
            longname = i18nc("object name (optional)", longname.toLatin1().constData());

        // create new deepskyobject
        DeepSkyObject *o = nullptr;
        if (type == 0)
            type = 1; //Make sure we use CATALOG_STAR, not STAR
        o = new DeepSkyObject(type, r, d, mag, name, name2, longname, cat, a, b, pa, pgc, ugc);
        o->EquatorialToHorizontal(data->lst(), data->geo()->lat());

        // Add the name(s) to the nameHash for fast lookup -jbb
        if (hasName)
        {
            nameHash[name.toLower()] = o;
            if (!longname.isEmpty())
                nameHash[longname.toLower()] = o;
            if (!name2.isEmpty())
                nameHash[name2.toLower()] = o;
        }

        Trixel trixel = m_skyMesh->index(o);

        //Assign object to general DeepSkyObjects list,
        //and a secondary list based on its catalog.
        m_DeepSkyList.append(o);
        appendIndex(o, &m_DeepSkyIndex, trixel);

        if (o->isCatalogM())
        {
            m_MessierList.append(o);
            appendIndex(o, &m_MessierIndex, trixel);
        }
        else if (o->isCatalogNGC())
        {
            m_NGCList.append(o);
            appendIndex(o, &m_NGCIndex, trixel);
        }
        else if (o->isCatalogIC())
        {
            m_ICList.append(o);
            appendIndex(o, &m_ICIndex, trixel);
        }
        else
        {
            m_OtherList.append(o);
            appendIndex(o, &m_OtherIndex, trixel);
        }

        // JM: VERY INEFFICIENT. Disabling for now until we figure out how to deal with dups. QSet?
        //if ( ! name.isEmpty() && !objectNames(type).contains(name))
        if (!name.isEmpty())
        {
            objectNames(type).append(name);
            objectLists(type).append(QPair<QString, SkyObject *>(name, o));
        }

        //Add long name to the list of object names
        //if ( ! longname.isEmpty() && longname != name  && !objectNames(type).contains(longname))
        if (!longname.isEmpty() && longname != name)
        {
            objectNames(type).append(longname);
            objectLists(type).append(QPair<QString, SkyObject *>(longname, o));
        }

        deep_sky_parser.ShowProgress();
    }

    for (auto &list : objectNames())
        list.removeDuplicates();
}

void DeepSkyComponent::mergeSplitFiles()
{
    //If user has downloaded the Steinicke NGC/IC catalog, then it is
    //split into multiple files.  Concatenate these into a single file.
    QString firstFile = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "ngcic01.dat";
    if (!QFile::exists(firstFile))
        return;
    QDir localDir        = QFileInfo(firstFile).absoluteDir();
    QStringList catFiles = localDir.entryList(QStringList("ngcic??.dat"));

    qCInfo(KSTARS) << "Merging split NGC/IC files";

    QString buffer;
    for (auto &fname : catFiles)
    {
        QFile f(localDir.absoluteFilePath(fname));
        if (f.open(QIODevice::ReadOnly))
        {
            QTextStream stream(&f);
            buffer += stream.readAll();

            f.close();
        }
        else
        {
            qCCritical(KSTARS) << QString("Error: Could not open %1 for reading").arg(fname);
        }
    }

    QFile fout(localDir.absoluteFilePath("ngcic.dat"));
    if (fout.open(QIODevice::WriteOnly))
    {
        QTextStream oStream(&fout);
        oStream << buffer;
        fout.close();

        //Remove the split-files
        foreach (const QString &fname, catFiles)
        {
            QString fullname = localDir.absoluteFilePath(fname);
            //DEBUG
            qCInfo(KSTARS) << "Removing " << fullname << " ...";
            QFile::remove(fullname);
        }
    }
}

void DeepSkyComponent::appendIndex(DeepSkyObject *o, DeepSkyIndex *dsIndex, Trixel trixel)
{
    if (!dsIndex->contains(trixel))
    {
        dsIndex->insert(trixel, new DeepSkyList());
    }
    dsIndex->value(trixel)->append(o);
}

void DeepSkyComponent::draw(SkyPainter *skyp)
{
#ifndef KSTARS_LITE
    if (!selected())
        return;

    bool drawFlag;

    drawFlag = Options::showMessier() && !(Options::hideOnSlew() && Options::hideMessier() && SkyMap::IsSlewing());

    drawDeepSkyCatalog(skyp, drawFlag, &m_MessierIndex, "MessColor", Options::showMessierImages() && !Options::showHIPS());

    drawFlag = Options::showNGC() && !(Options::hideOnSlew() && Options::hideNGC() && SkyMap::IsSlewing());

    drawDeepSkyCatalog(skyp, drawFlag, &m_NGCIndex, "NGCColor");

    drawFlag = Options::showIC() && !(Options::hideOnSlew() && Options::hideIC() && SkyMap::IsSlewing());

    drawDeepSkyCatalog(skyp, drawFlag, &m_ICIndex, "ICColor");

    drawFlag = Options::showOther() && !(Options::hideOnSlew() && Options::hideOther() && SkyMap::IsSlewing());

    drawDeepSkyCatalog(skyp, drawFlag, &m_OtherIndex, "NGCColor");
#else
    Q_UNUSED(skyp)
#endif
}

double DeepSkyComponent::determineDeepSkyMagnitudeLimit() {
    double maglim = Options::magLimitDrawDeepSky();
    double lgmin  = log10(MINZOOM);
    double lgmax  = log10(MAXZOOM);
    double lgz    = log10(Options::zoomFactor());

    if (lgz <= 0.75 * lgmax)
        maglim -= (Options::magLimitDrawDeepSky() - Options::magLimitDrawDeepSkyZoomOut()) * (0.75 * lgmax - lgz) /
                  (0.75 * lgmax - lgmin);

    return maglim;
}

void DeepSkyComponent::drawDeepSkyCatalog(SkyPainter *skyp, bool drawObject, DeepSkyIndex *dsIndex,
                                          const QString &colorString, bool drawImage)
{
#ifndef KSTARS_LITE
    if (!(drawObject || drawImage))
        return;

    SkyMap *map           = SkyMap::Instance();
    const Projector *proj = map->projector();
    KStarsData *data      = KStarsData::Instance();

    UpdateID updateID    = data->updateID();
    UpdateID updateNumID = data->updateNumID();

    skyp->setPen(data->colorScheme()->colorNamed(colorString));
    skyp->setBrush(Qt::NoBrush);

    m_hideLabels = (map->isSlewing() && Options::hideOnSlew()) ||
                   !(Options::showDeepSkyMagnitudes() || Options::showDeepSkyNames());

    bool showUnknownMagObjects = Options::showUnknownMagObjects();

    //adjust maglimit for ZoomLevel
    m_zoomMagLimit = DeepSkyComponent::determineDeepSkyMagnitudeLimit();

    double labelMagLim = Options::deepSkyLabelDensity();
    double lgmin  = log10(MINZOOM);
    double lgmax  = log10(MAXZOOM);
    double lgz    = log10(Options::zoomFactor());
    labelMagLim += (Options::magLimitDrawDeepSky() - labelMagLim) * (lgz - lgmin) / (lgmax - lgmin);
    if (labelMagLim > Options::magLimitDrawDeepSky())
        labelMagLim = Options::magLimitDrawDeepSky();

    //DrawID drawID = m_skyMesh->drawID();
    MeshIterator region(m_skyMesh, DRAW_BUF);

    auto zoomFactor = Options::zoomFactor();
    auto sizeRescaling = dms::PI * zoomFactor / 10800.0;
    while (region.hasNext())
    {
        Trixel trixel       = region.next();
        DeepSkyList *dsList = dsIndex->value(trixel);
        if (dsList == nullptr)
            continue;

        for (auto &obj : *dsList)
        {
            //if ( obj->drawID == drawID ) continue;  // only draw each line once
            //obj->drawID = drawID;

            if (obj->updateID != updateID)
            {
                obj->updateID = updateID;
                if (obj->updateNumID != updateNumID)
                {
                    obj->updateCoords(data->updateNum());
                }
                obj->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            }

            float mag  = obj->mag();
            float size = obj->a() * sizeRescaling;

            //only draw objects if flags set, it's bigger than 1 pixel (unless
            //zoom > 2000.), and it's brighter than maglim (unless mag is
            //undefined (=99.9)
            bool sizeCriterion = (size > 1.0 || zoomFactor > 2000.);
            bool magCriterion  = (mag < float(m_zoomMagLimit)) || (showUnknownMagObjects && (std::isnan(mag) || mag > 36.0));
            if (sizeCriterion && magCriterion)
            {
                bool drawn = skyp->drawDeepSkyObject(obj, drawImage);
                if (drawn && !(m_hideLabels || mag > labelMagLim))
                    addLabel(proj->toScreen(obj), obj);
                //FIXME: find a better way to do above
            }
        }
    }
#else
    Q_UNUSED(skyp)
    Q_UNUSED(drawObject)
    Q_UNUSED(dsIndex)
    Q_UNUSED(colorString)
    Q_UNUSED(drawImage)
#endif
}

void DeepSkyComponent::addLabel(const QPointF &p, DeepSkyObject *obj)
{
    int idx = int(obj->mag() * 10.0);
    if (idx < 0)
        idx = 0;
    if (idx > MAX_LINENUMBER_MAG)
        idx = MAX_LINENUMBER_MAG;
    m_labelList[idx]->append(SkyLabel(p, obj));
}

void DeepSkyComponent::drawLabels()
{
#ifndef KSTARS_LITE
    if (m_hideLabels)
        return;

    SkyLabeler *labeler = SkyLabeler::Instance();
    labeler->setPen(QColor(KStarsData::Instance()->colorScheme()->colorNamed("DSNameColor")));

    int max = int(m_zoomMagLimit * 10.0);
    if (max < 0)
        max = 0;
    if (max > MAX_LINENUMBER_MAG)
        max = MAX_LINENUMBER_MAG;

    for (int i = 0; i <= max; i++)
    {
        LabelList *list = m_labelList[i];

        for (const auto &item : *list)
        {
            labeler->drawNameLabel(item.obj, item.o);
        }
        list->clear();
    }
#endif
}

SkyObject *DeepSkyComponent::findByName(const QString &name)
{
    return nameHash[name.toLower()];
}

void DeepSkyComponent::objectsInArea(QList<SkyObject *> &list, const SkyRegion &region)
{
    for (SkyRegion::const_iterator it = region.constBegin(); it != region.constEnd(); ++it)
    {
        Trixel trixel = it.key();

        if (m_DeepSkyIndex.contains(trixel))
        {
            DeepSkyList *dsoList = m_DeepSkyIndex.value(trixel);

            for (auto &dso : *dsoList)
                list.append(dso);
        }
    }
}

//we multiply each catalog's smallest angular distance by the
//following factors before selecting the final nearest object:
// IC catalog = 0.8
// NGC catalog = 0.6
// "other" catalog = 0.6
// Messier object = 0.25
SkyObject *DeepSkyComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    if (!selected())
        return nullptr;

    SkyObject *oTry  = nullptr;
    SkyObject *oBest = nullptr;
    double rTry      = maxrad;
    double rBest     = maxrad;
    double r;
    DeepSkyList *dsList;
    SkyObject *obj;

    MeshIterator region(m_skyMesh, OBJ_NEAREST_BUF);

    while (region.hasNext())
    {
        dsList = m_ICIndex[region.next()];
        if (!dsList)
            continue;

        for (auto &item : *dsList)
        {
            obj = item;
            r   = obj->angularDistanceTo(p).Degrees();
            if (r < rTry)
            {
                rTry = r;
                oTry = obj;
            }
        }
    }

    rTry *= 0.8;
    if (rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    rTry = maxrad;
    region.reset();
    while (region.hasNext())
    {
        dsList = m_NGCIndex[region.next()];
        if (!dsList)
            continue;
        for (auto &item : *dsList)
        {
            obj = item;
            r   = obj->angularDistanceTo(p).Degrees();
            if (r < rTry)
            {
                rTry = r;
                oTry = obj;
            }
        }
    }

    rTry *= 0.6;
    if (rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    rTry = maxrad;

    region.reset();
    while (region.hasNext())
    {
        dsList = m_OtherIndex[region.next()];
        if (!dsList)
            continue;
        for (auto &item : *dsList)
        {
            obj = item;
            r   = obj->angularDistanceTo(p).Degrees();
            if (r < rTry)
            {
                rTry = r;
                oTry = obj;
            }
        }
    }

    rTry *= 0.6;
    if (rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    rTry = maxrad;

    region.reset();
    while (region.hasNext())
    {
        dsList = m_MessierIndex[region.next()];
        if (!dsList)
            continue;
        for (auto &item : *dsList)
        {
            obj = item;
            r   = obj->angularDistanceTo(p).Degrees();
            if (r < rTry)
            {
                rTry = r;
                oTry = obj;
            }
        }
    }

    // -jbb: this is the template of the non-indexed way
    //
    //foreach ( SkyObject *o, m_MessierList ) {
    //  r = o->angularDistanceTo( p ).Degrees();
    //  if ( r < rTry ) {
    //          rTry = r;
    //          oTry = o;
    //  }
    //}

    rTry *= 0.25;
    if (rTry < rBest)
    {
        rBest = rTry;
        oBest = oTry;
    }

    maxrad = rBest;
    return oBest;
}

void DeepSkyComponent::clearList(QList<DeepSkyObject *> &list)
{
    while (!list.isEmpty())
    {
        SkyObject *o = list.takeFirst();
        removeFromNames(o);
        delete o;
    }
}

/*
    SPDX-FileCopyrightText: 2009 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "flagcomponent.h"

#include "ksfilereader.h"
#include "kstarsdata.h"
#include "Options.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "skypainter.h"
#include "auxiliary/kspaths.h"
#include "projections/projector.h"
#include "skyobjects/skypoint.h"

#include <KLocalizedString>

#include <QDir>
#include <QtMath>
#include <QStandardPaths>

FlagComponent::FlagComponent(SkyComposite *parent) : PointListComponent(parent)
{
    // Add the default flag images to available images list
    m_Names.append(i18n("No icon"));
    m_Images.append(QImage());
    m_Names.append(i18n("Default"));
    m_Images.append(QImage(KSPaths::locate(QStandardPaths::AppDataLocation, "defaultflag.gif")));

    QDir appDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation));
    appDir.setNameFilters(QStringList() << "flag*");
    // Add all other images found in user appdata directory
    for (auto &item : appDir.entryList())
    {
        QString path = appDir.absoluteFilePath(item);
        m_Images.append(QImage(path));

        QString fileName = item.replace(QRegExp("\\.[^.]*$"), QString()).replace(QRegExp("^flag"), QString()).replace('_', ' ');

        m_Names.append(fileName);
    }

    loadFromFile();
}

void FlagComponent::draw(SkyPainter *skyp)
{
    // Return if flags must not be draw
    if (!selected())
        return;

    // Return if no images are available
    if (m_Names.size() < 1)
        return;

    // Draw all flags
    skyp->drawFlags();
}

bool FlagComponent::selected()
{
    return Options::showFlags();
}

void FlagComponent::loadFromFile()
{
    bool imageFound = false;
    QList<QStringList> flagList = KStarsData::Instance()->userdb()->GetAllFlags();

    for (auto &flagEntry : flagList)
    {
        // Read coordinates
        dms r(flagEntry.at(0));
        dms d(flagEntry.at(1));

        m_EpochCoords.append(qMakePair(r.Degrees(), d.Degrees()));

        std::shared_ptr<SkyPoint> flagPoint(new SkyPoint(r, d));

        // Convert to JNow
        toJ2000(flagPoint.get(), flagEntry.at(2));

        flagPoint->updateCoordsNow(KStarsData::Instance()->updateNum());

        pointList().append(std::move(flagPoint));

        // Read epoch
        m_Epoch.append(flagEntry.at(2));

        // Read image name
        QString str = flagEntry.at(3);
        str         = str.replace('_', ' ');
        for (int i = 0; i < m_Names.size(); ++i)
        {
            if (str == m_Names.at(i))
            {
                m_FlagImages.append(i);
                imageFound = true;
            }
        }

        // If the image specified in db does not exist,
        // use the default one
        if (!imageFound)
            m_FlagImages.append(0);

        imageFound = false;

        // If there is no label, use an empty string, red color and continue.
        m_Labels.append(flagEntry.at(4));

        // color label

        QRegExp rxLabelColor("^#[a-fA-F0-9]{6}$");
        if (rxLabelColor.exactMatch(flagEntry.at(5)))
        {
            m_LabelColors.append(QColor(flagEntry.at(5)));
        }
        else
        {
            m_LabelColors.append(QColor("red"));
        }
    }
}

void FlagComponent::saveToFile()
{
    /*
    TODO: This is a really bad way of storing things. Adding one flag shouldn't
    involve writing a new file/table every time. Needs fixing.
    */
    KStarsData::Instance()->userdb()->DeleteAllFlags();    

    for (int i = 0; i < size(); ++i)
    {
        KStarsData::Instance()->userdb()->AddFlag(QString::number(epochCoords(i).first),
                                                  QString::number(epochCoords(i).second), epoch(i),
                                                  imageName(i).replace(' ', '_'), label(i), labelColor(i).name());
    }
}

void FlagComponent::add(const SkyPoint &flagPoint, QString epoch, QString image, QString label, QColor labelColor)
{
    //JM 2015-02-21: Insert original coords in list and convert skypint to JNow
    // JM 2017-02-07: Discard above! We add RAW epoch coordinates to list.
    // If not J2000, we convert to J2000
    m_EpochCoords.append(qMakePair(flagPoint.ra().Degrees(), flagPoint.dec().Degrees()));

    std::shared_ptr<SkyPoint> newFlagPoint(new SkyPoint(flagPoint.ra(), flagPoint.dec()));

    toJ2000(newFlagPoint.get(), epoch);

    newFlagPoint->updateCoordsNow(KStarsData::Instance()->updateNum());

    pointList().append(std::move(newFlagPoint));
    m_Epoch.append(epoch);

    for (int i = 0; i < m_Names.size(); i++)
    {
        if (image == m_Names.at(i))
            m_FlagImages.append(i);
    }

    m_Labels.append(label);
    m_LabelColors.append(labelColor);
}

void FlagComponent::remove(int index)
{
    // check if flag of required index exists
    if (index > pointList().size() - 1)
    {
        return;
    }

    pointList().removeAt(index);
    m_EpochCoords.removeAt(index);
    m_Epoch.removeAt(index);
    m_FlagImages.removeAt(index);
    m_Labels.removeAt(index);
    m_LabelColors.removeAt(index);

// request SkyMap update
#ifndef KSTARS_LITE
    SkyMap::Instance()->forceUpdate();
#endif
}

void FlagComponent::updateFlag(int index, const SkyPoint &flagPoint, QString epoch, QString image, QString label,
                               QColor labelColor)
{
    if (index < 0 || index > pointList().size() - 1)
        return;

    std::shared_ptr<SkyPoint> existingFlag = pointList().at(index);

    existingFlag->setRA0(flagPoint.ra());
    existingFlag->setDec0(flagPoint.dec());

    // If epoch not J2000, to convert to J2000
    toJ2000(existingFlag.get(), epoch);

    existingFlag->updateCoordsNow(KStarsData::Instance()->updateNum());

    m_EpochCoords.replace(index, qMakePair(flagPoint.ra().Degrees(), flagPoint.dec().Degrees()));

    m_Epoch.replace(index, epoch);

    for (int i = 0; i < m_Names.size(); i++)
    {
        if (image == m_Names.at(i))
            m_FlagImages.replace(index, i);
    }

    m_Labels.replace(index, label);
    m_LabelColors.replace(index, labelColor);
}

QStringList FlagComponent::getNames()
{
    return m_Names;
}

int FlagComponent::size()
{
    return pointList().size();
}

QString FlagComponent::epoch(int index)
{
    if (index > m_Epoch.size() - 1)
    {
        return QString();
    }

    return m_Epoch.at(index);
}

QString FlagComponent::label(int index)
{
    if (index > m_Labels.size() - 1)
    {
        return QString();
    }

    return m_Labels.at(index);
}

QColor FlagComponent::labelColor(int index)
{
    if (index > m_LabelColors.size() - 1)
    {
        return QColor();
    }

    return m_LabelColors.at(index);
}

QImage FlagComponent::image(int index)
{
    if (index > m_FlagImages.size() - 1)
    {
        return QImage();
    }

    if (m_FlagImages.at(index) > m_Images.size() - 1)
    {
        return QImage();
    }

    return m_Images.at(m_FlagImages.at(index));
}

QString FlagComponent::imageName(int index)
{
    if (index > m_FlagImages.size() - 1)
    {
        return QString();
    }

    if (m_FlagImages.at(index) > m_Names.size() - 1)
    {
        return QString();
    }

    return m_Names.at(m_FlagImages.at(index));
}

QList<QImage> FlagComponent::imageList()
{
    return m_Images;
}

QList<int> FlagComponent::getFlagsNearPix(SkyPoint *point, int pixelRadius)
{
#ifdef KSTARS_LITE
    const Projector *proj = SkyMapLite::Instance()->projector();
#else
    const Projector *proj = SkyMap::Instance()->projector();
#endif
    QPointF pos = proj->toScreen(point);
    QList<int> retVal;
    int ptr = 0;

    for (auto &cp : pointList())
    {
        if (std::isnan(cp->ra().Degrees()) || std::isnan(cp->dec().Degrees()))
            continue;
        cp->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
        QPointF pos2 = proj->toScreen(cp.get());
        int dx       = (pos2 - pos).x();
        int dy       = (pos2 - pos).y();

        if (qSqrt(dx * dx + dy * dy) <= pixelRadius)
        {
            //point is inside pixelRadius circle
            retVal.append(ptr);
        }

        ptr++;
    }

    return retVal;
}

QImage FlagComponent::imageList(int index)
{
    if (index < 0 || index > m_Images.size() - 1)
    {
        return QImage();
    }

    return m_Images.at(index);
}

void FlagComponent::toJ2000(SkyPoint *p, QString epoch)
{
    KStarsDateTime dt;
    dt.setFromEpoch(epoch);

    if (dt.djd() == J2000)
        return;

    p->catalogueCoord(dt.djd());

    // Store J2000 coords in RA0, DEC0
    p->setRA0(p->ra());
    p->setDec0(p->dec());
}

QPair<double, double> FlagComponent::epochCoords(int index)
{
    if (index > m_FlagImages.size() - 1)
    {
        QPair<double, double> coord = qMakePair(0, 0);
        return coord;
    }

    return m_EpochCoords.at(index);
}

void FlagComponent::update(KSNumbers *num)
{
    if (!selected())
        return;
    KStarsData *data = KStarsData::Instance();

    for (auto &p : pointList())
    {
        if (num)
            p->updateCoordsNow(num);

        p->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    }
}

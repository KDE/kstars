/*
    SPDX-FileCopyrightText: 2015 M.S.Adityan <msadityan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "constellationartcomponent.h"

#include "constellationsart.h"
#include "culturelist.h"
#include "kspaths.h"
#include "Options.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#include "skypainter.h"
#endif

#include <QSqlError>
#include <QSqlQuery>

ConstellationArtComponent::ConstellationArtComponent(SkyComposite *parent, CultureList *cultures) : SkyComponent(parent)
{
    cultureName = cultures->current();
    records     = 0;
    loadData();
}

ConstellationArtComponent::~ConstellationArtComponent()
{
    deleteData();
}

void ConstellationArtComponent::deleteData()
{
    qDeleteAll(m_ConstList);
    m_ConstList.clear();
}

void ConstellationArtComponent::loadData()
{
    if (m_ConstList.isEmpty())
    {
        QSqlDatabase skydb = QSqlDatabase::addDatabase("QSQLITE", "skycultures");
        QString dbfile     = KSPaths::locate(QStandardPaths::AppDataLocation, "skycultures.sqlite");

        skydb.setDatabaseName(dbfile);
        if (skydb.open() == false)
        {
            qWarning() << "Unable to open sky cultures database file " << dbfile;
            return;
        }
        QSqlQuery get_query(skydb);

        if (cultureName == "Western")
        {
            if (!get_query.exec("SELECT * FROM western"))
            {
                qDebug() << get_query.lastError();
                return;
            }
        }
        if (cultureName == "Inuit")
        {
            if (!get_query.exec("SELECT * FROM inuit"))
            {
                qDebug() << get_query.lastError();
                return;
            }
        }

        while (get_query.next())
        {
            QString abbreviation = get_query.value("Abbreviation").toString();
            QString filename     = get_query.value("Filename").toString();
            QString midpointRA   = get_query.value("MidpointRA").toString();
            QString midpointDEC  = get_query.value("MidpointDEC").toString();
            double pa            = get_query.value("Position Angle").toDouble();
            double w             = get_query.value("Width").toDouble();
            double h             = get_query.value("Height").toDouble();

            dms midpointra  = dms::fromString(midpointRA, false);
            dms midpointdec = dms::fromString(midpointDEC, true);

            // appends constellation info
            ConstellationsArt *ca = new ConstellationsArt(midpointra, midpointdec, pa, w, h, abbreviation, filename);
            m_ConstList.append(ca);
            //qDebug()<<"Successfully read skyculture.sqlite"<<abbreviation<<filename<<midpointRA<<midpointDEC<<pa<<w<<h;
            records++;
        }
        //qDebug()<<"Successfully processed"<<records<<"records for"<<cultureName<<"sky culture";
        skydb.close();
    }
}

void ConstellationArtComponent::showList()
{
    int i = 0;
    for (i = 0; i < m_ConstList.size(); i++)
    {
        qDebug() << m_ConstList[i]->getAbbrev() << m_ConstList[i]->getImageFileName();
        qDebug() << m_ConstList[i]->pa();
    }
}

void ConstellationArtComponent::draw(SkyPainter *skyp)
{
    Q_UNUSED(skyp)
#ifndef KSTARS_LITE
    if (Options::showConstellationArt() && SkyMap::IsSlewing() == false)
    {
        for (int i = 0; i < records; i++)
            skyp->drawConstellationArtImage(m_ConstList[i]);
    }

//Loops through the QList containing all data required to draw constellations.
#endif
}

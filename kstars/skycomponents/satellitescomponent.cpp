/*
    SPDX-FileCopyrightText: 2009 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "satellitescomponent.h"

#include "ksfilereader.h"
#include "ksnotification.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skylabeler.h"
#include "skymap.h"
#include "skypainter.h"
#include "skyobjects/satellite.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QtConcurrent>

SatellitesComponent::SatellitesComponent(SkyComposite *parent) : SkyComponent(parent)
{
    QtConcurrent::run(this, &SatellitesComponent::loadData);
}

SatellitesComponent::~SatellitesComponent()
{
    qDeleteAll(m_groups);
    m_groups.clear();
}

void SatellitesComponent::loadData()
{
    KSFileReader fileReader;
    QString line;
    QStringList group_infos;

    if (!fileReader.open("satellites.dat"))
        return;

    emitProgressText(i18n("Loading satellites"));

    while (fileReader.hasMoreLines())
    {
        line = fileReader.readLine();
        if (line.trimmed().isEmpty() || line.at(0) == '#')
            continue;
        group_infos = line.split(';');
        m_groups.append(new SatelliteGroup(group_infos.at(0), group_infos.at(1), QUrl(group_infos.at(2))));
    }

    objectNames(SkyObject::SATELLITE).clear();
    objectLists(SkyObject::SATELLITE).clear();

    foreach (SatelliteGroup *group, m_groups)
    {
        for (int i = 0; i < group->size(); i++)
        {
            Satellite *sat = group->at(i);

            if (sat->selected() && nameHash.contains(sat->name().toLower()) == false)
            {
                objectNames(SkyObject::SATELLITE).append(sat->name());
                objectLists(SkyObject::SATELLITE).append(QPair<QString, const SkyObject *>(sat->name(), sat));
                nameHash[sat->name().toLower()] = sat;
            }
        }
    }
}

bool SatellitesComponent::selected()
{
    return Options::showSatellites();
}

void SatellitesComponent::update(KSNumbers *)
{
    // Return if satellites must not be draw
    if (!selected())
        return;

    foreach (SatelliteGroup *group, m_groups)
    {
        group->updateSatellitesPos();
    }
}

void SatellitesComponent::draw(SkyPainter *skyp)
{
#ifndef KSTARS_LITE
    // Return if satellites must not be draw
    if (!selected())
        return;

    bool hideLabels = (!Options::showSatellitesLabels() || (SkyMap::Instance()->isSlewing() && Options::hideLabels()));

    foreach (SatelliteGroup *group, m_groups)
    {
        for (int i = 0; i < group->size(); i++)
        {
            Satellite *sat = group->at(i);

            if (sat->selected())
            {
                bool drawn = false;
                if (Options::showVisibleSatellites())
                {
                    if (sat->isVisible())
                        drawn = skyp->drawSatellite(sat);
                }
                else
                {
                    drawn = skyp->drawSatellite(sat);
                }

                if (drawn && !hideLabels)
                    SkyLabeler::AddLabel(sat, SkyLabeler::SATELLITE_LABEL);
            }
        }
    }
#else
    Q_UNUSED(skyp);
#endif
}

void SatellitesComponent::drawLabel(Satellite *sat, const QPointF& pos)
{
    SkyLabeler *labeler = SkyLabeler::Instance();
    labeler->setPen(KStarsData::Instance()->colorScheme()->colorNamed("SatLabelColor"));
    labeler->drawNameLabel(sat, pos);
}

void SatellitesComponent::drawTrails(SkyPainter *skyp)
{
    Q_UNUSED(skyp);
}

void SatellitesComponent::updateTLEs()
{
    int i = 0;
    QProgressDialog progressDlg(i18n("Update TLEs..."), i18n("Abort"), 0, m_groups.count());
    progressDlg.setWindowModality(Qt::WindowModal);
    progressDlg.setValue(0);

    foreach (SatelliteGroup *group, m_groups)
    {
        if (progressDlg.wasCanceled())
            return;

        if (group->tleUrl().isEmpty())
            continue;

        progressDlg.setLabelText(i18n("Update %1 satellites", group->name()));
        progressDlg.setWindowTitle(i18nc("@title:window", "Satellite Orbital Elements Update"));

        QNetworkAccessManager manager;
        QNetworkReply *response = manager.get(QNetworkRequest(group->tleUrl()));

        // Wait synchronously
        QEventLoop event;
        QObject::connect(response, SIGNAL(finished()), &event, SLOT(quit()));
        event.exec();

        if (response->error() == QNetworkReply::NoError)
        {
            QFile file(group->tleFilename().toLocalFile());
            if (file.open(QFile::WriteOnly))
            {
                file.write(response->readAll());
                file.close();
                group->readTLE();
                group->updateSatellitesPos();
                progressDlg.setValue(++i);
            }
            else
            {
                KSNotification::error(file.errorString());
                return;
            }
        }
        else
        {
            KSNotification::error(response->errorString());
            return;
        }
    }
}

QList<SatelliteGroup *> SatellitesComponent::groups()
{
    return m_groups;
}

Satellite *SatellitesComponent::findSatellite(QString name)
{
    foreach (SatelliteGroup *group, m_groups)
    {
        for (int i = 0; i < group->size(); i++)
        {
            Satellite *sat = group->at(i);
            if (sat->name() == name)
                return sat;
        }
    }

    return nullptr;
}

SkyObject *SatellitesComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    if (!selected())
        return nullptr;

    //KStarsData* data = KStarsData::Instance();

    SkyObject *oBest = nullptr;
    double rBest     = maxrad;
    double r;

    foreach (SatelliteGroup *group, m_groups)
    {
        for (int i = 0; i < group->size(); i++)
        {
            Satellite *sat = group->at(i);
            if (!sat->selected())
                continue;

            r = sat->angularDistanceTo(p).Degrees();
            //qDebug() << sat->name();
            //qDebug() << "r = " << r << " - max = " << rBest;
            //qDebug() << "ra2=" << sat->ra().Degrees() << " - dec2=" << sat->dec().Degrees();
            if (r < rBest)
            {
                rBest = r;
                oBest = sat;
            }
        }
    }

    maxrad = rBest;
    return oBest;
}

SkyObject *SatellitesComponent::findByName(const QString &name)
{
    return nameHash[name.toLower()];
}

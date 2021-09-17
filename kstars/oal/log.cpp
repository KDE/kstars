/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "log.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "ksutils.h"
#include "observinglist.h"
#include "skymap.h"
#include "skycomponents/constellationboundarylines.h"
#include "skycomponents/skymapcomposite.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"

#include "kstars_debug.h"

OAL::Log::~Log()
{
    //qDeleteAll(m_targetList);
    qDeleteAll(m_observerList);
    qDeleteAll(m_eyepieceList);
    qDeleteAll(m_lensList);
    qDeleteAll(m_filterList);
    qDeleteAll(m_siteList);
    qDeleteAll(m_sessionList);
    qDeleteAll(m_scopeList);
    qDeleteAll(m_observationList);
}

void OAL::Log::writeBegin()
{
    output       = "";
    //m_targetList = KSUtils::makeVanillaPointerList(KStarsData::Instance()->observingList()->sessionList());
    m_targetList = KStarsData::Instance()->observingList()->sessionList();
    writer       = new QXmlStreamWriter(&output);
    writer->setAutoFormatting(true);
    writer->writeStartDocument();
    writer->writeNamespace("http://observation.sourceforge.net/openastronomylog", "oal");
    writer->writeNamespace("http://www.w3.org/2001/XMLSchema-instance", "xsi");
    writer->writeNamespace("http://observation.sourceforge.net/openastronomylog oal20.xsd", "schemaLocation");
    writer->writeStartElement("oal:observations");
    writer->writeAttribute("version", "2.0");
}

QString OAL::Log::writeLog(bool _native)
{
    native = _native;
    writeBegin();
    if (native)
        writeGeoDate();
    writeObservers();
    writeSites();
    writeSessions();
    writeTargets();
    writeScopes();
    writeEyepieces();
    writeLenses();
    writeFilters();
    writeImagers();
    writeObservations();
    writeEnd();
    return output;
}

void OAL::Log::writeEnd()
{
    writer->writeEndDocument();
    delete writer;
}

void OAL::Log::writeObservers()
{
    writer->writeStartElement("observers");
    foreach (OAL::Observer *o, m_observerList)
        writeObserver(o);
    writer->writeEndElement();
}

void OAL::Log::writeSites()
{
    writer->writeStartElement("sites");
    foreach (OAL::Site *o, m_siteList)
        writeSite(o);
    writer->writeEndElement();
}

void OAL::Log::writeSessions()
{
    writer->writeStartElement("sessions");
    foreach (OAL::Session *o, m_sessionList)
        writeSession(o);
    writer->writeEndElement();
}

void OAL::Log::writeTargets()
{
    writer->writeStartElement("targets");
    //foreach (SkyObject *o, m_targetList)
    for (auto &o : m_targetList)
    {
        writeTarget(o.data());
    }
    writer->writeEndElement();
}

void OAL::Log::writeScopes()
{
    writer->writeStartElement("scopes");
    foreach (OAL::Scope *o, m_scopeList)
        writeScope(o);
    writer->writeEndElement();
}

void OAL::Log::writeEyepieces()
{
    writer->writeStartElement("eyepieces");
    foreach (OAL::Eyepiece *o, m_eyepieceList)
        writeEyepiece(o);
    writer->writeEndElement();
}

void OAL::Log::writeLenses()
{
    writer->writeStartElement("lenses");
    foreach (OAL::Lens *o, m_lensList)
        writeLens(o);
    writer->writeEndElement();
}

void OAL::Log::writeFilters()
{
    writer->writeStartElement("filters");
    foreach (OAL::Filter *o, m_filterList)
        writeFilter(o);
    writer->writeEndElement();
}

void OAL::Log::writeImagers()
{
    writer->writeStartElement("imagers");
    writer->writeEndElement();
}

void OAL::Log::writeObservations()
{
    foreach (OAL::Observation *o, m_observationList)
        writeObservation(o);
}

void OAL::Log::writeTarget(SkyObject *o)
{
    writer->writeStartElement("target");
    writer->writeAttribute("id", o->name().remove(' '));
    QString typeString;
    if (native)
        writer->writeAttribute("type", o->typeName());
    else
    {
        switch (o->type())
        {
            case 0:
                typeString = "oal:starTargetType";
                break;
            case 1:
                typeString = "oal:starTargetType";
                break;
            case 2:
                typeString = "oal:PlanetTargetType";
                break;
            case 3:
                typeString = "oal:deepSkyOC";
                break;
            case 4:
                typeString = "oal:deepSkyGC";
                break;
            case 5:
                typeString = "oal:deepSkyGN";
                break;
            case 6:
                typeString = "oal:deepSkyPN";
                break;
            case 8:
                typeString = "oal:deepSkyGX";
                break;
            case 9:
                typeString = "oal:CometTargetType";
                break;
            case 12:
                typeString = "oal:MoonTargetType";
                break;
            case 13:
                typeString = "oal:deepSkyAS";
                break;
            case 14:
                typeString = "oal:deepSkyCG";
                break;
            case 15:
                typeString = "oal:deepSkyDN";
                break;
            case 16:
                typeString = "oal:deepSkyQS";
                break;
            case 17:
                typeString = "oal:deepSkyMS";
                break;
            default:
                typeString = "oal:deepSkyNA";
                break;
        }
        writer->writeAttribute("xsi:type", typeString);
    }
    writer->writeStartElement("datasource");
    writer->writeCDATA("KStars");
    writer->writeEndElement();
    writer->writeStartElement("name");
    QString name = o->name();
    if (name == "star")
    {
        if (((StarObject *)o)->getHDIndex() != 0)
            name = QString("HD %1").arg(((StarObject *)o)->getHDIndex());
    }
    writer->writeCDATA(name);
    writer->writeEndElement();
    writer->writeStartElement("position");
    writer->writeStartElement("ra");
    writer->writeAttribute("unit", "rad");
    writer->writeCharacters(QString::number(o->ra().radians()));
    writer->writeEndElement();
    writer->writeStartElement("dec");
    writer->writeAttribute("unit", "rad");
    writer->writeCharacters(QString::number(o->dec().radians()));
    writer->writeEndElement();
    writer->writeEndElement();
    if (native && !KStarsData::Instance()->observingList()->getTime(o).isEmpty())
    {
        writer->writeStartElement("time");
        writer->writeCDATA(KStarsData::Instance()->observingList()->getTime(o));
        writer->writeEndElement();
    }
    writer->writeStartElement("constellation");
    writer->writeCDATA(KStarsData::Instance()
                           ->skyComposite()
                           ->constellationBoundary()
                           ->constellationName(o));
    writer->writeEndElement();
    writer->writeStartElement("notes");
    writer->writeCDATA(KStarsData::Instance()->getUserData(o->name()).userLog);
    writer->writeEndElement();
    writer->writeEndElement();
}

void OAL::Log::writeObserver(OAL::Observer *o)
{
    writer->writeStartElement("observer");
    writer->writeAttribute("id", o->id());
    writer->writeStartElement("name");
    writer->writeCDATA(o->name());
    writer->writeEndElement();
    writer->writeStartElement("surname");
    writer->writeCDATA(o->surname());
    writer->writeEndElement();
    writer->writeStartElement("contact");
    writer->writeCDATA(o->contact());
    writer->writeEndElement();
    writer->writeEndElement();
}

void OAL::Log::writeSite(OAL::Site *s)
{
    writer->writeStartElement("site");
    writer->writeAttribute("id", s->id());
    writer->writeStartElement("name");
    writer->writeCDATA(s->name());
    writer->writeEndElement();
    writer->writeStartElement("latitude");
    writer->writeAttribute("unit", s->latUnit());
    writer->writeCharacters(QString::number(s->latitude()));
    writer->writeStartElement("longitude");
    writer->writeAttribute("unit", s->lonUnit());
    writer->writeCharacters(QString::number(s->longitude()));
    writer->writeEndElement();
    writer->writeEndElement();
}

void OAL::Log::writeSession(OAL::Session *s)
{
    writer->writeStartElement("session");
    writer->writeAttribute("id", s->id());
    writer->writeStartElement("begin");
    writer->writeCharacters(s->begin().date().toString("yyyy-MM-dd") + 'T' + s->begin().time().toString("hh:mm:ss"));
    writer->writeEndElement();
    writer->writeStartElement("end");
    writer->writeCharacters(s->end().date().toString("yyyy-MM-dd") + 'T' + s->end().time().toString("hh:mm:ss"));
    writer->writeEndElement();
    writer->writeStartElement("site");
    writer->writeCharacters(s->site());
    writer->writeEndElement();
    writer->writeStartElement("weather");
    writer->writeCDATA(s->weather());
    writer->writeEndElement();
    writer->writeStartElement("equipment");
    writer->writeCDATA(s->equipment());
    writer->writeEndElement();
    writer->writeStartElement("comments");
    writer->writeCDATA(s->comments());
    writer->writeEndElement();
    writer->writeEndElement();
}

void OAL::Log::writeScope(OAL::Scope *s)
{
    writer->writeStartElement("scope");
    writer->writeAttribute("id", s->id());
    writer->writeStartElement("model");
    writer->writeCDATA(s->model());
    writer->writeEndElement();
    writer->writeStartElement("type");
    writer->writeCDATA(s->type().at(0));
    writer->writeEndElement();
    writer->writeStartElement("vendor");
    writer->writeCDATA(s->vendor());
    writer->writeEndElement();
    if (s->driver() != i18n("None"))
    {
        writer->writeStartElement("driver");
        writer->writeCharacters(s->driver());
        writer->writeEndElement();
    }
    writer->writeStartElement("aperture");
    writer->writeCharacters(QString::number(s->aperture()));
    writer->writeEndElement();
    writer->writeStartElement("focalLength");
    writer->writeCharacters(QString::number(s->focalLength()));
    writer->writeEndElement();
    writer->writeEndElement();
}

void OAL::Log::writeEyepiece(OAL::Eyepiece *ep)
{
    writer->writeStartElement("eyepiece");
    writer->writeAttribute("id", ep->id());
    writer->writeStartElement("model");
    writer->writeCDATA(ep->model());
    writer->writeEndElement();
    writer->writeStartElement("vendor");
    writer->writeCDATA(ep->vendor());
    writer->writeEndElement();
    writer->writeStartElement("focalLength");
    writer->writeCharacters(QString::number(ep->focalLength()));
    writer->writeEndElement();
    writer->writeStartElement("apparantFOV");
    writer->writeAttribute("unit", ep->fovUnit());
    writer->writeCharacters(QString::number(ep->appFov()));
    writer->writeEndElement();
    writer->writeEndElement();
}

void OAL::Log::writeLens(OAL::Lens *l)
{
    writer->writeStartElement("lens");
    writer->writeAttribute("id", l->id());
    writer->writeStartElement("model");
    writer->writeCDATA(l->model());
    writer->writeEndElement();
    writer->writeStartElement("vendor");
    writer->writeCDATA(l->vendor());
    writer->writeEndElement();
    writer->writeStartElement("factor");
    writer->writeCharacters(QString::number(l->factor()));
    writer->writeEndElement();
    writer->writeEndElement();
}

void OAL::Log::writeFilter(OAL::Filter *f)
{
    writer->writeStartElement("filter");
    writer->writeAttribute("id", f->id());

    // Model
    writer->writeStartElement("model");
    writer->writeCDATA(f->model());
    writer->writeEndElement();
    // Vendor
    writer->writeStartElement("vendor");
    writer->writeCDATA(f->vendor());
    writer->writeEndElement();
    // Type
    writer->writeStartElement("type");
    writer->writeCDATA(f->type());
    writer->writeEndElement();    
    // Color
    writer->writeStartElement("color");
    writer->writeCDATA(f->color());
    writer->writeEndElement();
    // Offset
    writer->writeStartElement("offset");
    writer->writeCDATA(QString::number(f->offset()));
    writer->writeEndElement();
    // Exposure
    writer->writeStartElement("exposure");
    writer->writeCDATA(QString::number(f->exposure()));
    writer->writeEndElement();
    // AutoFocus
    writer->writeStartElement("autofocus");
    writer->writeCDATA(QString::number(f->useAutoFocus() ? 1 : 0));
    writer->writeEndElement();
    // Locked Filter
    writer->writeStartElement("lockedfilter");
    writer->writeCDATA(f->lockedFilter());
    writer->writeEndElement();

    writer->writeEndElement();
}

void OAL::Log::writeObservation(OAL::Observation *o)
{
    writer->writeStartElement("observation");
    writer->writeStartElement("observer");
    writer->writeCharacters(o->observer());
    writer->writeEndElement();
    writer->writeStartElement("site");
    writer->writeCharacters(o->site());
    writer->writeEndElement();
    writer->writeStartElement("session");
    writer->writeCharacters(o->session());
    writer->writeEndElement();
    writer->writeStartElement("target");
    writer->writeCharacters(o->target().remove(' '));
    writer->writeEndElement();
    writer->writeStartElement("begin");
    writer->writeCharacters(o->begin().date().toString("yyyy-MM-dd") + 'T' + o->begin().time().toString("hh:mm:ss"));
    writer->writeEndElement();
    writer->writeStartElement("faintestStar");
    writer->writeCharacters(QString::number(o->faintestStar()));
    writer->writeEndElement();
    writer->writeStartElement("seeing");
    writer->writeCharacters(QString::number(o->seeing()));
    writer->writeEndElement();
    writer->writeStartElement("scope");
    writer->writeCharacters(o->scope());
    writer->writeEndElement();
    writer->writeStartElement("eyepiece");
    writer->writeCharacters(o->eyepiece());
    writer->writeEndElement();
    writer->writeStartElement("lens");
    writer->writeCharacters(o->lens());
    writer->writeEndElement();
    writer->writeStartElement("filter");
    writer->writeCharacters(o->filter());
    writer->writeEndElement();
    writer->writeStartElement("result");
    writer->writeAttribute("xsi:type", "oal:findingsType");
    writer->writeAttribute("lang", o->lang());
    writer->writeStartElement("description");
    writer->writeCDATA(o->result());
    writer->writeEndElement();
    writer->writeEndElement();
    writer->writeEndElement();
}

void OAL::Log::writeGeoDate()
{
    writer->writeStartElement("geodate");
    writer->writeStartElement("name");
    writer->writeCDATA(KStarsData::Instance()->observingList()->geoLocation()->name());
    writer->writeEndElement();
    writer->writeStartElement("province");
    writer->writeCDATA(KStarsData::Instance()->observingList()->geoLocation()->province());
    writer->writeEndElement();
    writer->writeStartElement("country");
    writer->writeCDATA(KStarsData::Instance()->observingList()->geoLocation()->country());
    writer->writeEndElement();
    writer->writeStartElement("date");
    writer->writeCDATA(KStarsData::Instance()->observingList()->dateTime().date().toString("ddMMyyyy"));
    writer->writeEndElement();
    writer->writeEndElement();
}

void OAL::Log::readBegin(QString input)
{
    reader = new QXmlStreamReader(input);
    while (!reader->atEnd())
    {
        reader->readNext();
        if (reader->isStartElement())
        {
            //       if( reader->name() == "oal:observations" && reader->attributes().value("version") == "2.0" )
            readLog();
        }
    }
}

void OAL::Log::readUnknownElement()
{
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
            readUnknownElement();
    }
}

void OAL::Log::readLog()
{
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
        {
            if (reader->name() == "targets")
                readTargets();
            else if (reader->name() == "sites")
                readSites();
            else if (reader->name() == "sessions")
                readSessions();
            else if (reader->name() == "observation")
                readObservation(reader->attributes().value("id").toString());
            else if (reader->name() == "geodate")
                readGeoDate();
            else
                readUnknownElement();
        }
    }
}

void OAL::Log::readTargets()
{
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
        {
            if (reader->name() == "target")
                readTarget();
            else
                readUnknownElement();
        }
    }
}

void OAL::Log::readObservers()
{
    KStars::Instance()->data()->userdb()->GetAllObservers(m_observerList);
}

void OAL::Log::readSites()
{
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
        {
            if (reader->name() == "site")
                readSite(reader->attributes().value("id").toString());
            else
                readUnknownElement();
        }
    }
}

void OAL::Log::readSessions()
{
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
        {
            if (reader->name() == "session")
                readSession(reader->attributes().value("id").toString(), reader->attributes().value("lang").toString());
            else
                readUnknownElement();
        }
    }
}

void OAL::Log::readScopes()
{
    KStars::Instance()->data()->userdb()->GetAllScopes(m_scopeList);
}

void OAL::Log::readEyepieces()
{
    KStars::Instance()->data()->userdb()->GetAllEyepieces(m_eyepieceList);
}

void OAL::Log::readLenses()
{
    KStars::Instance()->data()->userdb()->GetAllLenses(m_lensList);
}

void OAL::Log::readFilters()
{
    KStars::Instance()->data()->userdb()->GetAllFilters(m_filterList);
}

void OAL::Log::readTarget()
{
    SkyObject *o = nullptr;
    SkyPoint pos;
    bool posOK = false;
    QString name, time, notes;
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
        {
            if (reader->name() == "name")
            {
                name = reader->readElementText();
                if (name != "star")
                {
                    o = KStarsData::Instance()->objectNamed(name);
                    if (!o)
                        o = KStarsData::Instance()
                                ->skyComposite()
                                ->findStarByGenetiveName(name);
                    if (o)
                        targetList().append(QSharedPointer<SkyObject>(o->clone()));
                }
            }
            else if (!o && reader->name() == "position")
            {
                pos = readPosition(posOK);
                if (posOK)
                {
                    double maxrd = 0.5;
                    while (!o && maxrd <= 2.0)
                    {
                        o = KStarsData::Instance()->skyComposite()->starNearest(&pos,
                                                                                maxrd);
                        if (!o)
                            maxrd += 0.5;
                    }

                    if (o)
                        targetList().append(QSharedPointer<SkyObject>(o->clone()));
                }
            }
            else if (reader->name() == "time")
            {
                time = reader->readElementText();
                if (o)
                    TimeHash.insert(o->name(), QTime::fromString(time, "h:mm:ss AP"));
            }
            else if (reader->name() == "notes")
            {
                notes = reader->readElementText();
                if (o)
                {
                    KStarsData::Instance()->updateUserLog(o->name(), notes);
                }
            }
            //   else  if( reader->name() == "datasource" )
            //         qDebug() << reader->readElementText();
            //     else if( reader->name() == "position" )
            //         readPosition();
            //     else if( reader->name() == "constellation" )
            //         qDebug() << reader->readElementText();
            else
                readUnknownElement();
        }
    }
}

void OAL::Log::readSite(const QString &id)
{
    QString name, latUnit, lonUnit, lat, lon;
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
        {
            if (reader->name() == "name")
            {
                name = reader->readElementText();
            }
            else if (reader->name() == "latitude")
            {
                lat     = reader->readElementText();
                latUnit = reader->attributes().value("unit").toString();
            }
            else if (reader->name() == "longitude")
            {
                lon     = reader->readElementText();
                lonUnit = reader->attributes().value("unit").toString();
            }
            else
                readUnknownElement();
        }
    }
    OAL::Site *o = new OAL::Site(id, name, lat.toDouble(), latUnit, lon.toDouble(), lonUnit);
    m_siteList.append(o);
}

void OAL::Log::readSession(const QString &id, const QString &lang)
{
    QString site, weather, equipment, comments, begin, end;
    KStarsDateTime beginDT, endDT;
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
        {
            if (reader->name() == "site")
            {
                site = reader->readElementText();
            }
            else if (reader->name() == "begin")
            {
                begin = reader->readElementText();
                beginDT.fromString(begin);
            }
            else if (reader->name() == "end")
            {
                end = reader->readElementText();
                endDT.fromString(begin);
            }
            else if (reader->name() == "weather")
            {
                weather = reader->readElementText();
            }
            else if (reader->name() == "equipment")
            {
                equipment = reader->readElementText();
            }
            else if (reader->name() == "comments")
            {
                comments = reader->readElementText();
            }
            else
                readUnknownElement();
        }
    }
    OAL::Session *o = new OAL::Session(id, site, beginDT, endDT, weather, equipment, comments, lang);
    m_sessionList.append(o);
}

SkyPoint OAL::Log::readPosition(bool &OK)
{
    SkyPoint p;
    bool RAOK = false, DEOK = false;
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
        {
            if (reader->name() == "ra")
            {
                qDebug() << reader->readElementText() << reader->attributes().value("unit");
                dms ra;
                if (reader->attributes().value("unit") == "rad")
                    ra.setRadians(reader->readElementText().toDouble(&RAOK));
                else
                    ra.setD(reader->readElementText().toDouble(&RAOK));

                p.setRA(ra);
            }
            else if (reader->name() == "dec")
            {
                qDebug() << reader->readElementText() << reader->attributes().value("unit");
                dms de;
                if (reader->attributes().value("unit") == "rad")
                    de.setRadians(reader->readElementText().toDouble(&DEOK));
                else
                    de.setD(reader->readElementText().toDouble(&DEOK));

                p.setDec(de);
            }
            else
                readUnknownElement();
        }
    }

    OK = RAOK && DEOK;

    return p;
}

void OAL::Log::readObservation(const QString &id)
{
    QString observer, site, session, target, faintestStar, seeing, scope, eyepiece, lens, filter, result, lang;
    KStarsDateTime begin;
    while (!reader->atEnd())
    {
        reader->readNext();
        if (reader->isEndElement())
            break;
        if (reader->isStartElement())
        {
            if (reader->name() == "observer")
                observer = reader->readElementText();
            else if (reader->name() == "site")
                site = reader->readElementText();
            else if (reader->name() == "session")
                session = reader->readElementText();
            else if (reader->name() == "target")
                target = reader->readElementText();
            else if (reader->name() == "begin")
                begin.fromString(reader->readElementText());
            else if (reader->name() == "faintestStar")
                faintestStar = reader->readElementText();
            else if (reader->name() == "seeing")
                seeing = reader->readElementText();
            else if (reader->name() == "scope")
                scope = reader->readElementText();
            else if (reader->name() == "eyepiece")
                eyepiece = reader->readElementText();
            else if (reader->name() == "lens")
                lens = reader->readElementText();
            else if (reader->name() == "filter")
                filter = reader->readElementText();
            else if (reader->name() == "result")
            {
                lang   = reader->attributes().value("lang").toString();
                result = readResult();
            }
            else
                readUnknownElement();
        }
    }
    OAL::Observation *o = new OAL::Observation(id, observer, site, session, target, begin, faintestStar.toDouble(),
                                               seeing.toDouble(), scope, eyepiece, lens, filter, result, lang);
    m_observationList.append(o);
}

QString OAL::Log::readResult()
{
    QString result;
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;
        if (reader->isStartElement())
        {
            if (reader->name() == "description")
                result = reader->readElementText();
            else
                readUnknownElement();
        }
    }
    return result;
}

void OAL::Log::readGeoDate()
{
    QString name, province, country, date;
    while (!reader->atEnd())
    {
        reader->readNext();

        if (reader->isEndElement())
            break;

        if (reader->isStartElement())
        {
            if (reader->name() == "name")
                name = reader->readElementText();
            else if (reader->name() == "province")
                province = reader->readElementText();
            else if (reader->name() == "country")
                country = reader->readElementText();
            else if (reader->name() == "date")
            {
                date = reader->readElementText();
            }
            else
                readUnknownElement();
        }
    }
    geo = KStarsData::Instance()->locationNamed(name, province, country);
    if (geo == nullptr)
    {
        qCWarning(KSTARS) << "Location " << name << ", " << province << ", " << country
                 << " not found in KStars. Using current location.";
        geo = KStarsData::Instance()->geo();
    }
    dt.setDate(QDate::fromString(date, "ddMMyyyy"));
}

OAL::Observer *OAL::Log::findObserverByName(const QString &name)
{
    foreach (OAL::Observer *obs, *observerList())
    {
        if (QString(obs->name() + ' ' + obs->surname()) == name)
            return obs;
    }
    return nullptr;
}

OAL::Observer *OAL::Log::findObserverById(const QString &id)
{
    foreach (OAL::Observer *obs, *observerList())
    {
        if (obs->id() == id)
            return obs;
    }
    return nullptr;
}

OAL::Session *OAL::Log::findSessionByName(const QString &id)
{
    foreach (OAL::Session *s, *sessionList())
    {
        if (s->id() == id)
            return s;
    }
    return nullptr;
}

OAL::Site *OAL::Log::findSiteById(const QString &id)
{
    foreach (OAL::Site *s, *siteList())
    {
        if (s->id() == id)
            return s;
    }
    return nullptr;
}

OAL::Site *OAL::Log::findSiteByName(const QString &name)
{
    foreach (OAL::Site *s, *siteList())
    {
        if (s->name() == name)
            return s;
    }
    return nullptr;
}

OAL::Scope *OAL::Log::findScopeById(const QString &id)
{
    foreach (OAL::Scope *s, *scopeList())
    {
        if (s->id() == id)
            return s;
    }
    return nullptr;
}

OAL::Eyepiece *OAL::Log::findEyepieceById(const QString &id)
{
    foreach (OAL::Eyepiece *e, *eyepieceList())
    {
        if (e->id() == id)
            return e;
    }
    return nullptr;
}

OAL::Lens *OAL::Log::findLensById(const QString &id)
{
    foreach (OAL::Lens *l, *lensList())
    {
        if (l->id() == id)
            return l;
    }
    return nullptr;
}

OAL::Filter *OAL::Log::findFilterById(const QString &id)
{
    foreach (OAL::Filter *f, *filterList())
    {
        if (f->id() == id)
            return f;
    }
    return nullptr;
}

OAL::Scope *OAL::Log::findScopeByName(const QString &name)
{
    foreach (OAL::Scope *s, *scopeList())
    {
        if (s->name() == name)
            return s;
    }
    return nullptr;
}

OAL::Eyepiece *OAL::Log::findEyepieceByName(const QString &name)
{
    foreach (OAL::Eyepiece *e, *eyepieceList())
    {
        if (e->name() == name)
            return e;
    }
    return nullptr;
}

OAL::Filter *OAL::Log::findFilterByName(const QString &name)
{
    foreach (OAL::Filter *f, *filterList())
    {
        if (f->name() == name)
            return f;
    }
    return nullptr;
}

OAL::Lens *OAL::Log::findLensByName(const QString &name)
{
    foreach (OAL::Lens *l, *lensList())
    {
        if (l->name() == name)
            return l;
    }
    return nullptr;
}

OAL::Observation *OAL::Log::findObservationByName(const QString &id)
{
    foreach (OAL::Observation *o, *observationList())
    {
        if (o->id() == id)
            return o;
    }
    return nullptr;
}

void OAL::Log::readAll()
{
    readObservers();
    readScopes();
    readEyepieces();
    readLenses();
    readFilters();
}

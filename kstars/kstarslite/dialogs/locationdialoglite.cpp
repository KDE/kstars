/***************************************************************************
                          locationialoglite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 21 2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "locationdialoglite.h"
#include "kstarsdata.h"
#include "kstarslite.h"
#include <QQmlContext>
#include "kspaths.h"
#include <QGeoPositionInfoSource>
#include "Options.h"
#include <QNetworkAccessManager>
#include <QNetworkSession>
#include <QNetworkConfigurationManager>
#include <QUrlQuery>
#include <QNetworkReply>

#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

LocationDialogLite::LocationDialogLite()
    :SelectedCity(nullptr), currentGeo(nullptr){
    KStarsLite *kstars = KStarsLite::Instance();
    kstars->qmlEngine()->rootContext()->setContextProperty("CitiesModel", &m_cityList);

    //initialize cities once KStarsData finishes loading everything
    connect(kstars, SIGNAL(dataLoadFinished()), this, SLOT(initCityList()));
    KStarsData* data = KStarsData::Instance();
    connect(data, SIGNAL(geoChanged()), this, SLOT(updateCurrentLocation()));

    nam = new QNetworkAccessManager(this);
    connect(nam, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(processLocationNameData(QNetworkReply*)));
}

void LocationDialogLite::getNameFromCoordinates(double latitude, double longitude) {
    QString lat = QString::number(latitude);
    QString lon = QString::number(longitude);
    QString latlng (lat + ", " + lon);

    QUrl url("http://maps.googleapis.com/maps/api/geocode/json");
    QUrlQuery query;
    query.addQueryItem("latlng", latlng);
    url.setQuery(query);
    qDebug() << "submitting request";

    nam->get(QNetworkRequest(url));
    connect(nam, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(processLocationNameData(QNetworkReply*)));
}

void LocationDialogLite::processLocationNameData(QNetworkReply *networkReply) {
    if (!networkReply)
        return;

    if (!networkReply->error()) {
        QJsonDocument document = QJsonDocument::fromJson(networkReply->readAll());

        if (document.isObject()) {
            QJsonObject obj = document.object();
            QJsonValue val;

            if (obj.contains(QStringLiteral("results"))) {
                val = obj["results"];

                QString city = val.toArray()[0].toObject()["address_components"].toArray()[2].toObject()["long_name"].toString();
                QString region = val.toArray()[0].toObject()["address_components"].toArray()[3].toObject()["long_name"].toString();
                QString country = val.toArray()[0].toObject()["address_components"].toArray()[4].toObject()["long_name"].toString();

                emit newNameFromCoordinates(city, region, country);
            } else {

            }
        }
    }
    networkReply->deleteLater();
}

void LocationDialogLite::initCityList() {
    KStarsData* data = KStarsData::Instance();
    QStringList cities;
    foreach ( GeoLocation *loc, data->getGeoList() )
    {
        QString name = loc->fullName();
        cities.append( name );
        filteredCityList.insert( name, loc );
    }

    //Sort the list of Cities alphabetically...note that filteredCityList may now have a different ordering!
    m_cityList.setStringList(cities);
    m_cityList.sort(0);

    QStringList TZ;

    for ( int i=0; i<25; ++i )
        TZ.append( QLocale().toString( (double)(i-12) ) );
    setProperty("TZList",TZ);

    QStringList DST;

    foreach( const QString& key, data->getRulebook().keys() ) {
        if( !key.isEmpty() )
            DST.append( key );
    }
    setProperty("DSTRules",DST);
}

void LocationDialogLite::filterCity(QString city, QString province, QString country) {
    KStarsData* data = KStarsData::Instance();
    QStringList cities;
    filteredCityList.clear();

    foreach ( GeoLocation *loc, data->getGeoList() ) {
        QString sc( loc->translatedName() );
        QString ss( loc->translatedCountry() );
        QString sp = "";
        if ( !loc->province().isEmpty() )
            sp = loc->translatedProvince();

        if ( sc.toLower().startsWith( city.toLower() ) &&
             sp.toLower().startsWith( province.toLower() ) &&
             ss.toLower().startsWith( country.toLower() ) ) {
            QString name = loc->fullName();
            cities.append( name );
            filteredCityList.insert( name, loc );
        }
    }
    m_cityList.setStringList(cities);
    m_cityList.sort(0);

    setProperty("currLocIndex", m_cityList.stringList().indexOf(m_currentLocation));
}

bool LocationDialogLite::addCity(QString city, QString province, QString country, QString latitude, QString longitude, QString TimeZoneString, QString TZRule) {
    QSqlDatabase mycitydb = getDB();

    if( mycitydb.isValid() ) {
        QString fullName;
        if(!city.isEmpty()) {
            fullName += city;
        }

        if(!province.isEmpty()) {
            fullName += ", " + province;
        }

        if(!country.isEmpty()) {
            fullName += ", " + country;
        }

        if(m_cityList.stringList().contains(fullName)) {
            return editCity(fullName, city, province, country, latitude, longitude, TimeZoneString, TZRule);
        }

        bool latOk(false), lngOk(false), tzOk(false);
        dms lat = createDms( latitude, true, &latOk );
        dms lng = createDms( longitude, true, &lngOk );
        //TimeZoneString.replace( QLocale().decimalPoint(), "." );
        double TZ = TimeZoneString.toDouble( &tzOk );

        if ( ! latOk || ! lngOk  || !tzOk) return false;

        //Strip off white space
        city = city.trimmed();
        province = province.trimmed();
        country = country.trimmed();
        GeoLocation *g = NULL;

        QSqlQuery add_query(mycitydb);
        add_query.prepare("INSERT INTO city(Name, Province, Country, Latitude, Longitude, TZ, TZRule) VALUES(:Name, :Province, :Country, :Latitude, :Longitude, :TZ, :TZRule)");
        add_query.bindValue(":Name", city);
        add_query.bindValue(":Province", province);
        add_query.bindValue(":Country", country);
        add_query.bindValue(":Latitude", lat.toDMSString());
        add_query.bindValue(":Longitude", lng.toDMSString());
        add_query.bindValue(":TZ", TZ);
        add_query.bindValue(":TZRule", TZRule);
        if (add_query.exec() == false)
        {
            qWarning() << add_query.lastError() << endl;
            return false;
        }

        //Add city to geoList
        g = new GeoLocation( lng, lat, city, province, country, TZ, & KStarsData::Instance()->Rulebook[ TZRule ] );
        KStarsData::Instance()->getGeoList().append(g);

        mycitydb.commit();
        mycitydb.close();
        return true;
    }

    return false;
}

bool LocationDialogLite::deleteCity(QString fullName) {
    QSqlDatabase mycitydb = getDB();
    GeoLocation *geo = filteredCityList.value(fullName);

    if( mycitydb.isValid() && geo && !geo->isReadOnly()) {
        QSqlQuery delete_query(mycitydb);
        delete_query.prepare("DELETE FROM city WHERE Name = :Name AND Province = :Province AND Country = :Country");
        delete_query.bindValue(":Name", geo->name());
        delete_query.bindValue(":Province", geo->province());
        delete_query.bindValue(":Country", geo->country());
        if (delete_query.exec() == false)
        {
            qWarning() << delete_query.lastError() << endl;
            return false;
        }

        filteredCityList.remove(geo->fullName());
        KStarsData::Instance()->getGeoList().removeOne(geo);
        delete(geo);
        mycitydb.commit();
        mycitydb.close();
        return true;
    }
    return false;
}

bool LocationDialogLite::editCity(QString fullName, QString city, QString province, QString country, QString latitude, QString longitude, QString TimeZoneString, QString TZRule) {
    QSqlDatabase mycitydb = getDB();
    GeoLocation *geo = filteredCityList.value(fullName);

    bool latOk(false), lngOk(false), tzOk(false);
    dms lat = createDms( latitude, true, &latOk );
    dms lng = createDms( longitude, true, &lngOk );
    double TZ = TimeZoneString.toDouble( &tzOk );

    if( mycitydb.isValid() && geo && !geo->isReadOnly() && latOk && lngOk && tzOk) {
        QSqlQuery update_query(mycitydb);
        update_query.prepare("UPDATE city SET Name = :newName, Province = :newProvince, Country = :newCountry, Latitude = :Latitude, Longitude = :Longitude, TZ = :TZ, TZRule = :TZRule WHERE "
                             "Name = :Name AND Province = :Province AND Country = :Country");
        update_query.bindValue(":newName", city);
        update_query.bindValue(":newProvince", province);
        update_query.bindValue(":newCountry", country);
        update_query.bindValue(":Name", geo->name());
        update_query.bindValue(":Province", geo->province());
        update_query.bindValue(":Country", geo->country());
        update_query.bindValue(":Latitude", lat.toDMSString());
        update_query.bindValue(":Longitude", lng.toDMSString());
        update_query.bindValue(":TZ", TZ);
        update_query.bindValue(":TZRule", TZRule);
        if (update_query.exec() == false)
        {
            qWarning() << update_query.lastError() << endl;
            return false;
        }

        geo->setName(city);
        geo->setProvince(province);
        geo->setCountry(country);
        geo->setLat(lat);
        geo->setLong(lng);
        geo->setTZ(TZ);
        geo->setTZRule(& KStarsData::Instance()->Rulebook[ TZRule ]);

        //If we are changing current location update it
        if(m_currentLocation == fullName) {
            setLocation(geo->fullName());
        }

        mycitydb.commit();
        mycitydb.close();
        return true;
    }
    return false;
}

QString LocationDialogLite::getCity(QString fullName) {
    GeoLocation *geo = filteredCityList.value(fullName);
    if(geo) {
        return geo->name();
    }
    return "";
}

QString LocationDialogLite::getProvince(QString fullName) {
    GeoLocation *geo = filteredCityList.value(fullName);
    if(geo) {
        return geo->province();
    }
    return "";
}

QString LocationDialogLite::getCountry(QString fullName) {
    GeoLocation *geo = filteredCityList.value(fullName);
    if(geo) {
        return geo->country();
    }
    return "";
}

double LocationDialogLite::getLatitude(QString fullName) {
    GeoLocation *geo = filteredCityList.value(fullName);
    if(geo) {
        return geo->lat()->Degrees();
    }
    return 0;
}

double LocationDialogLite::getLongitude(QString fullName) {
    GeoLocation *geo = filteredCityList.value(fullName);
    if(geo) {
        return geo->lng()->Degrees();
    }
    return 0;
}

int LocationDialogLite::getTZ(QString fullName) {
    GeoLocation *geo = filteredCityList.value(fullName);
    if(geo) {
        return m_TZList.indexOf(QString::number(geo->TZ0()));
    }
    return -1;
}

int LocationDialogLite::getDST(QString fullName) {
    GeoLocation *geo = filteredCityList.value(fullName);
    QMap<QString, TimeZoneRule>& Rulebook = KStarsData::Instance()->Rulebook;
    if(geo) {
        foreach( const QString& key, Rulebook.keys() ) {
            if( !key.isEmpty() && geo->tzrule()->equals(&Rulebook[key]) )
                return m_DSTRules.indexOf(key);
        }
    }
    return -1;
}

bool LocationDialogLite::isDuplicate(QString city, QString province, QString country) {
    KStarsData *data = KStarsData::Instance();
    foreach ( GeoLocation *loc, data->getGeoList() ) {
        QString sc( loc->translatedName() );
        QString ss( loc->translatedCountry() );
        QString sp = "";
        if ( !loc->province().isEmpty() )
            sp = loc->translatedProvince();

        if ( sc.toLower() == city.toLower() &&
             sp.toLower() == province.toLower() &&
             ss.toLower() == country.toLower()  ) {
            return true;
        }
    }
    return false;
}

bool LocationDialogLite::isReadOnly(QString fullName) {
    GeoLocation *geo = filteredCityList.value(fullName);
    if(geo) {
        return geo->isReadOnly();
    } else {
        return true; //We return true if geolocation wasn't found
    }
}

QSqlDatabase LocationDialogLite::getDB() {
    QSqlDatabase mycitydb = QSqlDatabase::database("mycitydb");
    QString dbfile = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "mycitydb.sqlite";

    // If it doesn't exist, create it
    if (QFile::exists(dbfile) == false)
    {
        mycitydb.setDatabaseName(dbfile);
        mycitydb.open();
        QSqlQuery create_query(mycitydb);
        QString query("CREATE TABLE city ( "
                      "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                      "Name TEXT DEFAULT NULL, "
                      "Province TEXT DEFAULT NULL, "
                      "Country TEXT DEFAULT NULL, "
                      "Latitude TEXT DEFAULT NULL, "
                      "Longitude TEXT DEFAULT NULL, "
                      "TZ REAL DEFAULT NULL, "
                      "TZRule TEXT DEFAULT NULL)");
        if (create_query.exec(query) == false)
        {
            qWarning() << create_query.lastError() << endl;
            return QSqlDatabase();
        }
    }
    else if (mycitydb.open() == false)
    {
        qWarning() << mycitydb.lastError() << endl;
        return QSqlDatabase();
    }

    return mycitydb;
}

bool LocationDialogLite::checkLongLat(QString longitude, QString latitude) {
    if ( longitude.isEmpty() || latitude.isEmpty() )
        return false;

    bool ok;
    double lng = createDms(longitude, true, &ok).Degrees();
    if( !ok || std::isnan(lng))
        return false;
    double lat = createDms(latitude, true, &ok).Degrees();
    if( !ok || std::isnan(lat))
        return false;

    if( fabs(lng) > 180 || fabs(lat) > 90 )
        return false;

    return true;
}

bool LocationDialogLite::setLocation(QString fullName) {
    KStarsData *data = KStarsData::Instance();

    GeoLocation *geo = filteredCityList.value(fullName);
    if(!geo) {
        foreach ( GeoLocation *loc, data->getGeoList() )
        {
            if( loc->fullName() == fullName) {
                geo = loc;
                break;
            }
        }
    }

    if(geo) {
        // set new location in options
        data->setLocation( *geo );

        // adjust local time to keep UT the same.
        // create new LT without DST offset
        KStarsDateTime ltime = geo->UTtoLT( data->ut() );

        // reset timezonerule to compute next dst change
        geo->tzrule()->reset_with_ltime( ltime, geo->TZ0(), data->isTimeRunningForward() );

        // reset next dst change time
        data->setNextDSTChange( geo->tzrule()->nextDSTChange() );

        // reset local sideral time
        data->syncLST();

        // Make sure Numbers, Moon, planets, and sky objects are updated immediately
        data->setFullTimeUpdate();

        // If the sky is in Horizontal mode and not tracking, reset focus such that
        // Alt/Az remain constant.
        if ( ! Options::isTracking() && Options::useAltAz() ) {
            SkyMapLite::Instance()->focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        }

        // recalculate new times and objects
        data->setSnapNextFocus();
        KStarsLite::Instance()->updateTime();
        return true;
    }
    return false;
}

dms LocationDialogLite::createDms (QString degree, bool deg, bool *ok )
{
    dms dmsAngle(0.0); // FIXME: Should we change this to NaN?
    bool check;
    check = dmsAngle.setFromString( degree, deg );
    if (ok) *ok = check; //ok might be a null pointer!

    return dmsAngle;
}

void LocationDialogLite::setCurrentLocation(QString loc) {
    if(m_currentLocation != loc) {
        m_currentLocation = loc;
        emit currentLocationChanged(loc);
    }
}

void LocationDialogLite::updateCurrentLocation() {
    currentGeo = KStarsData::Instance()->geo();
    setCurrentLocation(currentGeo->fullName());
}

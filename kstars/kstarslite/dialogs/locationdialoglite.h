/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dms.h"

#include <QHash>
#include <QSqlDatabase>
#include <QStringListModel>

class QGeoPositionInfoSource;
class QNetworkAccessManager;
class QNetworkSession;
class QNetworkReply;

class GeoLocation;

/**
 * @class LocationDialogLite
 * A backend of location dialog declared in QML.
 *
 * @author Artem Fedoskin, Jason Harris
 * @version 1.0
 */
class LocationDialogLite : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentLocation READ getCurrentLocation WRITE setCurrentLocation NOTIFY currentLocationChanged)
    Q_PROPERTY(QStringList TZList MEMBER m_TZList NOTIFY TZListChanged)
    Q_PROPERTY(QStringList DSTRules MEMBER m_DSTRules NOTIFY DSTRulesChanged)
    Q_PROPERTY(int currLocIndex MEMBER m_currLocIndex NOTIFY currLocIndexChanged)
  public:
    typedef enum { CITY_ADD, CITY_UPDATE, CITY_REMOVE } CityOperation;

    LocationDialogLite();

    Q_INVOKABLE void filterCity(const QString &city, const QString &province, const QString &country);

    void setCurrentLocation(const QString &loc);
    QString getCurrentLocation() { return m_currentLocation; }
    Q_INVOKABLE bool editCity(const QString &fullName, const QString &city, const QString &province,
                              const QString &country, const QString &latitude,
                              const QString &longitude, const QString &TimeZoneString, const QString &TZRule);

    Q_INVOKABLE bool setLocation(const QString &fullName);

    Q_INVOKABLE bool addCity(const QString &city, const QString &province, const QString &country,
                             const QString &latitude, const QString &longitude,
                             const QString &TimeZoneString, const QString &TZRule);
    Q_INVOKABLE bool deleteCity(const QString &fullName);

    Q_INVOKABLE bool isReadOnly(const QString &fullName);

    Q_INVOKABLE QString getCity(const QString &fullName);
    Q_INVOKABLE QString getProvince(const QString &fullName);
    Q_INVOKABLE QString getCountry(const QString &fullName);
    Q_INVOKABLE double getLatitude(const QString &fullName);
    Q_INVOKABLE double getLongitude(const QString &fullName);
    Q_INVOKABLE int getTZ(const QString &fullName);
    Q_INVOKABLE int getDST(const QString &fullName);
    Q_INVOKABLE bool isDuplicate(const QString &city, const QString &province, const QString &country);

    /**
     * @brief checkLongLat checks whether given longitude and latitude are valid
     */
    Q_INVOKABLE bool checkLongLat(const QString &longitude, const QString &latitude);

    /**
     * TODO - port dmsBox to QML
     * @brief createDms creates dms from string
     * @param degree string that should be converted to degree
     * @param deg if true, the value is in degrees.  Otherwise, it is in hours.
     * @param ok
     * @return angle in dms
     */
    dms createDms(const QString &degree, bool deg, bool *ok);

    /**
     * @short Retrieve name of location by latitude and longitude. Name will be sent with
     * sendNameFromCoordinates signal
     */
    Q_INVOKABLE void getNameFromCoordinates(double latitude, double longitude);

  public slots:
    void initCityList();
    void updateCurrentLocation();

    void processLocationNameData(QNetworkReply *rep);

  signals:
    void currentLocationChanged(QString);
    void TZListChanged(QStringList);
    void DSTRulesChanged(QStringList);
    void currLocIndexChanged(int);
    void newNameFromCoordinates(QString city, QString region, QString country);

  private:
    /**
     * @short checks whether database with cities is already created. Creates a new otherwise
     * @return city database
     */
    QSqlDatabase getDB();

    QStringListModel m_cityList;
    QHash<QString, GeoLocation *> filteredCityList;
    GeoLocation *SelectedCity { nullptr };
    GeoLocation *currentGeo { nullptr };
    QString m_currentLocation;
    int m_currLocIndex { 0 };

    QStringList m_TZList;
    QStringList m_DSTRules;

    //Retrieve the name of city
    QNetworkAccessManager *nam { nullptr };
};

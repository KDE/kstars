/** *************************************************************************
                          locationialoglite.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 21 2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef LOCATIONDIALOGLITE_H_
#define LOCATIONDIALOGLITE_H_

#include <QStringListModel>
#include <QSqlDatabase>
#include <QGeoPositionInfo>

#include "skyobjects/skyobject.h"

/** @class LocationDialogLite
 * A backend of location dialog declared in QML.
 *
 * @short Backend for location dialog in QML
 * @author Artem Fedoskin, Jason Harris
 * @version 1.0
 */

class GeoLocation;
class QGeoPositionInfoSource;

class QNetworkAccessManager;
class QNetworkSession;
class QNetworkReply;

class LocationDialogLite : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLocation READ getCurrentLocation WRITE setCurrentLocation NOTIFY currentLocationChanged)
    Q_PROPERTY(QStringList TZList MEMBER m_TZList NOTIFY TZListChanged)
    Q_PROPERTY(QStringList DSTRules MEMBER m_DSTRules NOTIFY DSTRulesChanged)
    Q_PROPERTY(int currLocIndex MEMBER m_currLocIndex NOTIFY currLocIndexChanged)
public:
    LocationDialogLite();
    typedef enum { CITY_ADD, CITY_UPDATE, CITY_REMOVE } CityOperation;

    Q_INVOKABLE void filterCity(QString city, QString province, QString country);

    void setCurrentLocation(QString loc);
    QString getCurrentLocation() { return m_currentLocation; }
    Q_INVOKABLE bool editCity(QString fullName, QString city, QString province, QString country, QString latitude, QString longitude, QString TimeZoneString, QString TZRule);

    Q_INVOKABLE bool setLocation(QString fullName);

    Q_INVOKABLE bool addCity(QString city, QString province, QString country, QString latitude, QString longitude, QString TimeZoneString, QString TZRule);
    Q_INVOKABLE bool deleteCity(QString fullName);

    Q_INVOKABLE bool isReadOnly(QString fullName);

    Q_INVOKABLE QString getCity(QString fullName);
    Q_INVOKABLE QString getProvince(QString fullName);
    Q_INVOKABLE QString getCountry(QString fullName);
    Q_INVOKABLE double getLatitude(QString fullName);
    Q_INVOKABLE double getLongitude(QString fullName);
    Q_INVOKABLE int getTZ(QString fullName);
    Q_INVOKABLE int getDST(QString fullName);
    Q_INVOKABLE bool isDuplicate(QString city, QString province, QString country);

    /**
     * @brief checkLongLat checks whether given longitude and latitude are valid
     */
    Q_INVOKABLE bool checkLongLat(QString longitude, QString latitude);

    /**
     * TODO - port dmsBox to QML
     * @brief createDms creates dms from string
     * @param degree string that should be converted to degree
     * @param deg if true, the value is in degrees.  Otherwise, it is in hours.
     * @param ok
     * @return angle in dms
     */
    dms createDms ( QString degree, bool deg, bool *ok );

    /**
     * @short retrieve name of location by latitude and longitude. Name will be sent with sendNameFromCoordinates signal
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
    GeoLocation *SelectedCity;
    GeoLocation *currentGeo;
    QString m_currentLocation;
    int m_currLocIndex;

    QStringList m_TZList;
    QStringList m_DSTRules;

    //Retrieve the name of city
    QNetworkAccessManager *nam;
};

#endif

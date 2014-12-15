/***************************************************************************
                          geolocation.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001-2005 by Jason Harris
    email                : jharris@30doradus.org
    copyright            : (C) 2003-2005 by Pablo de Vicente
    email                : p.devicente@wanadoo.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "geolocation.h"
#include "timezonerule.h"


GeoLocation::GeoLocation(dms lng, dms lat,
                          const QString &name, const QString &province, const QString &country, double tz, TimeZoneRule *tzrule, bool readOnly, int iEllips, double hght ) {
    Longitude = lng;
    Latitude = lat;
    Name = name;
    Province = province;
    Country = country;
    TimeZone = tz;
    TZrule = tzrule;
    Height = hght;
    indexEllipsoid = iEllips;
    ReadOnly = readOnly;
    setEllipsoid ( indexEllipsoid );    
    geodToCart();
}

GeoLocation::GeoLocation(double x, double y, double z,
                          const QString &name, const QString &province,
                          const QString &country, double TZ,
                          TimeZoneRule *tzrule, bool readOnly, int iEllips ) {
    PosCartX = x;
    PosCartY = y;
    PosCartZ = z;
    Name = name;
    Province = province;
    Country = country;
    TimeZone = TZ;
    TZrule = tzrule;
    indexEllipsoid = iEllips;
    ReadOnly = readOnly;
    setEllipsoid ( indexEllipsoid );
    cartToGeod();
}

QString GeoLocation::fullName() const {
    if ( province().isEmpty() ) {
        return QString("%1, %2").arg(translatedName(), translatedCountry());
    } else {
        return QString("%1, %2, %3").arg(translatedName(), translatedProvince(), translatedCountry());
    }
}

void GeoLocation::setEllipsoid(int i) {
    static const double A[] = { 6378140.0,       6378137.0,       6378137.0,       6378137.0,        6378136.0 };
    static const double F[] = { 0.0033528131779, 0.0033528106812, 0.0033528131779, 0.00335281066474, 0.0033528131779 };

    Q_ASSERT(i >= 0 && i < sizeof(A)/sizeof(A[0]) && "Index must be in bounds");
    axis       = A[i];
    flattening = F[i];
}

void GeoLocation::changeEllipsoid(int index) {
    setEllipsoid(index);
    cartToGeod();
}

QString GeoLocation::translatedName() const {
    QString context;
    if( province().isEmpty() ) {
        context = QString("City in %1").arg(country());
    } else {
        context = QString("City in %1 %2").arg(province(), country());
    }
    return Name.isEmpty() ? QString() : i18nc(context.toUtf8().data(), Name.toUtf8().data());
}

QString GeoLocation::translatedProvince() const {
    return Province.isEmpty() ? QString() : i18nc(QString("Region/state in " + country()).toUtf8().data(), Province.toUtf8().data());
}

QString GeoLocation::translatedCountry() const {
    return Country.isEmpty() ? QString() : i18nc("Country name", Country.toUtf8().data());
}

void GeoLocation::cartToGeod()
{
    static const double RIT = 2.7778e-6;
    double e2, rpro, lat1, xn, s1, sqrtP2, latd, sinl;

    e2 = 2*flattening-flattening*flattening;

    sqrtP2 = sqrt(PosCartX*PosCartX+PosCartY*PosCartY);

    rpro = PosCartZ/sqrtP2;
    latd = atan2(rpro, (1-e2));
    lat1 = 0.;

    while ( fabs( latd-lat1 ) > RIT ) {
        lat1 = latd;
        s1 = sin(lat1);
        xn = axis/(sqrt(1-e2*s1*s1));
        latd = atan2( (long double)rpro*(1+e2*xn*s1), PosCartZ );
    }

    sinl = sin(latd);
    xn = axis/( sqrt(1-e2*sinl*sinl) );

    Height = sqrtP2/cos(latd)-xn;
    Longitude.setRadians( atan2(PosCartY,PosCartX) );
    Latitude.setRadians(latd);
}

void GeoLocation::geodToCart() {
    double e2, xn;
    double sinLong, cosLong, sinLat, cosLat;

    e2 = 2*flattening-flattening*flattening;

    Longitude.SinCos(sinLong,cosLong);
    Latitude.SinCos(sinLat,cosLat);

    xn = axis/( sqrt(1-e2*sinLat*sinLat) );
    PosCartX = (xn+Height)*cosLat*cosLong;
    PosCartY = (xn+Height)*cosLat*sinLong;
    PosCartZ = (xn*(1-e2)+Height)*sinLat;
}

void GeoLocation::TopocentricVelocity(double vtopo[], dms gst) {

    double Wearth = 7.29211510e-5;     // rads/s
    dms angularVEarth;

    dms time= GSTtoLST(gst);
    // angularVEarth.setRadians(time.Hours()*Wearth*3600.);
    double se, ce;
    // angularVEarth.SinCos(se,ce);
    time.SinCos(se,ce);

    double d0 = sqrt(PosCartX*PosCartX+PosCartY*PosCartY);
    // km/s
    vtopo[0] = - d0 * Wearth * se /1000.;
    vtopo[1] = d0 * Wearth * ce /1000.;
    vtopo[2] = 0.;
}

double GeoLocation::LMST( double jd )
{
    int divresult;
    double ut, tu, gmst, theta;

    ut = ( jd + 0.5 ) - floor( jd + 0.5 );
    jd -= ut;
    tu = ( jd - 2451545. ) / 36525.;
    
    gmst = 24110.54841 + tu * ( 8640184.812866 + tu * ( 0.093104 - tu * 6.2e-6 ) );
    divresult = (int)( ( gmst + 8.6400e4 * 1.00273790934 * ut ) / 8.6400e4 );
    gmst = ( gmst + 8.6400e4 * 1.00273790934 * ut ) - (double)divresult * 8.6400e4;
    theta = 2. * dms::PI * gmst / (24. * 60. * 60.);
    divresult = (int)( ( theta + Longitude.radians() ) / ( 2. * dms::PI ) );
    return( ( theta + Longitude.radians() ) - (double)divresult * 2. * dms::PI );
}

bool GeoLocation::isReadOnly() const
{
    return ReadOnly;
}

void GeoLocation::setReadOnly(bool value)
{
    ReadOnly = value;
}



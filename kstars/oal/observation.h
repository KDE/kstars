/***************************************************************************
                          observation.h  -  description

                             -------------------
    begin                : Wednesday July 8, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef OBSERVATION_H_
#define OBSERVATION_H_

#include <QString>

#include "oal.h"
#include "kstarsdatetime.h"
#include "skyobjects/skyobject.h"

using namespace OAL;

typedef QPair<QString, QString> Result;

class OAL::Observation {
public:
    enum SURFACE_BRIGHTNESS_UNIT {
        SB_MAGS_PER_ARCSEC = 0,
        SB_MAGS_PAR_ARCMIN = 1,
        SB_WRONG_UNIT = 2
    };

    Observation()
    {}

    Observation(const QString &id, const QString &target, const QString &observer, const KStarsDateTime &begin,
                const QList<Result> &results, const QString &site, const QString &session, const QString &scope,
                const QString &eyepiece, const QString &lens, const QString &filter, const QString &imager,
                const QString &accessories, const QStringList &images, const double magn, const bool magnDefined,
                const double faintest, const bool faintestDefined, const double skyquality, const SURFACE_BRIGHTNESS_UNIT squnit,
                const bool skyqualityDefined, const double seeing, const bool seeingDefined, const KStarsDateTime &end,
                const bool endDefined)
    {
        setObservation(id, target, observer, begin, results, site, session, scope, eyepiece, lens, filter, imager,
                       accessories, images, magn, magnDefined, faintest, faintestDefined, skyquality, squnit,
                       skyqualityDefined, seeing, seeingDefined, end, endDefined);
    }

    QString id() const { return m_Id; }
    QString target() const { return m_Target; }
    QString observer() const { return m_Observer; }
    QString site() const { return m_Site; }
    QString session() const { return m_Session; }
    QString scope() const { return m_Scope; }
    QString eyepiece() const { return m_Eyepiece; }
    QString lens() const { return m_Lens; }
    QString filter() const { return m_Filter; }
    QString imager() const { return m_Filter; }
    QString accessories() const { return m_Accessories; }
    QList<Result> results() const { return m_Results; }
    double magnification() const { return m_Magnification; }
    double seeing() const { return m_Seeing; }
    double faintestStar() const { return m_FaintestStar; }
    double skyQuality() const { return m_SkyQuality; }
    SURFACE_BRIGHTNESS_UNIT skyQualityUnit() const { return m_SkyQualityUnit; }
    KStarsDateTime begin() const { return m_Begin; }
    KStarsDateTime end() const { return m_End; }
    QStringList images() const { return m_ImageRefs; }

    bool isMagnificationDefined() const { return m_MagnificationDefined; }
    bool isFaintestStarDefined() const { return m_FaintestStarDefined; }
    bool isSkyQualityDefined() const { return m_SkyQualityDefined; }
    bool isSeeingDefined() const { return m_SeeingDefined; }
    bool isEndDefined() const { return m_EndDefined; }

    void setObservation(const QString &id, const QString &target, const QString &observer, const KStarsDateTime &begin,
                        const QList<Result> &results, const QString &site, const QString &session, const QString &scope,
                        const QString &eyepiece, const QString &lens, const QString &filter, const QString &imager,
                        const QString &accessories, const QStringList &images, const double magn, const bool magnDefined,
                        const double faintest, const bool faintestDefined, const double skyquality, const SURFACE_BRIGHTNESS_UNIT squnit,
                        const bool skyqualityDefined, const double seeing, const bool seeingDefined, const KStarsDateTime &end,
                        const bool endDefined);

private:
    QString m_Id;
    QString m_Target;
    QString m_Observer;
    QString m_Site;
    QString m_Session;
    QString m_Scope;
    QString m_Eyepiece;
    QString m_Lens;
    QString m_Filter;
    QString m_Imager;
    QString m_Accessories;
    QList<Result> m_Results;
    QStringList m_ImageRefs;
    double m_Magnification;
    double m_Seeing;
    double m_FaintestStar;
    double m_SkyQuality;
    SURFACE_BRIGHTNESS_UNIT m_SkyQualityUnit;
    KStarsDateTime m_Begin;
    KStarsDateTime m_End;

    bool m_MagnificationDefined;
    bool m_FaintestStarDefined;
    bool m_SkyQualityDefined;
    bool m_SeeingDefined;
    bool m_EndDefined;
};

#endif

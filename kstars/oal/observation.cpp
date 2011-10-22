/***************************************************************************
                          observation.cpp  -  description

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

#include "observation.h"
#include "observer.h"
#include "observationtarget.h"
#include "site.h"
#include "session.h"
#include "scope.h"
#include "eyepiece.h"
#include "filter.h"
#include "lens.h"

using namespace OAL;

void Observation::setObservation(const QString &id, const QString &target, const QString &observer, const KStarsDateTime &begin,
                                 const QList<Result> &results, const QString &site, const QString &session, const QString &scope,
                                 const QString &eyepiece, const QString &lens, const QString &filter, const QString &imager,
                                 const QString &accessories, const QStringList &images, const double magn, const bool magnDefined,
                                 const double faintest, const bool faintestDefined, const double skyquality, const SURFACE_BRIGHTNESS_UNIT squnit,
                                 const bool skyqualityDefined, const double seeing, const bool seeingDefined, const KStarsDateTime &end,
                                 const bool endDefined)
{
    m_Id = id;
    m_Target = target;
    m_Observer = observer;
    m_Begin = begin;
    m_Results = results;
    m_Site = site;
    m_Session = session;
    m_Scope = scope;
    m_Eyepiece = eyepiece;
    m_Lens = lens;
    m_Filter = filter;
    m_Imager = imager;
    m_Accessories = accessories;
    m_ImageRefs = images;
    m_Magnification = magn;
    m_MagnificationDefined = magnDefined;
    m_FaintestStar = faintest;
    m_FaintestStarDefined = faintestDefined;
    m_SkyQuality = skyquality;
    m_SkyQualityUnit = squnit;
    m_SkyQualityDefined = skyqualityDefined;
    m_Seeing = seeing;
    m_SeeingDefined = seeingDefined;
    m_End = end;
    m_EndDefined = endDefined;
}

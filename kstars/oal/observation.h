/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "oal/oal.h"
#include "kstarsdatetime.h"
#include "skyobjects/skyobject.h"
#include "oal/observer.h"
#include "oal/site.h"
#include "oal/session.h"
#include "oal/scope.h"
#include "oal/eyepiece.h"
#include "oal/filter.h"
#include "oal/lens.h"

#include <QString>

using namespace OAL;

/**
 * @class OAL::Observation
 *
 * Information on observation session details.
 */
class OAL::Observation
{
    public:
        Observation(QString id, QString observer, QString site, QString session, QString target, KStarsDateTime begin,
                    double faintestStar, double seeing, QString scope, QString eyepiece, QString lens, QString filter,
                    QString result, QString lang)
        {
            setObservation(id, observer, site, session, target, begin, faintestStar, seeing, scope, eyepiece, lens, filter,
                           result, lang);
        }
        Observation(QString id, Observer *observer, Session *session, SkyObject *target, KStarsDateTime begin,
                    double faintestStar, double seeing, Scope *scope, Eyepiece *eyepiece, Lens *lens, Filter *filter,
                    QString result, QString lang);
        QString id() const
        {
            return m_Name;
        }
        QString target() const
        {
            return m_Target;
        }
        QString observer() const
        {
            return m_Observer;
        }
        QString site() const
        {
            return m_Site;
        }
        QString session() const
        {
            return m_Session;
        }
        QString scope() const
        {
            return m_Scope;
        }
        QString eyepiece() const
        {
            return m_Eyepiece;
        }
        QString lens() const
        {
            return m_Lens;
        }
        QString filter() const
        {
            return m_Filter;
        }
        QString lang() const
        {
            return m_Lang;
        }
        QString result() const
        {
            return m_Result;
        }
        double seeing() const
        {
            return m_Seeing;
        }
        double faintestStar() const
        {
            return m_FaintestStar;
        }
        KStarsDateTime begin() const
        {
            return m_Begin;
        }
        void setObservation(QString _id, QString _observer, QString _site, QString _session, QString _target,
                            KStarsDateTime _begin, double _faintestStar, double _seeing, QString _scope, QString _eyepiece,
                            QString _lens, QString _filter, QString _result, QString _lang = "en");

    private:
        QString m_Name;
        QString m_Target;
        QString m_Observer;
        QString m_Site;
        QString m_Session;
        QString m_Scope;
        QString m_Result;
        QString m_Eyepiece;
        QString m_Lens;
        QString m_Filter;
        QString m_Lang;
        double m_Seeing;
        double m_FaintestStar;
        KStarsDateTime m_Begin;
};

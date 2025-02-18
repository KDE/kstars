/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "oal/oal.h"
#include <QString>
#include <QDateTime>
#include "ekos/ekos.h"

#define NULL_FILTER "--"
#define DATETIME_FORMAT "yyyy-MM-dd hh:mm:ss"

struct filterProperties
{
    QString vendor;
    QString model;
    QString type;
    QString color;
    int offset;
    double exposure;
    bool useAutoFocus;
    QString lockedFilter;
    int absFocusPos;
    double focusTemperature;
    double focusAltitude;
    QString focusDatetime;
    double focusTicksPerTemp;
    double focusTicksPerAlt;
    double wavelength;

    filterProperties(QString _vendor, QString _model, QString _type, QString _color,
                     int _offset = 0, double _exposure = 1.0, bool _useAutoFocus = false, QString _lockedFilter = NULL_FILTER,
                     int _absFocusPos = 0, double _focusTemperature = Ekos::INVALID_VALUE, double _focusAltitude = Ekos::INVALID_VALUE,
                     QString _focusDatetime = "", double _focusTicksPerTemp = 0.0, double _focusTicksPerAlt = 0.0, double _wavelength = 500.0) :
        vendor(_vendor), model(_model), type(_type), color(_color),
        offset(_offset), exposure(_exposure), useAutoFocus(_useAutoFocus), lockedFilter(_lockedFilter),
          absFocusPos(_absFocusPos), focusTemperature(_focusTemperature), focusAltitude(_focusAltitude), focusDatetime(_focusDatetime),
        focusTicksPerTemp(_focusTicksPerTemp), focusTicksPerAlt(_focusTicksPerAlt), wavelength(_wavelength) {}
};

/**
 * @class OAL::Filter
 *
 * Information of user filters
 */

class OAL::Filter
{
    public:
        Filter(const QString &id, const filterProperties *fp);

        QString id() const
        {
            return m_Id;
        }
        QString name() const
        {
            return m_Name;
        }
        QString model() const
        {
            return m_Model;
        }
        QString vendor() const
        {
            return m_Vendor;
        }
        QString type() const
        {
            return m_Type;
        }
        QString color() const
        {
            return m_Color;
        }

        // Additional fields used by Ekos
        int offset() const
        {
            return m_Offset;
        }
        void setOffset(int _offset)
        {
            m_Offset = _offset;
        }

        double exposure() const
        {
            return m_Exposure;
        }
        void setExposure(double _exposure)
        {
            m_Exposure = _exposure;
        }

        QString lockedFilter() const
        {
            return m_LockedFilter;
        }
        void setLockedFilter(const QString &_filter)
        {
            m_LockedFilter = _filter;
        }

        bool useAutoFocus() const
        {
            return m_UseAutoFocus;
        }
        void setUseAutoFocus(bool enabled)
        {
            m_UseAutoFocus = enabled;
        }

        int absoluteFocusPosition()
        {
            return m_AbsoluteFocusPosition;
        }
        void setAbsoluteFocusPosition(int newAbsFocusPos)
        {
            m_AbsoluteFocusPosition = newAbsFocusPos;
        }

        double focusTemperature()
        {
            return m_FocusTemperature;
        }
        void setFocusTemperature(double newFocusTemperature)
        {
            m_FocusTemperature = newFocusTemperature;
        }

        double focusAltitude()
        {
            return m_FocusAltitude;
        }
        void setFocusAltitude(double newFocusAltitude)
        {
            m_FocusAltitude = newFocusAltitude;
        }

        QDateTime focusDatetime()
        {
            return QDateTime::fromString(m_FocusDatetime, DATETIME_FORMAT);
        }
        void setFocusDatetime(QDateTime newFocusDatetime)
        {
            m_FocusDatetime = newFocusDatetime.toString(DATETIME_FORMAT);
        }

        double focusTicksPerTemp()
        {
            return m_FocusTicksPerTemp;
        }
        void setFocusTicksPerTemp(double newFocusTicksPerTemp)
        {
            m_FocusTicksPerTemp = newFocusTicksPerTemp;
        }

        double focusTicksPerAlt()
        {
            return m_FocusTicksPerAlt;
        }
        void setFocusTicksPerAlt(double newFocusTicksPerAlt)
        {
            m_FocusTicksPerAlt = newFocusTicksPerAlt;
        }

        double wavelength()
        {
            return m_Wavelength;
        }
        void setWavelength(double newWavelength)
        {
            m_Wavelength = newWavelength;
        }

    private:
        QString m_Id, m_Model, m_Vendor, m_Type, m_Color, m_Name, m_LockedFilter;
        int m_Offset { 0 };
        int m_AbsoluteFocusPosition { 0 };
        double m_Exposure { 1.0 };
        bool m_UseAutoFocus { false };
        double m_FocusTemperature { 0 };
        double m_FocusAltitude { 0 };
        QString m_FocusDatetime { "" };
        double m_FocusTicksPerTemp { 0 };
        double m_FocusTicksPerAlt { 0 };
        double m_Wavelength { 0 };
};

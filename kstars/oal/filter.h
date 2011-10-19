/***************************************************************************
                          filter.h  -  description

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
#ifndef FILTER_H_
#define FILTER_H_

#include "oal.h"

#include <QString>

class OAL::Filter {
    public:
        enum FILTER_TYPE {
            FT_OTHER = 0,
            FT_BROAD_BAND = 1,
            FT_NARROW_BAND = 2,
            FT_O_III = 3,
            FT_H_BETA = 4,
            FT_H_ALPHA = 5,
            FT_COLOR = 6,
            FT_NEUTRAL = 7,
            FT_CORRECTIVE = 8,
            FT_SOLAR = 9,
            FT_BAD_TYPE = 10
        };

        enum FILTER_COLOR {
            FC_LIGHT_RED = 0,
            FC_RED = 1,
            FC_DEEP_RED = 2,
            FC_ORANGE = 3,
            FC_LIGHT_YELLOW = 4,
            FC_DEEP_YELLOW = 5,
            FC_YELLOW = 6,
            FC_YELLOW_GREEN = 7,
            FC_LIGHT_GREEN = 8,
            FC_GREEN = 9,
            FC_MEDIUM_BLUE = 10,
            FC_PALE_BLUE = 11,
            FC_BLUE = 12,
            FC_DEEP_BLUE = 13,
            FC_VIOLET = 14
        };

        static QStringList filterTypes();
        static QStringList filterColors();

        static QString filterTypeScreenRepresentation( const FILTER_TYPE type );
        static QString filterTypeOALRepresentation( const FILTER_TYPE type );
        static FILTER_TYPE oalRepresentationToFilterType( const QString &type );

        static QString filterColorScreenRepresentation( const FILTER_COLOR color );
        static QString filterColorOALRepresentation ( const FILTER_COLOR color );
        static FILTER_COLOR oalRepresentationToFilterColor( const QString &color );

        Filter( const QString& id, const QString& model, const QString& vendor, const FILTER_TYPE type, const FILTER_COLOR color ) { setFilter( id, model, vendor, type, color ); }
        QString id() const { return m_Id; }
        QString name() const { return m_Name; }
        QString model() const { return m_Model; }
        QString vendor() const { return m_Vendor; }
        FILTER_TYPE type() const { return m_Type; }
        QString typeScreenRepresentation() const;
        QString typeOALRepresentation() const;
        FILTER_COLOR color() const { return m_Color; }
        QString colorScreenRepresentation() const;
        QString colorOALRepresentation() const;
        void setFilter( const QString& _id, const QString &_model, const QString& _vendor, const FILTER_TYPE _type, const FILTER_COLOR _color );
    private:
        static const QMap<FILTER_TYPE, QString> typeScreenMapping;
        static const QMap<FILTER_TYPE, QString> typeOALMapping;
        static const QMap<FILTER_COLOR, QString> colorScreenMapping;
        static const QMap<FILTER_COLOR, QString> colorOALMapping;

        QString m_Id, m_Model, m_Vendor, m_Name;
        FILTER_TYPE m_Type;
        FILTER_COLOR m_Color;
};

#endif

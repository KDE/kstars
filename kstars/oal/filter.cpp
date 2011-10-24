/***************************************************************************
                          filter.cpp  -  description

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

#include "filter.h"
#include "QMap"

using namespace OAL;

namespace {
    QMap<Filter::FILTER_TYPE, QString> getFTScreenMappings()
    {
        QMap<Filter::FILTER_TYPE, QString> retVal;

        retVal.insert( Filter::FT_OTHER, i18n( "Other" ) );
        retVal.insert( Filter::FT_BROAD_BAND, i18n( "Broad band" ) );
        retVal.insert( Filter::FT_NARROW_BAND, i18n( "Narrow band" ) );
        retVal.insert( Filter::FT_O_III, i18n( "O-III" ) );
        retVal.insert( Filter::FT_H_BETA, i18n( "H-Beta" ) );
        retVal.insert( Filter::FT_H_ALPHA, i18n( "H-Alpha" ) );
        retVal.insert( Filter::FT_COLOR, i18n( "Color" ) );
        retVal.insert( Filter::FT_NEUTRAL, i18n( "Neutral" ) );
        retVal.insert( Filter::FT_CORRECTIVE, i18n( "Corrective" ) );
        retVal.insert( Filter::FT_SOLAR, i18n( "Solar" ) );

        return retVal;
    }

    QMap<Filter::FILTER_TYPE, QString> getFTOALMappings()
    {
        QMap<Filter::FILTER_TYPE, QString> retVal;

        retVal.insert( Filter::FT_OTHER, "other" );
        retVal.insert( Filter::FT_BROAD_BAND, "broad band" );
        retVal.insert( Filter::FT_NARROW_BAND, "narrow band" );
        retVal.insert( Filter::FT_O_III, "O-III");
        retVal.insert( Filter::FT_H_BETA, "H-beta" );
        retVal.insert( Filter::FT_H_ALPHA, "H-alpha" );
        retVal.insert( Filter::FT_COLOR, "color" );
        retVal.insert( Filter::FT_NEUTRAL, "neutral" );
        retVal.insert( Filter::FT_CORRECTIVE, "corrective" );
        retVal.insert( Filter::FT_SOLAR, "solar" );

        return retVal;
    }

    QMap<Filter::FILTER_COLOR, QString> getFCScreenMappings()
    {
        QMap<Filter::FILTER_COLOR, QString> retVal;

        retVal.insert( Filter::FC_NONE, i18n( "None" ) );
        retVal.insert( Filter::FC_LIGHT_RED, i18n( "Light red" ) );
        retVal.insert( Filter::FC_RED, i18n( "Red" ) );
        retVal.insert( Filter::FC_DEEP_RED, i18n( "Deep red" ) );
        retVal.insert( Filter::FC_ORANGE, i18n( "Orange" ) );
        retVal.insert( Filter::FC_LIGHT_YELLOW, i18n( "Light yellow" ) );
        retVal.insert( Filter::FC_DEEP_YELLOW, i18n( "Deep yellow" ) );
        retVal.insert( Filter::FC_YELLOW, i18n( "Yellow" ) );
        retVal.insert( Filter::FC_YELLOW_GREEN, i18n( "Yellow-green" ) );
        retVal.insert( Filter::FC_LIGHT_GREEN, i18n( "Light green" ) );
        retVal.insert( Filter::FC_GREEN, i18n( "Green" ) );
        retVal.insert( Filter::FC_MEDIUM_BLUE, i18n( "Medium blue" ) );
        retVal.insert( Filter::FC_PALE_BLUE, i18n( "Pale blue" ) );
        retVal.insert( Filter::FC_BLUE, i18n( "Blue" ) );
        retVal.insert( Filter::FC_DEEP_BLUE, i18n( "Deep blue" ) );
        retVal.insert( Filter::FC_VIOLET, i18n( "Violet" ) );

        return retVal;
    }

    QMap<Filter::FILTER_COLOR, QString> getFCOALMAppings()
    {
        QMap<Filter::FILTER_COLOR, QString> retVal;

        retVal.insert( Filter::FC_NONE, QString() );
        retVal.insert( Filter::FC_LIGHT_RED, "light red" );
        retVal.insert( Filter::FC_RED, "red" );
        retVal.insert( Filter::FC_DEEP_RED, "deep red" );
        retVal.insert( Filter::FC_ORANGE, "orange" );
        retVal.insert( Filter::FC_LIGHT_YELLOW, "light yellow" );
        retVal.insert( Filter::FC_DEEP_YELLOW, "deep yellow" );
        retVal.insert( Filter::FC_YELLOW, "yellow" );
        retVal.insert( Filter::FC_YELLOW_GREEN, "yellow-green" );
        retVal.insert( Filter::FC_LIGHT_GREEN, "light green" );
        retVal.insert( Filter::FC_GREEN, "green" );
        retVal.insert( Filter::FC_MEDIUM_BLUE, "medium blue" );
        retVal.insert( Filter::FC_PALE_BLUE, "pale blue" );
        retVal.insert( Filter::FC_BLUE, "blue" );
        retVal.insert( Filter::FC_DEEP_BLUE, "deep blue" );
        retVal.insert( Filter::FC_VIOLET, "violet" );

        return retVal;
    }
}

const QMap<Filter::FILTER_TYPE, QString> Filter::typeScreenMapping = getFTScreenMappings();
const QMap<Filter::FILTER_TYPE, QString> Filter::typeOALMapping = getFTOALMappings();
const QMap<Filter::FILTER_COLOR, QString> Filter::colorScreenMapping = getFCScreenMappings();
const QMap<Filter::FILTER_COLOR, QString> Filter::colorOALMapping = getFCOALMAppings();

QString Filter::name() const
{
    return m_Vendor + ' ' + m_Model + " - " + typeScreenRepresentation() + ' ' +  + " (" + m_Id + ')';
}

QString Filter::typeScreenRepresentation() const
{
    return filterTypeScreenRepresentation(m_Type);
}

QString Filter::typeOALRepresentation() const
{
    return filterTypeOALRepresentation(m_Type);
}

QString Filter::colorScreenRepresentation() const
{
    return filterColorScreenRepresentation(m_Color);
}

QString Filter::colorOALRepresentation() const
{
    return filterColorOALRepresentation(m_Color);
}

void Filter::setFilter(const QString &id, const QString &model, const QString &vendor, const FILTER_TYPE type, const FILTER_COLOR color)
{
    m_Id = id;
    m_Model = model;
    m_Vendor = vendor;
    m_Type = type;
    m_Color = color;
}

QStringList Filter::filterTypes()
{
    return typeScreenMapping.values();
}

QStringList Filter::filterColors()
{
    return colorScreenMapping.values();
}

QString Filter::filterTypeScreenRepresentation(const FILTER_TYPE type)
{
    return typeScreenMapping.value(type);
}

QString Filter::filterTypeOALRepresentation(const FILTER_TYPE type)
{
    return typeOALMapping.value(type);
}

Filter::FILTER_TYPE Filter::oalRepresentationToFilterType(const QString &type)
{
    return typeOALMapping.key(type);
}

QString Filter::filterColorScreenRepresentation(const FILTER_COLOR color)
{
    return colorScreenMapping.value(color);
}

QString Filter::filterColorOALRepresentation(const FILTER_COLOR color)
{
    return colorOALMapping.value(color);
}

Filter::FILTER_COLOR Filter::oalRepresentationToFilterColor(const QString &color)
{
    return colorOALMapping.key(color, FC_NONE);
}

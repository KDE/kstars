/***************************************************************************
                  nameresolver.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sat 23 Aug 2014 23:38:42 CDT
    copyright            : (c) 2014 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/



#ifndef NAMERESOLVER_H
#define NAMERESOLVER_H

/* #include "../../datahandlers/catalogentrydata.h" */
#include "skyobject.h"

// Forward declarations
class QString;
class CatalogEntryData;

/**
 * @namespace NameResolver
 *
 * @short Uses name resolver services to get coordinates and
 * information about deep-sky objects, given their names
 *
 * This class uses the CDS Sesame name resolver (and potentially other
 * resolvers) to resolve the names of deep-sky objects, and obtain
 * coordinates, fluxes, alternate designations, and possibly other
 * data.
 *
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 */

namespace NameResolver {

    /**
     * @short Resolve the name of the given DSO and extract data from
     * various sources
     */
    class CatalogEntryData resolveName( const QString &name );

    namespace NameResolverInternals {

        /**
         * @short Resolve the name and obtain basic data using CDS Sesame service
         *
         * @param data the structure to fill with the obtained data
         * @param name the name (identifier) to resolve
         * @return Success value
         */
        bool sesameResolver( class CatalogEntryData &data, const QString &name );

        /**
         * @short Retrieve additional data from SIMBAD
         *
         * If the object was resolved by SIMBAD, obtain additional data
         * that is not available through CDS Sesame, such as the major and
         * minor axes and position angle of the object.
         *
         * @param data the structure containing data from Sesame, to be
         * filled with additional data
         * @return Success value
         */
        //        bool getDataFromSimbad( class CatalogEntryData &data );

        /**
         * @short Interprets object type returned by Sesame
         *
         * This method reads the object type string returned by Sesame
         * / other resolvers and attempts to convert it into a
         * SkyObject type.
         *
         * @return a SkyObject::TYPE.
         */
        SkyObject::TYPE interpretObjectType( const QString &typeString );

    }

};

#endif




/*
    SPDX-FileCopyrightText: 2014 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef NAMERESOLVER_H
#define NAMERESOLVER_H

#include "skyobject.h"

// Forward declarations
class QString;
class CatalogObject;

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

namespace NameResolver
{
/**
 * @short Resolve the name of the given DSO and extract data from
 * various sources
 */
std::pair<bool, CatalogObject> resolveName(const QString &name);

namespace NameResolverInternals
{
/**
 * @short Resolve the name and obtain basic data using CDS Sesame service
 *
 * @param data the structure to fill with the obtained data
 * @param name the name (identifier) to resolve
 * @return Success value and the object
 */
std::pair<bool, CatalogObject> sesameResolver(const QString &name);

/*
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
//        bool getDataFromSimbad( CatalogObject &data );

/**
 * @short Interprets object type returned by Sesame
 *
 * This method reads the object type string returned by Sesame
 * / other resolvers and attempts to convert it into a
 * SkyObject type.
 *
 * @param typeString An abbreviated string containing the type of the object
 * @param caseSensitive Should we make a case-sensitive check?
 *
 * @return a SkyObject::TYPE.
 */
SkyObject::TYPE interpretObjectType(const QString &typeString, bool caseSensitive = true);
} // namespace NameResolverInternals
}; // namespace NameResolver

#endif

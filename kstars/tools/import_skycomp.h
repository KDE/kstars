/***************************************************************************
                  import_skycomp.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2021-08-03
    copyright            : (C) 2021 by Valentin Boettcher
    email                : hiro at protagon.space; @hiro98:tchncs.de
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * Some glue code to import `_Internet_Resolved` and
 * `_Manual_Additions` into the new DSO database.
 */

#pragma once

#include <utility>
#include <QString>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QFileInfo>
#include "kspaths.h"
#include "catalogsdb.h"

namespace SkyComponentsImport
{
/**
 * Get the skycomponent database from the specified path.
 *
 * \returns wether the database could be found and read and the
 * database itself.
 */
std::pair<bool, QSqlDatabase> get_skycomp_db(const QString &path);

/**
 * Get the skycomponent database from the standard path.
 *
 * \returns wether the database could be found and read and the
 * database itself.
 */
std::pair<bool, QSqlDatabase> get_skycomp_db();

/**
 * \returns wether the operation succeeded, an error message and if it
 * was successful the objects from the catalogs with \p ids.
 */
std::tuple<bool, QString, CatalogsDB::CatalogObjectVector>
get_objects(QSqlDatabase db, const std::list<int> &ids = { 1, 2 });

} // namespace SkyComponentsImport

/***************************************************************************
                  qstring_hash.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2021-06-03
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

#pragma once
#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
#include <QHash>
#include <QString>
#include <functional>

namespace std
{
template <>
struct hash<QString>
{
    std::size_t operator()(const QString &s) const noexcept { return (size_t)qHash(s); }
};
} // namespace std
#endif

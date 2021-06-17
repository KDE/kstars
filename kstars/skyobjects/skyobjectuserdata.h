/***************************************************************************
                  skyobjectuserdata.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 20 2008
    copyright            : (C) 2008 by Akarsh Simha
    email                : akarshsimha@gmail.com
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

#include <QString>
#include <QUrl>
#include <map>
#include <vector>

namespace SkyObjectUserdata
{
/**
 * Stores the tite and URL of a webpage.
 *
 * @note As we still have no std::variant support we use an enum.
 */
struct LinkData
{
    enum class Type
    {
        website = 0,
        image   = 1
    };

    QString title;
    QUrl url;
    Type type;
};

using Type     = LinkData::Type;
using LinkList = std::vector<LinkData>;

/**
 * Stores Users' Logs, Pictures and Websites regarding an object in
 * the sky.
 *
 * @short Auxiliary information associated with a SkyObject.
 * @author Akarsh Simha, Valentin Boettcher
 * @version 2.0
 */
struct Data
{
    std::map<LinkData::Type, LinkList> links{ { Type::website, {} },
                                              { Type::image, {} } };
    QString userLog;

    inline LinkList &images() { return links.at(Type::image); };
    inline const LinkList &images() const { return links.at(Type::image); };

    inline LinkList &websites() { return links.at(Type::website); };
    inline const LinkList &websites() const { return links.at(Type::website); };

    auto findLinkByTitle(const QString &title, const Type type) const
    {
        return std::find_if(cbegin(links.at(type)), cend(links.at(type)),
                            [&title](const auto &entry) { return entry.title == title; });
    };

    auto findLinkByTitle(const QString &title, const Type type)
    {
        return std::find_if(begin(links.at(type)), end(links.at(type)),
                            [&title](const auto &entry) { return entry.title == title; });
    };

    void addLink(QString title, QUrl url, Type type)
    {
        links[type].push_back(LinkData{ std::move(title), std::move(url), type });
    }
};
} // namespace SkyObjectUserdata

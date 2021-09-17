/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

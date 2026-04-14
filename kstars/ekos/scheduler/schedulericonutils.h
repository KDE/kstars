#pragma once

#include <QIcon>
#include <QString>

#include <initializer_list>

namespace Ekos
{

// Prefer freedesktop/KDE-standard names first and keep older names only as compatibility fallbacks.
inline QIcon firstThemedIcon(std::initializer_list<const char *> names)
{
    for (const char *name : names)
    {
        const QIcon icon = QIcon::fromTheme(QString::fromLatin1(name));
        if (!icon.isNull())
            return icon;
    }

    return {};
}

}

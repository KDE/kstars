#ifndef QTKEEPEMPTYPARTS_H
#define QTKEEPEMPTYPARTS_H

#include <QString>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt
{
    static auto KeepEmptyParts = QString::KeepEmptyParts;
}
#endif

#endif // QTKEEPEMPTYPARTS_H

#ifndef QTSKIPEMPTYPARTS_H
#define QTSKIPEMPTYPARTS_H

#include <QString>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt
{
    static auto SkipEmptyParts = QString::SkipEmptyParts;
}
#endif

#endif // QTSKIPEMPTYPARTS_H

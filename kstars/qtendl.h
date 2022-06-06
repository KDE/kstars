#ifndef QTENDL_H
#define QTENDL_H

#include <QTextStream>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt
{
    static auto endl = ::endl;
}
#endif

#endif // QTENDL_H

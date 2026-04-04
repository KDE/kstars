#ifndef QTCOMPAT_H
#define QTCOMPAT_H

#include <QtGlobal>
#include <QMouseEvent>
#include <QDropEvent>
#include <QCheckBox>
#include <QImage>

namespace QtCompat
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
inline QPointF mousePos(QMouseEvent *e)
{
    return e->position();
}
inline QPointF mouseGlobalPos(QMouseEvent *e)
{
    return e->globalPosition();
}
inline int mouseX(QMouseEvent *e)
{
    return qRound(e->position().x());
}
inline int mouseY(QMouseEvent *e)
{
    return qRound(e->position().y());
}
inline int mouseGlobalX(QMouseEvent *e)
{
    return qRound(e->globalPosition().x());
}
inline int mouseGlobalY(QMouseEvent *e)
{
    return qRound(e->globalPosition().y());
}

// QCheckBox signal
// Use checkStateChanged(Qt::CheckState) instead of stateChanged(int)

inline QPoint dropPos(QDropEvent *e)
{
    return e->position().toPoint();
}

// QImage mirrored
inline QImage mirrored(const QImage &img, bool horizontal = false, bool vertical = true)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    return img.mirrored(horizontal, vertical);
#else
    Qt::Orientations orient;
    if (horizontal) orient |= Qt::Horizontal;
    if (vertical) orient |= Qt::Vertical;
    return img.flipped(orient);
#endif
}
#else
inline QPointF mousePos(QMouseEvent *e)
{
    return e->localPos();
}
inline QPointF mouseGlobalPos(QMouseEvent *e)
{
    return e->globalPos();
}
inline int mouseX(QMouseEvent *e)
{
    return e->x();
}
inline int mouseY(QMouseEvent *e)
{
    return e->y();
}
inline int mouseGlobalX(QMouseEvent *e)
{
    return e->globalX();
}
inline int mouseGlobalY(QMouseEvent *e)
{
    return e->globalY();
}

inline QPoint dropPos(QDropEvent *e)
{
    return e->pos();
}

// QImage mirrored
inline QImage mirrored(const QImage &img, bool horizontal = false, bool vertical = true)
{
    return img.mirrored(horizontal, vertical);
}
#endif
}

#endif // QTCOMPAT_H

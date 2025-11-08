/*  Text item that flashes up in a big bold font
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>, 2025

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "flashingtextitem.h"
#include <QFont>

FlashingTextItem::FlashingTextItem(QCustomPlot *parentPlot)
    : QCPItemText(parentPlot)
{
    m_timer.setSingleShot(true);
    m_holdTimer.setSingleShot(true);
    QObject::connect(&m_timer, &QTimer::timeout, [this, parentPlot]
    {
        applyNormalFont();
        // Use the QCPAbstractItem accessor, not the ctor parameter:
        if (parentPlot)
            parentPlot->replot(QCustomPlot::rpQueuedReplot);
    });
}

void FlashingTextItem::setText(const QString &t, int holdMs, int shrinkMs)
{
    // Base (small) item invisible
    QCPItemText::setText(t);
    applyNormalFont();
    setVisible(false);

    // Ensure overlay exists and mirrors base positioning
    ensureOverlayCreated();
    m_overlay->setText(t);
    m_overlay->position->setType(position->type());
    m_overlay->position->setCoords(position->coords());
    m_overlay->setPositionAlignment(positionAlignment());
    m_overlay->setRotation(rotation());

    // Big font + opaque color
    QFont f = font();
    f.setBold(m_bigBold);
    f.setPixelSize(m_bigPx);
    m_overlay->setFont(f);
    QColor c = color();
    c.setAlpha(255);
    m_overlay->setColor(c);
    m_overlay->setVisible(true);
    if (parentPlot())
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);

    // After hold, start the shrink animation
    m_holdTimer.stop();
    QObject::disconnect(&m_holdTimer, nullptr, nullptr, nullptr);
    QObject::connect(&m_holdTimer, &QTimer::timeout, [this, shrinkMs]
    {
        startShrinkAnimation(shrinkMs);
    });
    m_holdTimer.start(qMax(0, holdMs));
}

void FlashingTextItem::startShrinkAnimation(int shrinkMs)
{
    if (!m_overlay || !parentPlot()) return;

    m_flashAnim.stop();
    m_flashAnim.setStartValue(1.0);
    m_flashAnim.setEndValue(0.0);
    m_flashAnim.setDuration(qMax(1, shrinkMs));
    m_flashAnim.setEasingCurve(QEasingCurve::OutCubic);
    QObject::disconnect(&m_flashAnim, nullptr, nullptr, nullptr);
    QObject::connect(&m_flashAnim, &QVariantAnimation::valueChanged, [this](const QVariant & v)
    {
        // shrink the font
        QFont f = m_overlay->font();
        f.setPixelSize(m_normalPx + (m_bigPx - m_normalPx) * v.toInt());
        m_overlay->setFont(f);
        // increase the transparency
        QColor c = m_overlay->color();
        c.setAlphaF(qBound(0.0, v.toDouble(), 1.0));
        m_overlay->setColor(c);
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    });
    QObject::connect(&m_flashAnim, &QVariantAnimation::finished, [this]
    {
        // make the base text visible
        setVisible(true);
        // make the overlay invisible
        if (m_overlay)
            m_overlay->setVisible(false);
        // replot
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    });

    m_flashAnim.start();
}

void FlashingTextItem::ensureOverlayCreated()
{
    if (m_overlay)
        return;

    m_overlay = new QCPItemText(parentPlot());
    m_overlay->position->setParentAnchor(position);
    m_overlay->setBrush(brush());
    m_overlay->setPen(pen());
    m_overlay->setColor(color());
    m_overlay->setVisible(false);
}

void FlashingTextItem::updateFontSizesFromPlot(QCustomPlot *plot)
{
    if (!m_useAutoScale || !plot)
        return;

    const int base = qMax(1, qMin(plot->width(), plot->height()));
    m_bigPx    = qMax(18, int(base * m_bigRatio));
    m_normalPx = qMax(10, int(base * m_normalRatio));

    // keep base item small (normal) by design
    applyNormalFont();
    // if overlay is up and not animating, keep it big during hold
    if (m_overlay && m_overlay->visible() && m_flashAnim.state() != QAbstractAnimation::Running)
    {
        QFont f = m_overlay->font();
        f.setPixelSize(m_bigPx);
        m_overlay->setFont(f);
    }
}

void FlashingTextItem::applyBigFont()
{
    QFont f = font();
    f.setBold(m_bigBold);
    f.setPixelSize(m_bigPx);
    setFont(f);
}

void FlashingTextItem::applyNormalFont()
{
    QFont f = font();
    f.setBold(m_normalBold);
    f.setPixelSize(m_normalPx);
    setFont(f);
}

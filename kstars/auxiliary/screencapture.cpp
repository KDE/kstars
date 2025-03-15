/*
    SPDX-FileCopyrightText: 2025 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screencapture.h"
#include <QGuiApplication>
#include <QThread>

ScreenCapture::ScreenCapture(QWidget *parent) : QWidget(parent), rubberBand(nullptr)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setGeometry(QGuiApplication::primaryScreen()->geometry());
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setCursor(Qt::CrossCursor);
}

ScreenCapture::~ScreenCapture()
{
    unsetCursor();
}

void ScreenCapture::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    grabKeyboard();
}

void ScreenCapture::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    releaseKeyboard();
}
void ScreenCapture::mousePressEvent(QMouseEvent *event)
{
    origin = event->pos();
    if (!rubberBand)
    {
        rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        rubberBand->setStyleSheet("border: 2px solid red;");
    }
    rubberBand->setGeometry(QRect(origin, QSize()));
    rubberBand->show();
}

void ScreenCapture::mouseMoveEvent(QMouseEvent *event)
{
    if (rubberBand)
    {
        rubberBand->setGeometry(QRect(origin, event->pos()).normalized());
    }
}

void ScreenCapture::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (rubberBand)
    {
        QRect selectedRect = rubberBand->geometry();
        rubberBand->hide();
        close();
        QRect rect(selectedRect.left() + geometry().left(),
                   selectedRect.top() + geometry().top(),
                   selectedRect.width(), selectedRect.height());
        hide();
        QCoreApplication::processEvents();
        // Instead of this, I can create an event filter for the CaptureWidget,
        // and look for the Paint event, emit a signal, and only grabWindow after that.
        // bool eventFilter(QObject *watched, QEvent *event) override {
        //     if (event->type() == QEvent::Paint) {
        //         emit paintCompleted(); // Signal emitted when repainting is done
        //     }
        //    return QWidget::eventFilter(watched, event);
        // }
        QThread::msleep(100);
        QScreen *screen = QGuiApplication::primaryScreen();
        if (!screen)
            emit aborted();
        else
        {
            QPixmap screenshot = screen->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height());
            emit areaSelected(screenshot.toImage());
        }
    }
}

void ScreenCapture::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        unsetCursor();
        close();
        emit aborted();
    }
    else
    {
        QWidget::keyPressEvent(event); // Pass other keys to base class
    }
}

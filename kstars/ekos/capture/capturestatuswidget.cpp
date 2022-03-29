/*  Progress and status of capture preparation and execution
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturestatuswidget.h"

namespace Ekos
{
CaptureStatusWidget::CaptureStatusWidget(QWidget * parent) : QWidget(parent)
{
    setupUi(this);

    statusLed = new KLed(Qt::gray, KLed::On, KLed::Flat, KLed::Circular, this);
    statusLed->setObjectName("statusLed");
    statusLayout->insertWidget(-1, statusLed, 0, Qt::AlignVCenter);

    statusText->setText(i18n("Idle"));
}

void CaptureStatusWidget::setCaptureState(CaptureState status)
{
    switch (status)
    {
    case CAPTURE_IDLE:
        setStatus(i18n("Idle"), Qt::gray);
        break;
    case CAPTURE_PAUSED:
        setStatus(i18n("Paused"), Qt::gray);
        break;
    case CAPTURE_SUSPENDED:
        setStatus(i18n("Suspended"), Qt::gray);
        break;
    case CAPTURE_COMPLETE:
        setStatus(i18n("Completed"), Qt::gray);
        break;

    case CAPTURE_PROGRESS:
        setStatus(i18n("Preparing..."), Qt::yellow);
        break;
    case CAPTURE_WAITING:
        setStatus(i18n("Waiting..."), Qt::yellow);
        break;
    case CAPTURE_DITHERING:
        setStatus(i18n("Dithering..."), Qt::yellow);
        break;
    case CAPTURE_FOCUSING:
        setStatus(i18n("Focusing..."), Qt::yellow);
        break;
    case CAPTURE_FILTER_FOCUS:
        setStatus(i18n("Filter change..."), Qt::yellow);
        break;
    case CAPTURE_CHANGING_FILTER:
        setStatus(i18n("Filter change..."), Qt::yellow);
        break;
    case CAPTURE_ALIGNING:
        setStatus(i18n("Aligning..."), Qt::yellow);
        break;
    case CAPTURE_CALIBRATING:
        setStatus(i18n("Calibrating..."), Qt::yellow);
        break;
    case CAPTURE_MERIDIAN_FLIP:
        setStatus(i18n("Meridian flip..."), Qt::yellow);
        break;

    case CAPTURE_CAPTURING:
        setStatus(i18n("Capturing"), Qt::darkGreen);
        break;
    case CAPTURE_IMAGE_RECEIVED:
        setStatus(i18n("Image received."), Qt::darkGreen);
        break;
    case CAPTURE_PAUSE_PLANNED:
        setStatus(i18n("Pause planned..."), Qt::darkGreen);
        break;

    case CAPTURE_ABORTED:
        setStatus(i18n("Aborted"), Qt::darkRed);
        break;

    case CAPTURE_GUIDER_DRIFT:
    case CAPTURE_SETTING_TEMPERATURE:
    case CAPTURE_SETTING_ROTATOR:
        // do nothing here, set from {@see Capture::updatePrepareState()}
        break;
    }
}

void CaptureStatusWidget::setFilterState(FilterState status)
{
    switch (status)
    {
    case FILTER_CHANGE:
    case FILTER_OFFSET:
    case FILTER_AUTOFOCUS:
        setStatus(Ekos::getFilterStatusString(status), Qt::yellow);
        break;
    case FILTER_IDLE:
        if (lastFilterState == FILTER_CHANGE)
            setStatus(i18n("Filter selected."), Qt::darkGreen);
        break;
    default:
        // do nothing
        break;
    }
    // remember the current status
    lastFilterState = status;
}

void CaptureStatusWidget::setStatus(QString text, Qt::GlobalColor color)
{
    statusText->setText(text);
    statusLed->setColor(color);
}

}

/*
    SPDX-FileCopyrightText: 2003-2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2016-2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitslabel.h"

#include "config-kstars.h"

#include "fitsdata.h"
#include "fitsview.h"
#include "kspopupmenu.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"
#include "ksnotification.h"
#include <QGuiApplication>

#ifdef HAVE_INDI
#include "basedevice.h"
#include "indi/indilistener.h"
#include "indi/indiconcretedevice.h"
#include "indi/indimount.h"
#endif

#include <QScrollBar>
#include <QToolTip>

#define BASE_OFFSET    50
#define ZOOM_DEFAULT   100.0
#define ZOOM_MIN       10
#define ZOOM_MAX       400
#define ZOOM_LOW_INCR  10
#define ZOOM_HIGH_INCR 50

FITSLabel::FITSLabel(FITSView *View, QWidget *parent) : QLabel(parent)
{
    this->view = View;
    prevscale = 0.0;
    //Rubber Band options
    roiRB = new QRubberBand( QRubberBand::Rectangle, this);
    roiRB->setAttribute(Qt::WA_TransparentForMouseEvents, 1);
    roiRB->setGeometry(QRect(QPoint(1, 1), QPoint(100, 100)));

    QPalette pal;
    QColor red70 = Qt::red;
    red70.setAlphaF(0.7);

    pal.setBrush(QPalette::Highlight, QBrush(red70));
    roiRB->setPalette(pal);
    // QToolTip::showText(QPoint(1, 1), "Move Once to show selection stats", this); // Replaced by persistent label

    m_roiStatsLabel = new QLabel(this);
    m_roiStatsLabel->setObjectName("roiStatsLabel");
    m_roiStatsLabel->hide();
    m_roiStatsLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    // Basic styling, can be adjusted. Make sure it's visible against typical FITS images.
    m_roiStatsLabel->setStyleSheet("QLabel#roiStatsLabel { background-color: rgba(0, 0, 0, 220); padding: 4px; border: 1px solid white; border-radius: 3px; font-weight: bold; }");

    QPalette statsPalette = m_roiStatsLabel->palette();
    statsPalette.setColor(QPalette::WindowText, Qt::white);
    m_roiStatsLabel->setPalette(statsPalette);

    m_roiStatsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop); // Align text to top-left within the label

    // Connect to FITSView's updated signal
    if (view)
    {
        connect(view, &FITSView::updated, this, &FITSLabel::handleImageDataUpdated);
        // connect(view, &FITSView::rectangleUpdated, this, &FITSLabel::updatePersistentRoiLabel); // This might cause issues if rect is raw
    }
}


void FITSLabel::setSize(double w, double h)
{
    m_Width  = w;
    m_Height = h;
    m_Size   = w * h;
}

bool FITSLabel::getMouseButtonDown()
{
    return mouseButtonDown;
}

/**
This method was added to make the panning function work.
If the mouse button is released, it resets mouseButtonDown variable and the mouse cursor.
 */
void FITSLabel::mouseReleaseEvent(QMouseEvent *e)
{
    float scale = (view->getCurrentZoom() / ZOOM_DEFAULT);

    double x = round(e->x() / scale);
    double y = round(e->y() / scale);

    m_p2.setX(x);//record second point for selection Rectangle
    m_p2.setY(y);

    if (view->getCursorMode() == FITSView::dragCursor)
    {
        mouseButtonDown = false;
        view->updateMouseCursor();
        if( isRoiSelected && view->isSelectionRectShown())
        {
            QRect roiRaw = roiRB->geometry();
            emit rectangleSelected(roiRaw.topLeft() / prevscale, roiRaw.bottomRight() / prevscale, true);
            // updateROIToolTip(e->globalPos());
            updatePersistentRoiLabel(roiRB->geometry());
        }
        if( e->modifiers () == Qt::ShiftModifier && view->isSelectionRectShown())
        {
            QRect roiRaw = roiRB->geometry();
            emit rectangleSelected(roiRaw.topLeft() / prevscale, roiRaw.bottomRight() / prevscale, true);
            // updateROIToolTip(e->globalPos());
            updatePersistentRoiLabel(roiRB->geometry());
        }
        isRoiSelected = false;
        // Only process the circle if relevant to Catalog Objects and a query is not already in progress
        if (view->showObjects && !view->m_ImageData->getCatQueryInProgress() && e->modifiers () == Qt::ShiftModifier)
            emit circleSelected(m_p1, m_p2);
    }

}

void FITSLabel::leaveEvent(QEvent *e)
{
    Q_UNUSED(e)
    view->updateMagnifyingGlass(-1, -1);
    emit mouseOverPixel(-1, -1);
}

/**
I added some things to the top of this method to allow panning and Scope slewing to function.
If you are in the dragMouse mode and the mousebutton is pressed, The method checks the difference
between the location of the last point stored and the current event point to see how the mouse has moved.
Then it moves the scrollbars and thus the view to the right location.
Then it stores the current point so next time it can do it again.
 */
void FITSLabel::mouseMoveEvent(QMouseEvent *e)
{
    const QSharedPointer<FITSData> &imageData = view->imageData();
    if (imageData.isNull())
        return;

    float scale = (view->getCurrentZoom() / ZOOM_DEFAULT);

    double x = round(e->x() / scale);
    double y = round(e->y() / scale);

    //Panning
    if (e->modifiers() != Qt::ShiftModifier  && view->getCursorMode() == FITSView::dragCursor && mouseButtonDown  )
    {
        QPoint newPoint = e->globalPos();
        int dx          = newPoint.x() - lastMousePoint.x();
        int dy          = newPoint.y() - lastMousePoint.y();
        view->horizontalScrollBar()->setValue(view->horizontalScrollBar()->value() - dx);
        view->verticalScrollBar()->setValue(view->verticalScrollBar()->value() - dy);

        lastMousePoint = newPoint;
    }
    if( e->buttons() & Qt::LeftButton && view->getCursorMode() == FITSView::dragCursor )
    {
        //Translation of ROI
        if(isRoiSelected && !mouseButtonDown)
        {

            int xdiff = x - prevPoint.x();
            int ydiff = y - prevPoint.y();
            roiRB->setGeometry(roiRB->geometry().translated(round(xdiff * scale), round(ydiff * scale)));
            prevPoint = QPoint(x, y);

            QRect roiRaw = roiRB->geometry();
            // Opting to update stats on the go is extremely laggy for large images, only update if small image
            if(!view->isLargeImage())
            {
                emit rectangleSelected(roiRaw.topLeft() / prevscale, roiRaw.bottomRight() / prevscale, true);
                updatePersistentRoiLabel(roiRB->geometry());
            }
        }
        //Stretching of ROI
        if(e->modifiers() == Qt::ShiftModifier && !isRoiSelected && view->isSelectionRectShown())
        {
            roiRB->setGeometry(QRect(m_p1 * scale, QPoint(x, y)*scale).normalized());

            QRect roiRaw = roiRB->geometry();
            // Opting to update stats on the go is extremely laggy for large images, only update if small image
            if(!view->isLargeImage())
            {
                emit rectangleSelected(roiRaw.topLeft() / prevscale, roiRaw.bottomRight() / prevscale, true);
                updatePersistentRoiLabel(roiRB->geometry());
            }
        }
    }

    uint8_t const *buffer = imageData->getImageBuffer();

    if (buffer == nullptr)
        return;

    x = round(e->x() / scale);
    y = round(e->y() / scale);

    if(QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier))
        view->updateMagnifyingGlass(x, y);
    else
        view->updateMagnifyingGlass(-1, -1);

    x = KSUtils::clamp(x, 1.0, m_Width);
    y = KSUtils::clamp(y, 1.0, m_Height);

    emit newStatus(QString("X:%1 Y:%2").arg(static_cast<int>(x)).arg(static_cast<int>(y)), FITS_POSITION);

    // Range is 0 to dim-1 when accessing array
    x -= 1;
    y -= 1;

    emit mouseOverPixel(x, y);

    int index = y * m_Width + x;
    QString stringValue;

    switch (imageData->getStatistics().dataType)
    {
        case TBYTE:
            stringValue = QLocale().toString(buffer[index]);
            break;

        case TSHORT:
            stringValue = QLocale().toString((reinterpret_cast<int16_t const*>(buffer))[index]);
            break;

        case TUSHORT:
            stringValue = QLocale().toString((reinterpret_cast<uint16_t const*>(buffer))[index]);
            break;

        case TLONG:
            stringValue = QLocale().toString((reinterpret_cast<int32_t const*>(buffer))[index]);
            break;

        case TULONG:
            stringValue = QLocale().toString((reinterpret_cast<uint32_t const*>(buffer))[index]);
            break;

        case TFLOAT:
            stringValue = QLocale().toString((reinterpret_cast<float const*>(buffer))[index], 'f', 5);
            break;

        case TLONGLONG:
            stringValue = QLocale().toString(static_cast<int>((reinterpret_cast<int64_t const*>(buffer))[index]));
            break;

        case TDOUBLE:
            stringValue = QLocale().toString((reinterpret_cast<float const*>(buffer))[index], 'f', 5);

            break;

        default:
            break;

    }

    if(view->isSelectionRectShown())
    {
        if (roiRB->geometry().contains(e->pos()))
            updatePersistentRoiLabel(roiRB->geometry());
    }

    emit newStatus(stringValue, FITS_VALUE);

    if (imageData->hasWCS() &&
            !view->isSelectionRectShown() &&
            view->getCursorMode() != FITSView::selectCursor)
    {
        QPointF wcsPixelPoint(x, y);
        SkyPoint wcsCoord;
        if(imageData->pixelToWCS(wcsPixelPoint, wcsCoord))
        {
            m_RA = wcsCoord.ra0();
            m_DE = wcsCoord.dec0();
            emit newStatus(QString("%1 , %2").arg(m_RA.toHMSString(), m_DE.toDMSString()), FITS_WCS);
        }

        bool objFound = false;
        for (auto &listObject : imageData->getSkyObjects())
        {
            if ((std::abs(listObject->x() - x) < 5 / scale) && (std::abs(listObject->y() - y) < 5 / scale))
            {
                QToolTip::showText(e->globalPos(),
                                   QToolTip::text() + '\n' + listObject->skyObject()->name() + '\n' + listObject->skyObject()->longname(), this);
                objFound = true;
                break;
            }
        }
        if (!objFound && !view->isSelectionRectShown())
            QToolTip::hideText();
    }

    double HFR = view->imageData()->getHFR(x + 1, y + 1, scale);


    if (HFR > 0)
    {
        QString tip = QToolTip::text();
        // Don't i18n away HFR: because the RegExp below checks for HFR: to make sure there aren't duplicate strings added.
        QString hfrStr = QString("HFR: %1").arg(HFR, 4, 'f', 2);
        if (tip.isEmpty() || tip == hfrStr)
            QToolTip::showText(e->globalPos(), hfrStr, this);
        else
        {
            QRegularExpression hfrRegEx("HFR\\: \\d+\\.\\d\\d");
            if (tip.contains(hfrRegEx))
                QToolTip::showText(e->globalPos(), tip.replace(hfrRegEx, hfrStr), this);
            else
                QToolTip::showText(e->globalPos(), QToolTip::text() + '\n' + hfrStr, this);
        }
    }

    e->accept();
}

/**
I added some things to the top of this method to allow panning and Scope slewing to function.
If in dragMouse mode, the Panning function works by storing the cursor position when the mouse was pressed and setting
the mouseButtonDown variable to true.
If in ScopeMouse mode and the mouse is clicked, if there is WCS data and a scope is available, the method will verify that you actually
do want to slew to the WCS coordinates associated with the click location.  If so, it calls the centerTelescope function.
 */

void FITSLabel::mousePressEvent(QMouseEvent *e)
{
    float scale = (view->getCurrentZoom() / ZOOM_DEFAULT);

    double x = round(e->x() / scale);
    double y = round(e->y() / scale);

    m_p1.setX(x);//record first point for selection Rectangle
    m_p1.setY(y);
    prevPoint = QPoint(x, y);
    prevscale = scale;
    x = KSUtils::clamp(x, 1.0, m_Width);
    y = KSUtils::clamp(y, 1.0, m_Height);

    if(e->buttons() & Qt::LeftButton && view->getCursorMode() == FITSView::dragCursor)
    {
        if(roiRB->geometry().contains(x * scale, y * scale))
            isRoiSelected = true;
    }

    if (view->getCursorMode() == FITSView::dragCursor && !isRoiSelected)
    {
        mouseButtonDown = true;
        lastMousePoint  = e->globalPos();
        view->updateMouseCursor();
    }
    else if (e->buttons() & Qt::LeftButton && view->getCursorMode() == FITSView::scopeCursor)
    {
#ifdef HAVE_INDI
        const QSharedPointer<FITSData> &view_data = view->imageData();
        if (view_data->hasWCS())
        {
            QPointF wcsPixelPoint(x, y);
            SkyPoint wcsCoord;
            if(view_data->pixelToWCS(wcsPixelPoint, wcsCoord))
            {
                auto ra = wcsCoord.ra0();
                auto dec = wcsCoord.dec0();
                if (KMessageBox::Continue == KMessageBox::warningContinueCancel(
                            nullptr,
                            "Slewing to Coordinates: \nRA: " + ra.toHMSString() +
                            "\nDec: " + dec.toDMSString(),
                            i18n("Continue Slew"), KStandardGuiItem::cont(),
                            KStandardGuiItem::cancel(), "continue_slew_warning"))
                {
                    centerTelescope(ra.Hours(), dec.Degrees());
                    view->setCursorMode(view->lastMouseMode);
                    view->updateScopeButton();
                }
            }
        }
#endif
    }


#ifdef HAVE_INDI
    const QSharedPointer<FITSData> &view_data = view->imageData();

    if (e->buttons() & Qt::RightButton && view->getCursorMode() != FITSView::scopeCursor)
    {
        mouseReleaseEvent(e);
        if (view_data->hasWCS())
        {
            for (auto &listObject : view_data->getSkyObjects())
            {
                if ((std::abs(listObject->x() - x) < 10 / scale) && (std::abs(listObject->y() - y) < 10 / scale))
                {
                    SkyObject *object = listObject->skyObject();
                    KSPopupMenu *pmenu;
                    pmenu = new KSPopupMenu();
                    object->initPopupMenu(pmenu);
                    QList<QAction *> actions = pmenu->actions();
                    for (auto action : actions)
                    {
                        if (action->text().left(7) == "Starhop")
                            pmenu->removeAction(action);
                        if (action->text().left(7) == "Angular")
                            pmenu->removeAction(action);
                        if (action->text().left(8) == "Add flag")
                            pmenu->removeAction(action);
                        if (action->text().left(12) == "Attach Label")
                            pmenu->removeAction(action);
                    }
                    pmenu->popup(e->globalPos());
                    KStars::Instance()->map()->setClickedObject(object);
                    break;
                }
            }
        }

        if (fabs(view->markerCrosshair.x() - x) <= 15 && fabs(view->markerCrosshair.y() - y) <= 15)
            emit markerSelected(0, 0);
    }
#endif

    if  (e->buttons() & Qt::LeftButton)
    {
        if (view->getCursorMode() == FITSView::selectCursor)
            emit pointSelected(x, y);
        else if (view->getCursorMode() == FITSView::crosshairCursor)
            emit pointSelected(x + 5 / scale, y + 5 / scale);
        else if (view->showObjects)
            emit highlightSelected(x, y);
    }
}

void FITSLabel::mouseDoubleClickEvent(QMouseEvent *e)
{
    double x, y;

    x = round(e->x() / (view->getCurrentZoom() / ZOOM_DEFAULT));
    y = round(e->y() / (view->getCurrentZoom() / ZOOM_DEFAULT));

    x = KSUtils::clamp(x, 1.0, m_Width);
    y = KSUtils::clamp(y, 1.0, m_Height);

    emit markerSelected(x, y);

    return;
}

void FITSLabel::centerTelescope(double raJ2000, double decJ2000)
{
#ifdef HAVE_INDI

    if (INDIListener::Instance()->size() == 0)
    {
        KSNotification::sorry(i18n("KStars did not find any active mounts."));
        return;
    }

    for (auto &oneDevice : INDIListener::devices())
    {
        if (!(oneDevice->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
            continue;

        if (oneDevice->isConnected() == false)
        {
            KSNotification::error(i18n("Mount %1 is offline. Please connect and retry again.", oneDevice->getDeviceName()));
            return;
        }

        auto mount = oneDevice->getMount();
        if (!mount)
            continue;

        SkyPoint selectedObject;
        selectedObject.setRA0(raJ2000);
        selectedObject.setDec0(decJ2000);
        selectedObject.apparentCoord(J2000, KStarsData::Instance()->ut().djd());
        mount->Slew(&selectedObject);
        return;
    }

    KSNotification::sorry(i18n("KStars did not find any active mounts."));

#else

    Q_UNUSED(raJ2000);
    Q_UNUSED(decJ2000);

#endif
}

void FITSLabel::showRubberBand(bool on)
{
    if(on)
    {
        roiRB->show();
        if (roiRB->isVisible() && !roiRB->geometry().isNull())
            updatePersistentRoiLabel(roiRB->geometry());
    }
    else
    {
        roiRB->hide();
        if (m_roiStatsLabel)
            m_roiStatsLabel->hide();
    }
}

/// Scales the rubberband on zoom
void FITSLabel::zoomRubberBand(double scale)
{
    QRect r = roiRB->geometry() ;

    if(prevscale == 0.0 )
        prevscale = scale;

    double ap = r.width() / r.height();
    double ow = r.width() * scale / prevscale;
    double oh = r.height() * scale / prevscale;

    int rx, ry;
    rx = round(r.topLeft().x() * scale / prevscale);
    ry = round(r.topLeft().y() * scale / prevscale);
    r.setTopLeft(QPoint(rx, ry));

    rx = round(r.bottomRight().x() * scale / prevscale);
    ry = round(r.bottomRight().y() * scale / prevscale);
    r.setBottomRight(QPoint(rx, ry));

    if (ap != r.width() / r.height())
    {
        r.setSize(QSize(ow, oh));
    }

    roiRB->setGeometry(r);
    prevscale = scale;
    if (roiRB->isVisible())
        updatePersistentRoiLabel(roiRB->geometry());
}
/// Intended to take raw rect as input from FITSView context
void FITSLabel::setRubberBand(QRect rect)
{
    float scale = (view->getCurrentZoom() / ZOOM_DEFAULT);

    int rx, ry;
    rx = round(rect.topLeft().x() * scale );
    ry = round(rect.topLeft().y() * scale );
    rect.setTopLeft(QPoint(rx, ry));

    rx = round(rect.bottomRight().x() * scale );
    ry = round(rect.bottomRight().y() * scale );
    rect.setBottomRight(QPoint(rx, ry));

    roiRB->setGeometry(rect);
    prevscale = scale;
    if (roiRB->isVisible())
        updatePersistentRoiLabel(roiRB->geometry());
}

void FITSLabel::updatePersistentRoiLabel(const QRect &roiGeometry)
{
    if (!view || !view->imageData() || !view->isSelectionRectShown() || !m_roiStatsLabel || roiGeometry.isNull() || roiGeometry.isEmpty())
    {
        if (m_roiStatsLabel)
            m_roiStatsLabel->hide();
        return;
    }

    const auto &imgData = view->imageData();
    if (!imgData) // Double check
    {
        m_roiStatsLabel->hide();
        return;
    }

    // Calculate stats
    auto result = QString("σ %1").arg(QString::number(imgData->getAverageStdDev(true), 'f', 2));
    result += QString("\nx̄ %1").arg(QString::number(imgData->getAverageMean(true), 'f', 2));
    result += QString("\nM %1").arg(QString::number(imgData->getAverageMedian(true), 'f', 2));

    m_roiStatsLabel->setText(result);
    m_roiStatsLabel->adjustSize(); // Adjust size to fit content first

    // Position the label (e.g., to the right of the ROI or inside)
    // Place it outside the rubber band, vertically centered, with adjustments to stay within FITSLabel
    QPoint labelPos;
    int labelHalfHeight = m_roiStatsLabel->height() / 2;
    int roiCenterY = roiGeometry.center().y();

    // Attempt to place to the right, vertically centered
    labelPos.setX(roiGeometry.right() + 5); // 5px offset to the right
    labelPos.setY(roiCenterY - labelHalfHeight);

    // Ensure the label is within the FITSLabel bounds horizontally
    if (labelPos.x() + m_roiStatsLabel->width() > width())   // If too far right
    {
        labelPos.setX(roiGeometry.left() - m_roiStatsLabel->width() - 5); // Try left of ROI
        if (labelPos.x() < 0)   // If still too far left (ROI is near left edge)
        {
            labelPos.setX(roiGeometry.right() - m_roiStatsLabel->width()); // Try inside right
            if (labelPos.x() < roiGeometry.left()) labelPos.setX(roiGeometry.left()); // Ensure it starts within ROI
        }
    }
    if (labelPos.x() < 0) labelPos.setX(0); // Final check for left boundary


    // Ensure the label is within the FITSLabel bounds vertically
    if (labelPos.y() < 0)   // If too high
    {
        labelPos.setY(0);
    }
    else if (labelPos.y() + m_roiStatsLabel->height() > height())   // If too low
    {
        labelPos.setY(height() - m_roiStatsLabel->height());
    }
    // Final check for vertical position if label is taller than view (should be rare for this small label)
    if (labelPos.y() < 0) labelPos.setY(0);


    m_roiStatsLabel->move(labelPos);
    m_roiStatsLabel->show();
    m_roiStatsLabel->raise(); // Ensure it's on top
}

void FITSLabel::handleImageDataUpdated()
{
    if (roiRB && roiRB->isVisible() && view && view->isSelectionRectShown() && m_roiStatsLabel)
    {
        // When FITSView signals it has updated (e.g. new image loaded),
        // we need to ensure the ROI statistics are recalculated for the *new* data
        // using the *current* ROI selection.
        // We do this by re-emitting the rectangleSelected signal with the current ROI's
        // raw (unscaled) coordinates. This will trigger FITSView::processRectangle.
        float currentGuiScale = view->getCurrentZoom() / ZOOM_DEFAULT;

        if (currentGuiScale > 0.0001) // Check for valid scale
        {
            QRect currentScaledRoi = roiRB->geometry();
            QPoint p1_raw(round(currentScaledRoi.left() / currentGuiScale), round(currentScaledRoi.top() / currentGuiScale));
            QPoint p2_raw(round(currentScaledRoi.right() / currentGuiScale), round(currentScaledRoi.bottom() / currentGuiScale));

            // Ensure the rectangle is normalized (p1 is top-left)
            QRect rawRoiRect = QRect(p1_raw, p2_raw).normalized();

            if (rawRoiRect.isValid())
            {
                // Tell FITSView to process this rectangle.
                // FITSView::processRectangle will call FITSData::calculateROIStatistics()
                // and then emit FITSView::rectangleUpdated() or FITSView::updated() again.
                // The third parameter must be true to ensure statistics are calculated.
                emit rectangleSelected(rawRoiRect.topLeft(), rawRoiRect.bottomRight(), true);

                // It's expected that after FITSView processes this, it will emit a signal
                // (like `updated` or `rectangleUpdated`) which will lead to
                // `updatePersistentRoiLabel` being called with the fresh statistics.
                // If that signal chain isn't perfectly reliable or timely,
                // a direct call here might show stale data briefly then update.
                // For now, rely on the signal chain initiated by rectangleSelected.
            }
            else
            {
                // If ROI somehow became invalid (e.g. due to extreme zoom out and fixed size ROI)
                // still try to update the label, which might hide it or show default.
                updatePersistentRoiLabel(currentScaledRoi);
            }
        }
        else
        {
            // Invalid scale, update with current geometry (might hide label or show default)
            updatePersistentRoiLabel(roiRB->geometry());
        }
        // The actual update of the label text with new stats should happen
        // when updatePersistentRoiLabel is called *after* FITSView has processed
        // the rectangleSelected signal and its own data is updated.
        // This typically happens if FITSView::updated() is emitted again, or if
        // FITSView::rectangleUpdated() is connected to a slot that updates the label.
        // Our current connection is FITSView::updated -> handleImageDataUpdated.
        // So, if emitting rectangleSelected causes FITSView to emit updated() again,
        // this function will be re-entered, and the *next* time updatePersistentRoiLabel
        // is called (implicitly or explicitly), it should have the correct stats.
        // To ensure the label *does* refresh after this trigger, we can call it:
        updatePersistentRoiLabel(roiRB->geometry());
    }
    else
    {
        if (m_roiStatsLabel)
            m_roiStatsLabel->hide();
    }
}

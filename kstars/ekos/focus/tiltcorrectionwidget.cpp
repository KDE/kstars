/*
    SPDX-FileCopyrightText: 2025 cfuture81

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tiltcorrectionwidget.h"

#include "Options.h"
#include "indi/indilistener.h"
#include "indi/clientmanager.h"

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QMessageBox>
#include <cmath>

namespace Ekos
{

// Custom widget that paints the rear-view diagram of the tilt plate
class TiltDiagramWidget : public QWidget
{
    public:
        enum PlateType { PLATE_3POINT, PLATE_4POINT };

        TiltDiagramWidget(QWidget *parent = nullptr) : QWidget(parent)
        {
            setMinimumSize(200, 200);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }

        void setPlateType(PlateType type)
        {
            m_type = type;
            update();
        }
        void setRotation(double degrees)
        {
            m_rotationDegrees = degrees;
            update();
        }
        void setAdjustments(double adj[4], int count)
        {
            m_count = count;
            for (int i = 0; i < count; i++)
                m_adj[i] = adj[i];
            update();
        }
        void setValid(bool valid)
        {
            m_valid = valid;
            update();
        }

    protected:
        void paintEvent(QPaintEvent *) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);

            int w = width();
            int h = height();
            // Reserve space at the top for the "Camera top" label, and bottom for the "rear view" label
            int reservedTop = 22;
            int reservedBottom = 24;
            int side = std::min(w, h - reservedTop - reservedBottom) - 20;
            int cx = w / 2;
            int cy = reservedTop + (h - reservedTop - reservedBottom) / 2;
            int radius = side / 2;

            // Draw plate outline (rotation-invariant since it's a circle)
            painter.setPen(QPen(Qt::gray, 2));
            painter.setBrush(QBrush(QColor(40, 40, 40)));
            painter.drawEllipse(QPoint(cx, cy), radius, radius);

            // Draw the rotated content (sensor rect, screws, "Camera top" arrow)
            painter.save();
            painter.translate(cx, cy);
            painter.rotate(m_rotationDegrees);
            painter.translate(-cx, -cy);

            // Draw sensor rectangle (rear view — mirrored LR from sky view)
            int sensorW = radius * 1.2;
            int sensorH = radius * 0.8;
            QRect sensorRect(cx - sensorW / 2, cy - sensorH / 2, sensorW, sensorH);
            painter.setPen(QPen(QColor(100, 100, 100), 1, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(sensorRect);

            // "Camera top" indicator inside the rotated frame: small arrow at top edge of plate
            painter.setPen(QPen(QColor(220, 200, 120), 2));
            QPointF arrowBase(cx, cy - radius + 10);
            QPointF arrowTip(cx, cy - radius - 4);
            painter.drawLine(arrowBase, arrowTip);
            painter.drawLine(arrowTip, QPointF(arrowTip.x() - 4, arrowTip.y() + 6));
            painter.drawLine(arrowTip, QPointF(arrowTip.x() + 4, arrowTip.y() + 6));

            if (!m_valid)
            {
                // Will draw the message after restoring (so it's not rotated)
            }
            else
            {
                // Draw screws (their positions follow the painter's rotation)
                if (m_type == PLATE_3POINT)
                    draw3Point(painter, cx, cy, radius);
                else
                    draw4Point(painter, cx, cy, radius);
            }

            painter.restore();

            // Sensor label inside sensor rect (NOT rotated — keep readable)
            painter.setPen(QColor(100, 100, 100));
            QFontMetrics fm(painter.font());
            QString sensorLabel = i18n("Sensor (rear view)");
            QPoint sensorTextPos(cx - fm.horizontalAdvance(sensorLabel) / 2, cy + 4);
            painter.drawText(sensorTextPos, sensorLabel);

            if (!m_valid)
            {
                painter.setPen(Qt::white);
                painter.drawText(rect(), Qt::AlignCenter, i18n("No tilt data"));
            }

            // Top label: Camera body top reference (NOT rotated)
            painter.setPen(QColor(180, 180, 180));
            QFont smallFont = painter.font();
            smallFont.setPointSize(8);
            painter.setFont(smallFont);
            QString topLabel = (std::abs(m_rotationDegrees) < 0.01)
                               ? i18n("↑ Camera top (logo side) ↑")
                               : i18n("Camera top rotated %1°", QString::number(m_rotationDegrees, 'f', 0));
            painter.drawText(QRect(0, 2, w, reservedTop), Qt::AlignCenter, topLabel);

            // Bottom label: viewing orientation
            painter.setPen(QColor(150, 150, 150));
            painter.drawText(QRect(0, h - reservedBottom, w, reservedBottom),
                             Qt::AlignCenter, i18n("View from behind camera"));
        }

    private:
        QPointF rotateAround(QPointF p, double cx, double cy, double angleDeg) const
        {
            if (std::abs(angleDeg) < 0.01)
                return p;
            double rad = angleDeg * M_PI / 180.0;
            double cosA = std::cos(rad);
            double sinA = std::sin(rad);
            double dx = p.x() - cx;
            double dy = p.y() - cy;
            return QPointF(cx + dx * cosA - dy * sinA,
                           cy + dx * sinA + dy * cosA);
        }

        void draw3Point(QPainter &painter, int cx, int cy, int radius)
        {
            // 3-point ETA: screws are physically labeled "1", "2", "3" by the manufacturer
            // ETA front view: screw 1 = upper-left, screw 3 = upper-right, screw 2 = bottom
            // In rear view (what we draw — what user sees standing behind camera),
            // LR is mirrored, so:
            //   Screw 1 appears upper-RIGHT (was upper-left in front)
            //   Screw 3 appears upper-LEFT (was upper-right in front)
            //   Screw 2 stays bottom-center
            // m_adj[0]=Point 1, [1]=Point 2, [2]=Point 3 (ETA naming)
            // Note: rotation is applied via the painter transform in paintEvent, not here.
            double r = radius * 0.70;

            QPointF p1(cx + r * 0.866, cy - r * 0.5);   // Screw 1: upper-right in rear view
            QPointF p2(cx, cy + r);                      // Screw 2: bottom-center
            QPointF p3(cx - r * 0.866, cy - r * 0.5);   // Screw 3: upper-left in rear view

            drawAdjustmentPoint(painter, p1, m_adj[0], "1");
            drawAdjustmentPoint(painter, p2, m_adj[1], "2");
            drawAdjustmentPoint(painter, p3, m_adj[2], "3");
        }

        void draw4Point(QPainter &painter, int cx, int cy, int radius)
        {
            double r = radius * 0.75;
            double diag = r * std::cos(M_PI / 4.0);  // r / sqrt(2)

            // 4-point: screws at the corners as physically seen from behind the camera
            // Numbered 1-4 by position in the rear view:
            //   1 = top-left,  2 = top-right
            //   3 = bottom-left, 4 = bottom-right
            // (m_adj indices match: [0]=screw 1, [1]=screw 2, [2]=screw 3, [3]=screw 4)
            // Note: rotation is applied via the painter transform in paintEvent, not here.
            QPointF p1(cx - diag, cy - diag);   // Screw 1: top-left (rear view)
            QPointF p2(cx + diag, cy - diag);   // Screw 2: top-right
            QPointF p3(cx - diag, cy + diag);   // Screw 3: bottom-left
            QPointF p4(cx + diag, cy + diag);   // Screw 4: bottom-right

            drawAdjustmentPoint(painter, p1, m_adj[0], "1");
            drawAdjustmentPoint(painter, p2, m_adj[1], "2");
            drawAdjustmentPoint(painter, p3, m_adj[2], "3");
            drawAdjustmentPoint(painter, p4, m_adj[3], "4");
        }

        void drawAdjustmentPoint(QPainter &painter, QPointF pos, double adjMm, const QString &label)
        {
            // Point circle
            int pointRadius = 12;
            QColor color;
            if (std::abs(adjMm) < 0.001)
                color = QColor(100, 100, 100);  // Negligible — gray
            else if (adjMm > 0)
                color = QColor(255, 80, 80);    // Increase — red
            else
                color = QColor(80, 150, 255);   // Decrease — blue

            painter.setPen(QPen(color, 2));
            painter.setBrush(QBrush(color.darker(200)));
            painter.drawEllipse(pos, pointRadius, pointRadius);

            // For the text labels, counter-rotate so they stay readable
            // even when the diagram is rotated.
            painter.save();
            painter.translate(pos);
            painter.rotate(-m_rotationDegrees);

            // Label inside circle
            painter.setPen(Qt::white);
            QFont boldFont = painter.font();
            boldFont.setBold(true);
            boldFont.setPointSize(9);
            painter.setFont(boldFont);
            painter.drawText(QRectF(-pointRadius, -pointRadius, pointRadius * 2, pointRadius * 2),
                             Qt::AlignCenter, label);

            // Value text below the point
            boldFont.setBold(false);
            boldFont.setPointSize(8);
            painter.setFont(boldFont);
            painter.setPen(color);

            QString valueStr;
            if (std::abs(adjMm) < 0.001)
                valueStr = QStringLiteral("0");
            else
                valueStr = i18n("%1%2 μm",
                                adjMm > 0 ? QStringLiteral("+") : QString(),
                                QString::number(adjMm * 1000.0, 'f', 0));

            QRectF textRect(-50, pointRadius + 6, 100, 16);
            painter.drawText(textRect, Qt::AlignCenter, valueStr);

            painter.restore();
        }

        PlateType m_type { PLATE_3POINT };
        double m_adj[4] { 0, 0, 0, 0 };
        int m_count { 3 };
        bool m_valid { false };
        double m_rotationDegrees { 0.0 };
};

// ============================================================================
// TiltCorrectionWidget implementation
// ============================================================================

TiltCorrectionWidget::TiltCorrectionWidget(QWidget *parent) : QGroupBox(parent)
{
    setTitle(i18n("Tilt Correction Advisory"));
    setupUI();
    updateVisibility();
}

void TiltCorrectionWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Settings row
    QHBoxLayout *settingsLayout = new QHBoxLayout();

    settingsLayout->addWidget(new QLabel(i18n("Plate:")));
    m_plateTypeCombo = new QComboBox();
    m_plateTypeCombo->addItem(i18n("3-point"), PLATE_3POINT);
    m_plateTypeCombo->addItem(i18n("4-point"), PLATE_4POINT);
    m_plateTypeCombo->setToolTip(i18n("Select your tilt plate type.\n"
                                      "3-point: ETA, Octopi, or any 3-screw plate (2 top, 1 bottom).\n"
                                      "4-point: Plates with screws at the four corners (UL/UR/LL/LR) — "
                                      "typical for TouTek-style tilt rings."));
    settingsLayout->addWidget(m_plateTypeCombo);

    settingsLayout->addSpacing(10);
    settingsLayout->addWidget(new QLabel(i18n("Radius:")));
    m_radiusSpin = new QDoubleSpinBox();
    m_radiusSpin->setRange(5.0, 100.0);
    m_radiusSpin->setValue(64.0);
    m_radiusSpin->setSuffix(i18nc("millimeters suffix", " mm"));
    m_radiusSpin->setDecimals(1);
    m_radiusSpin->setToolTip(i18n("Distance from plate center to adjustment point (bolt circle radius).\n"
                                  "Known values:\n"
                                  "  Wanderer ETA M54: 64 mm\n"
                                  "  ZWO Tilt Plates (M42/M48/M54/M68): 40 mm\n"
                                  "  TouTek M42 Tilter (Ø100mm): 46 mm"));
    settingsLayout->addWidget(m_radiusSpin);

    settingsLayout->addSpacing(10);
    settingsLayout->addWidget(new QLabel(i18n("Thread:")));
    m_threadPresetCombo = new QComboBox();
    m_threadPresetCombo->addItem(i18nc("Thread preset", "M2.5 (0.45 mm)"), 0.45);
    m_threadPresetCombo->addItem(i18nc("Thread preset", "M3 (0.50 mm)"), 0.50);
    m_threadPresetCombo->addItem(i18nc("Thread preset", "M3 fine (0.35 mm)"), 0.35);
    m_threadPresetCombo->addItem(i18nc("Thread preset", "M4 (0.70 mm)"), 0.70);
    m_threadPresetCombo->addItem(i18nc("Thread preset", "M4 fine (0.50 mm)"), 0.50);
    m_threadPresetCombo->addItem(i18nc("Thread preset", "M5 (0.80 mm)"), 0.80);
    m_threadPresetCombo->addItem(i18nc("Thread preset", "M5 fine (0.50 mm)"), 0.50);
    m_threadPresetCombo->addItem(i18nc("Thread preset", "M6 (1.00 mm)"), 1.00);
    m_threadPresetCombo->addItem(i18nc("Thread preset", "Wanderer ETA (0.50 mm)"), 0.50);
    m_threadPresetCombo->addItem(i18nc("Thread preset", "Custom"), -1.0);
    m_threadPresetCombo->setCurrentIndex(8);  // Default to ETA
    m_threadPresetCombo->setToolTip(i18n("Common adjustment screw thread types.\n"
                                         "Select to auto-fill pitch, or choose Custom to enter manually."));
    settingsLayout->addWidget(m_threadPresetCombo);

    settingsLayout->addSpacing(10);
    settingsLayout->addWidget(new QLabel(i18n("Pitch:")));
    m_screwPitchSpin = new QDoubleSpinBox();
    m_screwPitchSpin->setRange(0.1, 2.0);
    m_screwPitchSpin->setValue(0.5);
    m_screwPitchSpin->setSuffix(i18nc("millimeters per turn suffix", " mm/turn"));
    m_screwPitchSpin->setDecimals(2);
    m_screwPitchSpin->setToolTip(i18n("Screw pitch (mm per full turn). Used to show turn count for manual plates."));
    settingsLayout->addWidget(m_screwPitchSpin);

    settingsLayout->addSpacing(10);
    settingsLayout->addWidget(new QLabel(i18n("Rotation:")));
    m_rotationSpin = new QDoubleSpinBox();
    m_rotationSpin->setRange(-180.0, 180.0);
    m_rotationSpin->setValue(0.0);
    m_rotationSpin->setSuffix(QStringLiteral(" °"));
    m_rotationSpin->setDecimals(0);
    m_rotationSpin->setSingleStep(5.0);
    m_rotationSpin->setToolTip(i18n("Camera rotation in optical train (0° = camera logo at top).\n"
                                    "Positive values rotate the camera clockwise as seen from behind.\n"
                                    "Diagram is adjusted so screw positions match what you physically see."));
    settingsLayout->addWidget(m_rotationSpin);

    settingsLayout->addSpacing(10);
    settingsLayout->addWidget(new QLabel(i18n("Mode:")));
    m_modeCombo = new QComboBox();
    m_modeCombo->addItem(i18nc("Tilt correction mode", "Relative (± deltas)"), MODE_RELATIVE);
    m_modeCombo->addItem(i18nc("Tilt correction mode", "Push-only (from zero)"), MODE_PUSH_ONLY);
    m_modeCombo->setToolTip(
        i18n("Choose how the corrections are presented:\n"
         "Relative: signed deltas (+ and -) to apply on top of current position.\n"
         "  Best for iterative tuning when already mid-range and screws can go both ways.\n"
         "Push-only: all values shifted to be positive (smallest = 0).\n"
         "  Best when starting from fully retracted (e.g. ETA at 0, manual screws unscrewed).\n"
         "  Push-only plates physically can't go negative, so use this for first-time tuning."));
    settingsLayout->addWidget(m_modeCombo);

    // Connect the thread preset to update the pitch
    connect(m_threadPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int)
    {
        double pitch = m_threadPresetCombo->currentData().toDouble();
        if (pitch > 0)
        {
            m_screwPitchSpin->blockSignals(true);
            m_screwPitchSpin->setValue(pitch);
            m_screwPitchSpin->blockSignals(false);
            m_screwPitch = pitch;
            recalculate();
        }
        // If Custom is selected, leave the pitch as-is (user enters it manually)
        saveSettings();
    });

    settingsLayout->addStretch();
    mainLayout->addLayout(settingsLayout);

    // Diagram and results
    QHBoxLayout *contentLayout = new QHBoxLayout();

    // Diagram (left side)
    m_diagramWidget = new TiltDiagramWidget(this);
    contentLayout->addWidget(m_diagramWidget, 2);

    // Text results (right side)
    QVBoxLayout *resultsLayout = new QVBoxLayout();
    m_point1Label = new QLabel(i18n("Point 1: —"));
    m_point2Label = new QLabel(i18n("Point 2: —"));
    m_point3Label = new QLabel(i18n("Point 3: —"));
    m_point4Label = new QLabel(i18n("Point 4: —"));
    m_summaryLabel = new QLabel("");

    QFont monoFont("Monospace");
    monoFont.setStyleHint(QFont::TypeWriter);
    m_point1Label->setFont(monoFont);
    m_point2Label->setFont(monoFont);
    m_point3Label->setFont(monoFont);
    m_point4Label->setFont(monoFont);

    resultsLayout->addWidget(m_point1Label);
    resultsLayout->addWidget(m_point2Label);
    resultsLayout->addWidget(m_point3Label);
    resultsLayout->addWidget(m_point4Label);
    resultsLayout->addSpacing(10);
    resultsLayout->addWidget(m_summaryLabel);

    // "Apply to ETA" button — only visible when Wanderer ETA is connected
    m_applyETAButton = new QPushButton(i18n("Apply to ETA"));
    m_applyETAButton->setToolTip(i18n("Send computed corrections to the Wanderer ETA M54 tilt plate via INDI.\n"
                                      "Reads current positions, adds the computed deltas, and moves all three points."));
    m_applyETAButton->setVisible(false);
    m_applyETAButton->setEnabled(false);
    resultsLayout->addSpacing(10);
    resultsLayout->addWidget(m_applyETAButton);

    connect(m_applyETAButton, &QPushButton::clicked, this, &TiltCorrectionWidget::applyToETA);

    resultsLayout->addStretch();

    contentLayout->addLayout(resultsLayout, 1);
    mainLayout->addLayout(contentLayout);

    // Connect signals
    connect(m_plateTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int)
    {
        m_plateType = static_cast<PlateType>(m_plateTypeCombo->currentData().toInt());
        m_point4Label->setVisible(m_plateType == PLATE_4POINT);
        m_applyETAButton->setVisible(m_plateType == PLATE_3POINT);
        recalculate();
        saveSettings();
    });

    connect(m_radiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val)
    {
        m_radius = val;
        recalculate();
        saveSettings();
    });

    connect(m_screwPitchSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val)
    {
        m_screwPitch = val;
        recalculate();
        saveSettings();
    });

    connect(m_rotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val)
    {
        m_rotationDegrees = val;
        recalculate();
        saveSettings();
    });

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int)
    {
        m_mode = static_cast<AdjustmentMode>(m_modeCombo->currentData().toInt());
        recalculate();
        saveSettings();
    });

    // Initially hide point 4 (3-point mode default)
    m_point4Label->setVisible(false);

    // Restore previously saved settings
    loadSettings();
}

void TiltCorrectionWidget::updateTilt(double lrMicrons, double tbMicrons,
                                      double sensorWidthMicrons, double sensorHeightMicrons)
{
    m_lrMicrons = lrMicrons;
    m_tbMicrons = tbMicrons;
    m_sensorWidthMicrons = sensorWidthMicrons;
    m_sensorHeightMicrons = sensorHeightMicrons;
    m_valid = true;
    m_etaApplied = false;  // Fresh data — allow applying corrections again
    recalculate();
}

void TiltCorrectionWidget::setInvalid()
{
    m_valid = false;
    m_point1Label->setText(i18n("Point 1: N/A"));
    m_point2Label->setText(i18n("Point 2: N/A"));
    m_point3Label->setText(i18n("Point 3: N/A"));
    m_point4Label->setText(i18n("Point 4: N/A"));
    m_summaryLabel->setText(QString());
    m_applyETAButton->setEnabled(false);
    static_cast<TiltDiagramWidget *>(m_diagramWidget)->setValid(false);
}

void TiltCorrectionWidget::recalculate()
{
    if (!m_valid)
    {
        setInvalid();
        return;
    }

    // Detect connected Wanderer ETA and auto-configure geometry
    detectETADevice();
    updateVisibility();

    if (m_plateType == PLATE_3POINT)
        calc3Point();
    else
        calc4Point();

    // Update diagram
    TiltDiagramWidget *diagram = static_cast<TiltDiagramWidget *>(m_diagramWidget);
    diagram->setPlateType(m_plateType == PLATE_3POINT ?
                          TiltDiagramWidget::PLATE_3POINT : TiltDiagramWidget::PLATE_4POINT);
    diagram->setRotation(m_rotationDegrees);
    diagram->setAdjustments(m_adj, m_plateType == PLATE_3POINT ? 3 : 4);
    diagram->setValid(true);
}

// ============================================================================
// Detect connected tilt plate devices and auto-configure geometry
// ============================================================================

void TiltCorrectionWidget::detectETADevice()
{
    QSharedPointer<ISD::GenericDevice> device;
    bool etaConnected = INDIListener::findDevice(QStringLiteral("Wanderer ETA M54"), device);

    if (etaConnected && m_plateType == PLATE_3POINT)
    {
        // Auto-fill ETA geometry if not already set
        constexpr double ETA_RADIUS = 64.0;
        constexpr double ETA_PITCH = 0.50;

        if (std::abs(m_radius - ETA_RADIUS) > 0.1)
        {
            m_radius = ETA_RADIUS;
            m_radiusSpin->blockSignals(true);
            m_radiusSpin->setValue(ETA_RADIUS);
            m_radiusSpin->blockSignals(false);
        }
        if (std::abs(m_screwPitch - ETA_PITCH) > 0.01)
        {
            m_screwPitch = ETA_PITCH;
            m_screwPitchSpin->blockSignals(true);
            m_screwPitchSpin->setValue(ETA_PITCH);
            m_screwPitchSpin->blockSignals(false);
        }

        // Lock geometry fields — ETA hardware values are fixed
        m_radiusSpin->setEnabled(false);
        m_screwPitchSpin->setEnabled(false);
        m_threadPresetCombo->setEnabled(false);
        m_radiusSpin->setToolTip(i18n("Locked: Wanderer ETA M54 radius is 64 mm"));
        m_screwPitchSpin->setToolTip(i18n("Locked: Wanderer ETA M54 pitch is 0.50 mm/turn"));
        m_threadPresetCombo->setToolTip(i18n("Locked: Wanderer ETA M54 detected"));

        m_applyETAButton->setVisible(true);
        m_applyETAButton->setToolTip(i18n("Send computed corrections to the Wanderer ETA M54 tilt plate via INDI.\n"
                                          "Reads current positions, adds the computed deltas, and moves all three points."));
    }
    else
    {
        // No ETA connected or not in 3-point mode — hide the button, unlock fields
        m_applyETAButton->setVisible(false);
        m_radiusSpin->setEnabled(true);
        m_screwPitchSpin->setEnabled(true);
        m_threadPresetCombo->setEnabled(true);
        m_radiusSpin->setToolTip(QString());
        m_screwPitchSpin->setToolTip(QString());
        m_threadPresetCombo->setToolTip(QString());
    }
}

void TiltCorrectionWidget::updateVisibility()
{
    // Show the widget if:
    //   1. A Wanderer ETA device is connected (auto-detect), OR
    //   2. The user has explicitly enabled the tilt plate option
    QSharedPointer<ISD::GenericDevice> device;
    bool etaConnected = INDIListener::findDevice(QStringLiteral("Wanderer ETA M54"), device);
    setVisible(etaConnected || Options::tiltPlatePresent());
}

void TiltCorrectionWidget::calc3Point()
{
    // ETA geometry (viewed from front/telescope side):
    //   Point 1: upper-left at angle 150° from horizontal
    //   Point 3: upper-right at angle 30° from horizontal
    //   Point 2: bottom-center at angle 270° (straight down)
    //
    // The tilt plane to CORRECT is the opposite of the measured tilt.
    // LR slope: microns per micron across sensor width
    // TB slope: microns per micron across sensor height

    double lrSlope = (m_sensorWidthMicrons > 0) ? m_lrMicrons / m_sensorWidthMicrons : 0.0;
    double tbSlope = (m_sensorHeightMicrons > 0) ? m_tbMicrons / m_sensorHeightMicrons : 0.0;

    // Convert radius from mm to microns for consistent units
    double R = m_radius * 1000.0;  // mm to microns

    // Point positions relative to center (front view):
    // Point 1: angle 150° (upper-left)
    // Point 3: angle 30°  (upper-right)
    // Point 2: angle 270° (bottom)
    double angles[3] = { 150.0 * M_PI / 180.0, 270.0 * M_PI / 180.0, 30.0 * M_PI / 180.0 };

    // For each point, the correction needed is the NEGATIVE of the tilt at that position
    // Z_correction = -(lrSlope * x + tbSlope * y)
    // These are RELATIVE deltas — apply on top of current position, can be + or -.
    for (int i = 0; i < 3; i++)
    {
        double x = R * std::cos(angles[i]);
        double y = R * std::sin(angles[i]);
        m_adj[i] = -(lrSlope * x + tbSlope * y);
    }

    // Convert from microns to mm for display
    for (int i = 0; i < 3; i++)
        m_adj[i] /= 1000.0;

    // Apply push-only normalization if requested: shift values so minimum is 0
    if (m_mode == MODE_PUSH_ONLY)
    {
        double minAdj = *std::min_element(m_adj, m_adj + 3);
        for (int i = 0; i < 3; i++)
            m_adj[i] -= minAdj;
    }

    // Update labels — show signed deltas (apply all together)
    auto formatPoint = [this](int idx, const QString & name) -> QString
    {
        double turns = m_adj[idx] / m_screwPitch;
        if (std::abs(m_adj[idx]) < 0.001)
            return i18n("%1: no change", name);
        return i18n("%1: %2%3 mm (%4%5 turns)",
                    name,
                    m_adj[idx] > 0 ? QStringLiteral("+") : QString(),
                    QString::number(m_adj[idx], 'f', 3),
                    turns > 0 ? QStringLiteral("+") : QString(),
                    QString::number(turns, 'f', 1));
    };

    m_point1Label->setText(formatPoint(0, i18nc("3-point tilt plate screw", "Point 1 (upper-right)")));
    m_point2Label->setText(formatPoint(1, i18nc("3-point tilt plate screw", "Point 2 (bottom)")));
    m_point3Label->setText(formatPoint(2, i18nc("3-point tilt plate screw", "Point 3 (upper-left)")));

    double maxAbs = std::max({std::abs(m_adj[0]), std::abs(m_adj[1]), std::abs(m_adj[2])});
    QString modeLabel = (m_mode == MODE_PUSH_ONLY)
                        ? i18n("Push-only mode (from zero)")
                        : i18n("Relative mode (± deltas)");
    QString summary = i18n("Total tilt: %1 μm\nMax adjustment: %2 mm\n%3",
                           QString::number(std::hypot(m_lrMicrons, m_tbMicrons), 'f', 0),
                           QString::number(maxAbs, 'f', 3),
                           modeLabel);
    m_summaryLabel->setText(summary);

    // Enable "Apply to ETA" button when corrections are non-zero and not already applied
    bool hasCorrection = (maxAbs >= 0.001);
    m_applyETAButton->setEnabled(hasCorrection && !m_etaApplied);
    if (hasCorrection && !m_etaApplied)
    {
        m_applyETAButton->setText(i18n("Apply to ETA"));
        m_applyETAButton->setToolTip(i18n("Send computed corrections to the Wanderer ETA M54 tilt plate via INDI.\n"
                                          "Reads current positions, adds the computed deltas, and moves all three points."));
    }
}

void TiltCorrectionWidget::calc4Point()
{
    // 4-point plate: screws at the four corners
    // Numbering matches the diagram (rear view, what the user sees standing behind the camera):
    //   Screw 1: top-left in rear view    (above sensor corner TR in FITS coords — LR mirrored)
    //   Screw 2: top-right in rear view   (above sensor corner TL)
    //   Screw 3: bottom-left in rear view (above sensor corner BR)
    //   Screw 4: bottom-right in rear view (above sensor corner BL)

    double lrSlope = (m_sensorWidthMicrons > 0) ? m_lrMicrons / m_sensorWidthMicrons : 0.0;
    double tbSlope = (m_sensorHeightMicrons > 0) ? m_tbMicrons / m_sensorHeightMicrons : 0.0;

    double R = m_radius * 1000.0;  // mm to microns
    const double C = R * std::cos(M_PI / 4.0);  // R / sqrt(2)

    // m_adj[0..3] correspond to Screw 1..4 in the diagram (rear view positions).
    // Screw 1 (rear-view top-left) sits above body-frame coordinate (+C, +C)
    m_adj[0] = -(lrSlope *  C + tbSlope *  C) / 1000.0;
    // Screw 2 (rear-view top-right) sits above body-frame (-C, +C)
    m_adj[1] = -(lrSlope * -C + tbSlope *  C) / 1000.0;
    // Screw 3 (rear-view bottom-left) sits above body-frame (+C, -C)
    m_adj[2] = -(lrSlope *  C + tbSlope * -C) / 1000.0;
    // Screw 4 (rear-view bottom-right) sits above body-frame (-C, -C)
    m_adj[3] = -(lrSlope * -C + tbSlope * -C) / 1000.0;

    // Apply push-only normalization if requested: shift values so minimum is 0
    if (m_mode == MODE_PUSH_ONLY)
    {
        double minAdj = *std::min_element(m_adj, m_adj + 4);
        for (int i = 0; i < 4; i++)
            m_adj[i] -= minAdj;
    }

    // Update labels
    auto formatPoint = [this](int idx, const QString & name) -> QString
    {
        double turns = m_adj[idx] / m_screwPitch;
        if (std::abs(m_adj[idx]) < 0.001)
            return i18n("%1: no change", name);
        return i18n("%1: %2%3 mm (%4%5 turns)",
                    name,
                    m_adj[idx] > 0 ? QStringLiteral("+") : QString(),
                    QString::number(m_adj[idx], 'f', 3),
                    turns > 0 ? QStringLiteral("+") : QString(),
                    QString::number(turns, 'f', 1));
    };

    m_point1Label->setText(formatPoint(0, i18nc("4-point tilt plate screw", "Screw 1 (top-left)")));
    m_point2Label->setText(formatPoint(1, i18nc("4-point tilt plate screw", "Screw 2 (top-right)")));
    m_point3Label->setText(formatPoint(2, i18nc("4-point tilt plate screw", "Screw 3 (bottom-left)")));
    m_point4Label->setText(formatPoint(3, i18nc("4-point tilt plate screw", "Screw 4 (bottom-right)")));

    double maxAbs = std::max(
    {
        std::abs(m_adj[0]), std::abs(m_adj[1]),
        std::abs(m_adj[2]), std::abs(m_adj[3])
    });
    QString modeLabel = (m_mode == MODE_PUSH_ONLY)
                        ? i18n("Push-only mode (from zero)")
                        : i18n("Relative mode (± deltas)");
    QString summary = i18n("Total tilt: %1 μm\nLR: %2 μm | TB: %3 μm\nMax adjustment: %4 mm\n%5",
                           QString::number(std::hypot(m_lrMicrons, m_tbMicrons), 'f', 0),
                           QString::number(m_lrMicrons, 'f', 0),
                           QString::number(m_tbMicrons, 'f', 0),
                           QString::number(maxAbs, 'f', 3),
                           modeLabel);
    m_summaryLabel->setText(summary);
}

// ============================================================================
// Settings persistence (via KStars Options / kstars.kcfg)
// ============================================================================

void TiltCorrectionWidget::loadSettings()
{
    // Block signals while restoring to avoid triggering recalc/save loops
    m_plateTypeCombo->blockSignals(true);
    m_radiusSpin->blockSignals(true);
    m_threadPresetCombo->blockSignals(true);
    m_screwPitchSpin->blockSignals(true);
    m_rotationSpin->blockSignals(true);
    m_modeCombo->blockSignals(true);

    int plateType = Options::tiltPlateType();
    if (plateType >= 0 && plateType < m_plateTypeCombo->count())
        m_plateTypeCombo->setCurrentIndex(plateType);
    m_plateType = static_cast<PlateType>(m_plateTypeCombo->currentData().toInt());
    m_point4Label->setVisible(m_plateType == PLATE_4POINT);
    m_applyETAButton->setVisible(m_plateType == PLATE_3POINT);

    double radius = Options::tiltRadius();
    m_radiusSpin->setValue(radius);
    m_radius = radius;

    int threadIndex = Options::tiltThreadIndex();
    if (threadIndex >= 0 && threadIndex < m_threadPresetCombo->count())
        m_threadPresetCombo->setCurrentIndex(threadIndex);

    double screwPitch = Options::tiltScrewPitch();
    m_screwPitchSpin->setValue(screwPitch);
    m_screwPitch = screwPitch;

    double rotation = Options::tiltRotation();
    m_rotationSpin->setValue(rotation);
    m_rotationDegrees = rotation;

    int mode = Options::tiltMode();
    if (mode >= 0 && mode < m_modeCombo->count())
        m_modeCombo->setCurrentIndex(mode);
    m_mode = static_cast<AdjustmentMode>(m_modeCombo->currentData().toInt());

    m_plateTypeCombo->blockSignals(false);
    m_radiusSpin->blockSignals(false);
    m_threadPresetCombo->blockSignals(false);
    m_screwPitchSpin->blockSignals(false);
    m_rotationSpin->blockSignals(false);
    m_modeCombo->blockSignals(false);
}

void TiltCorrectionWidget::saveSettings()
{
    Options::setTiltPlateType(m_plateTypeCombo->currentIndex());
    Options::setTiltRadius(m_radiusSpin->value());
    Options::setTiltThreadIndex(m_threadPresetCombo->currentIndex());
    Options::setTiltScrewPitch(m_screwPitchSpin->value());
    Options::setTiltRotation(m_rotationSpin->value());
    Options::setTiltMode(m_modeCombo->currentIndex());
}

// ============================================================================
// Apply corrections to Wanderer ETA M54 via INDI
// ============================================================================

void TiltCorrectionWidget::applyToETA()
{
    // Sanity checks
    if (m_plateType != PLATE_3POINT || !m_valid)
        return;

    constexpr double ETA_MAX_TRAVEL = 1.200;  // mm — maximum plate travel per point

    // 1. Find the Wanderer ETA device
    QSharedPointer<ISD::GenericDevice> device;
    if (!INDIListener::findDevice(QStringLiteral("Wanderer ETA M54"), device))
    {
        m_summaryLabel->setText(i18n("⚠ Wanderer ETA M54 not connected"));
        m_applyETAButton->setEnabled(false);
        m_applyETAButton->setToolTip(i18n("Wanderer ETA not connected"));
        return;
    }

    // 2. Read current positions from POSITION_READBACK
    auto readbackProp = device->getBaseDevice().getNumber("POSITION_READBACK");
    if (!readbackProp)
    {
        m_summaryLabel->setText(i18n("⚠ Cannot read ETA positions (POSITION_READBACK not available)"));
        return;
    }

    auto p1Read = readbackProp.findWidgetByName("POINT_1_READ");
    auto p2Read = readbackProp.findWidgetByName("POINT_2_READ");
    auto p3Read = readbackProp.findWidgetByName("POINT_3_READ");

    if (!p1Read || !p2Read || !p3Read)
    {
        m_summaryLabel->setText(i18n("⚠ ETA position readback fields not found"));
        return;
    }

    double currentPos[3] = { p1Read->getValue(), p2Read->getValue(), p3Read->getValue() };

    // 3. Compute new target positions: current + correction delta
    double target[3];
    for (int i = 0; i < 3; i++)
        target[i] = currentPos[i] + m_adj[i];

    // 4. Smart-shift to stay within 0.000–1.200 mm range
    double minTarget = *std::min_element(target, target + 3);
    if (minTarget < 0.0)
    {
        // Shift all up so minimum becomes 0
        double shift = -minTarget;
        for (int i = 0; i < 3; i++)
            target[i] += shift;
    }

    double maxTarget = *std::max_element(target, target + 3);
    if (maxTarget > ETA_MAX_TRAVEL)
    {
        // The full correction doesn't fit. Clamp by shifting down so max = 1.200.
        // This gives the best partial correction within the available range.
        double shift = maxTarget - ETA_MAX_TRAVEL;
        for (int i = 0; i < 3; i++)
            target[i] -= shift;

        // If any point went below 0 after clamping, the correction range exceeds 1.2mm total
        double newMin = *std::min_element(target, target + 3);
        if (newMin < -0.001)
        {
            m_summaryLabel->setText(i18n("⚠ Correction range (%1 mm) exceeds ETA plate travel (%2 mm).\n"
                                         "Cannot apply even partially. Zero the plate first.",
                                         QString::number(maxTarget - minTarget, 'f', 3),
                                         QString::number(ETA_MAX_TRAVEL, 'f', 3)));
            return;
        }

        // Clamp any tiny negative rounding to zero
        for (int i = 0; i < 3; i++)
            target[i] = std::max(0.0, target[i]);

        // Ask user if they want the partial correction
        auto reply = QMessageBox::question(this,
                                           i18n("Partial Correction"),
                                           i18n("Full correction exceeds plate travel. Apply best-fit partial correction?\n\n"
             "P1: %1 mm, P2: %2 mm, P3: %3 mm\n\n"
             "Run autofocus again afterward to apply the remainder.",
                                                QString::number(target[0], 'f', 3),
                                                QString::number(target[1], 'f', 3),
                                                QString::number(target[2], 'f', 3)),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (reply != QMessageBox::Yes)
            return;
    }

    // 5. Send new targets via INDI — sequentially to avoid half-duplex conflicts.
    //    Store targets and initiate the first move. Subsequent moves are triggered
    //    by watching the property state change to IPS_OK.
    auto p1TargetProp = device->getBaseDevice().getNumber("POINT_1_TARGET");
    auto p2TargetProp = device->getBaseDevice().getNumber("POINT_2_TARGET");
    auto p3TargetProp = device->getBaseDevice().getNumber("POINT_3_TARGET");

    if (!p1TargetProp || !p2TargetProp || !p3TargetProp)
    {
        m_summaryLabel->setText(i18n("⚠ ETA target properties not available"));
        return;
    }

    // Store targets for sequential execution
    m_etaTargets[0] = target[0];
    m_etaTargets[1] = target[1];
    m_etaTargets[2] = target[2];
    m_etaDevice = device;
    m_etaCurrentPoint = 0;

    // Connect to property updates to detect when each move completes
    connect(device.get(), &ISD::GenericDevice::propertyUpdated, this,
            &TiltCorrectionWidget::onETAPropertyUpdate, Qt::UniqueConnection);

    // Disable button while applying
    m_applyETAButton->setEnabled(false);
    m_summaryLabel->setText(i18n("⏳ Applying corrections to ETA (Point 1)..."));

    // Send first point
    sendETAPoint(0);
}

void TiltCorrectionWidget::sendETAPoint(int pointIndex)
{
    if (!m_etaDevice || pointIndex < 0 || pointIndex > 2)
        return;

    static const char *propNames[3] = { "POINT_1_TARGET", "POINT_2_TARGET", "POINT_3_TARGET" };
    static const char *elemNames[3] = { "POINT_1", "POINT_2", "POINT_3" };

    auto prop = m_etaDevice->getBaseDevice().getNumber(propNames[pointIndex]);
    if (!prop)
        return;

    auto elem = prop.findWidgetByName(elemNames[pointIndex]);
    if (!elem)
        return;

    elem->setValue(m_etaTargets[pointIndex]);
    m_etaDevice->getClientManager()->sendNewProperty(prop);
}

void TiltCorrectionWidget::onETAPropertyUpdate(INDI::Property prop)
{
    if (m_etaCurrentPoint < 0 || m_etaCurrentPoint > 2)
        return;

    static const char *propNames[3] = { "POINT_1_TARGET", "POINT_2_TARGET", "POINT_3_TARGET" };

    // Check if this is the property we're waiting for
    if (!prop.isNameMatch(propNames[m_etaCurrentPoint]))
        return;

    // Wait for IPS_OK (move complete) or IPS_ALERT (error)
    if (prop.getState() == IPS_BUSY)
        return;

    if (prop.getState() == IPS_ALERT)
    {
        // Move failed — abort the sequence
        m_summaryLabel->setText(i18n("⚠ ETA Point %1 move failed. Stopped.", m_etaCurrentPoint + 1));
        m_applyETAButton->setEnabled(true);
        disconnect(m_etaDevice.get(), &ISD::GenericDevice::propertyUpdated, this,
                   &TiltCorrectionWidget::onETAPropertyUpdate);
        m_etaCurrentPoint = -1;
        return;
    }

    // IPS_OK — move to next point
    m_etaCurrentPoint++;

    if (m_etaCurrentPoint < 3)
    {
        m_summaryLabel->setText(i18n("⏳ Applying corrections to ETA (Point %1)...", m_etaCurrentPoint + 1));
        sendETAPoint(m_etaCurrentPoint);
    }
    else
    {
        // All 3 points done — keep button disabled until next autofocus provides fresh data
        m_summaryLabel->setText(i18n("✓ Applied to ETA: P1→%1  P2→%2  P3→%3 mm. Run autofocus again to re-measure.",
                                     QString::number(m_etaTargets[0], 'f', 3),
                                     QString::number(m_etaTargets[1], 'f', 3),
                                     QString::number(m_etaTargets[2], 'f', 3)));
        m_applyETAButton->setEnabled(false);
        m_applyETAButton->setText(i18n("Done"));
        m_applyETAButton->setToolTip(i18n("Corrections already applied. Run autofocus again to get fresh tilt measurements."));
        disconnect(m_etaDevice.get(), &ISD::GenericDevice::propertyUpdated, this,
                   &TiltCorrectionWidget::onETAPropertyUpdate);
        m_etaCurrentPoint = -1;
        m_etaApplied = true;
    }
}

}

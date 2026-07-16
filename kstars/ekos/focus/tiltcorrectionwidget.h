/*
    SPDX-FileCopyrightText: 2025 cfuture81

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QSharedPointer>

#include "indi/indiproperty.h"

namespace ISD
{
class GenericDevice;
}

namespace Ekos
{

/**
 * @brief TiltCorrectionWidget provides a visual advisory for tilt plate adjustments.
 *
 * Given the measured tilt (LR and TB in microns) from the Aberration Inspector,
 * this widget computes and displays the suggested adjustments for each point
 * on the user's tilt plate. It supports:
 *   - 3-point plates (120° spacing, 2 top + 1 bottom): ETA, Octopi, manual 3-point
 *   - 4-point plates (90° spacing, push/pull): manual plates with T/B/L/R screws
 *
 * The widget shows a rear-view diagram (as seen from behind the camera) with
 * color-coded arrows indicating direction and magnitude of adjustment.
 */
class TiltCorrectionWidget : public QGroupBox
{
        Q_OBJECT

    public:
        enum PlateType
        {
            PLATE_3POINT = 0,  // 2 top, 1 bottom (ETA, Octopi, manual 3-point)
            PLATE_4POINT       // Top/Bottom/Left/Right push-pull
        };

        enum AdjustmentMode
        {
            MODE_RELATIVE = 0,  // Signed deltas (apply on top of current position)
            MODE_PUSH_ONLY      // All values normalized to positive (push from zero)
        };

        /**
         * @brief Construct the tilt correction advisory widget
         * @param parent widget
         */
        explicit TiltCorrectionWidget(QWidget *parent = nullptr);
        ~TiltCorrectionWidget() = default;

        /**
         * @brief Update the advisory with new tilt measurements
         * @param lrMicrons Left-to-Right tilt in microns (positive = left higher than right)
         * @param tbMicrons Top-to-Bottom tilt in microns (positive = top higher than bottom)
         * @param sensorWidthMicrons Sensor width span used for tilt calculation (microns)
         * @param sensorHeightMicrons Sensor height span used for tilt calculation (microns)
         */
        void updateTilt(double lrMicrons, double tbMicrons, double sensorWidthMicrons, double sensorHeightMicrons);

        /**
         * @brief Mark results as invalid (e.g., when tilt calculation fails)
         */
        void setInvalid();

        /**
         * @brief Update widget visibility based on ETA device detection and Options::tiltPlatePresent.
         *        Called on construction and can be called externally to refresh.
         */
        void updateVisibility();

    private:
        void setupUI();
        void recalculate();
        void loadSettings();
        void saveSettings();

        /**
         * @brief Apply the computed corrections to the Wanderer ETA M54 via INDI.
         * Reads current positions from POSITION_READBACK, adds m_adj[] deltas,
         * smart-shifts to stay within 0.000–1.200 mm range, and sends new targets.
         */
        void applyToETA();

        /**
         * @brief Detect connected tilt plate devices (e.g. Wanderer ETA M54) and
         * auto-configure radius/pitch. Shows or hides the Apply button accordingly.
         */
        void detectETADevice();

        // --- Calculation ---

        /**
         * @brief Calculate 3-point adjustments
         * Point 1 (upper-left), Point 3 (upper-right), Point 2 (bottom-center)
         * viewed from behind the camera.
         */
        void calc3Point();

        /**
         * @brief Calculate 4-point adjustments
         * Top, Bottom, Left, Right screws aligned with sensor edges.
         */
        void calc4Point();

        // --- UI elements ---
        QComboBox *m_plateTypeCombo { nullptr };
        QComboBox *m_threadPresetCombo { nullptr };
        QComboBox *m_modeCombo { nullptr };             // Adjustment mode (relative vs push-only)
        QDoubleSpinBox *m_radiusSpin { nullptr };      // Bolt circle radius in mm
        QDoubleSpinBox *m_screwPitchSpin { nullptr };  // mm per full turn (optional)
        QDoubleSpinBox *m_rotationSpin { nullptr };    // Camera rotation angle in degrees
        QWidget *m_diagramWidget { nullptr };           // Custom painted rear-view diagram
        QLabel *m_point1Label { nullptr };
        QLabel *m_point2Label { nullptr };
        QLabel *m_point3Label { nullptr };
        QLabel *m_point4Label { nullptr };  // Only for 4-point mode
        QLabel *m_summaryLabel { nullptr };
        QPushButton *m_applyETAButton { nullptr };   // "Apply to ETA" button (3-point mode only)

        // --- Sequential ETA apply state ---
        QSharedPointer<ISD::GenericDevice> m_etaDevice;
        double m_etaTargets[3] { 0.0, 0.0, 0.0 };
        int m_etaCurrentPoint { -1 };  // -1 = idle, 0-2 = waiting for that point
        bool m_etaApplied { false };   // True after successful apply; reset on fresh updateTilt()

        void sendETAPoint(int pointIndex);
        void onETAPropertyUpdate(INDI::Property prop);

        // --- State ---
        PlateType m_plateType { PLATE_3POINT };
        AdjustmentMode m_mode { MODE_RELATIVE };
        double m_radius { 64.0 };       // Default bolt circle radius (mm) — Wanderer ETA M54
        double m_screwPitch { 0.5 };    // Default screw pitch (mm/turn)

        // Tilt input (from Aberration Inspector)
        double m_lrMicrons { 0.0 };
        double m_tbMicrons { 0.0 };
        double m_sensorWidthMicrons { 0.0 };
        double m_sensorHeightMicrons { 0.0 };
        double m_rotationDegrees { 0.0 };  // Camera rotation in optical train (degrees)
        bool m_valid { false };

        // Calculated adjustments (mm)
        double m_adj[4] { 0.0, 0.0, 0.0, 0.0 };  // Up to 4 points
};

}

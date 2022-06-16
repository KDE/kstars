/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QWidget>
#include "qcustomplot.h"
#include "ekos/ekos.h"
#include "ekos/focus/polynomialfit.h"
#include "ekos/focus/curvefit.h"

class FocusHFRVPlot : public QCustomPlot
{
    public:
        FocusHFRVPlot(QWidget *parent = nullptr);

        /**
         * @brief add a single focus position result
         * @param pos focuser position or iteration number
         * @param newHFR HFR value for the given position
         * @param pulseDuration Pulse duration in ms for relative focusers that only support timers,
         *        or the number of ticks in a relative or absolute focuser
         */
        void addPosition(double pos, double newHFR, int pulseDuration, bool plot = true);

        /**
         * @brief add a single focus position result with error (sigma)
         * @param pos focuser position or iteration number
         * @param newHFR HFR value for the given position
         * @param sigma value is the error (sigma) in the HFR measurement
         * @param pulseDuration Pulse duration in ms for relative focusers that only support timers,
         *        or the number of ticks in a relative or absolute focuser
         */
        void addPositionWithSigma(double pos, double newHFR, double sigma, int pulseDuration, bool plot = true);

        /**
         * @brief sets the plot title
         * @param title the new plot title
         */
        void setTitle(const QString &title, bool plot = true);

        /**
         * @brief updates the plot title
         * @param title the new plot title
         */
        void updateTitle(const QString &title, bool plot = true);

        /**
         * @brief Annotate's the plot's solution graph with the solution position.
         * @param solutionPosition focuser position of the focal point
         * @param solutionValue HFR value on the focal point
         */
        void drawMinimum(double solutionPosition, double solutionValue, bool plot = true);

        /**
         * @brief Draws the polynomial on the plot's graph.
         * @param polyFit pointer to the polynomial approximation
         * @param isVShape is the polynomial of a V shape (true) or U shape (false)?
         * @param makeVisible make the polynomial graph visible (true) or use the last state (false)?
         */
        void drawPolynomial(Ekos::PolynomialFit *polyFit, bool isVShape, bool makeVisible, bool plot = true);

        /**
         * @brief Draws the curve on the plot's graph.
         * @param curveFit pointer to the curve approximation
         * @param isVShape is the curve of a V shape (true) or U shape (false)?
         * @param makeVisible make the graph visible (true) or use the last state (false)?
         */
        void drawCurve(Ekos::CurveFitting *curveFit, bool isVShape, bool makeVisible, bool plot = true);

        /**
         * @brief Refresh the entire graph
         * @param polyFit pointer to the polynomial approximation
         * @param solutionPosition focuser position of the focal point
         * @param solutionValue HFR value on the focal point
         */
        void redraw(Ekos::PolynomialFit *polyFit, double solutionPosition, double solutionValue);

        /**
         * @brief Refresh the entire graph
         * @param curveFit pointer to the curve approximation
         * @param solutionPosition focuser position of the focal point
         * @param solutionValue HFR value on the focal point
         */
        void redrawCurve(Ekos::CurveFitting *curveFit, double solutionPosition, double solutionValue);

        /**
         * @brief Initialize and reset the entire HFR V-plot
         * @param showPosition show focuser position (true) or show focusing iteration number (false)
         */
        void init(bool showPosition);

        /// basic font size from which all others are derived
        int basicFontSize() const
        {
            return m_basicFontSize;
        }
        void setBasicFontSize(int basicFontSize);

    private:
        /**
         * @brief Draw the HFR plot for all recent focuser positions
         * @param currentHFR current HFR value
         * @param pulseDuration Pulse duration in ms for relative focusers that only support timers,
         *        or the number of ticks in a relative or absolute focuser
         */
        void drawHFRPlot(double currentHFR, int pulseDuration);

        /**
         * @brief Draw all positions and values of the current focusing run.
         */
        void drawHFRIndices();

        /**
         * @brief Initialize and reset the HFR plot
         * @param showPosition show the focuser positions (true) or the focus iteration number (false)
         */

        /**
         * @brief Set pen color depending upon the solution is sound (V-Shape) or unsound (U-Shape)
         * @param isSound
         */
        void setSolutionVShape(bool isVShape);

        /**
         * @brief Clear all the items on HFR plot
         */
        void clearItems();

        QCPGraph *polynomialGraph = nullptr;
        QVector<double> hfr_position, hfr_value, hfr_sigma;

        // Error bars for the focus plot
        bool useErrorBars = false;
        QCPErrorBars * errorBars = nullptr;

        // Title text for the focus plot
        QCPItemText *plotTitle  { nullptr };

        /// Maximum HFR recorded
        double maxHFR { -1 };
        /// List of V curve plot points
        /// V-Curve graph
        QCPGraph *v_graph { nullptr };
        /// focus point
        QCPGraph *focusPoint { nullptr };
        /// show focus position (true) or use focus step number?
        bool m_showPositions = true;
        /// is there a current polynomial solution
        bool m_isVShape = false;
        /// basic font size from which all others are derived
        int m_basicFontSize = 10;

        bool m_polynomialGraphIsVisible = false;
};

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
         * @brief add a single focus position result with error (sigma)
         * @param pos focuser position or iteration number
         * @param newHFR HFR value for the given position
         * @param sigma value is the error (sigma) in the HFR measurement
         * @param pulseDuration Pulse duration in ms for relative focusers that only support timers,
         *        or the number of ticks in a relative or absolute focuser
         */
        void addPosition(double pos, double newHFR, double sigma, bool outlier, int pulseDuration, bool plot = true);

        /**
         * @brief sets the plot title
         * @param title the new plot title
         */
        void setTitle(const QString &title, bool plot = true);
        QString title() const;

        /**
         * @brief updates the plot title
         * @param title the new plot title
         */
        void finalUpdates(const QString &title, bool plot = true);

        /**
         * @brief Annotate's the plot's solution graph with the solution position.
         * @param solutionPosition focuser position of the focal point
         * @param solutionValue HFR value on the focal point
         */
        void drawMinimum(double solutionPosition, double solutionValue, bool plot = true);

        /**
         * @brief Draw the CFZ on the graph
         * @param solutionPosition focuser position of the focal point
         * @param solutionValue HFR value on the focal point
         * @param cfzSteps The CFZ size
         * @param plot Whether to plot
         */
        void drawCFZ(double solutionPosition, double solutionValue, int cfzSteps, bool plot);

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
         * @param yAxisLabel is the label to display
         * @param starUnits the units multiplier to display the pixel data
         * @param minimum whether the curve shape is a minimum or maximum
         * @param useWeights whether or not to display weights on the graph
         * @param showPosition show focuser position (true) or show focusing iteration number (false)
         */
        void init(QString yAxisLabel, double starUnits, bool minimum, bool useWeights, bool showPosition);

        /// basic font size from which all others are derived
        int basicFontSize() const
        {
            return m_basicFontSize;
        }
        void setBasicFontSize(int basicFontSize);

    private:
        /**
         * @brief Draw the HFR plot for all recent focuser positions
         * @param currentValue current HFR value
         * @param pulseDuration Pulse duration in ms for relative focusers that only support timers,
         *        or the number of ticks in a relative or absolute focuser
         */
        void drawHFRPlot(double currentValue, int pulseDuration);

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

        /**
         * @brief Convert input value to output display value
         * @param value to convert
         */
        double getDisplayValue(const double value);

        QCPGraph *polynomialGraph = nullptr;
        QVector<double> m_position, m_value, m_displayValue, m_sigma;
        QVector<bool> m_goodPosition;

        // Error bars for the focus plot
        bool m_useErrorBars = false;
        QCPErrorBars * errorBars = nullptr;

        // CFZ
        QCPItemBracket * CFZ = nullptr;

        // Title text for the focus plot
        QCPItemText *plotTitle  { nullptr };

        /// Maximum and minimum y-values recorded
        double minValue { -1 }, maxValue { -1 };
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
        /// Is the curve V-shaped or n-shaped
        bool m_Minimum = true;
        // Units multiplier for star measure value
        double m_starUnits = 1.0;

        bool m_polynomialGraphIsVisible = false;
};

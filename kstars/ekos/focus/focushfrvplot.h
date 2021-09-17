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
    void addPosition(double pos, double newHFR, int pulseDuration);

    /**
     * @brief Annotate's the plot's solution graph with the solution position.
     * @param solutionPosition focuser position of the focal point
     * @param solutionValue HFR value on the focal point
     */
    void drawMinimum(double solutionPosition, double solutionValue);

    /**
     * @brief Draws the polynomial on the plot's graph.
     * @param polyFit pointer to the polynomial approximation
     * @param isVShape is the polynomial of a V shape (true) or U shape (false)?
     * @param makeVisible make the polynomial graph visible (true) or use the last state (false)?
     */
    void drawPolynomial(Ekos::PolynomialFit *polyFit, bool isVShape, bool makeVisible);

    /**
     * @brief Refresh the entire graph
     * @param polyFit pointer to the polynomial approximation
     * @param solutionPosition focuser position of the focal point
     * @param solutionValue HFR value on the focal point
     */
    void redraw(Ekos::PolynomialFit *polyFit, double solutionPosition, double solutionValue);

    /**
     * @brief Initialize and reset the entire HFR V-plot
     * @param showPosition show focuser position (true) or show focusing iteration number (false)
     */
    void init(bool showPosition);

    /// basic font size from which all others are derived
    int basicFontSize() const { return m_basicFontSize; }
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

    QCPGraph *polynomialGraph = nullptr;
    QVector<double> hfr_position, hfr_value;

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

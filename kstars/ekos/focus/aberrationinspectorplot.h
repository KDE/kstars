/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "aberrationinspectorutils.h"
#include "qcustomplot.h"

// AberrationInspectorPlot manages the QCustomPlot graph of focus position vs measure for the 9 mosaic tiles
// Each tile has its own curve. The legend shows which curve refers to which tile. The maximum or minimum of each
// curve is also displayed. The tooltip of each max / min gives more info. Optionally, a label describing the max / min
// can be displayed; and the CFZ can be displayed around the centre tile max / min.
//
namespace Ekos
{

class CurveFitting;

class AberrationInspectorPlot : public QCustomPlot
{
    public:
        /**
         * @brief create an AberrationInspectorPlot graph
         * @param parent widget
         */
        AberrationInspectorPlot(QWidget *parent = nullptr);

        /**
         * @brief add all data for all datapoints for all curves
         * @param position focuser position
         * @param measure for the associated position
         * @param weight for the associated measure
         * @param outlier or not
         */
        void addData(QVector<int> position, QVector<double> measure, QVector<double> weight, QVector<bool> outlier);

        /**
         * @brief Add the max / min solution to the plot
         * @param tile number
         * @param solution position
         * @param solution value
         */
        void drawMaxMin(int tile, double solutionPosition, double solutionValue);

        /**
         * @brief Draw the Critical Focus Zone centred on solution
         * @param solution position
         * @param solution measure
         * @param CFZ size in steps
         */
        void drawCFZ(double solutionPosition, double solutionMeasure, int cfzSteps);

        /**
         * @brief Draw the curve
         * @param tile
         * @param curveFit pointer to generate data points
         * @param maxmin is the curve max or min
         * @param measure of the curve, e.g. HFR
         * @param fit is whether a curve fit was achieved
         * @param R2 of the curve fit
         */
        void drawCurve(int tile, Ekos::CurveFitting *curveFit, int maxmin, double measure, bool fit, double R2);

        /**
         * @brief Refresh the entire graph
         * @param useTile array indicating which tiles to show/hide
         */
        void redrawCurve(bool useTile[NUM_TILES]);

        /**
         * @brief Initialize the plot
         * @param yAxisLabel is the label to display
         * @param starUnits the units multiplier to display the pixel data
         * @param useWeights whether or not weights have been used
         * @param show labels on the plot
         * @param show CFZ on the plot
         */
        void init(QString yAxisLabel, double starUnits, bool useWeights, bool showLabels, bool showCFZ);

        /**
         * @brief show / hide labels on plot
         * @return setting
         */
        void setShowLabels(bool setting);

        /**
         * @brief show / hide CFZ on plot
         * @return setting
         */
        void setShowCFZ(bool setting);

    private:
        /**
         * @brief return font size
         * @return font size
         */
        int basicFontSize() const
        {
            return m_basicFontSize;
        }

        /**
         * @brief set font size
         * @param font size
         */
        void setBasicFontSize(int fontSize);

        /**
         * @brief Setup the axes based on the data
         * @param tile to process
         */
        void setAxes(const int tile);

        /**
         * @brief Convert input measure to output display measure
         * @param measure to convert
         * @return converted measure
         */
        double getDisplayMeasure(const double measure);

        QVector<int> m_positions;
        QVector<QVector<double>> m_measures;

        // Legend items for the plot
        QCPAbstractLegendItem *m_legendItems[NUM_TILES] { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

        // Text items highlighting the max / min of each curve
        bool m_showLabels = false;
        QCPItemText *m_labelItems[NUM_TILES] { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

        // CFZ
        bool m_showCFZ = false;
        QCPItemBracket *m_CFZ = nullptr;

        // V curves
        QCPGraph *m_graph[NUM_TILES] { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
        // Focus points plotted as graphs
        QCPGraph *focusPoint[NUM_TILES] { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

        // basic font size from which all others are derived
        int m_basicFontSize = 10;
        // Units multiplier for star measure value
        double m_starUnits = 1.0;

        // Max / Mins used to set the graph axes
        int m_minPosition = -1;
        int m_maxPosition = -1;
        double m_minMeasure = -1.0;
        double m_maxMeasure = -1.0;
};

}

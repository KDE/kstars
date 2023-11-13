/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aberrationinspectorplot.h"
#include "curvefit.h"
#include "klocalizedstring.h"

namespace Ekos
{

#define DEFAULT_BASIC_FONT_SIZE 10

AberrationInspectorPlot::AberrationInspectorPlot(QWidget *parent) : QCustomPlot (parent)
{
    setBackground(QBrush(Qt::black));

    xAxis->setBasePen(QPen(Qt::white, 1));
    yAxis->setBasePen(QPen(Qt::white, 1));

    xAxis->setTickPen(QPen(Qt::white, 1));
    yAxis->setTickPen(QPen(Qt::white, 1));

    xAxis->setSubTickPen(QPen(Qt::white, 1));
    yAxis->setSubTickPen(QPen(Qt::white, 1));

    xAxis->setTickLabelColor(Qt::white);
    yAxis->setTickLabelColor(Qt::white);

    xAxis->setLabelColor(Qt::white);
    yAxis->setLabelColor(Qt::white);

    xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    xAxis->grid()->setZeroLinePen(Qt::NoPen);
    yAxis->grid()->setZeroLinePen(Qt::NoPen);

    setInteractions(QCP::iRangeZoom);
    setInteraction(QCP::iRangeDrag, true);

    QVector<QCPScatterStyle::ScatterShape> shapes;
    shapes << QCPScatterStyle::ssCross;
    shapes << QCPScatterStyle::ssPlus;
    shapes << QCPScatterStyle::ssCircle;
    shapes << QCPScatterStyle::ssSquare;
    shapes << QCPScatterStyle::ssDisc;
    shapes << QCPScatterStyle::ssDiamond;
    shapes << QCPScatterStyle::ssStar;
    shapes << QCPScatterStyle::ssTriangle;
    shapes << QCPScatterStyle::ssTriangleInverted;

    this->setAutoAddPlottableToLegend(true);
    for (int i = 0; i < NUM_TILES; i++)
    {
        m_graph[i] = addGraph();
        m_graph[i]->setLineStyle(QCPGraph::lsLine);
        m_graph[i]->setPen(QPen(QColor(TILE_COLOUR[i]), 2, Qt::SolidLine));
        m_graph[i]->setScatterStyle(shapes[i]);
        m_graph[i]->setName(TILE_NAME[i]);

        // Store the auto generated legends array for later manipulation
        m_legendItems[i] = this->legend->item(i);
    }

    this->setAutoAddPlottableToLegend(false);
    for (int i = 0; i < NUM_TILES; i++)
    {
        // Add focus point graphs
        focusPoint[i] = addGraph();
        focusPoint[i]->setLineStyle(QCPGraph::lsImpulse);
        focusPoint[i]->setPen(QPen(QColor(TILE_COLOUR[i]), 2, Qt::SolidLine));
        focusPoint[i]->setScatterStyle(QCPScatterStyle(shapes[i], TILE_COLOUR[i], TILE_COLOUR[i], 10));
    }

    // determine font size
    if (parent != nullptr)
        setBasicFontSize(parent->font().pointSize());
    else
        setBasicFontSize(DEFAULT_BASIC_FONT_SIZE);

    connect(this, &QCustomPlot::mouseMove, [this](QMouseEvent * event)
    {
        double key = xAxis->pixelToCoord(event->localPos().x());
        if (xAxis->range().contains(key))
        {
            QCPGraph *graph = qobject_cast<QCPGraph *>(plottableAt(event->pos(), false));

            if (graph)
            {
                for (int i = 0; i < NUM_TILES; i++)
                {
                    if (graph == focusPoint[i])
                    {
                        if (focusPoint[i]->visible())
                        {
                            int positionKey = focusPoint[i]->findBegin(key);
                            double focusPosition = focusPoint[i]->dataMainKey(positionKey);
                            double focusMeasure = focusPoint[i]->dataMainValue(positionKey);
                            QToolTip::showText(
                                event->globalPos(),
                                i18nc("Graphics tooltip; %2 is tile code; %3 is tile name, %4 is Focus Position; %5 is Focus Measure;",
                                      "<style>table { background-color: white;}</style>"
                                      "<font color='%1'><table>"
                                      "<tr><td>Tile: </td><td>%2 (%3)</td></tr>"
                                      "<tr><td>Pos:  </td><td>%4</td></tr>"
                                      "<tr><td>Val:  </td><td>%5</td></tr>"
                                      "</table></font>",
                                      TILE_COLOUR[i],
                                      TILE_NAME[i], TILE_LONGNAME[i],
                                      QString::number(focusPosition, 'f', 0),
                                      QString::number(focusMeasure, 'g', 3)));
                        }
                        break;
                    }
                }
            }
        }
    });
}

void AberrationInspectorPlot::init(QString yAxisLabel, double starUnits, bool useWeights, bool showLabels, bool showCFZ)
{
    Q_UNUSED(useWeights);

    yAxis->setLabel(yAxisLabel);
    m_starUnits = starUnits;
    m_showLabels = showLabels;
    m_showCFZ = showCFZ;
    for (int i = 0; i < NUM_TILES; i++)
    {
        m_graph[i]->data()->clear();
        focusPoint[i]->data().clear();
        focusPoint[i]->setData(QVector<double> {}, QVector<double> {});
    }
    // Displat the legend
    this->legend->setVisible(true);
}

void AberrationInspectorPlot::setAxes(const int tile)
{
    if (tile == -1)
    {
        // Recalculate axes based on current setting of member variables
        const double xborder = (m_maxPosition - m_minPosition) / 10.0;
        xAxis->setRange(m_minPosition - xborder, m_maxPosition + xborder);

        const double yborder = (m_maxMeasure - m_minMeasure) / 10.0;
        yAxis->setRange(m_minMeasure - yborder, m_maxMeasure + yborder);
        return;
    }
    // x-range - since its the same for each curve only do it once
    if (tile == 0)
    {
        if (m_positions.empty())
            return;

        m_minPosition = *std::min_element(m_positions.constBegin(), m_positions.constEnd());
        m_maxPosition = *std::max_element(m_positions.constBegin(), m_positions.constEnd());
        const double border = (m_maxPosition - m_minPosition) / 10.0;
        xAxis->setRange(m_minPosition - border, m_maxPosition + border);
    }

    // y range
    if (m_measures[tile].empty())
        return;

    if (tile == 0)
    {
        m_minMeasure = *std::min_element(m_measures[tile].constBegin(), m_measures[tile].constEnd());
        m_maxMeasure = *std::max_element(m_measures[tile].constBegin(), m_measures[tile].constEnd());
    }
    else
    {
        m_minMeasure = std::min(m_minMeasure, *std::min_element(m_measures[tile].constBegin(), m_measures[tile].constEnd()));
        m_maxMeasure = std::max(m_maxMeasure, *std::max_element(m_measures[tile].constBegin(), m_measures[tile].constEnd()));
    }
    const double border = (m_maxMeasure - m_minMeasure) / 10.0;
    yAxis->setRange(m_minMeasure - border, m_maxMeasure + border);
}

void AberrationInspectorPlot::addData(QVector<int> positions, QVector<double> measures, QVector<double> weights,
                                      QVector<bool> outliers)
{
    Q_UNUSED(weights);
    Q_UNUSED(outliers);
    if (m_positions.count() == 0)
        m_positions.append(positions);

    // Convert the incoming measures (e.g. pixels) to display measures (e.g. arc-secs)
    QVector<double> displayMeasures;
    for (int i = 0; i < measures.count(); i++)
        displayMeasures.push_back(getDisplayMeasure(measures[i]));
    m_measures.append(displayMeasures);

    setAxes(m_measures.count() - 1);
}

void AberrationInspectorPlot::drawMaxMin(int tile, double solutionPosition, double solutionMeasure)
{
    // Do nothing for invalid positions
    if (solutionPosition <= 0)
        return;

    double displayMeasure = getDisplayMeasure(solutionMeasure);
    m_minMeasure = std::min(m_minMeasure, displayMeasure);
    m_maxMeasure = std::max(m_maxMeasure, displayMeasure);

    focusPoint[tile]->addData(solutionPosition, displayMeasure);
    m_labelItems[tile] = new QCPItemText(this);
    m_labelItems[tile]->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    m_labelItems[tile]->setColor(TILE_COLOUR[tile]);
    m_labelItems[tile]->setBrush(Qt::white);
    m_labelItems[tile]->setPen(Qt::NoPen);
    m_labelItems[tile]->setFont(QFont(font().family(), (int) std::round(0.8 * basicFontSize())));
    m_labelItems[tile]->position->setType(QCPItemPosition::ptPlotCoords);
    m_labelItems[tile]->setText(QString::number(solutionPosition, 'f', 0));
    m_labelItems[tile]->position->setCoords(solutionPosition, displayMeasure * 0.8);
    m_labelItems[tile]->setVisible(m_showLabels);
}

void AberrationInspectorPlot::drawCFZ(double solutionPosition, double solutionMeasure, int cfzSteps)
{
    // Do nothing for invalid positions
    if (solutionPosition <= 0 || solutionMeasure <= 0)
        return;

    if (!m_CFZ)
        m_CFZ = new QCPItemBracket(this);

    m_CFZ->left->setType(QCPItemPosition::ptPlotCoords);
    m_CFZ->right->setType(QCPItemPosition::ptPlotCoords);

    double y = m_minMeasure * 0.95;

    m_CFZ->left->setCoords(solutionPosition + cfzSteps / 2.0, y);
    m_CFZ->right->setCoords(solutionPosition - cfzSteps / 2.0, y);
    m_CFZ->setLength(15);
    m_CFZ->setAntialiased(false);
    m_CFZ->setPen(QPen(QColor(Qt::yellow)));
    m_CFZ->setVisible(m_showCFZ);
}

void AberrationInspectorPlot::setShowCFZ(bool setting)
{
    m_showCFZ = setting;
    if (m_CFZ)
        m_CFZ->setVisible(setting);
}

void AberrationInspectorPlot::setShowLabels(bool setting)
{
    m_showLabels = setting;
    for (int tile = 0; tile < NUM_TILES; tile++)
    {
        if (m_labelItems[tile])
            m_labelItems[tile]->setVisible(setting);
    }
}

void AberrationInspectorPlot::drawCurve(int tile, Ekos::CurveFitting *curveFit, int maxmin, double measure, bool fit,
                                        double R2)
{
    Q_UNUSED(fit);
    Q_UNUSED(R2);

    if (curveFit == nullptr)
        return;

    if (!fit)
        return;

    // Extend max/mins if appropriate
    m_minPosition = std::min(m_minPosition, maxmin);
    m_maxPosition = std::max(m_maxPosition, maxmin);
    m_minMeasure = std::min(m_minMeasure, measure);
    m_maxMeasure = std::max(m_maxMeasure, measure);
    setAxes(-1);

    if (m_graph[tile] != nullptr)
    {
        m_graph[tile]->data()->clear();
        QCPRange range = xAxis->range();
        double interval = range.size() / 40.0;

        for(double x = range.lower; x < range.upper; x += interval)
        {
            double y = getDisplayMeasure(curveFit->f(x));
            m_graph[tile]->addData(x, y);
        }
    }
}

// Show only the graph elements relevant to the passed in useTile array
void AberrationInspectorPlot::redrawCurve(bool useTile[NUM_TILES])
{
    for (int i = 0; i < NUM_TILES; i++)
    {
        // Show / hide the appropriate curves
        if (m_graph[i])
            m_graph[i]->setVisible(useTile[i]);

        // Only display legend entries for displayed graphs
        this->legend->item(i)->setVisible(useTile[i]);

        // Show / hide the focus point and text labels appopriate to the selected curves
        if (focusPoint[i])
            focusPoint[i]->setVisible(useTile[i]);
        if (m_labelItems[i])
            m_labelItems[i]->setVisible(useTile[i] && m_showLabels);
    }
}

void AberrationInspectorPlot::setBasicFontSize(int basicFontSize)
{
    m_basicFontSize = basicFontSize;

    // Axis Labels Settings
    yAxis->setLabelFont(QFont(font().family(), basicFontSize));
    xAxis->setTickLabelFont(QFont(font().family(), (int) std::round(0.9 * basicFontSize)));
    yAxis->setTickLabelFont(QFont(font().family(), (int) std::round(0.9 * basicFontSize)));
}

// Internally calculations are done in units of pixels for HFR and FWHM
// If user preference is arcsecs then convert measures for display purposes.
double AberrationInspectorPlot::getDisplayMeasure(const double measure)
{
    return measure * m_starUnits;
}

}

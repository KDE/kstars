/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focushfrvplot.h"

#include "klocalizedstring.h"

#include "curvefit.h"

#define DEFAULT_BASIC_FONT_SIZE 10

FocusHFRVPlot::FocusHFRVPlot(QWidget *parent) : QCustomPlot (parent)
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

    yAxis->setLabel(i18n("Value"));

    setInteractions(QCP::iRangeZoom);
    setInteraction(QCP::iRangeDrag, true);

    polynomialGraph = addGraph();
    polynomialGraph->setLineStyle(QCPGraph::lsLine);
    polynomialGraph->setPen(QPen(QColor(140, 140, 140), 2, Qt::DotLine));
    polynomialGraph->setScatterStyle(QCPScatterStyle::ssNone);

    v_graph = addGraph();
    v_graph->setLineStyle(QCPGraph::lsNone);
    v_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::white, Qt::white, 14));

    focusPoint = addGraph();
    focusPoint->setLineStyle(QCPGraph::lsImpulse);
    focusPoint->setPen(QPen(QColor(140, 140, 140), 2, Qt::SolidLine));
    focusPoint->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::white, Qt::yellow, 10));

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
                if(graph == v_graph)
                {
                    int positionKey = v_graph->findBegin(key);
                    double focusPosition = v_graph->dataMainKey(positionKey);
                    double focusValue = v_graph->dataMainValue(positionKey);
                    QToolTip::showText(
                        event->globalPos(),
                        i18nc("Graphics tooltip; %1 is the Focus Position; %2 is the Focus Value;",
                              "<table>"
                              "<tr><td>POS:   </td><td>%1</td></tr>"
                              "<tr><td>VAL:   </td><td>%2</td></tr>"
                              "</table>",
                              QString::number(focusPosition, 'f', 0),
                              QString::number(focusValue, 'g', 3)));
                }
                else if (graph == focusPoint)
                {
                    int positionKey = focusPoint->findBegin(key);
                    double focusPosition = focusPoint->dataMainKey(positionKey);
                    double focusValue = focusPoint->dataMainValue(positionKey);
                    QToolTip::showText(
                        event->globalPos(),
                        i18nc("Graphics tooltip; %1 is the Minimum Focus Position; %2 is the Focus Value;",
                              "<table>"
                              "<tr><td>MIN:   </td><td>%1</td></tr>"
                              "<tr><td>VAL:   </td><td>%2</td></tr>"
                              "</table>",
                              QString::number(focusPosition, 'f', 0),
                              QString::number(focusValue, 'g', 3)));

                }
            }
        }
    });
    // Add the error bars
    errorBars = new QCPErrorBars((this)->xAxis, (this)->yAxis);
}

void FocusHFRVPlot::drawHFRIndices()
{
    // Setup error bars
    QVector<double> err;
    if (m_useErrorBars)
    {
        errorBars->removeFromLegend();
        errorBars->setAntialiased(false);
        errorBars->setDataPlottable((this)->v_graph);
        errorBars->setPen(QPen(QColor(180, 180, 180)));
    }

    // Put the sample number inside the plot point's circle.
    for (int i = 0; i < m_position.size(); ++i)
    {
        QCPItemText *textLabel = new QCPItemText(this);
        textLabel->setPositionAlignment(Qt::AlignCenter | Qt::AlignHCenter);
        textLabel->position->setType(QCPItemPosition::ptPlotCoords);
        textLabel->position->setCoords(m_position[i], m_displayValue[i]);
        if (m_goodPosition[i])
        {
            textLabel->setText(QString::number(i + 1));
            textLabel->setFont(QFont(font().family(), (int) std::round(1.2 * basicFontSize())));
            textLabel->setColor(Qt::red);
        }
        else
        {
            textLabel->setText("X");
            textLabel->setFont(QFont(font().family(), (int) std::round(2 * basicFontSize())));
            textLabel->setColor(Qt::black);
        }
        textLabel->setPen(Qt::NoPen);

        if (m_useErrorBars)
            err.push_front(m_sigma[i]);
    }
    // Setup the error bar data if we're using it
    errorBars->setVisible(m_useErrorBars);
    if (m_useErrorBars)
        errorBars->setData(err);
}

void FocusHFRVPlot::init(QString yAxisLabel, double starUnits, bool minimum, bool useWeights, bool showPosition)
{
    yAxis->setLabel(yAxisLabel);
    m_starUnits = starUnits;
    m_Minimum = minimum;
    m_showPositions = showPosition;
    m_position.clear();
    m_value.clear();
    m_displayValue.clear();
    m_sigma.clear();
    m_goodPosition.clear();
    polynomialGraph->data()->clear();
    focusPoint->data().clear();
    m_useErrorBars = useWeights;
    errorBars->data().clear();
    // the next step seems necessary (QCP bug?)
    v_graph->setData(QVector<double> {}, QVector<double> {});
    focusPoint->setData(QVector<double> {}, QVector<double> {});
    m_polynomialGraphIsVisible = false;
    m_isVShape = false;
    minValue = -1;
    maxValue = -1;
    FocusHFRVPlot::clearItems();
    replot();
}

void FocusHFRVPlot::drawHFRPlot(double currentValue, int pulseDuration)
{
    // DrawHFRPlot is the base on which other things are built upon.
    // Clear any previous annotations.
    FocusHFRVPlot::clearItems();

    v_graph->setData(m_position, m_displayValue);
    drawHFRIndices();

    double currentDisplayValue = getDisplayValue(currentValue);

    if (minValue > currentDisplayValue || minValue < 0.0)
        minValue = currentDisplayValue;
    if (maxValue < currentDisplayValue)
        maxValue = currentDisplayValue;

    double minVal = currentDisplayValue / 2.5;
    if (m_displayValue.size() > 0)
        minVal = std::min(minValue, *std::min_element(m_displayValue.begin(), m_displayValue.end()));

    // True for the position-based algorithms and those that simulate position.
    if (m_showPositions)
    {
        const double minPosition = m_position.empty() ?
                                   0 : *std::min_element(m_position.constBegin(), m_position.constEnd());
        const double maxPosition = m_position.empty() ?
                                   1e6 : *std::max_element(m_position.constBegin(), m_position.constEnd());
        xAxis->setRange(minPosition - pulseDuration, maxPosition + pulseDuration);
    }
    else
    {
        //xAxis->setLabel(i18n("Iteration"));
        xAxis->setRange(1, m_displayValue.count() + 1);
    }

    if (m_displayValue.size() == 1)
        // 1 point gets placed in the middle vertically.
        yAxis->setRange(0, 2 * getDisplayValue(maxValue));
    else
    {
        double upper;
        m_Minimum ? upper = 1.5 * maxValue : upper = 1.2 * maxValue;
        yAxis->setRange(minVal - (0.25 * (upper - minVal)), upper);
    }
    replot();
}

void FocusHFRVPlot::addPosition(double pos, double newValue, double sigma, bool outlier, int pulseDuration, bool plot)
{
    m_position.append(pos);
    m_value.append(newValue);
    m_displayValue.append(getDisplayValue(newValue));
    m_sigma.append(sigma);
    outlier ? m_goodPosition.append(false) : m_goodPosition.append(true);

    if (plot)
        drawHFRPlot(newValue, pulseDuration);
}

void FocusHFRVPlot::setTitle(const QString &title, bool plot)
{
    plotTitle = new QCPItemText(this);
    plotTitle->setColor(QColor(255, 255, 255));
    plotTitle->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    plotTitle->position->setType(QCPItemPosition::ptAxisRectRatio);
    plotTitle->position->setCoords(0.5, 0);
    plotTitle->setText("");
    plotTitle->setFont(QFont(font().family(), 11));
    plotTitle->setVisible(true);

    plotTitle->setText(title);
    if (plot) replot();
}

QString FocusHFRVPlot::title() const
{
    if (plotTitle)
        return plotTitle->text();
    return QString();
}

void FocusHFRVPlot::finalUpdates(const QString &title, bool plot)
{
    // Update a previously set title without having to redraw everything
    if (plotTitle != nullptr)
    {
        plotTitle->setText(title);
        if (plot) replot();
    }
}
void FocusHFRVPlot::setSolutionVShape(bool isVShape)
{
    m_isVShape = isVShape;

    QPen pen;
    pen.setWidth(1);

    if (isVShape)
    {
        pen.setColor(QColor(180, 180, 180));
    }
    else
    {
        pen.setColor(QColor(254, 0, 0));
        // clear focus point
        focusPoint->data().clear();
    }

    polynomialGraph->setPen(pen);
}

void FocusHFRVPlot::clearItems()
{
    // Clear all the items on the HFR plot and reset pointers
    QCustomPlot::clearItems();
    plotTitle = nullptr;
    CFZ = nullptr;
}

void FocusHFRVPlot::drawMinimum(double solutionPosition, double solutionValue, bool plot)
{
    focusPoint->data()->clear();

    // do nothing for invalid positions
    if (solutionPosition < 0)
        return;

    double displayValue = getDisplayValue(solutionValue);
    minValue = std::min(minValue, displayValue);
    maxValue = std::max(maxValue, displayValue);

    focusPoint->addData(solutionPosition, displayValue);
    QCPItemText *textLabel = new QCPItemText(this);
    textLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    textLabel->setColor(Qt::red);
    textLabel->setPadding(QMargins(0, 0, 0, 0));
    textLabel->setBrush(Qt::white);
    textLabel->setPen(Qt::NoPen);
    textLabel->setFont(QFont(font().family(), (int) std::round(0.8 * basicFontSize())));
    textLabel->position->setType(QCPItemPosition::ptPlotCoords);
    textLabel->setText(QString::number(solutionPosition, 'f', 0));
    if (m_Minimum)
        textLabel->position->setCoords(solutionPosition, (maxValue + 2 * displayValue) / 3);
    else
        textLabel->position->setCoords(solutionPosition, (2 * displayValue + minValue) / 3);
    if (plot) replot();
}

void FocusHFRVPlot::drawCFZ(double solutionPosition, double solutionValue, int cfzSteps, bool plot)
{
    // do nothing for invalid positions
    if (solutionPosition < 0 || solutionValue < 0)
        return;

    if (!plot)
    {
        if (CFZ)
            CFZ->setVisible(false);
    }
    else
    {
        if (!CFZ)
            CFZ = new QCPItemBracket(this);

        CFZ->left->setType(QCPItemPosition::ptPlotCoords);
        CFZ->right->setType(QCPItemPosition::ptPlotCoords);

        double y;
        if (m_Minimum)
            y = (7 * minValue - maxValue) / 6;
        else
            y = (maxValue + 2 * minValue) / 3;

        CFZ->left->setCoords(solutionPosition + cfzSteps / 2.0, y);
        CFZ->right->setCoords(solutionPosition - cfzSteps / 2.0, y);
        CFZ->setLength(15);
        CFZ->setAntialiased(false);
        CFZ->setPen(QPen(QColor(Qt::yellow)));
        CFZ->setVisible(true);
    }
    replot();
}

void FocusHFRVPlot::drawPolynomial(Ekos::PolynomialFit *polyFit, bool isVShape, bool makeVisible, bool plot)
{
    if (polyFit == nullptr)
        return;

    // do nothing if graph is not visible and should not be made as such
    if(makeVisible)
        m_polynomialGraphIsVisible = true;
    else if (m_polynomialGraphIsVisible == false)
        return;

    setSolutionVShape(isVShape);
    if (polynomialGraph != nullptr)
    {
        polynomialGraph->data()->clear();
        QCPRange range = xAxis->range();
        double interval = range.size() / 20.0;

        for(double x = range.lower ; x < range.upper ; x += interval)
        {
            double y = getDisplayValue(polyFit->f(x));
            polynomialGraph->addData(x, y);
        }
        if (plot) replot();
    }
}

void FocusHFRVPlot::drawCurve(Ekos::CurveFitting *curveFit, bool isVShape, bool makeVisible, bool plot)
{
    if (curveFit == nullptr)
        return;

    // do nothing if graph is not visible and should not be made as such
    if(makeVisible)
        m_polynomialGraphIsVisible = true;
    else if (!makeVisible || !m_polynomialGraphIsVisible)
        return;

    setSolutionVShape(isVShape);
    if (polynomialGraph != nullptr)
    {
        polynomialGraph->data()->clear();
        QCPRange range = xAxis->range();
        double interval = range.size() / 20.0;

        for(double x = range.lower ; x < range.upper ; x += interval)
        {
            double y = getDisplayValue(curveFit->f(x));
            polynomialGraph->addData(x, y);
        }
        if (plot) replot();
    }
}

void FocusHFRVPlot::redraw(Ekos::PolynomialFit *polyFit, double solutionPosition, double solutionValue)
{
    if (m_value.empty() == false)
        drawHFRPlot(m_value.last(), 0);

    drawPolynomial(polyFit, m_isVShape, false);
    drawMinimum(solutionPosition, solutionValue);
}

void FocusHFRVPlot::redrawCurve(Ekos::CurveFitting *curveFit, double solutionPosition, double solutionValue)
{
    if (m_value.empty() == false)
        drawHFRPlot(solutionValue, 0);

    drawCurve(curveFit, m_isVShape, false);
    drawMinimum(solutionPosition, solutionValue);
}

void FocusHFRVPlot::setBasicFontSize(int basicFontSize)
{
    m_basicFontSize = basicFontSize;

    // Axis Labels Settings
    yAxis->setLabelFont(QFont(font().family(), basicFontSize));
    xAxis->setTickLabelFont(QFont(font().family(), (int) std::round(0.9 * basicFontSize)));
    yAxis->setTickLabelFont(QFont(font().family(), (int) std::round(0.9 * basicFontSize)));
}

// Internally calculations are done in units of pixels for HFR and FWHM
// If user preference is arcsecs then convert values for display purposes.
double FocusHFRVPlot::getDisplayValue(const double value)
{
    return value * m_starUnits;
}


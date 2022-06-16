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

    yAxis->setLabel(i18n("HFR"));

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
                    double halfFluxRadius = v_graph->dataMainValue(positionKey);
                    QToolTip::showText(
                        event->globalPos(),
                        i18nc("HFR graphics tooltip; %1 is the Focus Position; %2 is the Half Flux Radius;",
                              "<table>"
                              "<tr><td>POS:   </td><td>%1</td></tr>"
                              "<tr><td>HFR:   </td><td>%2</td></tr>"
                              "</table>",
                              QString::number(focusPosition, 'f', 0),
                              QString::number(halfFluxRadius, 'f', 2)));
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
    if (useErrorBars)
    {
        errorBars->removeFromLegend();
        errorBars->setAntialiased(false);
        errorBars->setDataPlottable((this)->v_graph);
        errorBars->setPen(QPen(QColor(180, 180, 180)));
    }

    // Put the sample number inside the plot point's circle.
    for (int i = 0; i < hfr_position.size(); ++i)
    {
        QCPItemText *textLabel = new QCPItemText(this);
        textLabel->setPositionAlignment(Qt::AlignCenter | Qt::AlignHCenter);
        textLabel->position->setType(QCPItemPosition::ptPlotCoords);
        textLabel->position->setCoords(hfr_position[i], hfr_value[i]);
        textLabel->setText(QString::number(i + 1));
        textLabel->setFont(QFont(font().family(), (int) std::round(1.2 * basicFontSize())));
        textLabel->setPen(Qt::NoPen);
        textLabel->setColor(Qt::red);

        if (useErrorBars)
            err.push_front(hfr_sigma[i]);
    }
    // Setup the error bar data if we're using it
    errorBars->setVisible(useErrorBars);
    if (useErrorBars)
        errorBars->setData(err);
}

void FocusHFRVPlot::init(bool showPosition)
{
    m_showPositions = showPosition;
    hfr_position.clear();
    hfr_value.clear();
    hfr_sigma.clear();
    polynomialGraph->data()->clear();
    focusPoint->data().clear();
    useErrorBars = false;
    errorBars->data().clear();
    // the next step seems necessary (QCP bug?)
    v_graph->setData(QVector<double> {}, QVector<double> {});
    focusPoint->setData(QVector<double> {}, QVector<double> {});
    m_polynomialGraphIsVisible = false;
    m_isVShape = false;
    maxHFR = -1;
    FocusHFRVPlot::clearItems();
    replot();
}

void FocusHFRVPlot::drawHFRPlot(double currentHFR, int pulseDuration)
{
    // DrawHFRPlot is the base on which other things are built upon.
    // Clear any previous annotations.
    FocusHFRVPlot::clearItems();

    v_graph->setData(hfr_position, hfr_value);
    drawHFRIndices();

    if (maxHFR < currentHFR || maxHFR < 0)
        maxHFR = currentHFR;

    double minHFRVal = currentHFR / 2.5;
    if (hfr_value.size() > 0)
        minHFRVal = std::max(0.0, *std::min_element(hfr_value.begin(), hfr_value.end()));

    // True for the position-based algorithms and those that simulate position.
    if (m_showPositions)
    {
        const double minPosition = hfr_position.empty() ?
                                   0 : *std::min_element(hfr_position.constBegin(), hfr_position.constEnd());
        const double maxPosition = hfr_position.empty() ?
                                   1e6 : *std::max_element(hfr_position.constBegin(), hfr_position.constEnd());
        xAxis->setRange(minPosition - pulseDuration, maxPosition + pulseDuration);
    }
    else
    {
        //xAxis->setLabel(i18n("Iteration"));
        xAxis->setRange(1, hfr_value.count() + 1);
    }

    if (hfr_value.size() == 1)
        // 1 point gets placed in the middle vertically.
        yAxis->setRange(0, 2 * maxHFR);
    else
    {
        // Allow about 20% of the plot space below the curve, so user can see the min points.
        const double upper = 1.1 * maxHFR;
        yAxis->setRange(std::max(0.0, minHFRVal - (0.25 * (upper - minHFRVal))), upper);
    }
    replot();
}

void FocusHFRVPlot::addPosition(double pos, double newHFR, int pulseDuration, bool plot)
{
    hfr_position.append(pos);
    hfr_value.append(newHFR);
    useErrorBars = false;
    if (plot)
        drawHFRPlot(newHFR, pulseDuration);
}

void FocusHFRVPlot::addPositionWithSigma(double pos, double newHFR, double sigma, int pulseDuration, bool plot)
{
    hfr_position.append(pos);
    hfr_value.append(newHFR);
    hfr_sigma.append(sigma);
    useErrorBars = true;
    if (plot)
        drawHFRPlot(newHFR, pulseDuration);
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

void FocusHFRVPlot::updateTitle(const QString &title, bool plot)
{
    // Update a previous set title without having to redraw everything
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
}

void FocusHFRVPlot::drawMinimum(double solutionPosition, double solutionValue, bool plot)
{
    focusPoint->data()->clear();

    // do nothing for invalid positions
    if (solutionPosition < 0)
        return;

    focusPoint->addData(solutionPosition, solutionValue);
    QCPItemText *textLabel = new QCPItemText(this);
    textLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    textLabel->setColor(Qt::red);
    textLabel->setPadding(QMargins(0, 0, 0, 0));
    textLabel->setBrush(Qt::white);
    textLabel->setPen(Qt::NoPen);
    textLabel->setFont(QFont(font().family(), (int) std::round(0.8 * basicFontSize())));
    textLabel->position->setType(QCPItemPosition::ptPlotCoords);
    textLabel->position->setCoords(solutionPosition, (maxHFR + 2 * solutionValue) / 3);
    textLabel->setText(QString::number(solutionPosition, 'f', 0));
    if (plot) replot();
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
            double y = polyFit->f(x);
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
            double y = curveFit->f(x);
            polynomialGraph->addData(x, y);
        }
        if (plot) replot();
    }
}

void FocusHFRVPlot::redraw(Ekos::PolynomialFit *polyFit, double solutionPosition, double solutionValue)
{
    if (hfr_value.empty() == false)
        drawHFRPlot(hfr_value.last(), 0);

    drawPolynomial(polyFit, m_isVShape, false);
    drawMinimum(solutionPosition, solutionValue);
}

void FocusHFRVPlot::redrawCurve(Ekos::CurveFitting *curveFit, double solutionPosition, double solutionValue)
{
    if (hfr_value.empty() == false)
        drawHFRPlot(hfr_value.last(), 0);

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


/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsstretchui.h"
#include "fitsview.h"
#include "fitsdata.h"
#include "Options.h"
#include "stretch.h"

#include <KMessageBox>

namespace
{

const char kStretchOffToolTip[] = "Stretch the image";
const char kStretchOnToolTip[] = "Disable stretching of the image.";

// The midtones slider works logarithmically (otherwise the useful range of the slider would
// be only way on the left. So these functions translate from a linear slider value
// logarithmically in the 0-1 range, assuming the slider  varies from 1 to 10000.
constexpr double HISTO_SLIDER_MAX = 10000.0;
constexpr double HISTO_SLIDER_FACTOR = 5.0;
double midValueFcn(int x)
{
    return pow(10, -(HISTO_SLIDER_FACTOR - (x / (HISTO_SLIDER_MAX / HISTO_SLIDER_FACTOR))));
}
int invertMidValueFcn(double x)
{
    return (int) 0.5 + (HISTO_SLIDER_MAX / HISTO_SLIDER_FACTOR) * (HISTO_SLIDER_FACTOR + log10(x));
}

// These are defaults for the histogram plot's QCustomPlot axes.
void setupAxisDefaults(QCPAxis *axis)
{
    axis->setBasePen(QPen(Qt::white, 1));
    axis->grid()->setPen(QPen(QColor(140, 140, 140, 140), 1, Qt::DotLine));
    axis->grid()->setSubGridPen(QPen(QColor(40, 40, 40), 1, Qt::DotLine));
    axis->grid()->setZeroLinePen(Qt::NoPen);
    axis->setBasePen(QPen(Qt::white, 1));
    axis->setTickPen(QPen(Qt::white, 1));
    axis->setSubTickPen(QPen(Qt::white, 1));
    axis->setTickLabelColor(Qt::white);
    axis->setLabelColor(Qt::white);
    axis->grid()->setVisible(true);
}
}

FITSStretchUI::FITSStretchUI(const QSharedPointer<FITSView> &view, QWidget * parent) : QWidget(parent)
{
    setupUi(this);
    m_View = view;
    setupButtons();
    setupHistoPlot();
    setupHistoSlider();
    setupConnections();
}

void FITSStretchUI::setupButtons()
{
    stretchButton->setIcon(QIcon::fromTheme("transform-move"));
    toggleHistoButton->setIcon(QIcon::fromTheme("histogram-symbolic"));
    autoButton->setIcon(QIcon::fromTheme("tools-wizard"));
    setupPresetText(true);
}

void FITSStretchUI::setupHistoPlot()
{
    histoPlot->setBackground(QBrush(QColor(25, 25, 25)));
    setupAxisDefaults(histoPlot->yAxis);
    setupAxisDefaults(histoPlot->xAxis);
    histoPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    histoPlot->setMaximumHeight(75);
    histoPlot->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    histoPlot->setVisible(false);

    connect(histoPlot, &QCustomPlot::mouseDoubleClick, this, &FITSStretchUI::onHistoDoubleClick);
    connect(histoPlot, &QCustomPlot::mouseMove, this, &FITSStretchUI::onHistoMouseMove);
}

void FITSStretchUI::onHistoDoubleClick(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (!m_View || !m_View->imageData() || !m_View->imageData()->isHistogramConstructed()) return;
    const double histogramSize = m_View->imageData()->getHistogramBinCount();
    histoPlot->xAxis->setRange(0, histogramSize + 1);
    histoPlot->replot();
}

// This creates 1-channel or RGB tooltips on this histogram.
void FITSStretchUI::onHistoMouseMove(QMouseEvent *event)
{
    const auto image = m_View->imageData();
    if (!image->isHistogramConstructed())
        return;

    const bool rgbHistogram = (image->channels() > 1);
    const int numPixels = image->width() * image->height();
    const int histogramSize = image->getHistogramBinCount();
    const int histoBin = std::max(0, std::min(histogramSize - 1,
                                  static_cast<int>(histoPlot->xAxis->pixelToCoord(event->x()))));

    QString tip = "";
    if (histoBin >= 0 && histoBin < histogramSize)
    {
        for (int c = 0; c < image->channels(); ++c)
        {
            const QVector<double> &intervals = image->getHistogramIntensity(c);
            const double lowRange = intervals[histoBin];
            const double highRange = lowRange + image->getHistogramBinWidth(c);

            if (rgbHistogram)
                tip.append(QString("<font color=\"%1\">").arg(c == 0 ? "red" : (c == 1) ? "lightgreen" : "lightblue"));

            if (image->getMax(c) > 1.1)
                tip.append(QString("%1 %2 %3: ").arg(lowRange, 0, 'f', 0).arg(QChar(0x2192)).arg(highRange, 0, 'f', 0));
            else
                tip.append(QString("%1 %2 %3: ").arg(lowRange, 0, 'f', 4).arg(QChar(0x2192)).arg(highRange, 0, 'f', 4));

            const int count = image->getHistogramFrequency(c)[histoBin];
            const double percentage = count * 100.0 / (double) numPixels;
            tip.append(QString("%1 %2%").arg(count).arg(percentage, 0, 'f', 2));
            if (rgbHistogram)
                tip.append("</font><br/>");
        }
    }
    if (tip.size() > 0)
        QToolTip::showText(event->globalPos(), tip, nullptr, QRect(), 10000);
}

void FITSStretchUI::setupHistoSlider()
{
    histoSlider->setOrientation(Qt::Horizontal);
    histoSlider->setMinimum(0);
    histoSlider->setMaximum(HISTO_SLIDER_MAX);
    histoSlider->setMinimumPosition(0);
    histoSlider->setMaximumPosition(HISTO_SLIDER_MAX);
    histoSlider->setMidPosition(HISTO_SLIDER_MAX / 2);
    histoSlider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    connect(histoSlider, &ctk3Slider::minimumPositionChanged, this, [ = ](int value)
    {
        StretchParams params = m_View->getStretchParams();
        const double shadowValue = value / HISTO_SLIDER_MAX;
        if (shadowValue != params.grey_red.shadows)
        {
            params.grey_red.shadows = shadowValue;

            // In this and below callbacks, we allow for "stretchPreviewSampling". That is,
            // it downsamples the image when the slider is moving, to allow slower computers
            // to render faster, so that the slider can adjust the image in real time.
            // When the mouse is released, a final render at full resolution will be done.
            m_View->setPreviewSampling(Options::stretchPreviewSampling());
            m_View->setStretchParams(params);
            m_View->setPreviewSampling(0);

            // The min and max sliders draw cursors, corresponding to their positions, on
            // this histogram plot.
            setCursors(params);
            histoPlot->replot();
        }
    });
    connect(histoSlider, &ctk3Slider::maximumPositionChanged, this, [ = ](int value)
    {
        StretchParams params = m_View->getStretchParams();
        const double highValue = value / HISTO_SLIDER_MAX;
        if (highValue != params.grey_red.highlights)
        {
            params.grey_red.highlights = highValue;
            m_View->setPreviewSampling(Options::stretchPreviewSampling());
            m_View->setStretchParams(params);
            m_View->setPreviewSampling(0);
            setCursors(params);
            histoPlot->replot();
        }
    });
    connect(histoSlider, &ctk3Slider::midPositionChanged, this, [ = ](int value)
    {
        StretchParams params = m_View->getStretchParams();
        const double midValue = midValueFcn(value);
        if (midValue != params.grey_red.midtones)
        {
            params.grey_red.midtones = midValue;
            m_View->setPreviewSampling(Options::stretchPreviewSampling());
            m_View->setStretchParams(params);
            m_View->setPreviewSampling(0);
        }
    });

    // We need this released callback since if Options::stretchPreviewSampling() is > 1,
    // then when the sliders are dragged, the stretched image is rendered in lower resolution.
    // However when the dragging is done (and the mouse is released) we want to end by rendering
    // in full resolution.
    connect(histoSlider, &ctk3Slider::released, this, [ = ](int minValue, int midValue, int maxValue)
    {
        StretchParams params = m_View->getStretchParams();
        const double shadowValue = minValue / HISTO_SLIDER_MAX;
        const double middleValue = midValueFcn(midValue);
        const double highValue = maxValue / HISTO_SLIDER_MAX;

        if (middleValue != params.grey_red.midtones ||
                highValue != params.grey_red.highlights ||
                shadowValue != params.grey_red.shadows)
        {
            params.grey_red.shadows = shadowValue;
            params.grey_red.midtones = middleValue;
            params.grey_red.highlights = highValue;
            m_View->setPreviewSampling(0);
            m_View->setStretchParams(params);
        }
    });
}

// Updates all the widgets in the stretch area to display the view's stretch parameters.
void FITSStretchUI::setStretchUIValues(const StretchParams1Channel &params)
{
    shadowsVal->setValue(params.shadows);
    midtonesVal->setValue(params.midtones);
    highlightsVal->setValue(params.highlights);

    bool stretchActive = m_View->isImageStretched();
    if (stretchActive)
    {
        stretchButton->setChecked(true);
        stretchButton->setToolTip(kStretchOnToolTip);

        if (m_View->getAutoStretch())
        {
            setupPresetText(true);
            autoButton->setEnabled(false);
            autoButton->setIcon(QIcon());
            autoButton->setIconSize(QSize(22, 22));
            autoButton->setToolTip("");
        }
        else
        {
            autoButton->setEnabled(true);
            autoButton->setIcon(QIcon::fromTheme("tools-wizard"));
            autoButton->setIconSize(QSize(22, 22));
            autoButton->setToolTip(i18n("Stretch using preset %1", m_StretchPresetNumber));
            setupPresetText(false);
        }
    }
    else
    {
        stretchButton->setChecked(false);
        stretchButton->setToolTip(kStretchOffToolTip);

        setupPresetText(false);
        autoButton->setEnabled(false);
        autoButton->setIcon(QIcon());
        autoButton->setIconSize(QSize(22, 22));
        autoButton->setToolTip("");
    }

    autoButton->setChecked(m_View->getAutoStretch());

    // Disable most of the UI if stretching is not active.
    shadowsVal->setEnabled(stretchActive);
    shadowsLabel->setEnabled(stretchActive);
    midtonesVal->setEnabled(stretchActive);
    midtonesLabel->setEnabled(stretchActive);
    highlightsVal->setEnabled(stretchActive);
    highlightsLabel->setEnabled(stretchActive);
    histoSlider->setEnabled(stretchActive);
}

void FITSStretchUI::setupConnections()
{
    connect(m_View.get(), &FITSView::mouseOverPixel, this, [ this ](int x, int y)
    {
        if (pixelCursors.size() != m_View->imageData()->channels())
            pixelCursors.fill(nullptr, m_View->imageData()->channels());

        if (!m_View || !m_View->imageData() || !m_View->imageData()->isHistogramConstructed()) return;
        auto image = m_View->imageData();
        const int nChannels = m_View->imageData()->channels();
        for (int c = 0; c < nChannels; ++c)
        {
            if (pixelCursors[c] != nullptr)
            {
                histoPlot->removeItem(pixelCursors[c]);
                pixelCursors[c] = nullptr;
            }
            if (x < 0 || y < 0 || x >= m_View->imageData()->width() ||
                    y >= m_View->imageData()->height())
                continue;
            int32_t bin = image->histogramBin(x, y, c);
            QColor color = Qt::darkGray;
            if (nChannels > 1)
                color = c == 0 ? QColor(255, 10, 65) : ((c == 1) ? QColor(144, 238, 144, 225) : QColor(173, 216, 230, 175));

            pixelCursors[c] = setCursor(bin, QPen(color, 2, Qt::SolidLine));
        }
        histoPlot->replot();
    });

    connect(highlightsVal, &QDoubleSpinBox::editingFinished, this, [ this ]()
    {
        StretchParams params = m_View->getStretchParams();
        params.grey_red.highlights = highlightsVal->value();
        setCursors(params);
        m_View->setStretchParams(params);
        histoSlider->setMaximumValue(params.grey_red.highlights * HISTO_SLIDER_MAX);
        histoPlot->replot();
    });

    connect(midtonesVal, &QDoubleSpinBox::editingFinished, this, [ this ]()
    {
        StretchParams params = m_View->getStretchParams();
        params.grey_red.midtones = midtonesVal->value();
        setCursors(params);
        m_View->setStretchParams(params);
        histoSlider->setMidValue(invertMidValueFcn(params.grey_red.midtones));
        histoPlot->replot();
    });

    connect(shadowsVal, &QDoubleSpinBox::editingFinished, this, [ this ]()
    {
        StretchParams params = m_View->getStretchParams();
        params.grey_red.shadows = shadowsVal->value();
        setCursors(params);
        m_View->setStretchParams(params);
        histoSlider->setMinimumValue(params.grey_red.shadows * HISTO_SLIDER_MAX);
        histoPlot->replot();
    });

    connect(stretchButton, &QPushButton::clicked, this, [ = ]()
    {
        setupPresetText(!m_View->isImageStretched());

        // This will toggle whether we're currently stretching.
        m_View->setStretch(!m_View->isImageStretched());
    });

    connect(autoButton, &QPushButton::clicked, this, [ = ]()
    {
        // If we're not currently using automatic stretch parameters, turn that on.
        // If we're already using automatic parameters, don't do anything.
        // User can just move the sliders to take manual control.
        if (!m_View->getAutoStretch())
        {
            m_View->setAutoStretchPreset(m_StretchPresetNumber);
            m_View->setAutoStretchParams();
        }
        else
            KMessageBox::information(this, "You are already using automatic stretching. To manually stretch, drag a slider.");
        setStretchUIValues(m_View->getStretchParams().grey_red);
    });

    connect(stretchPreset, &QPushButton::clicked, this, [ = ]()
    {
        m_StretchPresetNumber = Stretch::nextPreset(m_StretchPresetNumber);
        setupPresetText(true);

        m_View->setAutoStretchPreset(m_StretchPresetNumber);
        m_View->setAutoStretchParams();
        setStretchUIValues(m_View->getStretchParams().grey_red);
    });

    connect(toggleHistoButton, &QPushButton::clicked, this, [ = ]()
    {
        histoPlot->setVisible(!histoPlot->isVisible());
    });

    // This is mostly useful right at the start, when the image is displayed without any user interaction.
    // Check for slider-in-use, as we don't wont to rescale while the user is active.
    connect(m_View.get(), &FITSView::newStatus, this, [ = ](const QString & unused)
    {
        Q_UNUSED(unused);
        setStretchUIValues(m_View->getStretchParams().grey_red);
    });

    connect(m_View.get(), &FITSView::newStretch, this, [ = ](const StretchParams & params)
    {
        histoSlider->setMinimumValue(params.grey_red.shadows * HISTO_SLIDER_MAX);
        histoSlider->setMaximumValue(params.grey_red.highlights * HISTO_SLIDER_MAX);
        histoSlider->setMidValue(invertMidValueFcn(params.grey_red.midtones));
    });
}

void FITSStretchUI::setupPresetText(bool enabled)
{
    if (enabled)
    {
        stretchPreset->setEnabled(true);
        const int next = Stretch::nextPreset(m_StretchPresetNumber);
        stretchPreset->setToolTip(
            i18n("Cycle through stretch presets. Current stretch: preset %1. "
                 "Click for preset %2.", m_StretchPresetNumber, next));
        stretchPreset->setText(QString("%1").arg(m_StretchPresetNumber));
    }
    else
    {
        stretchPreset->setEnabled(false);
        stretchPreset->setToolTip("");
        stretchPreset->setText("");
    }
}

namespace
{
// Converts from the position of the min or max slider position (on a 0 to 1.0 scale) to an
// x-axis position on the histogram plot, which varies from 0 to the number of bins in the histogram.
double toHistogramPosition(double position, const QSharedPointer<FITSData> &data)
{
    if (!data->isHistogramConstructed())
        return 0;
    const double size = data->getHistogramBinCount();
    return position * size;
}
}

// Adds a vertical line on the histogram plot (the cursor for the min or max slider).
QCPItemLine * FITSStretchUI::setCursor(int position, const QPen &pen)
{
    QCPItemLine *line = new QCPItemLine(histoPlot);
    line->setPen(pen);
    const double top = histoPlot->yAxis->range().upper;
    const double bottom = histoPlot->yAxis->range().lower;
    line->start->setCoords(position + .5, bottom);
    line->end->setCoords(position + .5, top);
    return line;
}

void FITSStretchUI::setCursors(const StretchParams &params)
{
    const QPen pen(Qt::white, 1, Qt::DotLine);
    removeCursors();
    auto data = m_View->imageData();
    minCursor = setCursor(toHistogramPosition(params.grey_red.shadows, data), pen);
    maxCursor = setCursor(toHistogramPosition(params.grey_red.highlights, data), pen);
}

void FITSStretchUI::removeCursors()
{
    if (minCursor != nullptr)
        histoPlot->removeItem(minCursor);
    minCursor = nullptr;

    if (maxCursor != nullptr)
        histoPlot->removeItem(maxCursor);
    maxCursor = nullptr;
}

void FITSStretchUI::generateHistogram()
{
    if (!m_View->imageData()->isHistogramConstructed())
        m_View->imageData()->constructHistogram();
    if (m_View->imageData()->isHistogramConstructed())
    {
        histoPlot->clearGraphs();
        const int nChannels = m_View->imageData()->channels();
        histoPlot->clearGraphs();
        histoPlot->clearItems();
        for (int i = 0; i < nChannels; ++i)
        {
            histoPlot->addGraph(histoPlot->xAxis, histoPlot->yAxis);
            auto graph = histoPlot->graph(i);
            graph->setLineStyle(QCPGraph::lsStepLeft);
            graph->setVisible(true);
            QColor color = Qt::lightGray;
            if (nChannels > 1)
                color = i == 0 ? QColor(255, 0, 0) : ((i == 1) ? QColor(0, 255, 0, 225) : QColor(0, 0, 255, 175));
            graph->setBrush(QBrush(color));
            graph->setPen(QPen(color));
            const QVector<double> &h = m_View->imageData()->getHistogramFrequency(i);
            const int size = m_View->imageData()->getHistogramBinCount();
            for (int j = 0; j < size; ++j)
                graph->addData(j, log1p(h[j]));
        }
        histoPlot->rescaleAxes();
        histoPlot->xAxis->setRange(0, m_View->imageData()->getHistogramBinCount() + 1);
    }

    histoPlot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);
    histoPlot->axisRect()->setRangeZoomAxes(histoPlot->xAxis, 0);
    histoPlot->axisRect()->setRangeDragAxes(histoPlot->xAxis, 0);
    histoPlot->xAxis->setTickLabels(false);
    histoPlot->yAxis->setTickLabels(false);

    // This controls the x-axis zoom in/out on the histogram plot.
    // It doesn't allow the x-axis to go less than 0, or more than the number of histogram bins.
    connect(histoPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this,
            [ = ](const QCPRange & newRange)
    {
        if (!m_View || !m_View->imageData() || !m_View->imageData()->isHistogramConstructed()) return;
        const double histogramSize = m_View->imageData()->getHistogramBinCount();
        double tLower = newRange.lower;
        double tUpper = newRange.upper;
        if (tLower < 0) tLower = 0;
        if (tUpper > histogramSize + 1) tUpper = histogramSize + 1;
        if (tLower != newRange.lower || tUpper != newRange.upper)
            histoPlot->xAxis->setRange(tLower, tUpper);

        histoSlider->setMinimum(std::max(0.0, HISTO_SLIDER_MAX * tLower / histogramSize));
        histoSlider->setMaximum(std::min((double)HISTO_SLIDER_MAX, HISTO_SLIDER_MAX * tUpper / histogramSize));
    });
}

void FITSStretchUI::setStretchValues(double shadows, double midtones, double highlights)
{
    StretchParams params = m_View->getStretchParams();
    params.grey_red.shadows = shadows;
    params.grey_red.midtones = midtones;
    params.grey_red.highlights = highlights;
    setCursors(params);
    m_View->setPreviewSampling(0);
    m_View->setStretchParams(params);
    histoSlider->setMinimumValue(params.grey_red.shadows * HISTO_SLIDER_MAX);
    histoSlider->setMidValue(invertMidValueFcn(params.grey_red.midtones));
    histoSlider->setMaximumValue(params.grey_red.highlights * HISTO_SLIDER_MAX);
    histoPlot->replot();
}

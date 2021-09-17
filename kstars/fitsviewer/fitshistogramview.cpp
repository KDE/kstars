/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitshistogramview.h"

#include "fits_debug.h"

#include "fitsdata.h"
#include "fitstab.h"
#include "fitsview.h"
#include "fitsviewer.h"

#include <KMessageBox>

#include <QtConcurrent>
#include <type_traits>

FITSHistogramView::FITSHistogramView(QWidget *parent) : QCustomPlot(parent)
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

    connect(this, &QCustomPlot::mouseMove, this, &FITSHistogramView::driftMouseOverLine);

    m_HistogramIntensity.resize(3);
    m_HistogramFrequency.resize(3);
    for (int i = 0; i < 3; i++)
    {
        m_HistogramIntensity[i].resize(256);
        for (int j = 0; j < 256; j++)
            m_HistogramIntensity[i][j] = j;
        m_HistogramFrequency[i].resize(256);
    }
}

void FITSHistogramView::showEvent(QShowEvent * event)
{
    Q_UNUSED(event)
    if (m_ImageData.isNull())
        return;
    if (!m_ImageData->isHistogramConstructed())
    {
        if (m_Linear)
            m_ImageData->constructHistogram();
        else
            createNonLinearHistogram();
    }
    syncGUI();
}

void FITSHistogramView::reset()
{
    isGUISynced = false;
}

void FITSHistogramView::syncGUI()
{
    if (isGUISynced)
        return;

    bool isColor = m_ImageData->channels() > 1;

    clearGraphs();
    graphs.clear();

    for (int n = 0; n < m_ImageData->channels(); n++)
    {
        graphs.append(addGraph());

        if (!m_Linear)
        {
            graphs[n]->setData(m_HistogramIntensity[n], m_HistogramFrequency[n]);
            numDecimals << 0;
        }
        else
        {
            graphs[n]->setData(m_ImageData->getHistogramIntensity(n), m_ImageData->getHistogramFrequency(n));

            double median = m_ImageData->getMedian(n);

            if (median > 100)
                numDecimals << 0;
            else if (median > 1)
                numDecimals << 2;
            else if (median > .01)
                numDecimals << 4;
            else if (median > .0001)
                numDecimals << 6;
            else
                numDecimals << 10;
        }
    }

    graphs[RED_CHANNEL]->setBrush(QBrush(QColor(170, 40, 80)));
    graphs[RED_CHANNEL]->setPen(QPen(Qt::red));

    if (isColor)
    {
        graphs[GREEN_CHANNEL]->setBrush(QBrush(QColor(80, 40, 170)));
        graphs[GREEN_CHANNEL]->setPen(QPen(Qt::green));

        graphs[BLUE_CHANNEL]->setBrush(QBrush(QColor(170, 40, 80)));
        graphs[BLUE_CHANNEL]->setPen(QPen(Qt::blue));
    }

    axisRect(0)->setRangeDrag(Qt::Horizontal);
    axisRect(0)->setRangeZoom(Qt::Horizontal);

    if (m_AxesLabelEnabled)
    {
        xAxis->setLabel(i18n("Intensity"));
        yAxis->setLabel(i18n("Frequency"));
    }

    //    xAxis->setRange(fits_min - ui->minEdit->singleStep(),
    //                                fits_max + ui->maxEdit->singleStep());

    xAxis->rescale();
    yAxis->rescale();

    setInteraction(QCP::iRangeDrag, true);
    setInteraction(QCP::iRangeZoom, true);
    setInteraction(QCP::iSelectPlottables, true);

    replot();
    resizePlot();

    isGUISynced = true;
}

void FITSHistogramView::resizePlot()
{
    if (width() < 300)
        yAxis->setTickLabels(false);
    else
        yAxis->setTickLabels(true);
    xAxis->ticker()->setTickCount(width() / 100);
}

void FITSHistogramView::driftMouseOverLine(QMouseEvent * event)
{
    double intensity = xAxis->pixelToCoord(event->localPos().x());

    uint8_t channels = m_ImageData->channels();
    QVector<double> freq(3, -1);

    if (m_Linear)
    {
        QVector<bool> inRange(3, false);
        for (int n = 0; n < channels; n++)
        {
            if (intensity >= m_ImageData->getMin(n) && intensity <= m_ImageData->getMax(n))
                inRange[n] = true;
        }

        if ( (channels == 1 && inRange[0] == false) || (!inRange[0] && !inRange[1] && !inRange[2]) )
        {
            QToolTip::hideText();
            return;
        }
    }

    if (xAxis->range().contains(intensity))
    {
        for (int n = 0; n < channels; n++)
        {
            int index = graphs[n]->findBegin(intensity, true);
            freq[n] = graphs[n]->dataMainValue(index);
        }

        if (channels == 1 && freq[0] > 0)
        {
            QToolTip::showText(
                event->globalPos(),
                i18nc("Histogram tooltip; %1 is intensity; %2 is frequency;",
                      "<table>"
                      "<tr><td>Intensity:   </td><td>%1</td></tr>"
                      "<tr><td>R Frequency:   </td><td>%2</td></tr>"
                      "</table>",
                      QString::number(intensity, 'f', numDecimals[0]),
                      QString::number(freq[0], 'f', 0)));
        }
        else if (freq[1] > 0)
        {
            QToolTip::showText(
                event->globalPos(),
                i18nc("Histogram tooltip; %1 is intensity; %2 is frequency;",
                      "<table>"
                      "<tr><td>Intensity:   </td><td>%1</td></tr>"
                      "<tr><td>R Frequency:   </td><td>%2</td></tr>"
                      "<tr><td>G Frequency:   </td><td>%3</td></tr>"
                      "<tr><td>B Frequency:   </td><td>%4</td></tr>"
                      "</table>",
                      QString::number(intensity, 'f', numDecimals[0]),
                      QString::number(freq[0], 'f', 0),
                      QString::number(freq[1], 'f', 0),
                      QString::number(freq[2], 'f', 0)));
        }
        else
            QToolTip::hideText();

        replot();
    }
}

void FITSHistogramView::setImageData(const QSharedPointer<FITSData> &data)
{
    m_ImageData = data;

    connect(m_ImageData.data(), &FITSData::dataChanged, [this]()
    {
        if (m_Linear)
        {
            m_ImageData->resetHistogram();
            m_ImageData->constructHistogram();
        }
        else
            createNonLinearHistogram();
        isGUISynced = false;
        syncGUI();
    });
}

void FITSHistogramView::createNonLinearHistogram()
{
    isGUISynced = false;

    int width = m_ImageData->width();
    int height = m_ImageData->height();

    const uint8_t channels = m_ImageData->channels();

    QImage rawImage;
    if (channels == 1)
    {
        rawImage = QImage(width, height, QImage::Format_Indexed8);

        rawImage.setColorCount(256);
        for (int i = 0; i < 256; i++)
            rawImage.setColor(i, qRgb(i, i, i));
    }
    else
    {
        rawImage = QImage(width, height, QImage::Format_RGB32);
    }

    Stretch stretch(width, height, m_ImageData->channels(), m_ImageData->dataType());
    // Compute new auto-stretch params.
    StretchParams stretchParams = stretch.computeParams(m_ImageData->getImageBuffer());

    stretch.setParams(stretchParams);
    stretch.run(m_ImageData->getImageBuffer(), &rawImage);

    m_HistogramFrequency[0].fill(0);
    if (channels > 1)
    {
        m_HistogramFrequency[1].fill(0);
        m_HistogramFrequency[2].fill(0);
    }
    uint32_t samples = width * height;
    const uint32_t sampleBy = (samples > 1000000 ? samples / 1000000 : 1);
    if (channels == 1)
    {
        for (int h = 0; h < height; h += sampleBy)
        {
            auto * scanLine = rawImage.scanLine(h);
            for (int w = 0; w < width; w += sampleBy)
                m_HistogramFrequency[0][scanLine[w]] += sampleBy;
        }
    }
    for (int h = 0; h < height; h += sampleBy)
    {
        auto * scanLine = reinterpret_cast<const QRgb *>((rawImage.scanLine(h)));
        for (int w = 0; w < width; w += sampleBy)
        {
            m_HistogramFrequency[0][qRed(scanLine[w])] += sampleBy;
            m_HistogramFrequency[1][qGreen(scanLine[w])] += sampleBy;
            m_HistogramFrequency[2][qBlue(scanLine[w])] += sampleBy;
        }
    }

    syncGUI();

}

/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitshistogram.h"

#include "fits_debug.h"

#include "Options.h"
#include "fitsdata.h"
#include "fitstab.h"
#include "fitsview.h"
#include "fitsviewer.h"

#include <KMessageBox>

#include <QtConcurrent>
#include <type_traits>
#include <zlib.h>

histogramUI::histogramUI(QDialog * parent) : QDialog(parent)
{
    setupUi(parent);
    setModal(false);
}

FITSHistogram::FITSHistogram(QWidget * parent) : QDialog(parent)
{
    ui = new histogramUI(this);
    tab = dynamic_cast<FITSTab *>(parent);

    customPlot = ui->histogramPlot;

    customPlot->setBackground(QBrush(Qt::black));

    customPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    customPlot->yAxis->setBasePen(QPen(Qt::white, 1));

    customPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    customPlot->yAxis->setTickPen(QPen(Qt::white, 1));

    customPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    customPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));

    customPlot->xAxis->setTickLabelColor(Qt::white);
    customPlot->yAxis->setTickLabelColor(Qt::white);

    customPlot->xAxis->setLabelColor(Qt::white);
    customPlot->yAxis->setLabelColor(Qt::white);

    // Reserve 3 channels
    cumulativeFrequency.resize(3);
    intensity.resize(3);
    frequency.resize(3);

    FITSMin.fill(0, 3);
    FITSMax.fill(0, 3);
    binWidth.fill(0, 3);

    rgbWidgets.resize(3);
    rgbWidgets[RED_CHANNEL] << ui->RLabel << ui->minREdit << ui->redSlider
                            << ui->maxREdit;
    rgbWidgets[GREEN_CHANNEL] << ui->GLabel << ui->minGEdit << ui->greenSlider
                              << ui->maxGEdit;
    rgbWidgets[BLUE_CHANNEL] << ui->BLabel << ui->minBEdit << ui->blueSlider
                             << ui->maxBEdit;

    minBoxes << ui->minREdit << ui->minGEdit << ui->minBEdit;
    maxBoxes << ui->maxREdit << ui->maxGEdit << ui->maxBEdit;
    sliders << ui->redSlider << ui->greenSlider << ui->blueSlider;

    customPlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    customPlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    customPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    customPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    customPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    customPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);

    connect(ui->applyB, &QPushButton::clicked, this, &FITSHistogram::applyScale);
    connect(ui->hideSaturated, &QCheckBox::stateChanged, [this]()
    {
        constructHistogram();
    });

    //    connect(customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), this,
    //            SLOT(checkRangeLimit(QCPRange)));
    connect(customPlot, &QCustomPlot::mouseMove, this,
            &FITSHistogram::driftMouseOverLine);

    for (int i = 0; i < 3; i++)
    {
        // Box --> Slider
        QVector<QWidget *> w = rgbWidgets[i];
        connect(qobject_cast<QDoubleSpinBox *>(w[1]), &QDoubleSpinBox::editingFinished, [this, i, w]()
        {
            double value = qobject_cast<QDoubleSpinBox *>(w[1])->value();
            w[2]->blockSignals(true);
            qobject_cast<ctkRangeSlider *>(w[2])->setMinimumPosition((value - FITSMin[i])*sliderScale[i]);
            w[2]->blockSignals(false);
        });
        connect(qobject_cast<QDoubleSpinBox *>(w[3]), &QDoubleSpinBox::editingFinished, [this, i, w]()
        {
            double value = qobject_cast<QDoubleSpinBox *>(w[3])->value();
            w[2]->blockSignals(true);
            qobject_cast<ctkRangeSlider *>(w[2])->setMaximumPosition((value - FITSMin[i] - sliderTick[i])*sliderScale[i]);
            w[2]->blockSignals(false);
        });

        // Slider --> Box
        connect(qobject_cast<ctkRangeSlider *>(w[2]), &ctkRangeSlider::minimumValueChanged, [this, i, w](int position)
        {
            qobject_cast<QDoubleSpinBox *>(w[1])->setValue(FITSMin[i] + (position / sliderScale[i]));
        });
        connect(qobject_cast<ctkRangeSlider *>(w[2]), &ctkRangeSlider::maximumValueChanged, [this, i, w](int position)
        {
            qobject_cast<QDoubleSpinBox *>(w[3])->setValue(FITSMin[i] + sliderTick[i] + (position / sliderScale[i]));
        });
    }

}

void FITSHistogram::showEvent(QShowEvent * event)
{
    Q_UNUSED(event)
    if (!m_Constructed)
        constructHistogram();
    syncGUI();
}

void FITSHistogram::constructHistogram()
{
    const QSharedPointer<FITSData> &imageData = tab->getView()->imageData();

    isGUISynced = false;

    switch (imageData->getStatistics().dataType)
    {
        case TBYTE:
            constructHistogram<uint8_t>();
            break;

        case TSHORT:
            constructHistogram<int16_t>();
            break;

        case TUSHORT:
            constructHistogram<uint16_t>();
            break;

        case TLONG:
            constructHistogram<int32_t>();
            break;

        case TULONG:
            constructHistogram<uint32_t>();
            break;

        case TFLOAT:
            constructHistogram<float>();
            break;

        case TLONGLONG:
            constructHistogram<int64_t>();
            break;

        case TDOUBLE:
            constructHistogram<double>();
            break;

        default:
            break;
    }

    m_Constructed = true;
    if (isVisible())
        syncGUI();
}

template <typename T> void FITSHistogram::constructHistogram()
{
    const QSharedPointer<FITSData> &imageData = tab->getView()->imageData();
    uint16_t width = imageData->width(), height = imageData->height();
    uint8_t channels = imageData->channels();

    auto * const buffer = reinterpret_cast<T const *>(imageData->getImageBuffer());

    double min, max;
    for (int i = 0 ; i < 3; i++)
    {
        imageData->getMinMax(&min, &max, i);
        FITSMin[i] = min;
        FITSMax[i] = max;
    }

    uint32_t samples = width * height;
    const uint32_t sampleBy = samples > 1000000 ? samples / 1000000 : 1;

    //binCount = static_cast<uint16_t>(sqrt(samples));
    binCount = qMin(FITSMax[0] - FITSMin[0], 400.0);
    if (binCount <= 0)
        binCount = 400;

    for (int n = 0; n < channels; n++)
    {
        intensity[n].fill(0, binCount);
        frequency[n].fill(0, binCount);
        cumulativeFrequency[n].fill(0, binCount);
        binWidth[n] = (FITSMax[n] - FITSMin[n]) / (binCount - 1);
        // Initialize the median to 0 in case the computation below fails.
        imageData->setMedian(0, n);
    }

    QVector<QFuture<void>> futures;

    for (int n = 0; n < channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            for (int i = 0; i < binCount; i++)
                intensity[n][i] = FITSMin[n] + (binWidth[n] * i);
        }));
    }

    for (int n = 0; n < channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            uint32_t offset = n * samples;

            for (uint32_t i = 0; i < samples; i += sampleBy)
            {
                int32_t id = rint((buffer[i + offset] - FITSMin[n]) / binWidth[n]);
                if (id < 0)
                    id = 0;
                frequency[n][id] += sampleBy;
            }
        }));
    }

    for (QFuture<void> future : futures)
        future.waitForFinished();

    futures.clear();

    for (int n = 0; n < channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            uint32_t accumulator = 0;
            for (int i = 0; i < binCount; i++)
            {
                accumulator += frequency[n][i];
                cumulativeFrequency[n].replace(i, accumulator);
            }
        }));
    }

    for (QFuture<void> future : futures)
        future.waitForFinished();

    futures.clear();

    for (int n = 0; n < channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            double median[3] = {0};
            const bool cutoffSpikes = ui->hideSaturated->isChecked();
            const uint32_t halfCumulative = cumulativeFrequency[n][binCount - 1] / 2;

            // Find which bin contains the median.
            int median_bin = -1;
            for (int i = 0; i < binCount; i++)
            {
                if (cumulativeFrequency[n][i] > halfCumulative)
                {
                    median_bin = i;
                    break;
                }
            }

            if (median_bin >= 0)
            {
                // The number of items in the median bin
                const uint32_t median_bin_size = frequency[n][median_bin] / sampleBy;
                if (median_bin_size > 0)
                {
                    // The median is this element inside the sorted median_bin;
                    const uint32_t samples_before_median_bin = median_bin == 0 ? 0 : cumulativeFrequency[n][median_bin - 1];
                    uint32_t median_position = (halfCumulative - samples_before_median_bin) / sampleBy;

                    if (median_position >= median_bin_size)
                        median_position = median_bin_size - 1;
                    if (median_position >= 0 && median_position < median_bin_size)
                    {
                        // Fill a vector with the values in the median bin (sampled by sampleBy).
                        std::vector<T> median_bin_samples(median_bin_size);
                        uint32_t upto = 0;
                        const uint32_t offset = n * samples;
                        for (uint32_t i = 0; i < samples; i += sampleBy)
                        {
                            if (upto >= median_bin_size) break;
                            const int32_t id = rint((buffer[i + offset] - FITSMin[n]) / binWidth[n]);
                            if (id == median_bin)
                                median_bin_samples[upto++] = buffer[i + offset];
                        }
                        // Get the Nth value using N = the median position.
                        if (upto > 0)
                        {
                            if (median_position >= upto) median_position = upto - 1;
                            std::nth_element(median_bin_samples.begin(), median_bin_samples.begin() + median_position,
                                             median_bin_samples.begin() + upto);
                            median[n] = median_bin_samples[median_position];
                        }
                    }
                }
            }

            imageData->setMedian(median[n], n);

            if (cutoffSpikes)
            {
                QVector<double> sortedFreq = frequency[n];
                std::sort(sortedFreq.begin(), sortedFreq.end());
                double cutoff = sortedFreq[binCount * 0.99];
                for (int i = 0; i < binCount; i++)
                {
                    if (frequency[n][i] >= cutoff)
                        frequency[n][i] = cutoff;
                }
            }

        }));
    }

    for (QFuture<void> future : futures)
        future.waitForFinished();

    // Custom index to indicate the overall contrast of the image
    if (cumulativeFrequency[RED_CHANNEL][binCount / 4] > 0)
        JMIndex = cumulativeFrequency[RED_CHANNEL][binCount / 8] / static_cast<double>(cumulativeFrequency[RED_CHANNEL][binCount /
                  4]);
    else
        JMIndex = 1;
    qCDebug(KSTARS_FITS) << "FITHistogram: JMIndex " << JMIndex;

    sliderTick.clear();
    sliderScale.clear();
    for (int n = 0; n < channels; n++)
    {
        sliderTick  << fabs(FITSMax[n] - FITSMin[n]) / 99.0;
        sliderScale << 99.0 / (FITSMax[n] - FITSMin[n] - sliderTick[n]);
    }
}

void FITSHistogram::syncGUI()
{
    if (isGUISynced)
        return;

    const QSharedPointer<FITSData> &imageData = tab->getView()->imageData();
    bool isColor = imageData->channels() > 1;
    // R/K is always enabled
    for (auto w : rgbWidgets[RED_CHANNEL])
        w->setEnabled(true);
    // G Channel
    for (auto w : rgbWidgets[GREEN_CHANNEL])
        w->setEnabled(isColor);
    // B Channel
    for (auto w : rgbWidgets[BLUE_CHANNEL])
        w->setEnabled(isColor);

    ui->meanEdit->setText(QString::number(imageData->getMean()));
    ui->medianEdit->setText(QString::number(imageData->getMedian()));

    for (int n = 0; n < imageData->channels(); n++)
    {
        double median = imageData->getMedian(n);

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

        minBoxes[n]->setDecimals(numDecimals[n]);
        minBoxes[n]->setSingleStep(fabs(FITSMax[n] - FITSMin[n]) / 20.0);
        minBoxes[n]->setMinimum(FITSMin[n]);
        minBoxes[n]->setMaximum(FITSMax[n] - sliderTick[n]);
        minBoxes[n]->setValue(FITSMin[n] + (sliders[n]->minimumValue() / sliderScale[n]));

        maxBoxes[n]->setDecimals(numDecimals[n]);
        maxBoxes[n]->setSingleStep(fabs(FITSMax[n] - FITSMin[n]) / 20.0);
        maxBoxes[n]->setMinimum(FITSMin[n] + sliderTick[n]);
        maxBoxes[n]->setMaximum(FITSMax[n]);
        maxBoxes[n]->setValue(FITSMin[n] + sliderTick[n] + (sliders[n]->maximumValue() / sliderScale[n]));
    }

    customPlot->clearGraphs();
    graphs.clear();

    for (int n = 0; n < imageData->channels(); n++)
    {
        graphs.append(customPlot->addGraph());
        graphs[n]->setData(intensity[n], frequency[n]);
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

    customPlot->axisRect(0)->setRangeDrag(Qt::Horizontal);
    customPlot->axisRect(0)->setRangeZoom(Qt::Horizontal);

    customPlot->xAxis->setLabel(i18n("Intensity"));
    customPlot->yAxis->setLabel(i18n("Frequency"));

    //    customPlot->xAxis->setRange(fits_min - ui->minEdit->singleStep(),
    //                                fits_max + ui->maxEdit->singleStep());

    customPlot->xAxis->rescale();
    customPlot->yAxis->rescale();

    customPlot->setInteraction(QCP::iRangeDrag, true);
    customPlot->setInteraction(QCP::iRangeZoom, true);
    customPlot->setInteraction(QCP::iSelectPlottables, true);

    customPlot->replot();
    resizePlot();

    isGUISynced = true;
}

void FITSHistogram::resizePlot()
{
    if (!m_Constructed)
        constructHistogram();

    if (customPlot->width() < 300)
        customPlot->yAxis->setTickLabels(false);
    else
        customPlot->yAxis->setTickLabels(true);
    customPlot->xAxis->ticker()->setTickCount(customPlot->width() / 100);
}

double FITSHistogram::getJMIndex() const
{
    return JMIndex;
}

void FITSHistogram::applyScale()
{
    QVector<double> min, max;

    min << minBoxes[0]->value() << minBoxes[1]->value() <<  minBoxes[2]->value();
    max << maxBoxes[0]->value() << maxBoxes[1]->value() << maxBoxes[2]->value();

    FITSHistogramCommand * histC;

    if (ui->logR->isChecked())
        type = FITS_LOG;
    else
        type = FITS_LINEAR;

    histC = new FITSHistogramCommand(tab, this, type, min, max);

    tab->getUndoStack()->push(histC);
}

void FITSHistogram::applyFilter(FITSScale ftype)
{
    QVector<double> min, max;

    min.append(ui->minREdit->value());

    FITSHistogramCommand * histC;

    type = ftype;

    histC = new FITSHistogramCommand(tab, this, type, min, max);

    tab->getUndoStack()->push(histC);
}

QVector<uint32_t> FITSHistogram::getCumulativeFrequency(int channel) const
{
    return cumulativeFrequency[channel];
}

FITSHistogramCommand::FITSHistogramCommand(QWidget * parent,
        FITSHistogram * inHisto,
        FITSScale newType,
        const QVector<double> &lmin,
        const QVector<double> &lmax)
{
    tab = dynamic_cast<FITSTab *>(parent);
    type = newType;
    histogram = inHisto;
    min = lmin;
    max = lmax;
}

FITSHistogramCommand::~FITSHistogramCommand()
{
    delete[] delta;
}

bool FITSHistogramCommand::calculateDelta(const uint8_t * buffer)
{
    const QSharedPointer<FITSData> &imageData = tab->getView()->imageData();

    uint8_t const * image_buffer = imageData->getImageBuffer();
    int totalPixels =
        imageData->width() * imageData->height() * imageData->channels();
    unsigned long totalBytes = totalPixels * imageData->getBytesPerPixel();

    auto * raw_delta = new uint8_t[totalBytes];

    if (raw_delta == nullptr)
    {
        qWarning() << "Error! not enough memory to create image delta" << endl;
        return false;
    }

    for (unsigned int i = 0; i < totalBytes; i++)
        raw_delta[i] = buffer[i] ^ image_buffer[i];

    compressedBytes = sizeof(uint8_t) * totalBytes + totalBytes / 64 + 16 + 3;
    delete[] delta;
    delta = new uint8_t[compressedBytes];

    if (delta == nullptr)
    {
        delete[] raw_delta;
        qCCritical(KSTARS_FITS)
                << "FITSHistogram Error: Ran out of memory compressing delta";
        return false;
    }

    int r = compress2(delta, &compressedBytes, raw_delta, totalBytes, 5);

    if (r != Z_OK)
    {
        delete[] raw_delta;
        /* this should NEVER happen */
        qCCritical(KSTARS_FITS)
                << "FITSHistogram Error: Failed to compress raw_delta";
        return false;
    }

    // qDebug() << "compressed bytes size " << compressedBytes << " bytes" <<
    // endl;

    delete[] raw_delta;

    return true;
}

bool FITSHistogramCommand::reverseDelta()
{
    FITSView * image = tab->getView();
    const QSharedPointer<FITSData> &imageData = image->imageData();
    uint8_t const * image_buffer = (imageData->getImageBuffer());

    int totalPixels =
        imageData->width() * imageData->height() * imageData->channels();
    unsigned long totalBytes = totalPixels * imageData->getBytesPerPixel();

    auto * output_image = new uint8_t[totalBytes];

    if (output_image == nullptr)
    {
        qWarning() << "Error! not enough memory to create output image" << endl;
        return false;
    }

    auto * raw_delta = new uint8_t[totalBytes];

    if (raw_delta == nullptr)
    {
        delete[] output_image;
        qWarning() << "Error! not enough memory to create image delta" << endl;
        return false;
    }

    int r = uncompress(raw_delta, &totalBytes, delta, compressedBytes);
    if (r != Z_OK)
    {
        qCCritical(KSTARS_FITS)
                << "FITSHistogram compression error in reverseDelta()";
        delete[] output_image;
        delete[] raw_delta;
        return false;
    }

    for (unsigned int i = 0; i < totalBytes; i++)
        output_image[i] = raw_delta[i] ^ image_buffer[i];

    imageData->setImageBuffer(output_image);

    delete[] raw_delta;

    return true;
}

void FITSHistogramCommand::redo()
{
    FITSView * image = tab->getView();
    const QSharedPointer<FITSData> &imageData = image->imageData();

    uint8_t const * image_buffer = imageData->getImageBuffer();
    uint8_t * buffer = nullptr;
    unsigned int size =
        imageData->width() * imageData->height() * imageData->channels();
    int BBP = imageData->getBytesPerPixel();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (delta != nullptr)
    {
        FITSImage::Statistic prevStats;
        imageData->saveStatistics(prevStats);

        reverseDelta();

        imageData->restoreStatistics(stats);

        stats = prevStats;
    }
    else
    {
        imageData->saveStatistics(stats);

        // If it's rotation of flip, no need to calculate delta
        if (type >= FITS_ROTATE_CW && type <= FITS_FLIP_V)
        {
            imageData->applyFilter(type);
        }
        else
        {
            buffer = new uint8_t[size * BBP];

            if (buffer == nullptr)
            {
                qWarning()
                        << "Error! not enough memory to create image buffer in redo()"
                        << endl;
                QApplication::restoreOverrideCursor();
                return;
            }

            memcpy(buffer, image_buffer, size * BBP);

            QVector<double> dataMin = min, dataMax = max;
            switch (type)
            {
                case FITS_AUTO:
                case FITS_LINEAR:
                    imageData->applyFilter(FITS_LINEAR, nullptr, &dataMin, &dataMax);
                    break;

                case FITS_LOG:
                    imageData->applyFilter(FITS_LOG, nullptr, &dataMin, &dataMax);
                    break;

                case FITS_SQRT:
                    imageData->applyFilter(FITS_SQRT, nullptr, &dataMin, &dataMax);
                    break;

                default:
                    imageData->applyFilter(type);
                    break;
            }

            calculateDelta(buffer);
            delete[] buffer;
        }
    }

    if (histogram != nullptr)
    {
        histogram->constructHistogram();

        if (tab->getViewer()->isStarsMarked())
            imageData->findStars().waitForFinished();
    }

    image->pushFilter(type);
    image->rescale(ZOOM_KEEP_LEVEL);
    image->updateFrame();

    QApplication::restoreOverrideCursor();
}

void FITSHistogramCommand::undo()
{
    FITSView * image = tab->getView();
    const QSharedPointer<FITSData> &imageData = image->imageData();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (delta != nullptr)
    {
        FITSImage::Statistic prevStats;
        imageData->saveStatistics(prevStats);

        reverseDelta();

        imageData->restoreStatistics(stats);

        stats = prevStats;
    }
    else
    {
        switch (type)
        {
            case FITS_ROTATE_CW:
                imageData->applyFilter(FITS_ROTATE_CCW);
                break;
            case FITS_ROTATE_CCW:
                imageData->applyFilter(FITS_ROTATE_CW);
                break;
            case FITS_FLIP_H:
            case FITS_FLIP_V:
                imageData->applyFilter(type);
                break;
            default:
                break;
        }
    }

    if (histogram != nullptr)
    {
        histogram->constructHistogram();

        if (tab->getViewer()->isStarsMarked())
            imageData->findStars().waitForFinished();
    }

    image->popFilter();
    image->rescale(ZOOM_KEEP_LEVEL);
    image->updateFrame();

    QApplication::restoreOverrideCursor();
}

QString FITSHistogramCommand::text() const
{
    switch (type)
    {
        case FITS_AUTO:
            return i18n("Auto Scale");
        case FITS_LINEAR:
            return i18n("Linear Scale");
        case FITS_LOG:
            return i18n("Logarithmic Scale");
        case FITS_SQRT:
            return i18n("Square Root Scale");

        default:
            if (type - 1 <= FITSViewer::filterTypes.count())
                return FITSViewer::filterTypes.at(type - 1);
            break;
    }

    return i18n("Unknown");
}

void FITSHistogram::driftMouseOverLine(QMouseEvent * event)
{
    double intensity = customPlot->xAxis->pixelToCoord(event->localPos().x());

    const QSharedPointer<FITSData> &imageData = tab->getView()->imageData();
    uint8_t channels = imageData->channels();
    QVector<double> freq(3, -1);

    QVector<bool> inRange(3, false);
    for (int n = 0; n < channels; n++)
    {
        if (intensity >= imageData->getMin(n) && intensity <= imageData->getMax(n))
            inRange[n] = true;
    }

    if ( (channels == 1 && inRange[0] == false) || (!inRange[0] && !inRange[1] && !inRange[2]) )
    {
        QToolTip::hideText();
        return;
    }

    if (customPlot->xAxis->range().contains(intensity))
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

        customPlot->replot();
    }
}

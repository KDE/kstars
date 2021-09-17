/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guidedriftgraph.h"
#include "klocalizedstring.h"
#include "ksnotification.h"
#include "kstarsdata.h"
#include "Options.h"

GuideDriftGraph::GuideDriftGraph(QWidget *parent)
{
    Q_UNUSED(parent);
    // Drift Graph Color Settings
    setBackground(QBrush(Qt::black));
    xAxis->setBasePen(QPen(Qt::white, 1));
    yAxis->setBasePen(QPen(Qt::white, 1));
    xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    xAxis->grid()->setZeroLinePen(Qt::NoPen);
    yAxis->grid()->setZeroLinePen(QPen(Qt::white, 1));
    xAxis->setBasePen(QPen(Qt::white, 1));
    yAxis->setBasePen(QPen(Qt::white, 1));
    yAxis2->setBasePen(QPen(Qt::white, 1));
    xAxis->setTickPen(QPen(Qt::white, 1));
    yAxis->setTickPen(QPen(Qt::white, 1));
    yAxis2->setTickPen(QPen(Qt::white, 1));
    xAxis->setSubTickPen(QPen(Qt::white, 1));
    yAxis->setSubTickPen(QPen(Qt::white, 1));
    yAxis2->setSubTickPen(QPen(Qt::white, 1));
    xAxis->setTickLabelColor(Qt::white);
    yAxis->setTickLabelColor(Qt::white);
    yAxis2->setTickLabelColor(Qt::white);
    xAxis->setLabelColor(Qt::white);
    yAxis->setLabelColor(Qt::white);
    yAxis2->setLabelColor(Qt::white);

    snrAxis = axisRect()->addAxis(QCPAxis::atLeft, 0);
    snrAxis->setVisible(false);
    // This will be reset to the actual data values.
    snrAxis->setRange(-100, 100);

    //Horizontal Axis Time Ticker Settings
    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%m:%s");
    xAxis->setTicker(timeTicker);

    // Axis Labels Settings
    yAxis2->setVisible(true);
    yAxis2->setTickLabels(true);
    yAxis->setLabelFont(QFont(font().family(), 10));
    yAxis2->setLabelFont(QFont(font().family(), 10));
    xAxis->setTickLabelFont(QFont(font().family(), 9));
    yAxis->setTickLabelFont(QFont(font().family(), 9));
    yAxis2->setTickLabelFont(QFont(font().family(), 9));
    yAxis->setLabelPadding(1);
    yAxis2->setLabelPadding(1);
    yAxis->setLabel(i18n("drift (arcsec)"));
    yAxis2->setLabel(i18n("pulse (ms)"));

    setupNSEWLabels();

    int scale =
        50;  //This is a scaling value between the left and the right axes of the driftGraph, it could be stored in kstars kcfg

    //Sets the default ranges
    xAxis->setRange(0, 120, Qt::AlignRight);
    yAxis->setRange(-3, 3);
    yAxis2->setRange(-3 * scale, 3 * scale);

    //This sets up the legend
    legend->setVisible(true);
    legend->setFont(QFont(font().family(), 7));
    legend->setTextColor(Qt::white);
    legend->setBrush(QBrush(Qt::black));
    legend->setIconSize(4, 12);
    legend->setFillOrder(QCPLegend::foColumnsFirst);
    axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft | Qt::AlignBottom);

    // RA Curve
    addGraph(xAxis, yAxis);
    graph(GuideGraph::G_RA)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
    graph(GuideGraph::G_RA)->setName("RA");
    graph(GuideGraph::G_RA)->setLineStyle(QCPGraph::lsStepLeft);

    // DE Curve
    addGraph(xAxis, yAxis);
    graph(GuideGraph::G_DEC)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
    graph(GuideGraph::G_DEC)->setName("DE");
    graph(GuideGraph::G_DEC)->setLineStyle(QCPGraph::lsStepLeft);

    // RA highlighted Point
    addGraph(xAxis, yAxis);
    graph(GuideGraph::G_RA_HIGHLIGHT)->setLineStyle(QCPGraph::lsNone);
    graph(GuideGraph::G_RA_HIGHLIGHT)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
    graph(GuideGraph::G_RA_HIGHLIGHT)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle,
            QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"), 2), QBrush(), 10));

    // DE highlighted Point
    addGraph(xAxis, yAxis);
    graph(GuideGraph::G_DEC_HIGHLIGHT)->setLineStyle(QCPGraph::lsNone);
    graph(GuideGraph::G_DEC_HIGHLIGHT)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
    graph(GuideGraph::G_DEC_HIGHLIGHT)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle,
            QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"), 2), QBrush(), 10));

    // RA Pulse
    addGraph(xAxis, yAxis2);
    QColor raPulseColor(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"));
    raPulseColor.setAlpha(75);
    graph(GuideGraph::G_RA_PULSE)->setPen(QPen(raPulseColor));
    graph(GuideGraph::G_RA_PULSE)->setBrush(QBrush(raPulseColor, Qt::Dense4Pattern));
    graph(GuideGraph::G_RA_PULSE)->setName("RA Pulse");
    graph(GuideGraph::G_RA_PULSE)->setLineStyle(QCPGraph::lsStepLeft);

    // DEC Pulse
    addGraph(xAxis, yAxis2);
    QColor dePulseColor(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"));
    dePulseColor.setAlpha(75);
    graph(GuideGraph::G_DEC_PULSE)->setPen(QPen(dePulseColor));
    graph(GuideGraph::G_DEC_PULSE)->setBrush(QBrush(dePulseColor, Qt::Dense4Pattern));
    graph(GuideGraph::G_DEC_PULSE)->setName("DEC Pulse");
    graph(GuideGraph::G_DEC_PULSE)->setLineStyle(QCPGraph::lsStepLeft);

    // SNR
    addGraph(xAxis, snrAxis);
    graph(GuideGraph::G_SNR)->setPen(QPen(Qt::yellow));
    graph(GuideGraph::G_SNR)->setName("SNR");
    graph(GuideGraph::G_SNR)->setLineStyle(QCPGraph::lsStepLeft);

    // RA RMS
    addGraph(xAxis, yAxis);
    graph(GuideGraph::G_RA_RMS)->setPen(QPen(Qt::red));
    graph(GuideGraph::G_RA_RMS)->setName("RA RMS");
    graph(GuideGraph::G_RA_RMS)->setLineStyle(QCPGraph::lsStepLeft);

    // DEC RMS
    addGraph(xAxis, yAxis);
    graph(GuideGraph::G_DEC_RMS)->setPen(QPen(Qt::red));
    graph(GuideGraph::G_DEC_RMS)->setName("DEC RMS");
    graph(GuideGraph::G_DEC_RMS)->setLineStyle(QCPGraph::lsStepLeft);

    // Total RMS
    addGraph(xAxis, yAxis);
    graph(GuideGraph::G_RMS)->setPen(QPen(Qt::red));
    graph(GuideGraph::G_RMS)->setName("RMS");
    graph(GuideGraph::G_RMS)->setLineStyle(QCPGraph::lsStepLeft);

    //This will prevent the highlighted points and Pulses from showing up in the legend.
    legend->removeItem(GuideGraph::G_DEC_RMS);
    legend->removeItem(GuideGraph::G_RA_RMS);
    legend->removeItem(GuideGraph::G_DEC_PULSE);
    legend->removeItem(GuideGraph::G_RA_PULSE);
    legend->removeItem(GuideGraph::G_DEC_HIGHLIGHT);
    legend->removeItem(GuideGraph::G_RA_HIGHLIGHT);

    setInteractions(QCP::iRangeZoom);
    axisRect()->setRangeZoom(Qt::Orientation::Vertical);
    setInteraction(QCP::iRangeDrag, true);
    //This sets the visibility of graph components to the stored values.
    graph(GuideGraph::G_RA)->setVisible(Options::rADisplayedOnGuideGraph()); //RA data
    graph(GuideGraph::G_DEC)->setVisible(Options::dEDisplayedOnGuideGraph()); //DEC data
    graph(GuideGraph::G_RA_HIGHLIGHT)->setVisible(Options::rADisplayedOnGuideGraph()); //RA highlighted point
    graph(GuideGraph::G_DEC_HIGHLIGHT)->setVisible(Options::dEDisplayedOnGuideGraph()); //DEC highlighted point
    graph(GuideGraph::G_RA_PULSE)->setVisible(Options::rACorrDisplayedOnGuideGraph()); //RA Pulses
    graph(GuideGraph::G_DEC_PULSE)->setVisible(Options::dECorrDisplayedOnGuideGraph()); //DEC Pulses
    graph(GuideGraph::G_SNR)->setVisible(Options::sNRDisplayedOnGuideGraph()); //SNR
    setRMSVisibility();

    updateCorrectionsScaleVisibility();
}

void GuideDriftGraph::guideHistory(int sliderValue, bool graphOnLatestPt)
{
    graph(GuideGraph::G_RA_HIGHLIGHT)->data()->clear(); //Clear RA highlighted point
    graph(GuideGraph::G_DEC_HIGHLIGHT)->data()->clear(); //Clear DEC highlighted point
    double t = graph(GuideGraph::G_RA)->dataMainKey(sliderValue); //Get time from RA data
    double ra = graph(GuideGraph::G_RA)->dataMainValue(sliderValue); //Get RA from RA data
    double de = graph(GuideGraph::G_DEC)->dataMainValue(sliderValue); //Get DEC from DEC data
    double raPulse = graph(GuideGraph::G_RA_PULSE)->dataMainValue(sliderValue); //Get RA Pulse from RA pulse data
    double dePulse = graph(GuideGraph::G_DEC_PULSE)->dataMainValue(sliderValue); //Get DEC Pulse from DEC pulse data
    graph(GuideGraph::G_RA_HIGHLIGHT)->addData(t, ra); //Set RA highlighted point
    graph(GuideGraph::G_DEC_HIGHLIGHT)->addData(t, de); //Set DEC highlighted point

    //This will allow the graph to scroll left and right along with the guide slider
    if (xAxis->range().contains(t) == false)
    {
        if(t < xAxis->range().lower)
        {
            xAxis->setRange(t, t + xAxis->range().size());
        }
        if(t > xAxis->range().upper)
        {
            xAxis->setRange(t - xAxis->range().size(), t);
        }
    }
    replot();
    double snr = 0;
    if (graph(GuideGraph::G_SNR)->data()->size() > 0)
        snr = graph(GuideGraph::G_SNR)->dataMainValue(sliderValue);
    double rms = graph(GuideGraph::G_RMS)->dataMainValue(sliderValue);

    if(!graphOnLatestPt)
    {
        QTime localTime = guideTimer;
        localTime = localTime.addSecs(t);

        QPoint localTooltipCoordinates = graph(GuideGraph::G_RA)->dataPixelPosition(sliderValue).toPoint();
        QPoint globalTooltipCoordinates = mapToGlobal(localTooltipCoordinates);

        if(raPulse == 0 && dePulse == 0)
        {
            QToolTip::showText(
                globalTooltipCoordinates,
                i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds; %4 is the RMS error in arcseconds; %5 is the SNR",
                      "<table>"
                      "<tr><td>LT:   </td><td>%1</td></tr>"
                      "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                      "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                      "<tr><td>RMS:   </td><td>%4 \"</td></tr>"
                      "<tr><td>SNR:   </td><td>%5 \"</td></tr>"
                      "</table>",
                      localTime.toString("hh:mm:ss AP"),
                      QString::number(ra, 'f', 2),
                      QString::number(de, 'f', 2),
                      QString::number(rms, 'f', 2),
                      QString::number(snr, 'f', 1)
                     ));
        }
        else
        {
            QToolTip::showText(
                globalTooltipCoordinates,
                i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds; %4 is the RMS error in arcseconds; %5 is the SNR; %6 is RA Pulse in ms; %7 is DE Pulse in ms",
                      "<table>"
                      "<tr><td>LT:   </td><td>%1</td></tr>"
                      "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                      "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                      "<tr><td>RMS:   </td><td>%4 \"</td></tr>"
                      "<tr><td>SNR:   </td><td>%5 \"</td></tr>"
                      "<tr><td>RA Pulse:   </td><td>%6 ms</td></tr>"
                      "<tr><td>DE Pulse:   </td><td>%7 ms</td></tr>"
                      "</table>",
                      localTime.toString("hh:mm:ss AP"),
                      QString::number(ra, 'f', 2),
                      QString::number(de, 'f', 2),
                      QString::number(rms, 'f', 2),
                      QString::number(snr, 'f', 1),
                      QString::number(raPulse, 'f', 2),
                      QString::number(dePulse, 'f', 2)
                     )); //The pulses were divided by 100 before they were put on the graph.
        }

    }
}

void GuideDriftGraph::handleVerticalPlotSizeChange()
{
}

void GuideDriftGraph::handleHorizontalPlotSizeChange()
{
}

void GuideDriftGraph::setupNSEWLabels()
{
    //Labels for N/S/E/W
    QColor raLabelColor(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"));
    QColor deLabelColor(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"));

    QCPItemText *northLabel = new QCPItemText(this);
    northLabel->setColor(deLabelColor);
    northLabel->setFont(QFont(font().family(), 9));
    northLabel->setText(i18nc("North", "N"));
    northLabel->position->setType(QCPItemPosition::ptViewportRatio);
    northLabel->position->setCoords(0.7, 0.12);
    northLabel->setVisible(true);

    QCPItemText *southLabel = new QCPItemText(this);
    southLabel->setColor(deLabelColor);
    southLabel->setFont(QFont(font().family(), 9));
    southLabel->setText(i18nc("South", "S"));
    southLabel->position->setType(QCPItemPosition::ptViewportRatio);
    southLabel->position->setCoords(0.7, 0.8);
    southLabel->setVisible(true);

    QCPItemText *westLabel = new QCPItemText(this);
    westLabel->setColor(raLabelColor);
    westLabel->setFont(QFont(font().family(), 9));
    westLabel->setText(i18nc("West", "W"));
    westLabel->position->setType(QCPItemPosition::ptViewportRatio);
    westLabel->position->setCoords(0.78, 0.12);
    westLabel->setVisible(true);

    QCPItemText *eastLabel = new QCPItemText(this);
    eastLabel->setColor(raLabelColor);
    eastLabel->setFont(QFont(font().family(), 9));
    eastLabel->setText(i18nc("East", "E"));
    eastLabel->position->setType(QCPItemPosition::ptViewportRatio);
    eastLabel->position->setCoords(0.8, 0.8);
    eastLabel->setVisible(true);

}

void GuideDriftGraph::autoScaleGraphs()
{
    yAxis->setRange(-3, 3);
    // First bool below is only_enlarge, 2nd is only look at values that are visible in X.
    // Net result is all RA & DEC points within the times being plotted should be visible.
    // This is only called when the autoScale button is pressed.
    graph(GuideGraph::G_RA)->rescaleValueAxis(false, true);
    graph(GuideGraph::G_DEC)->rescaleValueAxis(true, true);
    replot();
}

void GuideDriftGraph::zoomX(int zoomLevel)
{
    double key = (guideTimer.isValid() || guideTimer.isNull()) ? 0 : guideTimer.elapsed() / 1000.0;

    // The # of seconds displayd on the x-axis of the drift-graph for the various zoom levels.
    static std::vector<int> zoomLevels = {15, 30, 60, 120, 300, 900, 1800, 3600, 7200, 14400};

    zoomLevel = std::max(0, zoomLevel);
    driftGraphZoomLevel = std::min(static_cast<int>(zoomLevels.size() - 1), zoomLevel);

    xAxis->setRange(key - zoomLevels[driftGraphZoomLevel], key);
}

void GuideDriftGraph::zoomInX()
{
    zoomX(driftGraphZoomLevel - 1);
    replot();
}

void GuideDriftGraph::zoomOutX()
{
    zoomX(driftGraphZoomLevel + 1);
    replot();
}

void GuideDriftGraph::setCorrectionGraphScale(int value)
{
    yAxis2->setRange(yAxis->range().lower * value,
                     yAxis->range().upper * value);
    replot();
}

void GuideDriftGraph::clear()
{
    graph(GuideGraph::G_RA)->data()->clear(); //RA data
    graph(GuideGraph::G_DEC)->data()->clear(); //DEC data
    graph(GuideGraph::G_RA_HIGHLIGHT)->data()->clear(); //RA highlighted point
    graph(GuideGraph::G_DEC_HIGHLIGHT)->data()->clear(); //DEC highlighted point
    graph(GuideGraph::G_RA_PULSE)->data()->clear(); //RA Pulses
    graph(GuideGraph::G_DEC_PULSE)->data()->clear(); //DEC Pulses
    graph(GuideGraph::G_SNR)->data()->clear(); //SNR
    graph(GuideGraph::G_RA_RMS)->data()->clear(); //RA RMS
    graph(GuideGraph::G_DEC_RMS)->data()->clear(); //DEC RMS
    graph(GuideGraph::G_RMS)->data()->clear(); //RMS
    clearItems();  //Clears dither text items from the graph
    setupNSEWLabels();
    replot();
}

void GuideDriftGraph::toggleShowPlot(GuideGraph::DRIFT_GRAPH_INDICES plot, bool isChecked)
{
    switch (plot)
    {
        case GuideGraph::G_RA:
            Options::setRADisplayedOnGuideGraph(isChecked);
            graph(GuideGraph::G_RA)->setVisible(isChecked);
            graph(GuideGraph::G_RA_HIGHLIGHT)->setVisible(isChecked);
            setRMSVisibility();
            replot();
            break;
        case GuideGraph::G_DEC:
            Options::setDEDisplayedOnGuideGraph(isChecked);
            graph(GuideGraph::G_DEC)->setVisible(isChecked);
            graph(GuideGraph::G_DEC_HIGHLIGHT)->setVisible(isChecked);
            setRMSVisibility();
            replot();
            break;
        case GuideGraph::G_RA_PULSE:
            Options::setRACorrDisplayedOnGuideGraph(isChecked);
            graph(GuideGraph::G_RA_PULSE)->setVisible(isChecked);
            updateCorrectionsScaleVisibility();
            break;
        case GuideGraph::G_DEC_PULSE:
            Options::setDECorrDisplayedOnGuideGraph(isChecked);
            graph(GuideGraph::G_DEC_PULSE)->setVisible(isChecked);
            updateCorrectionsScaleVisibility();
            break;
        case GuideGraph::G_SNR:
            Options::setSNRDisplayedOnGuideGraph(isChecked);
            graph(GuideGraph::G_SNR)->setVisible(isChecked);
            replot();
            break;
        case GuideGraph::G_RMS:
            Options::setRMSDisplayedOnGuideGraph(isChecked);
            setRMSVisibility();
            replot();
            break;
        default:
            break;
    }
}

void GuideDriftGraph::setRMSVisibility()
{
    if (!Options::rMSDisplayedOnGuideGraph())
    {
        graph(GuideGraph::G_RA_RMS)->setVisible(false);
        graph(GuideGraph::G_DEC_RMS)->setVisible(false);
        graph(GuideGraph::G_RMS)->setVisible(false);
        return;
    }

    if ((Options::dEDisplayedOnGuideGraph() && Options::rADisplayedOnGuideGraph()) ||
            (!Options::dEDisplayedOnGuideGraph() && !Options::rADisplayedOnGuideGraph()))
    {
        graph(GuideGraph::G_RA_RMS)->setVisible(false);
        graph(GuideGraph::G_DEC_RMS)->setVisible(false);
        graph(GuideGraph::G_RMS)->setVisible(true);
    }
    else if (!Options::dEDisplayedOnGuideGraph() && Options::rADisplayedOnGuideGraph())
    {
        graph(GuideGraph::G_RA_RMS)->setVisible(true);
        graph(GuideGraph::G_DEC_RMS)->setVisible(false);
        graph(GuideGraph::G_RMS)->setVisible(false);
    }
    else
    {
        graph(GuideGraph::G_RA_RMS)->setVisible(false);
        graph(GuideGraph::G_DEC_RMS)->setVisible(true);
        graph(GuideGraph::G_RMS)->setVisible(false);
    }
}

void GuideDriftGraph::exportGuideData()
{
    int numPoints = graph(GuideGraph::G_RA)->dataCount();
    if (numPoints == 0)
        return;

    QUrl exportFile = QFileDialog::getSaveFileUrl(this, i18nc("@title:window", "Export Guide Data"), guideURLPath,
                      "CSV File (*.csv)");
    if (exportFile.isEmpty()) // if user presses cancel
        return;
    if (exportFile.toLocalFile().endsWith(QLatin1String(".csv")) == false)
        exportFile.setPath(exportFile.toLocalFile() + ".csv");

    QString path = exportFile.toLocalFile();

    if (QFile::exists(path))
    {
        int r = KMessageBox::warningContinueCancel(nullptr,
                i18n("A file named \"%1\" already exists. "
                     "Overwrite it?",
                     exportFile.fileName()),
                i18n("Overwrite File?"), KStandardGuiItem::overwrite());
        if (r == KMessageBox::Cancel)
            return;
    }

    if (!exportFile.isValid())
    {
        QString message = i18n("Invalid URL: %1", exportFile.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
        return;
    }

    QFile file;
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return;
    }

    QTextStream outstream(&file);

    outstream <<
              "Frame #, Time Elapsed (sec), Local Time (HMS), RA Error (arcsec), DE Error (arcsec), RA Pulse  (ms), DE Pulse (ms)" <<
              endl;

    for (int i = 0; i < numPoints; i++)
    {
        double t = graph(GuideGraph::G_RA)->dataMainKey(i);
        double ra = graph(GuideGraph::G_RA)->dataMainValue(i);
        double de = graph(GuideGraph::G_DEC)->dataMainValue(i);
        double raPulse = graph(GuideGraph::G_RA_PULSE)->dataMainValue(i);
        double dePulse = graph(GuideGraph::G_DEC_PULSE)->dataMainValue(i);

        QTime localTime = guideTimer;
        localTime = localTime.addSecs(t);

        outstream << i << ',' << t << ',' << localTime.toString("hh:mm:ss AP") << ',' << ra << ',' << de << ',' << raPulse << ',' <<
                  dePulse << ',' << endl;
    }
    file.close();
}

void GuideDriftGraph::resetTimer()
{
    guideTimer = QTime::currentTime();
}

void GuideDriftGraph::connectGuider(Ekos::GuideInterface *guider)
{
    connect(guider, &Ekos::GuideInterface::newAxisDelta, this, &GuideDriftGraph::setAxisDelta);
    connect(guider, &Ekos::GuideInterface::newAxisPulse, this, &GuideDriftGraph::setAxisPulse);
    connect(guider, &Ekos::GuideInterface::newAxisSigma, this, &GuideDriftGraph::setAxisSigma);
    connect(guider, &Ekos::GuideInterface::newSNR, this, &GuideDriftGraph::setSNR);

    resetTimer();
}

void GuideDriftGraph::setAxisDelta(double ra, double de)
{
    // Time since timer started.
    double key = guideTimer.elapsed() / 1000.0;

    // similar to same operation in Guide::setAxisDelta
    ra = -ra;

    graph(GuideGraph::G_RA)->addData(key, ra);
    graph(GuideGraph::G_DEC)->addData(key, de);

    if(graphOnLatestPt)
    {
        xAxis->setRange(key, xAxis->range().size(), Qt::AlignRight);
        graph(GuideGraph::G_RA_HIGHLIGHT)->data()->clear(); //Clear highlighted RA point
        graph(GuideGraph::G_DEC_HIGHLIGHT)->data()->clear(); //Clear highlighted DEC point
        graph(GuideGraph::G_RA_HIGHLIGHT)->addData(key, ra); //Set highlighted RA point to latest point
        graph(GuideGraph::G_DEC_HIGHLIGHT)->addData(key, de); //Set highlighted DEC point to latest point
    }
    replot();
}

void GuideDriftGraph::setAxisSigma(double ra, double de)
{
    const double key = guideTimer.elapsed() / 1000.0;
    const double total = std::hypot(ra, de);
    graph(GuideGraph::G_RA_RMS)->addData(key, ra);
    graph(GuideGraph::G_DEC_RMS)->addData(key, de);
    graph(GuideGraph::G_RMS)->addData(key, total);
}

void GuideDriftGraph::setAxisPulse(double ra, double de)
{
    double key = guideTimer.elapsed() / 1000.0;
    graph(GuideGraph::G_RA_PULSE)->addData(key, ra);
    graph(GuideGraph::G_DEC_PULSE)->addData(key, de);
}

void GuideDriftGraph::setSNR(double snr)
{
    double key = guideTimer.elapsed() / 1000.0;
    graph(GuideGraph::G_SNR)->addData(key, snr);

    // Sets the SNR axis to have the maximum be 95% of the way up from the middle to the top.
    QCPGraphData snrMax = *std::min_element(graph(GuideGraph::G_SNR)->data()->begin(),
                                            graph(GuideGraph::G_SNR)->data()->end(),
                                            [](QCPGraphData const & s1, QCPGraphData const & s2)
    {
        return s1.value > s2.value;
    });
    snrAxis->setRange(-1.05 * snrMax.value, 1.05 * snrMax.value);
}

void GuideDriftGraph::updateCorrectionsScaleVisibility()
{
    bool isVisible = (Options::rACorrDisplayedOnGuideGraph() || Options::dECorrDisplayedOnGuideGraph());
    yAxis2->setVisible(isVisible);
    replot();
}

void GuideDriftGraph::mouseOverLine(QMouseEvent *event)
{
    double key = xAxis->pixelToCoord(event->localPos().x());

    if (xAxis->range().contains(key))
    {
        if (plottableAt(event->pos(), false))
        {
            int raIndex = graph(GuideGraph::G_RA)->findBegin(key);
            int deIndex = graph(GuideGraph::G_DEC)->findBegin(key);
            int rmsIndex = graph(GuideGraph::G_RMS)->findBegin(key);

            double raDelta = graph(GuideGraph::G_RA)->dataMainValue(raIndex);
            double deDelta = graph(GuideGraph::G_DEC)->dataMainValue(deIndex);

            double raPulse = graph(GuideGraph::G_RA_PULSE)->dataMainValue(raIndex); //Get RA Pulse from RA pulse data
            double dePulse = graph(GuideGraph::G_DEC_PULSE)->dataMainValue(deIndex); //Get DEC Pulse from DEC pulse data

            double rms = graph(GuideGraph::G_RMS)->dataMainValue(rmsIndex);
            double snr = 0;
            if (graph(GuideGraph::G_SNR)->data()->size() > 0)
            {
                int snrIndex = graph(GuideGraph::G_SNR)->findBegin(key);
                snr = graph(GuideGraph::G_SNR)->dataMainValue(snrIndex);
            }

            // Compute time value:
            QTime localTime = guideTimer;

            localTime = localTime.addSecs(key);

            QToolTip::hideText();
            if(raPulse == 0 && dePulse == 0)
            {
                QToolTip::showText(
                    event->globalPos(),
                    i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds; %4 is the RMS error in arcseconds; %5 is the SNR",
                          "<table>"
                          "<tr><td>LT:   </td><td>%1</td></tr>"
                          "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                          "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                          "<tr><td>RMS:   </td><td>%4 \"</td></tr>"
                          "<tr><td>SNR:   </td><td>%5 \"</td></tr>"
                          "</table>",
                          localTime.toString("hh:mm:ss AP"),
                          QString::number(raDelta, 'f', 2), QString::number(deDelta, 'f', 2),
                          QString::number(rms, 'f', 2), QString::number(snr, 'f', 1)));
            }
            else
            {
                QToolTip::showText(
                    event->globalPos(),
                    i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds; %4 is the RMS error in arcseconds; %5 is the SNR; %6 is RA Pulse in ms; %7 is DE Pulse in ms",
                          "<table>"
                          "<tr><td>LT:   </td><td>%1</td></tr>"
                          "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                          "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                          "<tr><td>RMS:   </td><td>%4 \"</td></tr>"
                          "<tr><td>SNR:   </td><td>%5 \"</td></tr>"
                          "<tr><td>RA Pulse:   </td><td>%6 ms</td></tr>"
                          "<tr><td>DE Pulse:   </td><td>%7 ms</td></tr>"
                          "</table>",
                          localTime.toString("hh:mm:ss AP"),
                          QString::number(raDelta, 'f', 2),
                          QString::number(deDelta, 'f', 2),
                          QString::number(rms, 'f', 2),
                          QString::number(snr, 'f', 1),
                          QString::number(raPulse, 'f', 2),
                          QString::number(dePulse, 'f', 2))); //The pulses were divided by 100 before they were put on the graph.
            }
        }
        else
            QToolTip::hideText();

        replot();
    }

    if (xAxis->range().contains(key))
    {
        QCPGraph *qcpgraph = qobject_cast<QCPGraph *>(plottableAt(event->pos(), false));

        if (qcpgraph)
        {
            int raIndex = graph(GuideGraph::G_RA)->findBegin(key);
            int deIndex = graph(GuideGraph::G_DEC)->findBegin(key);
            int rmsIndex = graph(GuideGraph::G_RMS)->findBegin(key);

            double raDelta = graph(GuideGraph::G_RA)->dataMainValue(raIndex);
            double deDelta = graph(GuideGraph::G_DEC)->dataMainValue(deIndex);

            double raPulse = graph(GuideGraph::G_RA_PULSE)->dataMainValue(raIndex); //Get RA Pulse from RA pulse data
            double dePulse = graph(GuideGraph::G_DEC_PULSE)->dataMainValue(deIndex); //Get DEC Pulse from DEC pulse data

            double rms = graph(GuideGraph::G_RMS)->dataMainValue(rmsIndex);
            double snr = 0;
            if (graph(GuideGraph::G_SNR)->data()->size() > 0)
            {
                int snrIndex = graph(GuideGraph::G_SNR)->findBegin(key);
                snr = graph(GuideGraph::G_SNR)->dataMainValue(snrIndex);
            }

            // Compute time value:
            QTime localTime = guideTimer;

            localTime = localTime.addSecs(key);

            QToolTip::hideText();
            if(raPulse == 0 && dePulse == 0)
            {
                QToolTip::showText(
                    event->globalPos(),
                    i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds; %4 is the RMS error in arcseconds; %5 is the SNR",
                          "<table>"
                          "<tr><td>LT:   </td><td>%1</td></tr>"
                          "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                          "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                          "<tr><td>RMS:   </td><td>%4 \"</td></tr>"
                          "<tr><td>SNR:   </td><td>%5 \"</td></tr>"
                          "</table>",
                          localTime.toString("hh:mm:ss AP"),
                          QString::number(raDelta, 'f', 2), QString::number(deDelta, 'f', 2),
                          QString::number(rms, 'f', 2), QString::number(snr, 'f', 1)));
            }
            else
            {
                QToolTip::showText(
                    event->globalPos(),
                    i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds; %4 is the RMS error in arcseconds; %5 is the SNR; %6 is RA Pulse in ms; %7 is DE Pulse in ms",
                          "<table>"
                          "<tr><td>LT:   </td><td>%1</td></tr>"
                          "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                          "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                          "<tr><td>RMS:   </td><td>%4 \"</td></tr>"
                          "<tr><td>SNR:   </td><td>%5 \"</td></tr>"
                          "<tr><td>RA Pulse:   </td><td>%6 ms</td></tr>"
                          "<tr><td>DE Pulse:   </td><td>%7 ms</td></tr>"
                          "</table>",
                          localTime.toString("hh:mm:ss AP"),
                          QString::number(raDelta, 'f', 2),
                          QString::number(deDelta, 'f', 2),
                          QString::number(rms, 'f', 2),
                          QString::number(snr, 'f', 1),
                          QString::number(raPulse, 'f', 2),
                          QString::number(dePulse, 'f', 2))); //The pulses were divided by 100 before they were put on the graph.
            }
        }
        else
            QToolTip::hideText();

        replot();
    }
}

void GuideDriftGraph::mouseClicked(QMouseEvent *event)
{
    if (event->buttons() & Qt::RightButton)
    {
        yAxis->setRange(-3, 3);
    }
}

void GuideDriftGraph::refreshColorScheme()
{
    if (graph(GuideGraph::G_RA) && graph(GuideGraph::G_DEC) && graph(GuideGraph::G_RA_HIGHLIGHT)
            && graph(GuideGraph::G_DEC_HIGHLIGHT) && graph(GuideGraph::G_RA_PULSE)
            && graph(GuideGraph::G_DEC_PULSE))
    {
        graph(GuideGraph::G_RA)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
        graph(GuideGraph::G_DEC)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
        graph(GuideGraph::G_RA_HIGHLIGHT)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
        graph(GuideGraph::G_RA_HIGHLIGHT)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle,
                QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"), 2), QBrush(), 10));
        graph(GuideGraph::G_DEC_HIGHLIGHT)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
        graph(GuideGraph::G_DEC_HIGHLIGHT)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle,
                QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"), 2), QBrush(), 10));

        QColor raPulseColor(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"));
        raPulseColor.setAlpha(75);
        graph(GuideGraph::G_RA_PULSE)->setPen(QPen(raPulseColor));
        graph(GuideGraph::G_RA_PULSE)->setBrush(QBrush(raPulseColor, Qt::Dense4Pattern));

        QColor dePulseColor(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"));
        dePulseColor.setAlpha(75);
        graph(GuideGraph::G_DEC_PULSE)->setPen(QPen(dePulseColor));
        graph(GuideGraph::G_DEC_PULSE)->setBrush(QBrush(dePulseColor, Qt::Dense4Pattern));
    }
}

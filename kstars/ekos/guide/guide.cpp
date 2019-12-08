/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guide.h"

#include "guideadaptor.h"
#include "kstars.h"
#include "ksmessagebox.h"
#include "ksnotification.h"
#include "kstarsdata.h"
#include "opscalibration.h"
#include "opsguide.h"
#include "Options.h"
#include "auxiliary/QProgressIndicator.h"
#include "ekos/auxiliary/darklibrary.h"
#include "externalguide/linguider.h"
#include "externalguide/phd2.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsviewer.h"
#include "internalguide/internalguider.h"

#include <KConfigDialog>

#include <basedevice.h>
#include <ekos_guide_debug.h>

#include "ui_manualdither.h"

#define CAPTURE_TIMEOUT_THRESHOLD 30000

namespace Ekos
{
Guide::Guide() : QWidget()
{
    // #1 Setup UI
    setupUi(this);

    // #2 Register DBus
    qRegisterMetaType<Ekos::GuideState>("Ekos::GuideState");
    qDBusRegisterMetaType<Ekos::GuideState>();
    new GuideAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Guide", this);

    // #3 Init Plots
    initPlots();

    // #4 Init View
    initView();

    // #5 Load all settings
    loadSettings();

    // #6 Init Connections
    initConnections();






    // Image Filters
    for (auto &filter : FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    // Progress Indicator
    pi = new QProgressIndicator(this);
    controlLayout->addWidget(pi, 1, 2, 1, 1);

    showFITSViewerB->setIcon(
        QIcon::fromTheme("kstars_fitsviewer"));
    connect(showFITSViewerB, &QPushButton::clicked, this, &Ekos::Guide::showFITSViewer);
    showFITSViewerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    guideAutoScaleGraphB->setIcon(
        QIcon::fromTheme("zoom-fit-best"));
    connect(guideAutoScaleGraphB, &QPushButton::clicked, this, &Ekos::Guide::slotAutoScaleGraphs);
    guideAutoScaleGraphB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    guideSaveDataB->setIcon(
        QIcon::fromTheme("document-save"));
    connect(guideSaveDataB, &QPushButton::clicked, this, &Ekos::Guide::exportGuideData);
    guideSaveDataB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    guideDataClearB->setIcon(
        QIcon::fromTheme("application-exit"));
    connect(guideDataClearB, &QPushButton::clicked, this, &Ekos::Guide::clearGuideGraphs);
    guideDataClearB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    // Exposure
    //Should we set the range for the spin box here?
    QList<double> exposureValues;
    exposureValues << 0.02 << 0.05 << 0.1 << 0.2 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 3.5 << 4 << 4.5 << 5 << 6 << 7 << 8 << 9 << 10 << 15 << 30;
    exposureIN->setRecommendedValues(exposureValues);
    connect(exposureIN, &NonLinearDoubleSpinBox::editingFinished, this, &Ekos::Guide::saveDefaultGuideExposure);





    // Init Internal Guider always
    internalGuider        = new InternalGuider();
    KConfigDialog *dialog = new KConfigDialog(this, "guidesettings", Options::self());
    opsCalibration        = new OpsCalibration(internalGuider);
    KPageWidgetItem *page = dialog->addPage(opsCalibration, i18n("Calibration"));
    page->setIcon(QIcon::fromTheme("tool-measure"));
    opsGuide = new OpsGuide();

    connect(opsGuide, &OpsGuide::settingsUpdated, [this]()
    {
        onThresholdChanged(Options::guideAlgorithm());
        configurePHD2Camera();
    });

    page = dialog->addPage(opsGuide, i18n("Guide"));
    page->setIcon(QIcon::fromTheme("kstars_guides"));

    internalGuider->setGuideView(guideView);

    // Set current guide type
    setGuiderType(-1);

    //This allows the current guideSubframe option to be loaded.
    if(guiderType == GUIDE_PHD2)
        setExternalGuiderBLOBEnabled(!Options::guideSubframeEnabled());

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);
}

Guide::~Guide()
{
    delete guider;
}

void Guide::handleHorizontalPlotSizeChange()
{
    driftPlot->xAxis->setScaleRatio(driftPlot->yAxis, 1.0);
    driftPlot->replot();
}

void Guide::handleVerticalPlotSizeChange()
{
    driftPlot->yAxis->setScaleRatio(driftPlot->xAxis, 1.0);
    driftPlot->replot();
}

void Guide::guideAfterMeridianFlip()
{
    //This will clear the tracking box selection
    //The selected guide star is no longer valid due to the flip
    guideView->setTrackingBoxEnabled(false);
    starCenter = QVector3D();

    if (Options::resetGuideCalibration())
        clearCalibration();
    guide();

}

void Guide::resizeEvent(QResizeEvent *event)
{
    if (event->oldSize().width() != -1)
    {
        if (event->oldSize().width() != size().width())
            handleHorizontalPlotSizeChange();
        else if (event->oldSize().height() != size().height())
            handleVerticalPlotSizeChange();
    }
    else
    {
        QTimer::singleShot(10, this, &Ekos::Guide::handleHorizontalPlotSizeChange);
    }
}

void Guide::buildTarget()
{
    double accuracyRadius = accuracyRadiusSpin->value();
    Options::setGuiderAccuracyThreshold(accuracyRadius);

    if (centralTarget)
    {
        concentricRings->data()->clear();
        redTarget->data()->clear();
        yellowTarget->data()->clear();
        centralTarget->data()->clear();
    }
    else
    {
        concentricRings = new QCPCurve(driftPlot->xAxis, driftPlot->yAxis);
        redTarget       = new QCPCurve(driftPlot->xAxis, driftPlot->yAxis);
        yellowTarget    = new QCPCurve(driftPlot->xAxis, driftPlot->yAxis);
        centralTarget   = new QCPCurve(driftPlot->xAxis, driftPlot->yAxis);
    }
    const int pointCount = 200;
    QVector<QCPCurveData> circleRings(
        pointCount * (5)); //Have to multiply by the number of rings, Rings at : 25%, 50%, 75%, 125%, 175%
    QVector<QCPCurveData> circleCentral(pointCount);
    QVector<QCPCurveData> circleYellow(pointCount);
    QVector<QCPCurveData> circleRed(pointCount);

    int circleRingPt = 0;
    for (int i = 0; i < pointCount; i++)
    {
        double theta = i / static_cast<double>(pointCount) * 2 * M_PI;

        for (double ring = 1; ring < 8; ring++)
        {
            if (ring != 4 && ring != 6)
            {
                if (i % (9 - static_cast<int>(ring)) == 0) //This causes fewer points to draw on the inner circles.
                {
                    circleRings[circleRingPt] = QCPCurveData(circleRingPt, accuracyRadius * ring * 0.25 * qCos(theta),
                                                accuracyRadius * ring * 0.25 * qSin(theta));
                    circleRingPt++;
                }
            }
        }

        circleCentral[i] = QCPCurveData(i, accuracyRadius * qCos(theta), accuracyRadius * qSin(theta));
        circleYellow[i]  = QCPCurveData(i, accuracyRadius * 1.5 * qCos(theta), accuracyRadius * 1.5 * qSin(theta));
        circleRed[i]     = QCPCurveData(i, accuracyRadius * 2 * qCos(theta), accuracyRadius * 2 * qSin(theta));
    }

    concentricRings->setLineStyle(QCPCurve::lsNone);
    concentricRings->setScatterSkip(0);
    concentricRings->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(255, 255, 255, 150), 1));

    concentricRings->data()->set(circleRings, true);
    redTarget->data()->set(circleRed, true);
    yellowTarget->data()->set(circleYellow, true);
    centralTarget->data()->set(circleCentral, true);

    concentricRings->setPen(QPen(Qt::white));
    redTarget->setPen(QPen(Qt::red));
    yellowTarget->setPen(QPen(Qt::yellow));
    centralTarget->setPen(QPen(Qt::green));

    concentricRings->setBrush(Qt::NoBrush);
    redTarget->setBrush(QBrush(QColor(255, 0, 0, 50)));
    yellowTarget->setBrush(
        QBrush(QColor(0, 255, 0, 50))); //Note this is actually yellow.  It is green on top of red with equal opacity.
    centralTarget->setBrush(QBrush(QColor(0, 255, 0, 50)));

    if (driftPlot->size().width() > 0)
        driftPlot->replot();
}

void Guide::clearGuideGraphs()
{
    driftGraph->graph(0)->data()->clear(); //RA data
    driftGraph->graph(1)->data()->clear(); //DEC data
    driftGraph->graph(2)->data()->clear(); //RA highlighted point
    driftGraph->graph(3)->data()->clear(); //DEC highlighted point
    driftGraph->graph(4)->data()->clear(); //RA Pulses
    driftGraph->graph(5)->data()->clear(); //DEC Pulses
    driftPlot->graph(0)->data()->clear(); //Guide data
    driftPlot->graph(1)->data()->clear(); //Guide highlighted point
    driftGraph->clearItems();  //Clears dither text items from the graph
    driftGraph->replot();
    driftPlot->replot();

    //Since the labels got cleared with clearItems above.
    setupNSEWLabels();
}

void Guide::setupNSEWLabels()
{
    //Labels for N/S/E/W
    QColor raLabelColor(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"));
    QColor deLabelColor(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"));

    //DriftGraph
    {
        QCPItemText *northLabel = new QCPItemText(driftGraph);
        northLabel->setColor(deLabelColor);
        northLabel->setText(i18nc("North","N"));
        northLabel->position->setType(QCPItemPosition::ptViewportRatio);
        northLabel->position->setCoords(0.6,0.1);
        northLabel->setVisible(true);

        QCPItemText *southLabel = new QCPItemText(driftGraph);
        southLabel->setColor(deLabelColor);
        southLabel->setText(i18nc("South","S"));
        southLabel->position->setType(QCPItemPosition::ptViewportRatio);
        southLabel->position->setCoords(0.6,0.8);
        southLabel->setVisible(true);

        QCPItemText *westLabel = new QCPItemText(driftGraph);
        westLabel->setColor(raLabelColor);
        westLabel->setText(i18nc("West","W"));
        westLabel->position->setType(QCPItemPosition::ptViewportRatio);
        westLabel->position->setCoords(0.8,0.1);
        westLabel->setVisible(true);

        QCPItemText *eastLabel = new QCPItemText(driftGraph);
        eastLabel->setColor(raLabelColor);
        eastLabel->setText(i18nc("East","E"));
        eastLabel->position->setType(QCPItemPosition::ptViewportRatio);
        eastLabel->position->setCoords(0.8,0.8);
        eastLabel->setVisible(true);

    }

    //DriftPlot
    {
        QCPItemText *northLabel = new QCPItemText(driftPlot);
        northLabel->setColor(deLabelColor);
        northLabel->setText(i18nc("North","N"));
        northLabel->position->setType(QCPItemPosition::ptViewportRatio);
        northLabel->position->setCoords(0.25,0.2);
        northLabel->setVisible(true);

        QCPItemText *southLabel = new QCPItemText(driftPlot);
        southLabel->setColor(deLabelColor);
        southLabel->setText(i18nc("South","S"));
        southLabel->position->setType(QCPItemPosition::ptViewportRatio);
        southLabel->position->setCoords(0.25,0.7);
        southLabel->setVisible(true);

        QCPItemText *westLabel = new QCPItemText(driftPlot);
        westLabel->setColor(raLabelColor);
        westLabel->setText(i18nc("West","W"));
        westLabel->position->setType(QCPItemPosition::ptViewportRatio);
        westLabel->position->setCoords(0.8,0.75);
        westLabel->setVisible(true);

        QCPItemText *eastLabel = new QCPItemText(driftPlot);
        eastLabel->setColor(raLabelColor);
        eastLabel->setText(i18nc("East","E"));
        eastLabel->position->setType(QCPItemPosition::ptViewportRatio);
        eastLabel->position->setCoords(0.3,0.75);
        eastLabel->setVisible(true);
    }
}

void Guide::slotAutoScaleGraphs()
{
    double accuracyRadius = accuracyRadiusSpin->value();

    double key = guideTimer.elapsed() / 1000.0;
    driftGraph->xAxis->setRange(key - 60, key);
    driftGraph->yAxis->setRange(-3, 3);
    driftGraph->graph(0)->rescaleValueAxis(true);
    driftGraph->replot();

    driftPlot->xAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    driftPlot->yAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    driftPlot->graph(0)->rescaleAxes(true);

    driftPlot->yAxis->setScaleRatio(driftPlot->xAxis, 1.0);
    driftPlot->xAxis->setScaleRatio(driftPlot->yAxis, 1.0);

    driftPlot->replot();
}

void Guide::guideHistory()
{
    int sliderValue = guideSlider->value();
    latestCheck->setChecked(sliderValue == guideSlider->maximum() - 1 || sliderValue == guideSlider->maximum());

    driftGraph->graph(2)->data()->clear(); //Clear RA highlighted point
    driftGraph->graph(3)->data()->clear(); //Clear DEC highlighted point
    driftPlot->graph(1)->data()->clear(); //Clear Guide highlighted point
    double t = driftGraph->graph(0)->dataMainKey(sliderValue); //Get time from RA data
    double ra = driftGraph->graph(0)->dataMainValue(sliderValue); //Get RA from RA data
    double de = driftGraph->graph(1)->dataMainValue(sliderValue); //Get DEC from DEC data
    double raPulse = driftGraph->graph(4)->dataMainValue(sliderValue); //Get RA Pulse from RA pulse data
    double dePulse = driftGraph->graph(5)->dataMainValue(sliderValue); //Get DEC Pulse from DEC pulse data
    driftGraph->graph(2)->addData(t, ra); //Set RA highlighted point
    driftGraph->graph(3)->addData(t, de); //Set DEC highlighted point

    //This will allow the graph to scroll left and right along with the guide slider
    if (driftGraph->xAxis->range().contains(t) == false)
    {
        if(t < driftGraph->xAxis->range().lower)
        {
            driftGraph->xAxis->setRange(t, t + driftGraph->xAxis->range().size());
        }
        if(t > driftGraph->xAxis->range().upper)
        {
            driftGraph->xAxis->setRange(t - driftGraph->xAxis->range().size(), t);
        }
    }
    driftGraph->replot();

    driftPlot->graph(1)->addData(ra, de); //Set guide highlighted point
    driftPlot->replot();

    if(!graphOnLatestPt)
    {
        QTime localTime = guideTimer;
        localTime = localTime.addSecs(t);

        QPoint localTooltipCoordinates = driftGraph->graph(0)->dataPixelPosition(sliderValue).toPoint();
        QPoint globalTooltipCoordinates = driftGraph->mapToGlobal(localTooltipCoordinates);

        if(raPulse == 0 && dePulse == 0)
        {
            QToolTip::showText(
                globalTooltipCoordinates,
                i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds",
                      "<table>"
                      "<tr><td>LT:   </td><td>%1</td></tr>"
                      "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                      "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                      "</table>",
                      localTime.toString("hh:mm:ss AP"), QString::number(ra, 'f', 2),
                      QString::number(de, 'f', 2)));
        }
        else
        {
            QToolTip::showText(
                globalTooltipCoordinates,
                i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds; %4 is RA Pulse in ms; %5 is DE Pulse in ms",
                      "<table>"
                      "<tr><td>LT:   </td><td>%1</td></tr>"
                      "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                      "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                      "<tr><td>RA Pulse:   </td><td>%4 ms</td></tr>"
                      "<tr><td>DE Pulse:   </td><td>%5 ms</td></tr>"
                      "</table>",
                      localTime.toString("hh:mm:ss AP"), QString::number(ra, 'f', 2),
                      QString::number(de, 'f', 2), QString::number(raPulse, 'f', 2), QString::number(dePulse, 'f', 2))); //The pulses were divided by 100 before they were put on the graph.
        }

    }
}

void Guide::setLatestGuidePoint(bool isChecked)
{
    graphOnLatestPt = isChecked;
    if(isChecked)
        guideSlider->setValue(guideSlider->maximum());
}

void Guide::toggleShowRAPlot(bool isChecked)
{
    Options::setRADisplayedOnGuideGraph(isChecked);
    driftGraph->graph(0)->setVisible(isChecked);
    driftGraph->graph(2)->setVisible(isChecked);
    driftGraph->replot();
}

void Guide::toggleShowDEPlot(bool isChecked)
{
    Options::setDEDisplayedOnGuideGraph(isChecked);
    driftGraph->graph(1)->setVisible(isChecked);
    driftGraph->graph(3)->setVisible(isChecked);
    driftGraph->replot();
}

void Guide::toggleRACorrectionsPlot(bool isChecked)
{
    Options::setRACorrDisplayedOnGuideGraph(isChecked);
    driftGraph->graph(4)->setVisible(isChecked);
    updateCorrectionsScaleVisibility();
}

void Guide::toggleDECorrectionsPlot(bool isChecked)
{
    Options::setDECorrDisplayedOnGuideGraph(isChecked);
    driftGraph->graph(5)->setVisible(isChecked);
    updateCorrectionsScaleVisibility();
}

void Guide::updateCorrectionsScaleVisibility()
{
    bool isVisible = (Options::rACorrDisplayedOnGuideGraph() || Options::dECorrDisplayedOnGuideGraph());
    driftGraph->yAxis2->setVisible(isVisible);
    correctionSlider->setVisible(isVisible);
    driftGraph->replot();
}

void Guide::setCorrectionGraphScale()
{
    driftGraph->yAxis2->setRange(driftGraph->yAxis->range().lower * correctionSlider->value(), driftGraph->yAxis->range().upper * correctionSlider->value());
    driftGraph->replot();
}

void Guide::exportGuideData()
{
    int numPoints = driftGraph->graph(0)->dataCount();
    if (numPoints == 0)
        return;

    QUrl exportFile = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Export Guide Data"), guideURLPath,
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

    outstream << "Frame #, Time Elapsed (sec), Local Time (HMS), RA Error (arcsec), DE Error (arcsec), RA Pulse  (ms), DE Pulse (ms)" << endl;

    for (int i = 0; i < numPoints; i++)
    {
        double t = driftGraph->graph(0)->dataMainKey(i);
        double ra = driftGraph->graph(0)->dataMainValue(i);
        double de = driftGraph->graph(1)->dataMainValue(i);
        double raPulse = driftGraph->graph(4)->dataMainValue(i);
        double dePulse = driftGraph->graph(5)->dataMainValue(i);

        QTime localTime = guideTimer;
        localTime = localTime.addSecs(t);

        outstream << i << ',' << t << ',' << localTime.toString("hh:mm:ss AP") << ',' << ra << ',' << de << ',' << raPulse << ',' << dePulse << ',' << endl;
    }
    appendLogText(i18n("Guide Data Saved as: %1", path));
    file.close();
}

QString Guide::setRecommendedExposureValues(QList<double> values)
{
    exposureIN->setRecommendedValues(values);
    return exposureIN->getRecommendedValuesString();
}

void Guide::addCamera(ISD::GDInterface *newCCD)
{
    ISD::CCD *ccd = static_cast<ISD::CCD *>(newCCD);

    if (CCDs.contains(ccd))
        return;
    if(guiderType != GUIDE_INTERNAL)
    {
        connect(ccd, &ISD::CCD::newBLOBManager, [ccd, this](INDI::Property * prop)
        {
            if (!strcmp(prop->getName(), "CCD1") ||  !strcmp(prop->getName(), "CCD2"))
            {
                ccd->setBLOBEnabled(false); //This will disable PHD2 external guide frames until it is properly connected.
                currentCCD = ccd;
            }
        });
        guiderCombo->clear();
        guiderCombo->setEnabled(false);
        if (guiderType == GUIDE_PHD2)
            guiderCombo->addItem("PHD2");
        else
            guiderCombo->addItem("LinGuider");
    }
    else
    {
        guiderCombo->setEnabled(true);
        guiderCombo->addItem(ccd->getDeviceName());
    }

    CCDs.append(ccd);
    checkCCD();
    configurePHD2Camera();
}

void Guide::configurePHD2Camera()
{
    //Maybe something like this can be done for Linguider?
    //But for now, Linguider doesn't support INDI Cameras
    if(guiderType != GUIDE_PHD2)
        return;
    //This prevents a crash if phd2guider is null
    if(!phd2Guider)
        return;
    //This way it doesn't check if the equipment isn't connected yet.
    //It will check again when the equipment is connected.
    if(!phd2Guider->isConnected())
        return;
    //This way it doesn't check if the equipment List has not been received yet.
    //It will ask for the list.  When the list is received it will check again.
    if(phd2Guider->getCurrentCamera().isEmpty())
    {
        phd2Guider->requestCurrentEquipmentUpdate();
        return;
    }

    //this checks to see if a CCD in the list matches the name of PHD2's camera
    ISD::CCD *ccdMatch = nullptr;
    QString currentPHD2CameraName = "None";
    foreach(ISD::CCD *ccd, CCDs)
    {
        if(phd2Guider->getCurrentCamera().contains(ccd->getDeviceName()))
        {
            ccdMatch = ccd;
            currentPHD2CameraName = (phd2Guider->getCurrentCamera());
            break;
        }
    }

    //If this method gives the same result as last time, no need to update the Camera info again.
    //That way the user doesn't see a ton of messages printing about the PHD2 external camera.
    //But lets make sure the blob is set correctly every time.
    if(lastPHD2CameraName == currentPHD2CameraName)
    {
        setExternalGuiderBLOBEnabled(!Options::guideSubframeEnabled());
        return;
    }

    //This means that a Guide Camera was connected before but it changed.
    if(currentCCD)
        setExternalGuiderBLOBEnabled(false);

    //Updating the currentCCD
    currentCCD = ccdMatch;

    //This updates the last camera name for the next time it is checked.
    lastPHD2CameraName = currentPHD2CameraName;

    //This sets a boolean that allows you to tell if the PHD2 camera is in Ekos
    phd2Guider->setCurrentCameraIsNotInEkos(currentCCD == nullptr);

    if(phd2Guider->isCurrentCameraNotInEkos())
    {
        appendLogText(i18n("PHD2's current camera: %1, is NOT connected to Ekos.  The PHD2 Guide Star Image will be received, but the full external guide frames cannot.", phd2Guider->getCurrentCamera()));
        subFrameCheck->setEnabled(false);
        //We don't want to actually change the user's subFrame Setting for when a camera really is connected, just check the box to tell the user.
        disconnect(subFrameCheck, &QCheckBox::toggled, this, &Ekos::Guide::setSubFrameEnabled);
        subFrameCheck->setChecked(true);
        return;
    }

    appendLogText(i18n("PHD2's current camera: %1, IS connected to Ekos.  You can select whether to use the full external guide frames or just receive the PHD2 Guide Star Image using the SubFrame checkbox.", phd2Guider->getCurrentCamera()));
    subFrameCheck->setEnabled(true);
    connect(subFrameCheck, &QCheckBox::toggled, this, &Ekos::Guide::setSubFrameEnabled);
    subFrameCheck->setChecked(Options::guideSubframeEnabled());
}

void Guide::addGuideHead(ISD::GDInterface *newCCD)
{
    if (guiderType != GUIDE_INTERNAL)
        return;

    ISD::CCD *ccd = static_cast<ISD::CCD *>(newCCD);

    CCDs.append(ccd);

    QString guiderName = ccd->getDeviceName() + QString(" Guider");

    if (guiderCombo->findText(guiderName) == -1)
    {
        guiderCombo->addItem(guiderName);
        //CCDs.append(static_cast<ISD::CCD *> (newCCD));
    }

    //checkCCD(CCDs.count()-1);
    //guiderCombo->setCurrentIndex(CCDs.count()-1);

    //setGuiderProcess(Options::useEkosGuider() ? GUIDE_INTERNAL : GUIDE_PHD2);
}

void Guide::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = dynamic_cast<ISD::Telescope *>(newTelescope);

    syncTelescopeInfo();
}

bool Guide::setCamera(const QString &device)
{
    if (guiderType != GUIDE_INTERNAL)
        return true;

    for (int i = 0; i < guiderCombo->count(); i++)
        if (device == guiderCombo->itemText(i))
        {
            guiderCombo->setCurrentIndex(i);
            checkCCD(i);
            return true;
        }

    return false;
}

QString Guide::camera()
{
    if (currentCCD)
        return currentCCD->getDeviceName();

    return QString();
}

void Guide::checkCCD(int ccdNum)
{
    if (guiderType != GUIDE_INTERNAL)
        return;

    if (ccdNum == -1)
    {
        ccdNum = guiderCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum <= CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        if (currentCCD->hasGuideHead() && guiderCombo->currentText().contains("Guider"))
            useGuideHead = true;
        else
            useGuideHead = false;

        ISD::CCDChip *targetChip =
            currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip && targetChip->isCapturing())
            return;

        if (guiderType != GUIDE_INTERNAL)
        {
            syncCCDInfo();
            return;
        }

        //connect(currentCCD, SIGNAL(FITSViewerClosed()), this, &Ekos::Guide::viewerClosed()), Qt::UniqueConnection);
        connect(currentCCD, &ISD::CCD::numberUpdated, this, &Ekos::Guide::processCCDNumber, Qt::UniqueConnection);
        connect(currentCCD, &ISD::CCD::newExposureValue, this, &Ekos::Guide::checkExposureValue, Qt::UniqueConnection);

        targetChip->setImageView(guideView, FITS_GUIDE);

        syncCCDInfo();
    }
}

void Guide::syncCCDInfo()
{
    INumberVectorProperty *nvp = nullptr;

    if (currentCCD == nullptr)
        return;

    if (useGuideHead)
        nvp = currentCCD->getBaseDevice()->getNumber("GUIDER_INFO");
    else
        nvp = currentCCD->getBaseDevice()->getNumber("CCD_INFO");

    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_X");
        if (np)
            ccdPixelSizeX = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np)
            ccdPixelSizeY = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np)
            ccdPixelSizeY = np->value;
    }

    updateGuideParams();
}

void Guide::setTelescopeInfo(double primaryFocalLength, double primaryAperture, double guideFocalLength, double guideAperture)
{
    if (primaryFocalLength > 0)
        focal_length = primaryFocalLength;
    if (primaryAperture > 0)
        aperture = primaryAperture;
    // If we have guide scope info, always prefer that over primary
    if (guideFocalLength > 0)
        focal_length = guideFocalLength;
    if (guideAperture > 0)
        aperture = guideAperture;

    updateGuideParams();
}

void Guide::syncTelescopeInfo()
{
    if (currentTelescope == nullptr || currentTelescope->isConnected() == false)
        return;

    INumberVectorProperty *nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "TELESCOPE_APERTURE");

        if (np && np->value > 0)
            primaryAperture = np->value;

        np = IUFindNumber(nvp, "GUIDER_APERTURE");
        if (np && np->value > 0)
            guideAperture = np->value;

        aperture = primaryAperture;

        //if (currentCCD && currentCCD->getTelescopeType() == ISD::CCD::TELESCOPE_GUIDE)
        if (FOVScopeCombo->currentIndex() == ISD::CCD::TELESCOPE_GUIDE)
            aperture = guideAperture;

        np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
        if (np && np->value > 0)
            primaryFL = np->value;

        np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        if (np && np->value > 0)
            guideFL = np->value;

        focal_length = primaryFL;

        //if (currentCCD && currentCCD->getTelescopeType() == ISD::CCD::TELESCOPE_GUIDE)
        if (FOVScopeCombo->currentIndex() == ISD::CCD::TELESCOPE_GUIDE)
            focal_length = guideFL;
    }

    updateGuideParams();
}

void Guide::updateGuideParams()
{
    if (currentCCD == nullptr)
        return;

    if (currentCCD->hasGuideHead() == false)
        useGuideHead = false;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (targetChip == nullptr)
    {
        appendLogText(i18n("Connection to the guide CCD is lost."));
        return;
    }

    if (targetChip->getFrameType() != FRAME_LIGHT)
        return;

    if(guiderType == GUIDE_INTERNAL)
        binningCombo->setEnabled(targetChip->canBin());

    int subBinX = 1, subBinY = 1;
    if (targetChip->canBin())
    {
        int maxBinX, maxBinY;
        targetChip->getBinning(&subBinX, &subBinY);
        targetChip->getMaxBin(&maxBinX, &maxBinY);

        binningCombo->blockSignals(true);

        binningCombo->clear();

        for (int i = 1; i <= maxBinX; i++)
            binningCombo->addItem(QString("%1x%2").arg(i).arg(i));

        binningCombo->setCurrentIndex(subBinX - 1);

        binningCombo->blockSignals(false);
    }

    if (frameSettings.contains(targetChip) == false)
    {
        int x, y, w, h;
        if (targetChip->getFrame(&x, &y, &w, &h))
        {
            if (w > 0 && h > 0)
            {
                int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
                targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

                QVariantMap settings;

                settings["x"]    = Options::guideSubframeEnabled() ? x : minX;
                settings["y"]    = Options::guideSubframeEnabled() ? y : minY;
                settings["w"]    = Options::guideSubframeEnabled() ? w : maxW;
                settings["h"]    = Options::guideSubframeEnabled() ? h : maxH;
                settings["binx"] = subBinX;
                settings["biny"] = subBinY;

                frameSettings[targetChip] = settings;
            }
        }
    }

    if (ccdPixelSizeX != -1 && ccdPixelSizeY != -1 && aperture != -1 && focal_length != -1)
    {
        FOVScopeCombo->setItemData(
            ISD::CCD::TELESCOPE_PRIMARY,
            i18nc("F-Number, Focal Length, Aperture",
                  "<nobr>F<b>%1</b> Focal Length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                  QString::number(primaryFL / primaryAperture, 'f', 1), QString::number(primaryFL, 'f', 2),
                  QString::number(primaryAperture, 'f', 2)),
            Qt::ToolTipRole);
        FOVScopeCombo->setItemData(
            ISD::CCD::TELESCOPE_GUIDE,
            i18nc("F-Number, Focal Length, Aperture",
                  "<nobr>F<b>%1</b> Focal Length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                  QString::number(guideFL / guideAperture, 'f', 1), QString::number(guideFL, 'f', 2),
                  QString::number(guideAperture, 'f', 2)),
            Qt::ToolTipRole);

        guider->setGuiderParams(ccdPixelSizeX, ccdPixelSizeY, aperture, focal_length);
        emit guideChipUpdated(targetChip);

        int x, y, w, h;
        if (targetChip->getFrame(&x, &y, &w, &h))
        {
            guider->setFrameParams(x, y, w, h, subBinX, subBinY);
        }

        l_Focal->setText(QString::number(focal_length, 'f', 1));
        l_Aperture->setText(QString::number(aperture, 'f', 1));
        if (aperture == 0)
        {
            l_FbyD->setText("0");
            // Pixel scale in arcsec/pixel
            pixScaleX = 0;
            pixScaleY = 0;
        }
        else
        {
            l_FbyD->setText(QString::number(focal_length / aperture, 'f', 1));
            // Pixel scale in arcsec/pixel
            pixScaleX = 206264.8062470963552 * ccdPixelSizeX / 1000.0 / focal_length;
            pixScaleY = 206264.8062470963552 * ccdPixelSizeY / 1000.0 / focal_length;
        }

        // FOV in arcmin
        double fov_w = (w * pixScaleX) / 60.0;
        double fov_h = (h * pixScaleY) / 60.0;

        l_FOV->setText(QString("%1x%2").arg(QString::number(fov_w, 'f', 1), QString::number(fov_h, 'f', 1)));
    }
}

void Guide::addST4(ISD::ST4 *newST4)
{
    if (guiderType != GUIDE_INTERNAL)
        return;

    foreach (ISD::ST4 *guidePort, ST4List)
    {
        if (!strcmp(guidePort->getDeviceName(), newST4->getDeviceName()))
            return;
    }

    ST4List.append(newST4);

    ST4Combo->addItem(newST4->getDeviceName());

    setST4(0);
}

bool Guide::setST4(const QString &device)
{
    if (guiderType != GUIDE_INTERNAL)
        return true;

    for (int i = 0; i < ST4List.count(); i++)
        if (ST4List.at(i)->getDeviceName() == device)
        {
            ST4Combo->setCurrentIndex(i);
            setST4(i);
            return true;
        }

    return false;
}

QString Guide::st4()
{
    if (guiderType != GUIDE_INTERNAL || ST4Combo->currentIndex() == -1)
        return QString();

    return ST4Combo->currentText();
}

void Guide::setST4(int index)
{
    if (ST4List.empty() || index >= ST4List.count() || guiderType != GUIDE_INTERNAL)
        return;

    ST4Driver = ST4List.at(index);

    GuideDriver = ST4Driver;
}

void Guide::setAO(ISD::ST4 *newAO)
{
    AODriver = newAO;
    //guider->setAO(true);
}

bool Guide::capture()
{
    buildOperationStack(GUIDE_CAPTURE);

    return executeOperationStack();
}

bool Guide::captureOneFrame()
{
    captureTimeout.stop();

    if (currentCCD == nullptr)
        return false;

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: lost connection to CCD."));
        return false;
    }

    // If CCD Telescope Type does not match desired scope type, change it
    if (currentCCD->getTelescopeType() != FOVScopeCombo->currentIndex())
        currentCCD->setTelescopeType(static_cast<ISD::CCD::TelescopeType>(FOVScopeCombo->currentIndex()));

    double seqExpose = exposureIN->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    targetChip->setCaptureMode(FITS_GUIDE);
    targetChip->setFrameType(FRAME_LIGHT);

    if (darkFrameCheck->isChecked())
        targetChip->setCaptureFilter(FITS_NONE);
    else
        targetChip->setCaptureFilter(static_cast<FITSScale>(filterCombo->currentIndex()));

    guideView->setBaseSize(guideWidget->size());
    setBusy(true);

    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        targetChip->setFrame(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(),
                             settings["h"].toInt());
    }

    currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);

    connect(currentCCD, &ISD::CCD::BLOBUpdated, this, &Ekos::Guide::newFITS, Qt::UniqueConnection);
    qCDebug(KSTARS_EKOS_GUIDE) << "Capturing frame...";

    double finalExposure = seqExpose;

    // Increase exposure for calibration frame if we need auto-select a star
    // To increase chances we detect one.
    if (operationStack.contains(GUIDE_STAR_SELECT) && Options::guideAutoStarEnabled())
        finalExposure *= 3;

    // Timeout is exposure duration + timeout threshold in seconds
    captureTimeout.start(finalExposure * 1000 + CAPTURE_TIMEOUT_THRESHOLD);

    targetChip->capture(finalExposure);

    return true;
}

bool Guide::abort()
{
    if (currentCCD && guiderType == GUIDE_INTERNAL)
    {
        captureTimeout.stop();
        pulseTimer.stop();
        ISD::CCDChip *targetChip =
            currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip->isCapturing())
            targetChip->abortExposure();
    }

    manualDitherB->setEnabled(false);

    setBusy(false);

    switch (state)
    {
        case GUIDE_IDLE:
        case GUIDE_CONNECTED:
        case GUIDE_DISCONNECTED:
            break;

        case GUIDE_CALIBRATING:
        case GUIDE_DITHERING:
        case GUIDE_STAR_SELECT:
        case GUIDE_CAPTURE:
        case GUIDE_GUIDING:
        case GUIDE_LOOPING:
            guider->abort();
            break;

        default:
            break;
    }

    return true;
}

void Guide::setBusy(bool enable)
{
    if (enable && pi->isAnimated())
        return;
    else if (enable == false && pi->isAnimated() == false)
        return;

    if (enable)
    {
        clearCalibrationB->setEnabled(false);
        guideB->setEnabled(false);
        captureB->setEnabled(false);
        loopB->setEnabled(false);

        darkFrameCheck->setEnabled(false);
        subFrameCheck->setEnabled(false);
        autoStarCheck->setEnabled(false);

        stopB->setEnabled(true);

        pi->startAnimation();

        //disconnect(guideView, SIGNAL(trackingStarSelected(int,int)), this, &Ekos::Guide::setTrackingStar(int,int)));
    }
    else
    {
        if(guiderType != GUIDE_LINGUIDER)
        {
            captureB->setEnabled(true);
            loopB->setEnabled(true);
            autoStarCheck->setEnabled(true);
            if(currentCCD)
                subFrameCheck->setEnabled(true);
        }
        if (guiderType == GUIDE_INTERNAL)
            darkFrameCheck->setEnabled(true);

        if (calibrationComplete)
            clearCalibrationB->setEnabled(true);
        guideB->setEnabled(true);
        stopB->setEnabled(false);
        pi->stopAnimation();

        connect(guideView, &FITSView::trackingStarSelected, this, &Ekos::Guide::setTrackingStar, Qt::UniqueConnection);
    }
}

void Guide::processCaptureTimeout()
{
    captureTimeoutCounter++;

    if (captureTimeoutCounter >= 3)
    {
        captureTimeoutCounter = 0;
        if (state == GUIDE_GUIDING)
            appendLogText(i18n("Exposure timeout. Aborting Autoguide."));
        else if (state == GUIDE_DITHERING)
            appendLogText(i18n("Exposure timeout. Aborting Dithering."));
        else if (state == GUIDE_CALIBRATING)
            appendLogText(i18n("Exposure timeout. Aborting Calibration."));

        abort();
        return;
    }

    appendLogText(i18n("Exposure timeout. Restarting exposure..."));
    currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    targetChip->abortExposure();
    targetChip->capture(exposureIN->value());
    captureTimeout.start(exposureIN->value() * 1000 + CAPTURE_TIMEOUT_THRESHOLD);
}

void Guide::newFITS(IBLOB *bp)
{
    INDI_UNUSED(bp);

    captureTimeout.stop();
    captureTimeoutCounter = 0;

    disconnect(currentCCD, &ISD::CCD::BLOBUpdated, this, &Ekos::Guide::newFITS);

    qCDebug(KSTARS_EKOS_GUIDE) << "Received guide frame.";

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    int subBinX = 1, subBinY = 1;
    targetChip->getBinning(&subBinX, &subBinY);

    if (starCenter.x() == 0 && starCenter.y() == 0)
    {
        int x = 0, y = 0, w = 0, h = 0;

        if (frameSettings.contains(targetChip))
        {
            QVariantMap settings = frameSettings[targetChip];
            x                    = settings["x"].toInt();
            y                    = settings["y"].toInt();
            w                    = settings["w"].toInt();
            h                    = settings["h"].toInt();
        }
        else
            targetChip->getFrame(&x, &y, &w, &h);

        starCenter.setX(w / (2 * subBinX));
        starCenter.setY(h / (2 * subBinY));
        starCenter.setZ(subBinX);
    }

    syncTrackingBoxPosition();

    setCaptureComplete();
}

void Guide::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    if (operationStack.isEmpty() == false)
    {
        executeOperationStack();
        return;
    }

    switch (state)
    {
        case GUIDE_IDLE:
        case GUIDE_ABORTED:
        case GUIDE_CONNECTED:
        case GUIDE_DISCONNECTED:
        case GUIDE_CALIBRATION_SUCESS:
        case GUIDE_CALIBRATION_ERROR:
        case GUIDE_DITHERING_ERROR:
            setBusy(false);
            break;

        case GUIDE_CAPTURE:
            state = GUIDE_IDLE;
            emit newStatus(state);
            setBusy(false);
            break;

        case GUIDE_LOOPING:
            capture();
            break;

        case GUIDE_CALIBRATING:
            guider->calibrate();
            break;

        case GUIDE_GUIDING:
            guider->guide();
            break;

        case GUIDE_DITHERING:
            guider->dither(Options::ditherPixels());
            break;

        // Feature only of internal guider
        case GUIDE_MANUAL_DITHERING:
            dynamic_cast<InternalGuider*>(guider)->processManualDithering();
            break;

        case GUIDE_REACQUIRE:
            guider->reacquire();
            break;

        case GUIDE_DITHERING_SETTLE:
            if (Options::ditherNoGuiding())
                return;
            capture();
            break;

        default:
            break;
    }

    emit newStarPixmap(guideView->getTrackingBoxPixmap(10));
}

void Guide::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_GUIDE) << text;

    emit newLog(text);
}

void Guide::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

void Guide::setDECSwap(bool enable)
{
    if (ST4Driver == nullptr || guider == nullptr)
        return;

    if (guiderType == GUIDE_INTERNAL)
    {
        dynamic_cast<InternalGuider *>(guider)->setDECSwap(enable);
        ST4Driver->setDECSwap(enable);
    }
}

bool Guide::sendPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs)
{
    if (GuideDriver == nullptr || (ra_dir == NO_DIR && dec_dir == NO_DIR))
        return false;

    if (state == GUIDE_CALIBRATING)
        pulseTimer.start((ra_msecs > dec_msecs ? ra_msecs : dec_msecs) + 100);

    return GuideDriver->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

bool Guide::sendPulse(GuideDirection dir, int msecs)
{
    if (GuideDriver == nullptr || dir == NO_DIR)
        return false;

    if (state == GUIDE_CALIBRATING)
        pulseTimer.start(msecs + 100);

    return GuideDriver->doPulse(dir, msecs);
}

QStringList Guide::getST4Devices()
{
    QStringList devices;

    foreach (ISD::ST4 *driver, ST4List)
        devices << driver->getDeviceName();

    return devices;
}


bool Guide::calibrate()
{
    // Set status to idle and let the operations change it as they get executed
    state = GUIDE_IDLE;
    emit newStatus(state);

    if (guiderType == GUIDE_INTERNAL)
    {
        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

        if (frameSettings.contains(targetChip))
        {
            targetChip->resetFrame();
            int x, y, w, h;
            targetChip->getFrame(&x, &y, &w, &h);
            QVariantMap settings      = frameSettings[targetChip];
            settings["x"]             = x;
            settings["y"]             = y;
            settings["w"]             = w;
            settings["h"]             = h;
            frameSettings[targetChip] = settings;

            subFramed = false;
        }
    }

    saveSettings();

    buildOperationStack(GUIDE_CALIBRATING);

    executeOperationStack();

    qCDebug(KSTARS_EKOS_GUIDE) << "Starting calibration using CCD:" << currentCCD->getDeviceName() << "via" << ST4Combo->currentText();

    return true;
}

bool Guide::guide()
{
    auto executeGuide = [this]()
    {
        if(guiderType != GUIDE_PHD2)
        {
            if (calibrationComplete == false)
            {
                calibrate();
                return;
            }
        }

        saveSettings();
        guider->guide();

        //If PHD2 gets a Guide command and it is looping, it will accept a lock position
        //but if it was not looping it will ignore the lock position and do an auto star automatically
        //This is not the default behavior in Ekos if auto star is not selected.
        //This gets around that by noting the position of the tracking box, and enforcing it after the state switches to guide.
        if(!Options::guideAutoStarEnabled())
        {
            if(guiderType == GUIDE_PHD2 && guideView->isTrackingBoxEnabled())
            {
                double x = starCenter.x();
                double y = starCenter.y();

                if(guideView->getImageData() != nullptr)
                {
                    if(guideView->getImageData()->width() > 50)
                    {
                        guideConnect = connect(this, &Guide::newStatus, this, [this, x, y](Ekos::GuideState newState)
                        {
                            if(newState == GUIDE_GUIDING)
                            {
                                phd2Guider->setLockPosition(x,y);
                                disconnect(guideConnect);
                            }
                        });
                    }
                }
            }
        }
    };

    if (Options::defaultCaptureCCD() == guiderCombo->currentText())
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, executeGuide]()
        {
            //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
            KSMessageBox::Instance()->disconnect(this);
            executeGuide();
        });

        KSMessageBox::Instance()->questionYesNo(i18n("The guide camera is identical to the primary imaging camera. Are you sure you want to continue?"));

        return false;
    }

    executeGuide();
    return true;
}

bool Guide::dither()
{
    if (Options::ditherNoGuiding() && state == GUIDE_IDLE)
    {
        ditherDirectly();
        return true;
    }

    if (state == GUIDE_DITHERING || state == GUIDE_DITHERING_SETTLE)
        return true;

    //This adds a dither text item to the graph where dithering occurred.
    double time = guideTimer.elapsed() / 1000.0;
    QCPItemText *ditherLabel = new QCPItemText(driftGraph);
    ditherLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    ditherLabel->position->setType(QCPItemPosition::ptPlotCoords);
    ditherLabel->position->setCoords(time, 1.5);
    ditherLabel->setColor(Qt::white);
    ditherLabel->setBrush(Qt::NoBrush);
    ditherLabel->setPen(Qt::NoPen);
    ditherLabel->setText("Dither");
    ditherLabel->setFont(QFont(font().family(), 10));

    if (guiderType == GUIDE_INTERNAL)
    {
        if (state != GUIDE_GUIDING)
            capture();

        setStatus(GUIDE_DITHERING);

        return true;
    }
    else
        return guider->dither(Options::ditherPixels());
}

bool Guide::suspend()
{
    if (state == GUIDE_SUSPENDED)
        return true;
    else if (state >= GUIDE_CAPTURE)
        return guider->suspend();
    else
        return false;
}

bool Guide::resume()
{
    if (state == GUIDE_GUIDING)
        return true;
    else if (state == GUIDE_SUSPENDED)
        return guider->resume();
    else
        return false;
}

void Guide::setCaptureStatus(CaptureState newState)
{
    switch (newState)
    {
        case CAPTURE_DITHERING:
            dither();
            break;

        default:
            break;
    }
}

void Guide::setPierSide(ISD::Telescope::PierSide newSide)
{
    Q_UNUSED(newSide);

    // If pier side changes in internal guider
    // and calibration was already done
    // then let's swap
    if (guiderType == GUIDE_INTERNAL &&
            state != GUIDE_GUIDING &&
            state != GUIDE_CALIBRATING &&
            calibrationComplete)
    {
        clearCalibration();
        appendLogText(i18n("Pier side change detected. Clearing calibration."));
    }
}

void Guide::setMountStatus(ISD::Telescope::Status newState)
{
    if (newState == ISD::Telescope::MOUNT_PARKING || newState == ISD::Telescope::MOUNT_SLEWING)
    {
        // reset the calibration if "Always reset calibration" is selected and the mount moves
        if (Options::resetGuideCalibration())
        {
            appendLogText(i18n("Mount is moving. Resetting calibration..."));
            clearCalibration();
        }

        // If we're guiding, and the mount either slews or parks, then we abort.
        if (state == GUIDE_GUIDING || state == GUIDE_DITHERING)
        {
        if (newState == ISD::Telescope::MOUNT_PARKING)
            appendLogText(i18n("Mount is parking. Aborting guide..."));
        else
            appendLogText(i18n("Mount is slewing. Aborting guide..."));

        abort();
        }
    }

    if (guiderType != GUIDE_INTERNAL)
        return;

    switch (newState)
    {
        case ISD::Telescope::MOUNT_SLEWING:
        case ISD::Telescope::MOUNT_PARKING:
        case ISD::Telescope::MOUNT_MOVING:
            captureB->setEnabled(false);
            loopB->setEnabled(false);
            clearCalibrationB->setEnabled(false);
            break;

        default:
            if (pi->isAnimated() == false)
            {
                captureB->setEnabled(true);
                loopB->setEnabled(true);
                clearCalibrationB->setEnabled(true);
            }
    }
}

void Guide::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Guide::setImageFilter(const QString &value)
{
    for (int i = 0; i < filterCombo->count(); i++)
        if (filterCombo->itemText(i) == value)
        {
            filterCombo->setCurrentIndex(i);
            break;
        }
}

void Guide::setCalibrationTwoAxis(bool enable)
{
    Options::setTwoAxisEnabled(enable);
}

void Guide::setCalibrationAutoStar(bool enable)
{
    autoStarCheck->setChecked(enable);
}

void Guide::setCalibrationAutoSquareSize(bool enable)
{
    Options::setGuideAutoSquareSizeEnabled(enable);
}

void Guide::setCalibrationPulseDuration(int pulseDuration)
{
    Options::setCalibrationPulseDuration(pulseDuration);
}

void Guide::setGuideBoxSizeIndex(int index)
{
    Options::setGuideSquareSizeIndex(index);
}

void Guide::setGuideAlgorithmIndex(int index)
{
    Options::setGuideAlgorithm(index);
}

void Guide::setSubFrameEnabled(bool enable)
{
    Options::setGuideSubframeEnabled(enable);
    if (subFrameCheck->isChecked() != enable)
        subFrameCheck->setChecked(enable);
    if(guiderType == GUIDE_PHD2)
        setExternalGuiderBLOBEnabled(!enable);
}


void Guide::setDitherSettings(bool enable, double value)
{
    Options::setDitherEnabled(enable);
    Options::setDitherPixels(value);
}


void Guide::clearCalibration()
{
    calibrationComplete = false;

    guider->clearCalibration();

    appendLogText(i18n("Calibration is cleared."));
}

void Guide::setStatus(Ekos::GuideState newState)
{
    if (newState == state)
    {
        // pass through the aborted state
        if (newState == GUIDE_ABORTED)
            emit newStatus(state);
        return;
    }

    GuideState previousState = state;

    state = newState;
    emit newStatus(state);

    switch (state)
    {
        case GUIDE_CONNECTED:
            appendLogText(i18n("External guider connected."));
            externalConnectB->setEnabled(false);
            externalDisconnectB->setEnabled(true);
            clearCalibrationB->setEnabled(true);
            guideB->setEnabled(true);

            if(guiderType == GUIDE_PHD2)
            {
                captureB->setEnabled(true);
                loopB->setEnabled(true);
                autoStarCheck->setEnabled(true);
                configurePHD2Camera();
                setExternalGuiderBLOBEnabled(!Options::guideSubframeEnabled());
                boxSizeCombo->setEnabled(true);
            }
            break;

        case GUIDE_DISCONNECTED:
            appendLogText(i18n("External guider disconnected."));
            setBusy(false); //This needs to come before caputureB since it will set it to enabled again.
            externalConnectB->setEnabled(true);
            externalDisconnectB->setEnabled(false);
            clearCalibrationB->setEnabled(false);
            guideB->setEnabled(false);
            captureB->setEnabled(false);
            loopB->setEnabled(false);
            autoStarCheck->setEnabled(false);
            boxSizeCombo->setEnabled(false);
            //setExternalGuiderBLOBEnabled(true);
#ifdef Q_OS_OSX
            repaint(); //This is a band-aid for a bug in QT 5.10.0
#endif
            break;

        case GUIDE_CALIBRATION_SUCESS:
            appendLogText(i18n("Calibration completed."));
            calibrationComplete = true;
            /*if (autoCalibrateGuide)
            {
                autoCalibrateGuide = false;
                guide();
            }
            else
                setBusy(false);*/
            if(guiderType != GUIDE_PHD2) //PHD2 will take care of this.  If this command is executed for PHD2, it might start guiding when it is first connected, if the calibration was completed already.
                guide();
            break;

        case GUIDE_IDLE:
        case GUIDE_CALIBRATION_ERROR:
            setBusy(false);
            manualDitherB->setEnabled(false);
            break;

        case GUIDE_CALIBRATING:
            appendLogText(i18n("Calibration started."));
            setBusy(true);
            break;

        case GUIDE_GUIDING:
            if (previousState == GUIDE_SUSPENDED || previousState == GUIDE_DITHERING_SUCCESS)
                appendLogText(i18n("Guiding resumed."));
            else
            {
                appendLogText(i18n("Autoguiding started."));
                setBusy(true);

                clearGuideGraphs();
                guideTimer = QTime::currentTime();
                refreshColorScheme();
            }
            manualDitherB->setEnabled(true);

            break;

        case GUIDE_ABORTED:
            appendLogText(i18n("Autoguiding aborted."));
            setBusy(false);
            break;

        case GUIDE_SUSPENDED:
            appendLogText(i18n("Guiding suspended."));
            break;

        case GUIDE_REACQUIRE:
            capture();
            break;

        case GUIDE_MANUAL_DITHERING:
            appendLogText(i18n("Manual dithering in progress."));
            break;

        case GUIDE_DITHERING:
            appendLogText(i18n("Dithering in progress."));
            break;

        case GUIDE_DITHERING_SETTLE:
            if (Options::ditherSettle() > 0)
                appendLogText(i18np("Post-dither settling for %1 second...", "Post-dither settling for %1 seconds...", Options::ditherSettle()));
            capture();
            break;

        case GUIDE_DITHERING_ERROR:
            appendLogText(i18n("Dithering failed."));
            // LinGuider guide continue after dithering failure
            if (guiderType != GUIDE_LINGUIDER)
            {
                //state = GUIDE_IDLE;
                state = GUIDE_ABORTED;
                setBusy(false);
            }
            break;

        case GUIDE_DITHERING_SUCCESS:
            appendLogText(i18n("Dithering completed successfully."));
            // Go back to guiding state immediately if using regular guider
            if (Options::ditherNoGuiding() == false)
            {
                setStatus(GUIDE_GUIDING);
                // Only capture again if we are using internal guider
                if (guiderType == GUIDE_INTERNAL)
                    capture();
            }
            break;
        default:
            break;
    }
}

void Guide::updateCCDBin(int index)
{
    if (currentCCD == nullptr || guiderType != GUIDE_INTERNAL)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    targetChip->setBinning(index + 1, index + 1);

    QVariantMap settings      = frameSettings[targetChip];
    settings["binx"]          = index + 1;
    settings["biny"]          = index + 1;
    frameSettings[targetChip] = settings;

    guider->setFrameParams(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(), settings["h"].toInt(),
                           settings["binx"].toInt(), settings["biny"].toInt());
}

void Guide::processCCDNumber(INumberVectorProperty *nvp)
{
    if (currentCCD == nullptr || strcmp(nvp->device, currentCCD->getDeviceName()) || guiderType != GUIDE_INTERNAL)
        return;

    if ((!strcmp(nvp->name, "CCD_BINNING") && useGuideHead == false) ||
            (!strcmp(nvp->name, "GUIDER_BINNING") && useGuideHead))
    {
        binningCombo->disconnect();
        binningCombo->setCurrentIndex(nvp->np[0].value - 1);
        connect(binningCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Guide::updateCCDBin);
    }
}

void Guide::checkExposureValue(ISD::CCDChip *targetChip, double exposure, IPState expState)
{
    if (guiderType != GUIDE_INTERNAL)
        return;

    INDI_UNUSED(exposure);

    if (expState == IPS_ALERT &&
            ((state == GUIDE_GUIDING) || (state == GUIDE_DITHERING) || (state == GUIDE_CALIBRATING)))
    {
        appendLogText(i18n("Exposure failed. Restarting exposure..."));
        currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);
        targetChip->capture(exposureIN->value());
    }
}

void Guide::setDarkFrameEnabled(bool enable)
{
    Options::setGuideDarkFrameEnabled(enable);
    if (darkFrameCheck->isChecked() != enable)
        darkFrameCheck->setChecked(enable);
}

void Guide::saveDefaultGuideExposure()
{
    Options::setGuideExposure(exposureIN->value());
    if(guiderType == GUIDE_PHD2)
        phd2Guider->requestSetExposureTime(exposureIN->value() * 1000);
}

void Guide::setStarPosition(const QVector3D &newCenter, bool updateNow)
{
    starCenter.setX(newCenter.x());
    starCenter.setY(newCenter.y());
    if (newCenter.z() > 0)
        starCenter.setZ(newCenter.z());

    if (updateNow)
        syncTrackingBoxPosition();
}

void Guide::syncTrackingBoxPosition()
{
    if(!currentCCD || guiderType == GUIDE_LINGUIDER)
        return;

    if(guiderType == GUIDE_PHD2)
    {
        //This way it won't set the tracking box on the Guide Star Image.
        if(guideView->getImageData() != nullptr)
        {
            if(guideView->getImageData()->width() < 50)
            {
                guideView->setTrackingBoxEnabled(false);
                return;
            }
        }
    }

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    Q_ASSERT(targetChip);

    int subBinX = 1, subBinY = 1;
    targetChip->getBinning(&subBinX, &subBinY);

    if (starCenter.isNull() == false)
    {
        double boxSize = boxSizeCombo->currentText().toInt();
        int x, y, w, h;
        targetChip->getFrame(&x, &y, &w, &h);
        // If box size is larger than image size, set it to lower index
        if (boxSize / subBinX >= w || boxSize / subBinY >= h)
        {
            int newIndex = boxSizeCombo->currentIndex() - 1;
            if (newIndex >= 0)
                boxSizeCombo->setCurrentIndex(newIndex);
            return;
        }

        // If binning changed, update coords accordingly
        if (subBinX != starCenter.z())
        {
            if (starCenter.z() > 0)
            {
                starCenter.setX(starCenter.x() * (starCenter.z() / subBinX));
                starCenter.setY(starCenter.y() * (starCenter.z() / subBinY));
            }

            starCenter.setZ(subBinX);
        }

        QRect starRect = QRect(starCenter.x() - boxSize / (2 * subBinX), starCenter.y() - boxSize / (2 * subBinY),
                               boxSize / subBinX, boxSize / subBinY);
        guideView->setTrackingBoxEnabled(true);
        guideView->setTrackingBox(starRect);
    }
}

bool Guide::setGuiderType(int type)
{
    // Use default guider option
    if (type == -1)
        type = Options::guiderType();
    else if (type == guiderType)
        return true;

    if (state == GUIDE_CALIBRATING || state == GUIDE_GUIDING || state == GUIDE_DITHERING)
    {
        appendLogText(i18n("Cannot change guider type while active."));
        return false;
    }

    if (guider != nullptr)
    {
        // Disconnect from host
        if (guider->isConnected())
            guider->Disconnect();

        // Disconnect signals
        guider->disconnect();
    }

    guiderType = static_cast<GuiderType>(type);

    switch (type)
    {
        case GUIDE_INTERNAL:
        {
            connect(internalGuider, SIGNAL(newPulse(GuideDirection, int)), this, SLOT(sendPulse(GuideDirection, int)));
            connect(internalGuider, SIGNAL(newPulse(GuideDirection, int, GuideDirection, int)), this,
                    SLOT(sendPulse(GuideDirection, int, GuideDirection, int)));
            connect(internalGuider, SIGNAL(DESwapChanged(bool)), swapCheck, SLOT(setChecked(bool)));
            connect(internalGuider, SIGNAL(newStarPixmap(QPixmap &)), this, SIGNAL(newStarPixmap(QPixmap &)));

            guider = internalGuider;

            internalGuider->setSquareAlgorithm(opsGuide->kcfg_GuideAlgorithm->currentIndex());
            internalGuider->setRegionAxis(opsGuide->kcfg_GuideRegionAxis->currentText().toInt());

            clearCalibrationB->setEnabled(true);
            guideB->setEnabled(true);
            captureB->setEnabled(true);
            loopB->setEnabled(true);
            darkFrameCheck->setEnabled(true);
            subFrameCheck->setEnabled(true);
            autoStarCheck->setEnabled(true);

            guiderCombo->setEnabled(true);
            ST4Combo->setEnabled(true);
            exposureIN->setEnabled(true);
            binningCombo->setEnabled(true);
            boxSizeCombo->setEnabled(true);
            filterCombo->setEnabled(true);

            externalConnectB->setEnabled(false);
            externalDisconnectB->setEnabled(false);

            controlGroup->setEnabled(true);
            infoGroup->setEnabled(true);
            label_6->setEnabled(true);
            FOVScopeCombo->setEnabled(true);
            l_3->setEnabled(true);
            spinBox_GuideRate->setEnabled(true);
            l_RecommendedGain->setEnabled(true);
            l_5->setEnabled(true);
            l_6->setEnabled(true);
            l_7->setEnabled(true);
            l_8->setEnabled(true);
            l_Aperture->setEnabled(true);
            l_FOV->setEnabled(true);
            l_FbyD->setEnabled(true);
            l_Focal->setEnabled(true);
            driftGraphicsGroup->setEnabled(true);

            guiderCombo->setToolTip(i18n("Select guide camera."));

            updateGuideParams();
        }
        break;

        case GUIDE_PHD2:
            if (phd2Guider.isNull())
                phd2Guider = new PHD2();

            guider = phd2Guider;
            phd2Guider->setGuideView(guideView);

            connect(phd2Guider, SIGNAL(newStarPixmap(QPixmap &)), this, SIGNAL(newStarPixmap(QPixmap &)));

            clearCalibrationB->setEnabled(true);
            captureB->setEnabled(false);
            loopB->setEnabled(false);
            darkFrameCheck->setEnabled(false);
            subFrameCheck->setEnabled(false);
            autoStarCheck->setEnabled(false);
            guideB->setEnabled(false); //This will be enabled later when equipment connects (or not)
            externalConnectB->setEnabled(false);

            checkBox_DirRA->setEnabled(false);
            eastControlCheck->setEnabled(false);
            westControlCheck->setEnabled(false);
            swapCheck->setEnabled(false);


            controlGroup->setEnabled(false);
            infoGroup->setEnabled(true);
            label_6->setEnabled(false);
            FOVScopeCombo->setEnabled(false);
            l_3->setEnabled(false);
            spinBox_GuideRate->setEnabled(false);
            l_RecommendedGain->setEnabled(false);
            l_5->setEnabled(false);
            l_6->setEnabled(false);
            l_7->setEnabled(false);
            l_8->setEnabled(false);
            l_Aperture->setEnabled(false);
            l_FOV->setEnabled(false);
            l_FbyD->setEnabled(false);
            l_Focal->setEnabled(false);
            driftGraphicsGroup->setEnabled(true);

            ST4Combo->setEnabled(false);
            exposureIN->setEnabled(true);
            binningCombo->setEnabled(false);
            boxSizeCombo->setEnabled(false);
            filterCombo->setEnabled(false);
            guiderCombo->setEnabled(false);

            if (Options::resetGuideCalibration())
                appendLogText(i18n("Warning: Reset Guiding Calibration is enabled. It is recommended to turn this option off for PHD2."));

            updateGuideParams();
            break;

        case GUIDE_LINGUIDER:
            if (linGuider.isNull())
                linGuider = new LinGuider();

            guider = linGuider;

            clearCalibrationB->setEnabled(true);
            captureB->setEnabled(false);
            loopB->setEnabled(false);
            darkFrameCheck->setEnabled(false);
            subFrameCheck->setEnabled(false);
            autoStarCheck->setEnabled(false);
            guideB->setEnabled(true);
            externalConnectB->setEnabled(true);

            controlGroup->setEnabled(false);
            infoGroup->setEnabled(false);
            driftGraphicsGroup->setEnabled(false);

            ST4Combo->setEnabled(false);
            exposureIN->setEnabled(false);
            binningCombo->setEnabled(false);
            boxSizeCombo->setEnabled(false);
            filterCombo->setEnabled(false);

            guiderCombo->setEnabled(false);

            updateGuideParams();

            break;
    }

    if (guider != nullptr)
    {
        connect(guider, &Ekos::GuideInterface::frameCaptureRequested, this, &Ekos::Guide::capture);
        connect(guider, &Ekos::GuideInterface::newLog, this, &Ekos::Guide::appendLogText);
        connect(guider, &Ekos::GuideInterface::newStatus, this, &Ekos::Guide::setStatus);
        connect(guider, &Ekos::GuideInterface::newStarPosition, this, &Ekos::Guide::setStarPosition);

        connect(guider, &Ekos::GuideInterface::newAxisDelta, this, &Ekos::Guide::setAxisDelta);
        connect(guider, &Ekos::GuideInterface::newAxisPulse, this, &Ekos::Guide::setAxisPulse);
        connect(guider, &Ekos::GuideInterface::newAxisSigma, this, &Ekos::Guide::setAxisSigma);

        connect(guider, &Ekos::GuideInterface::guideEquipmentUpdated, this, &Ekos::Guide::configurePHD2Camera);
    }

    externalConnectB->setEnabled(false);
    externalDisconnectB->setEnabled(false);

    if (guider != nullptr && guiderType != GUIDE_INTERNAL)
    {
        externalConnectB->setEnabled(!guider->isConnected());
        externalDisconnectB->setEnabled(guider->isConnected());
    }

    if (guider != nullptr)
        guider->Connect();

    return true;
}

void Guide::updateTrackingBoxSize(int currentIndex)
{
    if (currentIndex >= 0)
    {
        Options::setGuideSquareSizeIndex(currentIndex);

        if (guiderType == GUIDE_INTERNAL)
            dynamic_cast<InternalGuider *>(guider)->setGuideBoxSize(boxSizeCombo->currentText().toInt());

        syncTrackingBoxPosition();
    }
}


/*
void Guide::onXscaleChanged( int i )
{
    int rx, ry;

    driftGraphics->getVisibleRanges( &rx, &ry );
    driftGraphics->setVisibleRanges( i*driftGraphics->getGridN(), ry );
    driftGraphics->update();

}

void Guide::onYscaleChanged( int i )
{
    int rx, ry;

    driftGraphics->getVisibleRanges( &rx, &ry );
    driftGraphics->setVisibleRanges( rx, i*driftGraphics->getGridN() );
    driftGraphics->update();
}
*/

void Guide::onThresholdChanged(int index)
{
    switch (guiderType)
    {
        case GUIDE_INTERNAL:
            dynamic_cast<InternalGuider *>(guider)->setSquareAlgorithm(index);
            break;

        default:
            break;
    }
}

void Guide::onInfoRateChanged(double val)
{
    Options::setGuidingRate(val);

    double gain = 0;

    if (val > 0.01)
        gain = 1000.0 / (val * 15.0);

    l_RecommendedGain->setText(i18n("P: %1", QString().setNum(gain, 'f', 2)));
}

void Guide::onEnableDirRA(bool enable)
{
    Options::setRAGuideEnabled(enable);
}

void Guide::onEnableDirDEC(bool enable)
{
    Options::setDECGuideEnabled(enable);
    updatePHD2Directions();
}

void Guide::syncSettings()
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;

    QObject *obj = sender();

    if ((pSB = qobject_cast<QSpinBox *>(obj)))
    {
        if (pSB == spinBox_MaxPulseRA)
            Options::setRAMaximumPulse(pSB->value());
        else if (pSB == spinBox_MaxPulseDEC)
            Options::setDECMaximumPulse(pSB->value());
        else if (pSB == spinBox_MinPulseRA)
            Options::setRAMinimumPulse(pSB->value());
        else if (pSB == spinBox_MinPulseDEC)
            Options::setDECMinimumPulse(pSB->value());
    }
    else if ((pDSB = qobject_cast<QDoubleSpinBox *>(obj)))
    {
        if (pDSB == spinBox_PropGainRA)
            Options::setRAProportionalGain(pDSB->value());
        else if (pDSB == spinBox_PropGainDEC)
            Options::setDECProportionalGain(pDSB->value());
        else if (pDSB == spinBox_IntGainRA)
            Options::setRAIntegralGain(pDSB->value());
        else if (pDSB == spinBox_IntGainDEC)
            Options::setDECIntegralGain(pDSB->value());
        else if (pDSB == spinBox_DerGainRA)
            Options::setRADerivativeGain(pDSB->value());
        else if (pDSB == spinBox_DerGainDEC)
            Options::setDECDerivativeGain(pDSB->value());
    }
    else if ((pCB = qobject_cast<QCheckBox*>(obj)))
    {
        if (pCB == autoStarCheck)
            Options::setGuideAutoStarEnabled(pCB->isChecked());
    }
}

void Guide::onControlDirectionChanged(bool enable)
{
    QObject *obj = sender();

    if (northControlCheck == dynamic_cast<QCheckBox *>(obj))
    {
        Options::setNorthDECGuideEnabled(enable);
        updatePHD2Directions();
    }
    else if (southControlCheck == dynamic_cast<QCheckBox *>(obj))
    {
        Options::setSouthDECGuideEnabled(enable);
        updatePHD2Directions();
    }
    else if (westControlCheck == dynamic_cast<QCheckBox *>(obj))
    {
        Options::setWestRAGuideEnabled(enable);
    }
    else if (eastControlCheck == dynamic_cast<QCheckBox *>(obj))
    {
        Options::setEastRAGuideEnabled(enable);
    }
}
void Guide::updatePHD2Directions()
{
    if(guiderType == GUIDE_PHD2)
        phd2Guider -> requestSetDEGuideMode(checkBox_DirDEC->isChecked(), northControlCheck->isChecked(), southControlCheck->isChecked());
}
void Guide::updateDirectionsFromPHD2(QString mode)
{
    //disable connections
    disconnect(checkBox_DirDEC, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirDEC);
    disconnect(northControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    disconnect(southControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);

    if(mode == "Auto")
    {
        checkBox_DirDEC->setChecked(true);
        northControlCheck->setChecked(true);
        southControlCheck->setChecked(true);
    }
    else if(mode == "North")
    {
        checkBox_DirDEC->setChecked(true);
        northControlCheck->setChecked(true);
        southControlCheck->setChecked(false);
    }
    else if(mode == "South")
    {
        checkBox_DirDEC->setChecked(true);
        northControlCheck->setChecked(false);
        southControlCheck->setChecked(true);
    }
    else //Off
    {
        checkBox_DirDEC->setChecked(false);
        northControlCheck->setChecked(true);
        southControlCheck->setChecked(true);
    }

    //Re-enable connections
    connect(checkBox_DirDEC, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirDEC);
    connect(northControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(southControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
}


void Guide::loadSettings()
{
    // Exposure
    exposureIN->setValue(Options::guideExposure());
    // Box Size
    boxSizeCombo->setCurrentIndex(Options::guideSquareSizeIndex());
    // Dark frame?
    darkFrameCheck->setChecked(Options::guideDarkFrameEnabled());
    // Subframed?
    subFrameCheck->setChecked(Options::guideSubframeEnabled());
    // Guiding Rate
    spinBox_GuideRate->setValue(Options::guidingRate());
    // RA/DEC enabled?
    checkBox_DirRA->setChecked(Options::rAGuideEnabled());
    checkBox_DirDEC->setChecked(Options::dECGuideEnabled());
    // N/S enabled?
    northControlCheck->setChecked(Options::northDECGuideEnabled());
    southControlCheck->setChecked(Options::southDECGuideEnabled());
    // W/E enabled?
    westControlCheck->setChecked(Options::westRAGuideEnabled());
    eastControlCheck->setChecked(Options::eastRAGuideEnabled());
    // PID Control - Proportional Gain
    spinBox_PropGainRA->setValue(Options::rAProportionalGain());
    spinBox_PropGainDEC->setValue(Options::dECProportionalGain());
    // PID Control - Integral Gain
    spinBox_IntGainRA->setValue(Options::rAIntegralGain());
    spinBox_IntGainDEC->setValue(Options::dECIntegralGain());
    // PID Control - Derivative Gain
    spinBox_DerGainRA->setValue(Options::rADerivativeGain());
    spinBox_DerGainDEC->setValue(Options::dECDerivativeGain());
    // Max Pulse Duration (ms)
    spinBox_MaxPulseRA->setValue(Options::rAMaximumPulse());
    spinBox_MaxPulseDEC->setValue(Options::dECMaximumPulse());
    // Min Pulse Duration (ms)
    spinBox_MinPulseRA->setValue(Options::rAMinimumPulse());
    spinBox_MinPulseDEC->setValue(Options::dECMinimumPulse());
    // Autostar
    autoStarCheck->setChecked(Options::guideAutoStarEnabled());
}

void Guide::saveSettings()
{
    // Exposure
    Options::setGuideExposure(exposureIN->value());
    // Box Size
    Options::setGuideSquareSizeIndex(boxSizeCombo->currentIndex());
    // Dark frame?
    Options::setGuideDarkFrameEnabled(darkFrameCheck->isChecked());
    // Subframed?
    Options::setGuideSubframeEnabled(subFrameCheck->isChecked());
    // Guiding Rate?
    Options::setGuidingRate(spinBox_GuideRate->value());
    // RA/DEC enabled?
    Options::setRAGuideEnabled(checkBox_DirRA->isChecked());
    Options::setDECGuideEnabled(checkBox_DirDEC->isChecked());
    // N/S enabled?
    Options::setNorthDECGuideEnabled(northControlCheck->isChecked());
    Options::setSouthDECGuideEnabled(southControlCheck->isChecked());
    // W/E enabled?
    Options::setWestRAGuideEnabled(westControlCheck->isChecked());
    Options::setEastRAGuideEnabled(eastControlCheck->isChecked());
    // PID Control - Proportional Gain
    Options::setRAProportionalGain(spinBox_PropGainRA->value());
    Options::setDECProportionalGain(spinBox_PropGainDEC->value());
    // PID Control - Integral Gain
    Options::setRAIntegralGain(spinBox_IntGainRA->value());
    Options::setDECIntegralGain(spinBox_IntGainDEC->value());
    // PID Control - Derivative Gain
    Options::setRADerivativeGain(spinBox_DerGainRA->value());
    Options::setDECDerivativeGain(spinBox_DerGainDEC->value());
    // Max Pulse Duration (ms)
    Options::setRAMaximumPulse(spinBox_MaxPulseRA->value());
    Options::setDECMaximumPulse(spinBox_MaxPulseDEC->value());
    // Min Pulse Duration (ms)
    Options::setRAMinimumPulse(spinBox_MinPulseRA->value());
    Options::setDECMinimumPulse(spinBox_MinPulseDEC->value());
}

void Guide::setTrackingStar(int x, int y)
{
    QVector3D newStarPosition(x, y, -1);
    setStarPosition(newStarPosition, true);

    if(guiderType == GUIDE_PHD2)
    {
        //The Guide Star Image is 32 pixels across or less, so this guarantees it isn't that.
        if(guideView->getImageData() != nullptr)
        {
            if(guideView->getImageData()->width() > 50)
                phd2Guider->setLockPosition(starCenter.x(),starCenter.y());
        }
    }

    /*if (state == GUIDE_STAR_SELECT)
    {
        guider->setStarPosition(newStarPosition);
        guider->calibrate();
    }*/

    if (operationStack.isEmpty() == false)
        executeOperationStack();
}

void Guide::setAxisDelta(double ra, double de)
{
    //If PHD2 starts guiding because somebody pusted the button remotely, we want to set the state to guiding.
    //If guide pulses start coming in, it must be guiding.
    if(guiderType == GUIDE_PHD2 && state != GUIDE_GUIDING)
        setStatus(GUIDE_GUIDING);

    // Time since timer started.
    double key = guideTimer.elapsed() / 1000.0;

    ra = -ra;  //The ra is backwards in sign from how it should be displayed on the graph.

    driftGraph->graph(0)->addData(key, ra);
    driftGraph->graph(1)->addData(key, de);

    int currentNumPoints = driftGraph->graph(0)->dataCount();
    guideSlider->setMaximum(currentNumPoints);
    if(graphOnLatestPt)
        guideSlider->setValue(currentNumPoints);

    // Expand range if it doesn't fit already
    if (driftGraph->yAxis->range().contains(ra) == false)
        driftGraph->yAxis->setRange(-1.25 * ra, 1.25 * ra);

    if (driftGraph->yAxis->range().contains(de) == false)
        driftGraph->yAxis->setRange(-1.25 * de, 1.25 * de);

    // Show last 120 seconds
    //driftGraph->xAxis->setRange(key, 120, Qt::AlignRight);
    if(graphOnLatestPt)
    {
        driftGraph->xAxis->setRange(key, driftGraph->xAxis->range().size(), Qt::AlignRight);
        driftGraph->graph(2)->data()->clear(); //Clear highlighted RA point
        driftGraph->graph(3)->data()->clear(); //Clear highlighted DEC point
        driftGraph->graph(2)->addData(key, ra); //Set highlighted RA point to latest point
        driftGraph->graph(3)->addData(key, de); //Set highlighted DEC point to latest point
    }
    driftGraph->replot();

    //Add to Drift Plot
    driftPlot->graph(0)->addData(ra, de);
    if(graphOnLatestPt)
    {
        driftPlot->graph(1)->data()->clear(); //Clear highlighted point
        driftPlot->graph(1)->addData(ra, de); //Set highlighted point to latest point
    }

    if (driftPlot->xAxis->range().contains(ra) == false || driftPlot->yAxis->range().contains(de) == false)
    {
        driftPlot->setBackground(QBrush(Qt::gray));
        QTimer::singleShot(300, this, [ = ]()
        {
            driftPlot->setBackground(QBrush(Qt::black));
            driftPlot->replot();
        });
    }

    driftPlot->replot();

    l_DeltaRA->setText(QString::number(ra, 'f', 2));
    l_DeltaDEC->setText(QString::number(de, 'f', 2));

    emit newAxisDelta(ra, de);

    profilePixmap = driftGraph->grab();
    emit newProfilePixmap(profilePixmap);
}

void Guide::setAxisSigma(double ra, double de)
{
    l_ErrRA->setText(QString::number(ra, 'f', 2));
    l_ErrDEC->setText(QString::number(de, 'f', 2));
    l_TotalRMS->setText(QString::number(sqrt(ra * ra + de * de), 'f', 2));
    emit newAxisSigma(ra, de);
}

QList<double> Guide::axisDelta()
{
    QList<double> delta;

    delta << l_DeltaRA->text().toDouble() << l_DeltaDEC->text().toDouble();

    return delta;
}

QList<double> Guide::axisSigma()
{
    QList<double> sigma;

    sigma << l_ErrRA->text().toDouble() << l_ErrDEC->text().toDouble();

    return sigma;
}

void Guide::setAxisPulse(double ra, double de)
{
    l_PulseRA->setText(QString::number(static_cast<int>(ra)));
    l_PulseDEC->setText(QString::number(static_cast<int>(de)));

    double key = guideTimer.elapsed() / 1000.0;

    driftGraph->graph(4)->addData(key, ra);
    driftGraph->graph(5)->addData(key, de);
}

void Guide::refreshColorScheme()
{
    // Drift color legend
    if (driftGraph)
    {
        if (driftGraph->graph(0) && driftGraph->graph(1) && driftGraph->graph(2) && driftGraph->graph(3) && driftGraph->graph(4) && driftGraph->graph(5))
        {
            driftGraph->graph(0)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
            driftGraph->graph(1)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
            driftGraph->graph(2)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
            driftGraph->graph(2)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle, QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"), 2), QBrush(), 10));
            driftGraph->graph(3)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
            driftGraph->graph(3)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle, QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"), 2), QBrush(), 10));

            QColor raPulseColor(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"));
            raPulseColor.setAlpha(75);
            driftGraph->graph(4)->setPen(QPen(raPulseColor));
            driftGraph->graph(4)->setBrush(QBrush(raPulseColor, Qt::Dense4Pattern));

            QColor dePulseColor(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"));
            dePulseColor.setAlpha(75);
            driftGraph->graph(5)->setPen(QPen(dePulseColor));
            driftGraph->graph(5)->setBrush(QBrush(dePulseColor, Qt::Dense4Pattern));
        }
    }
}

void Guide::driftMouseClicked(QMouseEvent *event)
{
    if (event->buttons() & Qt::RightButton)
    {
        driftGraph->yAxis->setRange(-3, 3);
    }
}

void Guide::driftMouseOverLine(QMouseEvent *event)
{
    double key = driftGraph->xAxis->pixelToCoord(event->localPos().x());

    if (driftGraph->xAxis->range().contains(key))
    {
        QCPGraph *graph = qobject_cast<QCPGraph *>(driftGraph->plottableAt(event->pos(), false));

        if (graph)
        {
            int raIndex = driftGraph->graph(0)->findBegin(key);
            int deIndex = driftGraph->graph(1)->findBegin(key);

            double raDelta = driftGraph->graph(0)->dataMainValue(raIndex);
            double deDelta = driftGraph->graph(1)->dataMainValue(deIndex);

            double raPulse = driftGraph->graph(4)->dataMainValue(raIndex); //Get RA Pulse from RA pulse data
            double dePulse = driftGraph->graph(5)->dataMainValue(deIndex); //Get DEC Pulse from DEC pulse data

            // Compute time value:
            QTime localTime = guideTimer;

            localTime = localTime.addSecs(key);

            QToolTip::hideText();
            if(raPulse == 0 && dePulse == 0)
            {
                QToolTip::showText(
                    event->globalPos(),
                    i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds;",
                          "<table>"
                          "<tr><td>LT:   </td><td>%1</td></tr>"
                          "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                          "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                          "</table>",
                          localTime.toString("hh:mm:ss AP"), QString::number(raDelta, 'f', 2),
                          QString::number(deDelta, 'f', 2)));
            }
            else
            {
                QToolTip::showText(
                    event->globalPos(),
                    i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds; %4 is RA Pulse in ms; %5 is DE Pulse in ms",
                          "<table>"
                          "<tr><td>LT:   </td><td>%1</td></tr>"
                          "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                          "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                          "<tr><td>RA Pulse:   </td><td>%4 ms</td></tr>"
                          "<tr><td>DE Pulse:   </td><td>%5 ms</td></tr>"
                          "</table>",
                          localTime.toString("hh:mm:ss AP"), QString::number(raDelta, 'f', 2),
                          QString::number(deDelta, 'f', 2), QString::number(raPulse, 'f', 2), QString::number(dePulse, 'f', 2))); //The pulses were divided by 100 before they were put on the graph.
            }
        }
        else
            QToolTip::hideText();

        driftGraph->replot();
    }
}

void Guide::buildOperationStack(GuideState operation)
{
    operationStack.clear();

    switch (operation)
    {
        case GUIDE_CAPTURE:
            if (Options::guideDarkFrameEnabled())
                operationStack.push(GUIDE_DARK);

            operationStack.push(GUIDE_CAPTURE);
            operationStack.push(GUIDE_SUBFRAME);
            break;

        case GUIDE_CALIBRATING:
            operationStack.push(GUIDE_CALIBRATING);
            if (guiderType == GUIDE_INTERNAL)
            {
                if (Options::guideDarkFrameEnabled())
                    operationStack.push(GUIDE_DARK);

                // Auto Star Selected Path
                if (Options::guideAutoStarEnabled())
                {
                    // If subframe is enabled and we need to auto select a star, then we need to make the final capture
                    // of the subframed image. This is only done if we aren't already subframed.
                    if (subFramed == false && Options::guideSubframeEnabled())
                        operationStack.push(GUIDE_CAPTURE);

                    // Do not subframe and auto-select star on Image Guiding mode
                    if (Options::imageGuidingEnabled() == false)
                    {
                        operationStack.push(GUIDE_SUBFRAME);
                        operationStack.push(GUIDE_STAR_SELECT);
                    }

                    operationStack.push(GUIDE_CAPTURE);

                    // If we are being ask to go full frame, let's do that first
                    if (subFramed == true && Options::guideSubframeEnabled() == false)
                        operationStack.push(GUIDE_SUBFRAME);
                }
                // Manual Star Selection Path
                else
                {
                    // In Image Guiding, we never need to subframe
                    if (Options::imageGuidingEnabled() == false)
                    {
                        // Final capture before we start calibrating
                        if (subFramed == false && Options::guideSubframeEnabled())
                            operationStack.push(GUIDE_CAPTURE);

                        // Subframe if required
                        operationStack.push(GUIDE_SUBFRAME);

                    }

                    // First capture an image
                    operationStack.push(GUIDE_CAPTURE);
                }

            }
            break;

        default:
            break;
    }
}

bool Guide::executeOperationStack()
{
    if (operationStack.isEmpty())
        return false;

    GuideState nextOperation = operationStack.pop();

    bool actionRequired = false;

    switch (nextOperation)
    {
        case GUIDE_SUBFRAME:
            actionRequired = executeOneOperation(nextOperation);
            break;

        case GUIDE_DARK:
            actionRequired = executeOneOperation(nextOperation);
            break;

        case GUIDE_CAPTURE:
            actionRequired = captureOneFrame();
            break;

        case GUIDE_STAR_SELECT:
            actionRequired = executeOneOperation(nextOperation);
            break;

        case GUIDE_CALIBRATING:
            if (guiderType == GUIDE_INTERNAL)
            {
                guider->setStarPosition(starCenter);
                dynamic_cast<InternalGuider *>(guider)->setImageGuideEnabled(Options::imageGuidingEnabled());

                // No need to calibrate
                if (Options::imageGuidingEnabled())
                {
                    setStatus(GUIDE_CALIBRATION_SUCESS);
                    break;
                }

                // Tracking must be engaged
                if (currentTelescope && currentTelescope->canControlTrack() && currentTelescope->isTracking() == false)
                    currentTelescope->setTrackEnabled(true);
            }

            if (guider->calibrate())
            {
                if (guiderType == GUIDE_INTERNAL)
                    disconnect(guideView, SIGNAL(trackingStarSelected(int, int)), this,
                               SLOT(setTrackingStar(int, int)));
                setBusy(true);
            }
            else
            {
                emit newStatus(GUIDE_CALIBRATION_ERROR);
                state = GUIDE_IDLE;
                appendLogText(i18n("Calibration failed to start."));
                setBusy(false);
            }
            break;

        default:
            break;
    }

    // If an additional action is required, return return and continue later
    if (actionRequired)
        return true;
    // Otherwise, continue processing the stack
    else
        return executeOperationStack();
}

bool Guide::executeOneOperation(GuideState operation)
{
    bool actionRequired = false;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    int subBinX, subBinY;
    targetChip->getBinning(&subBinX, &subBinY);

    switch (operation)
    {
        case GUIDE_SUBFRAME:
        {
            // Check if we need and can subframe
            if (subFramed == false && Options::guideSubframeEnabled() == true && targetChip->canSubframe())
            {
                int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
                targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

                int offset = boxSizeCombo->currentText().toInt() / subBinX;

                int x = starCenter.x();
                int y = starCenter.y();

                x     = (x - offset * 2) * subBinX;
                y     = (y - offset * 2) * subBinY;
                int w = offset * 4 * subBinX;
                int h = offset * 4 * subBinY;

                if (x < minX)
                    x = minX;
                if (y < minY)
                    y = minY;
                if ((x + w) > maxW)
                    w = maxW - x;
                if ((y + h) > maxH)
                    h = maxH - y;

                targetChip->setFrame(x, y, w, h);

                subFramed            = true;
                QVariantMap settings = frameSettings[targetChip];
                settings["x"]        = x;
                settings["y"]        = y;
                settings["w"]        = w;
                settings["h"]        = h;
                settings["binx"]     = subBinX;
                settings["biny"]     = subBinY;

                frameSettings[targetChip] = settings;

                starCenter.setX(w / (2 * subBinX));
                starCenter.setY(h / (2 * subBinX));
            }
            // Otherwise check if we are already subframed
            // and we need to go back to full frame
            // or if we need to go back to full frame since we need
            // to reaquire a star
            else if (subFramed &&
                     (Options::guideSubframeEnabled() == false ||
                      state == GUIDE_REACQUIRE))
            {
                targetChip->resetFrame();

                int x, y, w, h;
                targetChip->getFrame(&x, &y, &w, &h);

                QVariantMap settings;
                settings["x"]             = x;
                settings["y"]             = y;
                settings["w"]             = w;
                settings["h"]             = h;
                settings["binx"]          = 1;
                settings["biny"]          = 1;
                frameSettings[targetChip] = settings;

                subFramed = false;

                starCenter.setX(w / (2 * subBinX));
                starCenter.setY(h / (2 * subBinX));

                //starCenter.setX(0);
                //starCenter.setY(0);
            }
        }
        break;

        case GUIDE_DARK:
        {
            // Do we need to take a dark frame?
            if (Options::guideDarkFrameEnabled())
            {
                QVariantMap settings = frameSettings[targetChip];
                uint16_t offsetX     = settings["x"].toInt() / settings["binx"].toInt();
                uint16_t offsetY     = settings["y"].toInt() / settings["biny"].toInt();

                FITSData *darkData = DarkLibrary::Instance()->getDarkFrame(targetChip, exposureIN->value());

                connect(DarkLibrary::Instance(), &DarkLibrary::darkFrameCompleted, this, [&](bool completed)
                {
                    DarkLibrary::Instance()->disconnect(this);
                    if (completed != darkFrameCheck->isChecked())
                        setDarkFrameEnabled(completed);
                    if (completed)
                        setCaptureComplete();
                    else
                        abort();
                });
                connect(DarkLibrary::Instance(), &DarkLibrary::newLog, this, &Ekos::Guide::appendLogText);

                actionRequired = true;

                targetChip->setCaptureFilter(static_cast<FITSScale>(filterCombo->currentIndex()));

                if (darkData)
                    DarkLibrary::Instance()->subtract(darkData, guideView, targetChip->getCaptureFilter(), offsetX,
                                                      offsetY);
                else
                {
                    DarkLibrary::Instance()->captureAndSubtract(targetChip, guideView, exposureIN->value(), offsetX, offsetY);
                }
            }
        }
        break;

        case GUIDE_STAR_SELECT:
        {
            state = GUIDE_STAR_SELECT;
            emit newStatus(state);

            if (Options::guideAutoStarEnabled())
            {
                bool autoStarCaptured = internalGuider->selectAutoStar();
                if (autoStarCaptured)
                {
                    appendLogText(i18n("Auto star selected."));
                }
                else
                {
                    appendLogText(i18n("Failed to select an auto star."));
                    actionRequired = true;
                    state = GUIDE_CALIBRATION_ERROR;
                    emit newStatus(state);
                    setBusy(false);
                }
            }
            else
            {
                appendLogText(i18n("Select a guide star to calibrate."));
                actionRequired = true;
            }
        }
        break;

        default:
            break;
    }

    return actionRequired;
}

void Guide::processGuideOptions()
{
    if (Options::guiderType() != guiderType)
    {
        guiderType = static_cast<GuiderType>(Options::guiderType());
        setGuiderType(Options::guiderType());
    }
}

void Guide::showFITSViewer()
{
    FITSData *data = guideView->getImageData();
    if (data)
    {
        QUrl url = QUrl::fromLocalFile(data->filename());

        if (fv.isNull())
        {
            if (Options::singleWindowCapturedFITS())
                fv = KStars::Instance()->genericFITSViewer();
            else
            {
                fv = new FITSViewer(Options::independentWindowFITS() ? nullptr : KStars::Instance());
                KStars::Instance()->addFITSViewer(fv);
            }

            fv->addFITS(url);
            FITSView *currentView = fv->getCurrentView();
            if (currentView)
                currentView->getImageData()->setAutoRemoveTemporaryFITS(false);
        }
        else
            fv->updateFITS(url, 0);

        fv->show();
    }
}

void Guide::setExternalGuiderBLOBEnabled(bool enable)
{
    // Nothing to do if guider is internal
    if (guiderType == GUIDE_INTERNAL)
        return;

    if(!currentCCD)
        return;

    currentCCD->setBLOBEnabled(enable);

    if(currentCCD->isBLOBEnabled())
    {
        if (currentCCD->hasGuideHead() && guiderCombo->currentText().contains("Guider"))
            useGuideHead = true;
        else
            useGuideHead = false;

        ISD::CCDChip *targetChip =
            currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip)
        {
            targetChip->setImageView(guideView, FITS_GUIDE);
            targetChip->setCaptureMode(FITS_GUIDE);
        }
        syncCCDInfo();
    }

}

void Guide::ditherDirectly()
{
    double ditherPulse = Options::ditherNoGuidingPulse();

    // Randomize pulse length. It is equal to 50% of pulse length + random value up to 50%
    // e.g. if ditherPulse is 500ms then final pulse is = 250 + rand(0 to 250)
    int ra_msec  = static_cast<int>((static_cast<double>(rand()) / RAND_MAX) * ditherPulse / 2.0 +  ditherPulse / 2.0);
    int ra_polarity = (rand() % 2 == 0) ? 1 : -1;

    int de_msec  = static_cast<int>((static_cast<double>(rand()) / RAND_MAX) * ditherPulse / 2.0 +  ditherPulse / 2.0);
    int de_polarity = (rand() % 2 == 0) ? 1 : -1;

    qCInfo(KSTARS_EKOS_GUIDE) << "Starting non-guiding dither...";
    qCDebug(KSTARS_EKOS_GUIDE) << "dither ra_msec:" << ra_msec << "ra_polarity:" << ra_polarity << "de_msec:" << de_msec << "de_polarity:" << de_polarity;

    bool rc = sendPulse(ra_polarity > 0 ? RA_INC_DIR : RA_DEC_DIR, ra_msec, de_polarity > 0 ? DEC_INC_DIR : DEC_DEC_DIR, de_msec);

    if (rc)
    {
        qCInfo(KSTARS_EKOS_GUIDE) << "Non-guiding dither successful.";
        QTimer::singleShot( (ra_msec > de_msec ? ra_msec : de_msec) + Options::ditherSettle() * 1000 + 100, [this]()
        {
            emit newStatus(GUIDE_DITHERING_SUCCESS);
            state = GUIDE_IDLE;
        });
    }
    else
    {
        qCWarning(KSTARS_EKOS_GUIDE) << "Non-guiding dither failed.";
        emit newStatus(GUIDE_DITHERING_ERROR);
        state = GUIDE_IDLE;
    }
}

void Guide::updateTelescopeType(int index)
{
    if (currentCCD == nullptr)
        return;

    focal_length = (index == ISD::CCD::TELESCOPE_PRIMARY) ? primaryFL : guideFL;
    aperture = (index == ISD::CCD::TELESCOPE_PRIMARY) ? primaryAperture : guideAperture;

    Options::setGuideScopeType(index);

    syncTelescopeInfo();
}

void Guide::setDefaultST4(const QString &driver)
{
    Options::setDefaultST4Driver(driver);
}

void Guide::setDefaultCCD(const QString &ccd)
{
    if (guiderType == GUIDE_INTERNAL)
        Options::setDefaultGuideCCD(ccd);
}

void Guide::handleManualDither()
{
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    if (targetChip == nullptr)
        return;

    Ui::ManualDither ditherDialog;
    QDialog container(this);
    ditherDialog.setupUi(&container);

    if (guiderType != GUIDE_INTERNAL)
    {
        ditherDialog.coordinatesR->setEnabled(false);
        ditherDialog.x->setEnabled(false);
        ditherDialog.y->setEnabled(false);
    }

    int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
    targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

    ditherDialog.x->setMinimum(minX);
    ditherDialog.x->setMaximum(maxX);
    ditherDialog.y->setMinimum(minY);
    ditherDialog.y->setMaximum(maxY);

    ditherDialog.x->setValue(starCenter.x());
    ditherDialog.y->setValue(starCenter.y());

    if (container.exec() == QDialog::Accepted)
    {
        if (ditherDialog.magnitudeR->isChecked())
            guider->dither(ditherDialog.magnitude->value());
        else
        {
            dynamic_cast<InternalGuider *>(guider)->ditherXY(ditherDialog.x->value(), ditherDialog.y->value());
        }
    }
}

bool Guide::connectGuider()
{
    return guider->Connect();
}

bool Guide::disconnectGuider()
{
    return guider->Disconnect();
}

void Guide::initPlots()
{
    // Drift Graph Color Settings
    driftGraph->setBackground(QBrush(Qt::black));
    driftGraph->xAxis->setBasePen(QPen(Qt::white, 1));
    driftGraph->yAxis->setBasePen(QPen(Qt::white, 1));
    driftGraph->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    driftGraph->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    driftGraph->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    driftGraph->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    driftGraph->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    driftGraph->yAxis->grid()->setZeroLinePen(QPen(Qt::white, 1));
    driftGraph->xAxis->setBasePen(QPen(Qt::white, 1));
    driftGraph->yAxis->setBasePen(QPen(Qt::white, 1));
    driftGraph->yAxis2->setBasePen(QPen(Qt::white, 1));
    driftGraph->xAxis->setTickPen(QPen(Qt::white, 1));
    driftGraph->yAxis->setTickPen(QPen(Qt::white, 1));
    driftGraph->yAxis2->setTickPen(QPen(Qt::white, 1));
    driftGraph->xAxis->setSubTickPen(QPen(Qt::white, 1));
    driftGraph->yAxis->setSubTickPen(QPen(Qt::white, 1));
    driftGraph->yAxis2->setSubTickPen(QPen(Qt::white, 1));
    driftGraph->xAxis->setTickLabelColor(Qt::white);
    driftGraph->yAxis->setTickLabelColor(Qt::white);
    driftGraph->yAxis2->setTickLabelColor(Qt::white);
    driftGraph->xAxis->setLabelColor(Qt::white);
    driftGraph->yAxis->setLabelColor(Qt::white);
    driftGraph->yAxis2->setLabelColor(Qt::white);

    //Horizontal Axis Time Ticker Settings
    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%m:%s");
    driftGraph->xAxis->setTicker(timeTicker);

    //Vertical Axis Labels Settings
    driftGraph->yAxis2->setVisible(true);
    driftGraph->yAxis2->setTickLabels(true);
    driftGraph->yAxis->setLabelFont(QFont(font().family(), 10));
    driftGraph->yAxis2->setLabelFont(QFont(font().family(), 10));
    driftGraph->yAxis->setTickLabelFont(QFont(font().family(), 9));
    driftGraph->yAxis2->setTickLabelFont(QFont(font().family(), 9));
    driftGraph->yAxis->setLabelPadding(1);
    driftGraph->yAxis2->setLabelPadding(1);
    driftGraph->yAxis->setLabel(i18n("drift (arcsec)"));
    driftGraph->yAxis2->setLabel(i18n("pulse (ms)"));

    setupNSEWLabels();

    //Sets the default ranges
    driftGraph->xAxis->setRange(0, 60, Qt::AlignRight);
    driftGraph->yAxis->setRange(-3, 3);
    int scale = 50;  //This is a scaling value between the left and the right axes of the driftGraph, it could be stored in kstars kcfg
    correctionSlider->setValue(scale);
    driftGraph->yAxis2->setRange(-3 * scale, 3 * scale);

    //This sets up the legend
    driftGraph->legend->setVisible(true);
    driftGraph->legend->setFont(QFont("Helvetica", 9));
    driftGraph->legend->setTextColor(Qt::white);
    driftGraph->legend->setBrush(QBrush(Qt::black));
    driftGraph->legend->setFillOrder(QCPLegend::foColumnsFirst);
    driftGraph->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft | Qt::AlignBottom);

    // RA Curve
    driftGraph->addGraph(driftGraph->xAxis, driftGraph->yAxis);
    driftGraph->graph(0)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
    driftGraph->graph(0)->setName("RA");
    driftGraph->graph(0)->setLineStyle(QCPGraph::lsStepLeft);

    // DE Curve
    driftGraph->addGraph(driftGraph->xAxis, driftGraph->yAxis);
    driftGraph->graph(1)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
    driftGraph->graph(1)->setName("DE");
    driftGraph->graph(1)->setLineStyle(QCPGraph::lsStepLeft);

    // RA highlighted Point
    driftGraph->addGraph(driftGraph->xAxis, driftGraph->yAxis);
    driftGraph->graph(2)->setLineStyle(QCPGraph::lsNone);
    driftGraph->graph(2)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
    driftGraph->graph(2)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle, QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"), 2), QBrush(), 10));

    // DE highlighted Point
    driftGraph->addGraph(driftGraph->xAxis, driftGraph->yAxis);
    driftGraph->graph(3)->setLineStyle(QCPGraph::lsNone);
    driftGraph->graph(3)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
    driftGraph->graph(3)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle, QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"), 2), QBrush(), 10));

    // RA Pulse
    driftGraph->addGraph(driftGraph->xAxis, driftGraph->yAxis2);
    QColor raPulseColor(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"));
    raPulseColor.setAlpha(75);
    driftGraph->graph(4)->setPen(QPen(raPulseColor));
    driftGraph->graph(4)->setBrush(QBrush(raPulseColor, Qt::Dense4Pattern));
    driftGraph->graph(4)->setName("RA Pulse");
    driftGraph->graph(4)->setLineStyle(QCPGraph::lsStepLeft);

    // DEC Pulse
    driftGraph->addGraph(driftGraph->xAxis, driftGraph->yAxis2);
    QColor dePulseColor(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"));
    dePulseColor.setAlpha(75);
    driftGraph->graph(5)->setPen(QPen(dePulseColor));
    driftGraph->graph(5)->setBrush(QBrush(dePulseColor, Qt::Dense4Pattern));
    driftGraph->graph(5)->setName("DEC Pulse");
    driftGraph->graph(5)->setLineStyle(QCPGraph::lsStepLeft);

    //This will prevent the highlighted points and Pulses from showing up in the legend.
    driftGraph->legend->removeItem(5);
    driftGraph->legend->removeItem(4);
    driftGraph->legend->removeItem(3);
    driftGraph->legend->removeItem(2);
    //Dragging and zooming settings
    // make bottom axis transfer its range to the top axis if the graph gets zoomed:
    connect(driftGraph->xAxis,  static_cast<void(QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            driftGraph->xAxis2, static_cast<void(QCPAxis::*)(const QCPRange &)>(&QCPAxis::setRange));
    // update the second vertical axis properly if the graph gets zoomed.
    connect(driftGraph->yAxis, static_cast<void(QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            this, &Ekos::Guide::setCorrectionGraphScale);
    driftGraph->setInteractions(QCP::iRangeZoom);
    driftGraph->setInteraction(QCP::iRangeDrag, true);

    connect(driftGraph, &QCustomPlot::mouseMove, this, &Ekos::Guide::driftMouseOverLine);
    connect(driftGraph, &QCustomPlot::mousePress, this, &Ekos::Guide::driftMouseClicked);


    //drift plot
    double accuracyRadius = 2;

    driftPlot->setBackground(QBrush(Qt::black));
    driftPlot->setSelectionTolerance(10);

    driftPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    driftPlot->yAxis->setBasePen(QPen(Qt::white, 1));

    driftPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    driftPlot->yAxis->setTickPen(QPen(Qt::white, 1));

    driftPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    driftPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));

    driftPlot->xAxis->setTickLabelColor(Qt::white);
    driftPlot->yAxis->setTickLabelColor(Qt::white);

    driftPlot->xAxis->setLabelColor(Qt::white);
    driftPlot->yAxis->setLabelColor(Qt::white);

    driftPlot->xAxis->setLabelFont(QFont(font().family(), 10));
    driftPlot->yAxis->setLabelFont(QFont(font().family(), 10));
    driftPlot->xAxis->setTickLabelFont(QFont(font().family(), 9));
    driftPlot->yAxis->setTickLabelFont(QFont(font().family(), 9));

    driftPlot->xAxis->setLabelPadding(2);
    driftPlot->yAxis->setLabelPadding(2);

    driftPlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    driftPlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    driftPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    driftPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    driftPlot->xAxis->grid()->setZeroLinePen(QPen(Qt::gray));
    driftPlot->yAxis->grid()->setZeroLinePen(QPen(Qt::gray));

    driftPlot->xAxis->setLabel(i18n("dRA (arcsec)"));
    driftPlot->yAxis->setLabel(i18n("dDE (arcsec)"));

    driftPlot->xAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    driftPlot->yAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);

    driftPlot->setInteractions(QCP::iRangeZoom);
    driftPlot->setInteraction(QCP::iRangeDrag, true);

    driftPlot->addGraph();
    driftPlot->graph(0)->setLineStyle(QCPGraph::lsNone);
    driftPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssStar, Qt::gray, 5));

    driftPlot->addGraph();
    driftPlot->graph(1)->setLineStyle(QCPGraph::lsNone);
    driftPlot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlusCircle, QPen(Qt::yellow, 2), QBrush(), 10));

    connect(rightLayout, &QSplitter::splitterMoved, this, &Ekos::Guide::handleVerticalPlotSizeChange);
    connect(driftSplitter, &QSplitter::splitterMoved, this, &Ekos::Guide::handleHorizontalPlotSizeChange);

    //This sets the values of all the Graph Options that are stored.
    accuracyRadiusSpin->setValue(Options::guiderAccuracyThreshold());
    showRAPlotCheck->setChecked(Options::rADisplayedOnGuideGraph());
    showDECPlotCheck->setChecked(Options::dEDisplayedOnGuideGraph());
    showRACorrectionsCheck->setChecked(Options::rACorrDisplayedOnGuideGraph());
    showDECorrectionsCheck->setChecked(Options::dECorrDisplayedOnGuideGraph());

    //This sets the visibility of graph components to the stored values.
    driftGraph->graph(0)->setVisible(Options::rADisplayedOnGuideGraph()); //RA data
    driftGraph->graph(1)->setVisible(Options::dEDisplayedOnGuideGraph()); //DEC data
    driftGraph->graph(2)->setVisible(Options::rADisplayedOnGuideGraph()); //RA highlighted point
    driftGraph->graph(3)->setVisible(Options::dEDisplayedOnGuideGraph()); //DEC highlighted point
    driftGraph->graph(4)->setVisible(Options::rACorrDisplayedOnGuideGraph()); //RA Pulses
    driftGraph->graph(5)->setVisible(Options::dECorrDisplayedOnGuideGraph()); //DEC Pulses
    updateCorrectionsScaleVisibility();

    driftPlot->resize(190, 190);
    driftPlot->replot();

    buildTarget();
}

void Guide::initView()
{
    guideView = new FITSView(guideWidget, FITS_GUIDE);
    guideView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    guideView->setBaseSize(guideWidget->size());
    guideView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(guideView);
    guideWidget->setLayout(vlayout);
    connect(guideView, &FITSView::trackingStarSelected, this, &Ekos::Guide::setTrackingStar);
}

void Guide::initConnections()
{
    // Exposure Timeout
    captureTimeout.setSingleShot(true);
    connect(&captureTimeout, &QTimer::timeout, this, &Ekos::Guide::processCaptureTimeout);

    // Guiding Box Size
    connect(boxSizeCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &Ekos::Guide::updateTrackingBoxSize);

    // Guider CCD Selection
    connect(guiderCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated), this, &Ekos::Guide::setDefaultCCD);
    connect(guiderCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this,
            [&](int index)
    {
        if (guiderType == GUIDE_INTERNAL)
        {
            starCenter = QVector3D();
            checkCCD(index);
        }
    }
           );

    FOVScopeCombo->setCurrentIndex(Options::guideScopeType());
    connect(FOVScopeCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &Ekos::Guide::updateTelescopeType);

    // Dark Frame Check
    connect(darkFrameCheck, &QCheckBox::toggled, this, &Ekos::Guide::setDarkFrameEnabled);
    // Subframe check
    if(guiderType != GUIDE_PHD2) //For PHD2, this is handled in the configurePHD2Camera method
        connect(subFrameCheck, &QCheckBox::toggled, this, &Ekos::Guide::setSubFrameEnabled);
    // ST4 Selection
    connect(ST4Combo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated), [&](const QString & text)
    {
        setDefaultST4(text);
        setST4(text);
    });

    // Binning Combo Selection
    connect(binningCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &Ekos::Guide::updateCCDBin);

    // RA/DEC Enable directions
    connect(checkBox_DirRA, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirRA);
    connect(checkBox_DirDEC, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirDEC);

    // N/W and W/E direction enable
    connect(northControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(southControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(westControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(eastControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);

    // Auto star check
    connect(autoStarCheck, &QCheckBox::toggled, this, &Ekos::Guide::syncSettings);

    // Declination Swap
    connect(swapCheck, &QCheckBox::toggled, this, &Ekos::Guide::setDECSwap);

    // PID Control - Proportional Gain
    connect(spinBox_PropGainRA, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);
    connect(spinBox_PropGainDEC, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // PID Control - Integral Gain
    connect(spinBox_IntGainRA, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);
    connect(spinBox_IntGainDEC, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // PID Control - Derivative Gain
    connect(spinBox_DerGainRA, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);
    connect(spinBox_DerGainDEC, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // Max Pulse Duration (ms)
    connect(spinBox_MaxPulseRA, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);
    connect(spinBox_MaxPulseDEC, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // Min Pulse Duration (ms)
    connect(spinBox_MinPulseRA, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);
    connect(spinBox_MinPulseDEC, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // Capture
    connect(captureB, &QPushButton::clicked, this, [this]()
    {
        state = GUIDE_CAPTURE;
        emit newStatus(state);

        if(guiderType == GUIDE_PHD2)
        {
            configurePHD2Camera();
            if(phd2Guider->isCurrentCameraNotInEkos())
                appendLogText(i18n("The PHD2 camera is not available to Ekos, so you cannot see the captured images.  But you will still see the Guide Star Image when you guide."));
            else if(Options::guideSubframeEnabled())
            {
                appendLogText(i18n("To receive PHD2 images other than the Guide Star Image, SubFrame must be unchecked.  Unchecking it now to enable your image captures.  You can re-enable it before Guiding"));
                subFrameCheck->setChecked(false);
            }
            phd2Guider->captureSingleFrame();
        }
        else
            capture();
    });

    connect(loopB, &QPushButton::clicked, this, [this]()
    {
        state = GUIDE_LOOPING;
        emit newStatus(state);

        if(guiderType == GUIDE_PHD2)
        {
            configurePHD2Camera();
            if(phd2Guider->isCurrentCameraNotInEkos())
                appendLogText(i18n("The PHD2 camera is not available to Ekos, so you cannot see the captured images.  But you will still see the Guide Star Image when you guide."));
            else if(Options::guideSubframeEnabled())
            {
                appendLogText(i18n("To receive PHD2 images other than the Guide Star Image, SubFrame must be unchecked.  Unchecking it now to enable your image captures.  You can re-enable it before Guiding"));
                subFrameCheck->setChecked(false);
            }
            phd2Guider->loop();
            stopB->setEnabled(true);
        }
        else
            capture();
    });

    // Stop
    connect(stopB, &QPushButton::clicked, this, &Ekos::Guide::abort);

    // Clear Calibrate
    //connect(calibrateB, &QPushButton::clicked, this, &Ekos::Guide::calibrate()));
    connect(clearCalibrationB, &QPushButton::clicked, this, &Ekos::Guide::clearCalibration);

    // Guide
    connect(guideB, &QPushButton::clicked, this, &Ekos::Guide::guide);

    // Connect External Guide
    connect(externalConnectB, &QPushButton::clicked, this, [&]()
    {
        //setExternalGuiderBLOBEnabled(false);
        guider->Connect();
    });
    connect(externalDisconnectB, &QPushButton::clicked, this, [&]()
    {
        //setExternalGuiderBLOBEnabled(true);
        guider->Disconnect();
    });

    // Pulse Timer
    pulseTimer.setSingleShot(true);
    connect(&pulseTimer, &QTimer::timeout, this, &Ekos::Guide::capture);

    //This connects all the buttons and slider below the guide plots.
    connect(accuracyRadiusSpin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &Ekos::Guide::buildTarget);
    connect(guideSlider, &QSlider::sliderMoved, this, &Ekos::Guide::guideHistory);
    connect(latestCheck, &QCheckBox::toggled, this, &Ekos::Guide::setLatestGuidePoint);
    connect(showRAPlotCheck, &QCheckBox::toggled, this, &Ekos::Guide::toggleShowRAPlot);
    connect(showDECPlotCheck, &QCheckBox::toggled, this, &Ekos::Guide::toggleShowDEPlot);
    connect(showRACorrectionsCheck, &QCheckBox::toggled, this, &Ekos::Guide::toggleRACorrectionsPlot);
    connect(showDECorrectionsCheck, &QCheckBox::toggled, this, &Ekos::Guide::toggleDECorrectionsPlot);
    connect(correctionSlider, &QSlider::sliderMoved, this, &Ekos::Guide::setCorrectionGraphScale);

    connect(showGuideRateToolTipB, &QPushButton::clicked, [this]()
    {
        QToolTip::showText(showGuideRateToolTipB->mapToGlobal(QPoint(10, 10)),
                           showGuideRateToolTipB->toolTip(),
                           showGuideRateToolTipB);
    });

    connect(manualDitherB, &QPushButton::clicked, this, &Guide::handleManualDither);

    // Guiding Rate - Advisory only
    onInfoRateChanged(spinBox_GuideRate->value());
    connect(spinBox_GuideRate, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &Ekos::Guide::onInfoRateChanged);
}

void Guide::removeDevice(ISD::GDInterface *device)
{
    device->disconnect(this);
    if (currentTelescope && !strcmp(currentTelescope->getDeviceName(), device->getDeviceName()))
    {
        currentTelescope = nullptr;
    }
    else if (CCDs.contains(static_cast<ISD::CCD *>(device)))
    {
        CCDs.removeAll(static_cast<ISD::CCD *>(device));
        guiderCombo->removeItem(guiderCombo->findText(device->getDeviceName()));
        guiderCombo->removeItem(guiderCombo->findText(device->getDeviceName() + QString(" Guider")));
        if (CCDs.empty())
            currentCCD = nullptr;
        checkCCD();
    }

    auto st4 = std::find_if(ST4List.begin(), ST4List.end(), [device](ISD::ST4 * st)
    {
        return !strcmp(st->getDeviceName(), device->getDeviceName());
    });

    if (st4 != ST4List.end())
    {
        ST4List.removeOne(*st4);

        if (AODriver && !strcmp(device->getDeviceName(), AODriver->getDeviceName()))
            AODriver = nullptr;

        ST4Combo->removeItem(ST4Combo->findText(device->getDeviceName()));
        if (ST4List.empty())
        {
            ST4Driver = GuideDriver = nullptr;
        }
        else
        {
            setST4(ST4Combo->currentText());
        }
    }

}


}

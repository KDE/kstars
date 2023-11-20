/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include <QLoggingCategory>
#include <QDir>
#include <QVectorIterator>
#include <QVariant>
#include <QTableWidgetItem>
#include "fileutilitycameradata.h"
#include "optimalsubexposurecalculator.h"
#include "optimalexposuredetail.h"
#include <string>
#include <iostream>
#include <filesystem>
#include "fileutilitycameradatadialog.h"
#include "exposurecalculatordialog.h"
#include "./ui_exposurecalculatordialog.h"
#include <kspaths.h>
#include <ekos_capture_debug.h>

/*
 *
 *
 * https://www.astrobin.com/forum/c/astrophotography/deep-sky/robin-glover-talk-questioning-length-of-single-exposure/
 * http://astro.physics.uiowa.edu/~kaaret/2013f_29c137/Lab03_noise.html#:~:text=The%20read%20noise%20of%20the,removing%20hot%20and%20dead%20pixels
 *
 * Resource for DSLR read-noise:
 * https://www.photonstophotos.net/Charts/RN_ADU.htm
 *
 */

OptimalExposure::OptimalExposureDetail
aSubExposureDetail;  // Added during refactoring to simplify and better support the noise graph

ExposureCalculatorDialog::ExposureCalculatorDialog(QWidget *parent,
        double aPreferredSkyQualityValue,
        double aPreferredFocalRatioValue,
        const QString &aPreferredCameraId) :
    QDialog(parent),
    aPreferredSkyQualityValue(aPreferredSkyQualityValue),
    aPreferredFocalRatioValue(aPreferredFocalRatioValue),
    aPreferredCameraId(aPreferredCameraId),
    ui(new Ui::ExposureCalculatorDialog)
{
    ui->setupUi(this);

    ui->exposureDiffLabel->setText(QString("\u0394="));

    QStringList availableCameraFiles = OptimalExposure::FileUtilityCameraData::getAvailableCameraFilesList();

    if(availableCameraFiles.length() == 0)
    {
        // qCWarning(KSTARS_EKOS_CAPTURE) << "Exposure Calculator - No Camera data available, closing dialog";
        // QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
        qCWarning(KSTARS_EKOS_CAPTURE) << "Exposure Calculator - No Camera data available, opening camera data download dialog";
        FileUtilityCameraDataDialog aCameraDownloadDialog(this, aPreferredCameraId);
        aCameraDownloadDialog.setWindowModality(Qt::WindowModal);
        aCameraDownloadDialog.exec();
        // QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
        // Look again for files...
        availableCameraFiles = OptimalExposure::FileUtilityCameraData::getAvailableCameraFilesList();

    }


    if(availableCameraFiles.length() > 0)
    {
        ui->imagingCameraSelector->clear();
        refreshCameraSelector(ui, availableCameraFiles, aPreferredCameraId);

        ui->exposureCalculatorFrame->setEnabled(false);

        hideGainSelectionWidgets();

        ui->gainSelector->setMinimum(0);
        ui->gainSelector->setMaximum(100);
        ui->gainSelector->setValue(50);

        ui->indiFocalRatio->setMinimum(1.0);
        ui->indiFocalRatio->setMaximum(12.0);
        ui->indiFocalRatio->setSingleStep(0.1);
        // was coded to default as ui->indiFocalRatio->setValue(5.0);
        ui->indiFocalRatio->setValue(aPreferredFocalRatioValue);

        // Setup the "user" controls.
        ui->userSkyQuality->setMinimum(16.00);
        ui->userSkyQuality->setMaximum(22.00);
        ui->userSkyQuality->setSingleStep(0.01);
        // was coded ui->userSkyQuality->setValue(20.0);
        ui->userSkyQuality->setValue(aPreferredSkyQualityValue);

        ui->noiseTolerance->setMinimum(0.02);
        ui->noiseTolerance->setMaximum(200.00);
        ui->noiseTolerance->setSingleStep(0.25);
        ui->noiseTolerance->setValue(5.0);

        ui->filterBandwidth->setMinimum(2.8);
        ui->filterBandwidth->setMaximum(300);
        ui->filterBandwidth->setValue(300);
        ui->filterBandwidth->setSingleStep(0.1);



        /*
            Experimental compensation for filters on light the pollution calculation are a bit tricky.
            Part 1...

            An unfiltered camera may include some IR and UV, and be able to read a bandwidth of say 360nm (at a reasonably high QE %).

            But for simplicity, the filter compensation calculation will be based on the range for visible light, and use a nominal
            bandwidth of 300, (roughly the bandwidth of a typical luminance filter).

            The filter compensation factor that will be applied to light pollution will be the filter bandwidth (from the UI) / 300.
            This means that a typical luminance filter would produce a "nuetral" compensation of 1.0 (300 / 300).

            But the user interface will allow selection of wider bands for true "unfiltered" exposure calculations.  Example: by selecting a
            bandwith of 360, the light pollution compensation will 1.2, calculated as (360 / 300).  This is to recognize that light pollution
            may be entering the IR and UV range of an unfiltered camera sensor.

            A Luminance filter may only pass 300nm, so the filter compensaton value would be 1.0 (300 / 300)
            An RGB filter may only pass 100nm, so the filter compensaton value would be 0.3333 = (100 / 300)
            An SHO filter may only pass 3nm, so the filter compensaton value would be 0.0100 = (3 / 300)

            Filters will reduce bandwidth, but also slightly reduce tranmission within the range that they "pass".
            The values stated are only for demonstration and testing purposes, further research is needed.

        */

        // Set up plot colors of Sub Exposure (night friendly)
        ui->qCustomPlotSubExposure->setBackground(QBrush(Qt::black));
        ui->qCustomPlotSubExposure->xAxis->setBasePen(QPen(Qt::white, 1));
        ui->qCustomPlotSubExposure->yAxis->setBasePen(QPen(Qt::white, 1));
        ui->qCustomPlotSubExposure->xAxis->setTickPen(QPen(Qt::white, 1));
        ui->qCustomPlotSubExposure->yAxis->setTickPen(QPen(Qt::white, 1));
        ui->qCustomPlotSubExposure->xAxis->setSubTickPen(QPen(Qt::white, 1));
        ui->qCustomPlotSubExposure->yAxis->setSubTickPen(QPen(Qt::white, 1));
        ui->qCustomPlotSubExposure->xAxis->setTickLabelColor(Qt::white);
        ui->qCustomPlotSubExposure->yAxis->setTickLabelColor(Qt::white);
        ui->qCustomPlotSubExposure->xAxis->setLabelColor(Qt::white);
        ui->qCustomPlotSubExposure->yAxis->setLabelColor(Qt::white);

        ui->qCustomPlotSubExposure->xAxis->grid()->setPen(QPen(Qt::darkGray));
        ui->qCustomPlotSubExposure->yAxis->grid()->setPen(QPen(Qt::darkGray));

        ui->qCustomPlotSubExposure->xAxis->setLabel("Gain");

        ui->qCustomPlotSubExposure->yAxis->setLabel("Exposure Time");

        ui->qCustomPlotSubExposure->addGraph();


        // Set up plot colors of Integrated Image Noise (night friendly)
        ui->qCustomPlotIntegrationNoise->setBackground(QBrush(Qt::black));
        ui->qCustomPlotIntegrationNoise->xAxis->setBasePen(QPen(Qt::white, 1));
        ui->qCustomPlotIntegrationNoise->yAxis->setBasePen(QPen(Qt::white, 1));
        ui->qCustomPlotIntegrationNoise->xAxis->setTickPen(QPen(Qt::white, 1));
        ui->qCustomPlotIntegrationNoise->yAxis->setTickPen(QPen(Qt::white, 1));
        ui->qCustomPlotIntegrationNoise->xAxis->setSubTickPen(QPen(Qt::white, 1));
        ui->qCustomPlotIntegrationNoise->yAxis->setSubTickPen(QPen(Qt::white, 1));
        ui->qCustomPlotIntegrationNoise->xAxis->setTickLabelColor(Qt::white);
        ui->qCustomPlotIntegrationNoise->yAxis->setTickLabelColor(Qt::white);
        ui->qCustomPlotIntegrationNoise->xAxis->setLabelColor(Qt::white);
        ui->qCustomPlotIntegrationNoise->yAxis->setLabelColor(Qt::white);

        ui->qCustomPlotIntegrationNoise->xAxis->grid()->setPen(QPen(Qt::darkGray));
        ui->qCustomPlotIntegrationNoise->yAxis->grid()->setPen(QPen(Qt::darkGray));

        ui->qCustomPlotIntegrationNoise->addGraph(ui->qCustomPlotIntegrationNoise->xAxis, ui->qCustomPlotIntegrationNoise->yAxis);
        ui->qCustomPlotIntegrationNoise->graph(0)->setPen(QPen(Qt::yellow));
        ui->qCustomPlotIntegrationNoise->graph(0)->setName("Integration Time to Noise Ratio");
        ui->qCustomPlotIntegrationNoise->xAxis->setLabel("Stacked Exposures");
        ui->qCustomPlotIntegrationNoise->yAxis->setLabel("Noise Ratio");

        //        ui->qCustomPlotIntegrationNoise->addGraph(ui->qCustomPlotIntegrationNoise->xAxis, ui->qCustomPlotIntegrationNoise->yAxis);
        //        ui->qCustomPlotIntegrationNoise->graph(1)->setPen(QPen(Qt::green));
        //        ui->qCustomPlotIntegrationNoise->graph(1)->setName("Integration to Noise Ratio");
        //        ui->qCustomPlotIntegrationNoise->yAxis2->setLabel("Noise Ratio");




        connect(ui->imagingCameraSelector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
                &ExposureCalculatorDialog::applyInitialInputs);

        connect(ui->cameraReadModeSelector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
                &ExposureCalculatorDialog::handleUserAdjustment);

        connect(ui->gainSelector, QOverload<int>::of(&QSpinBox::valueChanged), this,
                &ExposureCalculatorDialog::handleUserAdjustment);

        connect(ui->userSkyQuality, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                &ExposureCalculatorDialog::handleUserAdjustment);

        connect(ui->indiFocalRatio, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                &ExposureCalculatorDialog::handleUserAdjustment);

        connect(ui->filterBandwidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                &ExposureCalculatorDialog::handleUserAdjustment);

        connect(ui->noiseTolerance, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                &ExposureCalculatorDialog::handleUserAdjustment);

        connect(ui->gainISODiscreteSelector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
                &ExposureCalculatorDialog::handleUserAdjustment);

        connect(ui->targetNoiseRatio, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                &ExposureCalculatorDialog::handleStackCalculation);

        // Hide the gain selector frames (until a camera is selected)
        hideGainSelectionWidgets();

        applyInitialInputs();

    }
    /*
        else
        {
            // qCWarning(KSTARS_EKOS_CAPTURE) << "Exposure Calculator - No Camera data available, closing dialog";
            // QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
            qCWarning(KSTARS_EKOS_CAPTURE) << "Exposure Calculator - No Camera data available, opening camera data download dialog";
            FileUtilityCameraDataDialog aCameraDownloadDialog(this, aPreferredCameraId);
            aCameraDownloadDialog.setWindowModality(Qt::WindowModal);
            aCameraDownloadDialog.exec();
            QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
        }
    */
}




int ExposureCalculatorDialog::getGainSelection(OptimalExposure::GainSelectionType aGainSelectionType)
{
    int aSelectedGain = 0;
    switch(aGainSelectionType)
    {

        case OptimalExposure::GAIN_SELECTION_TYPE_NORMAL:
            // aSelectedGain = ui->gainSlider->value();
            aSelectedGain = ui->gainSelector->value();
            break;

        case OptimalExposure::GAIN_SELECTION_TYPE_ISO_DISCRETE:
            // qCInfo(KSTARS_EKOS_CAPTURE) << " iso selector text: " << ui->isoDiscreteSelector->currentText();
            aSelectedGain = ui->gainISODiscreteSelector->currentText().toInt();
            break;

        case OptimalExposure::GAIN_SELECTION_TYPE_FIXED:
            // qCInfo(KSTARS_EKOS_CAPTURE) << "Fixed read-noise camera gain set to 0";
            aSelectedGain = 0;  // Fixed noise cameras have read noise data at 0 gain.
            break;

    }

    return(aSelectedGain);
}


QString skyQualityToBortleClassNumber(double anSQMValue)
{

    QString aBortleClassNumber;
    if(anSQMValue < 18.38)
    {
        aBortleClassNumber = "8 to 9";
    }
    else if(anSQMValue < 18.94)
    {
        aBortleClassNumber = "7";
    }
    else if(anSQMValue < 19.50)
    {
        aBortleClassNumber = "6";
    }
    else if(anSQMValue < 20.49)
    {
        aBortleClassNumber = "5";
    }
    else if(anSQMValue < 21.69)
    {
        aBortleClassNumber = "4";
    }
    else if(anSQMValue < 21.89)
    {
        aBortleClassNumber = "3";
    }
    else if(anSQMValue < 21.99)
    {
        aBortleClassNumber = "2";
    }
    else aBortleClassNumber = "1";

    return(aBortleClassNumber);
}

// calculate a Bortle style color based on SQM
QColor makeASkyQualityColor(double anSQMValue)
{
    QColor aSkyBrightnessColor;
    int aHueDegree, aSaturation, aValue;

    if(anSQMValue < 18.32)  // White Zone
    {
        aHueDegree = 40;
        aSaturation = 0;    // Saturation must move from
        aValue = 254;       // Value must move from 254 to 240
    }
    else if(anSQMValue < 18.44)  // From White Zone at 18.32 transitioning to Red Zone at 18.44
    {
        aHueDegree = 0;     // Hue is Red,
        // Saturation must transition from 0 to 255 as SQM moves from 18.32 to 18.44
        aSaturation = (int)(255 * ((anSQMValue - (double)18.32) / (18.44 - 18.32)));
        aValue = 254;
    }
    else if(anSQMValue < 21.82 )
    {
        // In the color range transitions hue of Bortle can be approximated with a polynomial
        aHueDegree = 17.351411032 * pow(anSQMValue, 4)
                     - 1384.0773705 * pow(anSQMValue, 3)
                     + 41383.66777 * pow(anSQMValue, 2)
                     - 549664.28976 * anSQMValue
                     + 2736244.0733;

        if(aHueDegree < 0) aHueDegree = 0;

        aSaturation = 255;
        aValue = 240;

    }
    else if(anSQMValue < 21.92) // Transition from Blue to Dark Gray between 21.82 and 21.92
    {
        aHueDegree = 240;
        // Saturation must transition from 255 to 0
        aSaturation = (int) ((2550 * (21.92 - anSQMValue)));
        // Value must transition from 240 to 100
        aValue = (int)(100 + (1400 * (21.92 - anSQMValue)));

    }
    else if(anSQMValue < 21.99)  // Dark gray zone
    {
        aHueDegree = 240;
        aSaturation = 0;
        aValue = 100;
    }
    else // Black zone should only be 21.99 and up
    {
        aHueDegree = 240;
        aSaturation = 0;
        aValue = 0;
    }

    // qCInfo(KSTARS_EKOS_CAPTURE) << "Sky Quality Color Hue: " << aHueDegree;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Sky Quality Color Saturation: " << aSaturation;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Sky Quality Color Value: " << aValue;

    aSkyBrightnessColor.setHsv(aHueDegree, aSaturation, aValue);

    return(aSkyBrightnessColor);
}


void refreshSkyQualityPresentation(Ui::ExposureCalculatorDialog *ui, double aSkyQualityValue)
{

    // qCInfo(KSTARS_EKOS_CAPTURE) << "\ta selected Sky Quality: " << aSkyQualityValue;
    QColor aSkyQualityColor = makeASkyQualityColor(aSkyQualityValue);

    ui->bortleScaleValue->setText(skyQualityToBortleClassNumber(aSkyQualityValue));

    // Update the skyQualityColor Widget
    QPalette pal = QPalette();

    pal.setColor(QPalette::Window, aSkyQualityColor);
    ui->skyQualityColor->setAutoFillBackground(true);
    ui->skyQualityColor->setPalette(pal);
    ui->skyQualityColor->show();

}


void ExposureCalculatorDialog::handleUserAdjustment()
{

    // This test for enabled was needed because dynamic changes to a
    // combo box during initialization of the calculator were firing
    // this method and prematurely triggering a calculation which was
    // crashing because the initialization was incomplete.

    if(ui->exposureCalculatorFrame->isEnabled())
    {
        // Recalculate and refresh the graph, with changed inputs from the ui
        QString aSelectedImagingCamera = ui->imagingCameraSelector->itemText(ui->imagingCameraSelector->currentIndex());

        // ui->cameraReadModeSelector->currentData()
        int aSelectedReadMode = ui->cameraReadModeSelector->currentData().toInt();

        double aFocalRatioValue = ui->indiFocalRatio->value();

        double aSkyQualityValue = ui->userSkyQuality->value();
        refreshSkyQualityPresentation(ui, aSkyQualityValue);

        double aNoiseTolerance = ui->noiseTolerance->value();

        // double aFilterCompensationValue = 1.0;
        double aFilterCompensationValue = ((double)ui->filterBandwidth->value() / (double)300);

        int aSelectedGainValue = getGainSelection(anOptimalSubExposureCalculator->getImagingCameraData().getGainSelectionType());


        // double aSelectedGainValue = ui->gainSlider->value();
        // qCInfo(KSTARS_EKOS_CAPTURE) << "\ta selected gain: " << aSelectedGainValue;

        calculateSubExposure(aNoiseTolerance, aSkyQualityValue, aFocalRatioValue, aFilterCompensationValue, aSelectedReadMode,
                             aSelectedGainValue);
    }
}

void ExposureCalculatorDialog::hideGainSelectionWidgets()
{
    ui->gainSelectionFrame->setEnabled(false);
    ui->gainSelectionFrame->setVisible(false);

    ui->gainSpinnerLabel->setVisible(false);
    ui->gainSelector->setVisible(false);
    ui->gainSelector->setEnabled(false);

    ui->gainISOSelectorLabel->setVisible(false);
    ui->gainISODiscreteSelector->setVisible(false);
    ui->gainISODiscreteSelector->setEnabled(false);

    ui->gainSelectionFixedLabel->setVisible(false);

    /*
    ui->gainSelectionISODiscreteFrame->setEnabled(false);
    ui->gainSelectionISODiscreteFrame->setVisible(false);
    ui->gainSelectionFixedFrame->setEnabled(false);
    ui->gainSelectionFixedFrame->setVisible(false);
    */
}


void ExposureCalculatorDialog::showGainSelectionNormalWidgets()
{
    ui->gainSpinnerLabel->setVisible(true);
    ui->gainSelector->setEnabled(true);
    ui->gainSelector->setVisible(true);

    ui->gainSelectionFrame->setEnabled(true);
    ui->gainSelectionFrame->setVisible(true);
}

void ExposureCalculatorDialog::showGainSelectionISODiscreteWidgets()
{
    ui->gainISOSelectorLabel->setVisible(true);
    ui->gainISODiscreteSelector->setEnabled(true);
    ui->gainISODiscreteSelector->setVisible(true);

    ui->gainSelectionFrame->setEnabled(true);
    ui->gainSelectionFrame->setVisible(true);
}

void ExposureCalculatorDialog::showGainSelectionFixedWidgets()
{
    ui->gainSelectionFixedLabel->setVisible(true);

    ui->gainSelectionFrame->setEnabled(true);
    ui->gainSelectionFrame->setVisible(true);
}


void ExposureCalculatorDialog::applyInitialInputs()
{
    ui->exposureCalculatorFrame->setEnabled(false);

    // QString aSelectedImagingCameraName = ui->imagingCameraSelector->itemText(ui->imagingCameraSelector->currentIndex());
    QString aSelectedImagingCameraFileName = ui->imagingCameraSelector->itemData(
                ui->imagingCameraSelector->currentIndex()).toString();


    // qCInfo(KSTARS_EKOS_CAPTURE) << ui->cameraReadModeSelector->currentData();

    int aSelectedReadMode = 0;

    double aFocalRatioValue = ui->indiFocalRatio->value();
    double aSkyQualityValue = ui->userSkyQuality->value();
    refreshSkyQualityPresentation(ui, aSkyQualityValue);

    double aNoiseTolerance = ui->noiseTolerance->value();

    // double aFilterCompensationValue = 1.0;
    // double aFilterCompensationValue = ui->filterSelection->itemData(ui->filterSelection->currentIndex()).toDouble();
    double aFilterCompensationValue = ((double)ui->filterBandwidth->value() / (double)300);

    initializeSubExposureCalculator(aNoiseTolerance, aSkyQualityValue, aFocalRatioValue, aFilterCompensationValue,
                                    aSelectedImagingCameraFileName);

    int aSelectedGainValue = ui->gainSelector->value();

    calculateSubExposure(aNoiseTolerance, aSkyQualityValue, aFocalRatioValue, aFilterCompensationValue, aSelectedReadMode,
                         aSelectedGainValue);

    ui->exposureCalculatorFrame->setEnabled(true);

}


void plotIntegratedNoise(Ui::ExposureCalculatorDialog *ui,
                         OptimalExposure::OptimalExposureDetail *subExposureDetail)
{

    ui->qCustomPlotIntegrationNoise->graph()->data()->clear();

    double aCoefficient = (subExposureDetail->getSubExposureTime() / subExposureDetail->getExposureTotalNoise());
    /*
        qCInfo(KSTARS_EKOS_CAPTURE) << "Noise Ratio Function: Noise Ratio = " << aCoefficient << " * Sqrt(Exposure Count)";

        qCInfo(KSTARS_EKOS_CAPTURE) << "Differential of Noise Ratio Function: = " << aCoefficient << " / (2 * Sqrt(Exposure Count)";

        qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Count Function (for desired Noise Ratio):"
                                    << "Exposure Count = (Noise Ratio / " << aCoefficient << ") ^2  = pow(Noise Ratio / " << aCoefficient << ", 2)";
    */
    double aTargetNoiseRatio = ui->targetNoiseRatio->value();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Target Noise Ratio: " << aTargetNoiseRatio;

    int aRequiredExposureCount = std::max(1, (int)(pow((aTargetNoiseRatio / aCoefficient), 2)));
    ui->exposureCount->setText(QString::number(aRequiredExposureCount));

    double aDifferential = aCoefficient / (2 * sqrt(aRequiredExposureCount));
    ui->exposureCountDifferential->setText(QString::number(aDifferential, 'f', 2));


    ui->qCustomPlotIntegrationNoise->graph()->data()->clear();

    // ui->qCustomPlotIntegrationNoise
    // ui->qCustomPlotIntegrationNoise->graph(0)->setData(ExposureCount, noise);

    QVector<double> xValue((aRequiredExposureCount * 2) + 1), yValue((aRequiredExposureCount * 2) + 1);
    for (int exposureCount = 1; exposureCount < (aRequiredExposureCount * 2) + 1; exposureCount++)
    {
        xValue[exposureCount] = exposureCount;
        yValue[exposureCount] = aCoefficient * pow(exposureCount, 0.5);
    }

    ui->qCustomPlotIntegrationNoise->graph(0)->setData(xValue, yValue);

    ui->qCustomPlotIntegrationNoise->xAxis->setRange(0, aRequiredExposureCount * 2);
    ui->qCustomPlotIntegrationNoise->yAxis->setRange(0, yValue[yValue.size() - 1]);

    // Also add a graph with a vertical line to show the computed integration
    ui->qCustomPlotIntegrationNoise->addGraph();

    QVector<double> selectedIntegrationSizeX(2), selectedIntegrationSizeY(2);
    selectedIntegrationSizeX[0] = aRequiredExposureCount;
    selectedIntegrationSizeY[0] = 0;
    selectedIntegrationSizeX[1] = aRequiredExposureCount;
    selectedIntegrationSizeY[1] = aTargetNoiseRatio;
    ui->qCustomPlotIntegrationNoise->graph(1)->setData(selectedIntegrationSizeX, selectedIntegrationSizeY);

    QPen penSelectedIntegrationSize;
    penSelectedIntegrationSize.setWidth(1);
    // penSelectedIntegrationSize.setColor(QColor(180, 0, 0));
    // On the black background need more contrast
    penSelectedIntegrationSize.setColor(QColor(240, 0, 0));

    ui->qCustomPlotIntegrationNoise->graph(1)->setPen(penSelectedIntegrationSize);

    ui->qCustomPlotIntegrationNoise->graph(1)->setScatterStyle(QCPScatterStyle::ssCircle);

    ui->qCustomPlotIntegrationNoise->graph()->rescaleAxes(true);
    ui->qCustomPlotIntegrationNoise->replot();

}

// Slot for adjustments made to desired noise ratio that require a refresh of the NR graph
void ExposureCalculatorDialog::handleStackCalculation()
{
    plotIntegratedNoise(ui, &aSubExposureDetail);
}


void plotSubExposureEnvelope(Ui::ExposureCalculatorDialog *ui,
                             OptimalExposure::OptimalSubExposureCalculator *anOptimalSubExposureCalculator,
                             OptimalExposure::OptimalExposureDetail *subExposureDetail)
{

    OptimalExposure::CameraExposureEnvelope aCameraExposureEnvelope =
        anOptimalSubExposureCalculator->calculateCameraExposureEnvelope();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Envelope has a set of: " << aCameraExposureEnvelope.getASubExposureVector().size();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Envelope has a minimum Exposure Time of " << aCameraExposureEnvelope.getExposureTimeMin();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Envelope has a maximum Exposure Time of " << aCameraExposureEnvelope.getExposureTimeMax();

    // anOptimalSubExposureCalculator->getImagingCameraData()

    // Reset the graph axis (But maybe this was not necessary,
    ui->qCustomPlotSubExposure->xAxis->setRange(anOptimalSubExposureCalculator->getImagingCameraData().getGainMin(),
            anOptimalSubExposureCalculator->getImagingCameraData().getGainMax());
    // But for the exposure yAxis include a bit of a margin so that data is not encoaching on the axis.
    ui->qCustomPlotSubExposure->yAxis->setRange(aCameraExposureEnvelope.getExposureTimeMin() - 10,
            aCameraExposureEnvelope.getExposureTimeMax() + 10);
    ui->qCustomPlotSubExposure->replot();

    // Prepare for the exposure line plot, move the data to parallel arrays for the custom plotter
    QVector<double> gain(aCameraExposureEnvelope.getASubExposureVector().size()),
            exposuretime(aCameraExposureEnvelope.getASubExposureVector().size());
    for(int index = 0; index < aCameraExposureEnvelope.getASubExposureVector().size(); index++)
    {
        OptimalExposure::CalculatedGainSubExposureTime aGainExposureTime = aCameraExposureEnvelope.getASubExposureVector()[index];
        gain[index] = (double)aGainExposureTime.getSubExposureGain();
        exposuretime[index] = aGainExposureTime.getSubExposureTime();
    }
    ui->qCustomPlotSubExposure->graph()->data()->clear();

    ui->qCustomPlotSubExposure->graph(0)->setData(gain, exposuretime);

    // Also add a graph with a vertical line to show the selected gain...
    ui->qCustomPlotSubExposure->addGraph();

    QVector<double> selectedExposureX(2), selectedExposureY(2);
    selectedExposureX[0] = subExposureDetail->getSelectedGain();
    selectedExposureY[0] = 0;
    selectedExposureX[1] = subExposureDetail->getSelectedGain();
    selectedExposureY[1] = subExposureDetail->getSubExposureTime();
    ui->qCustomPlotSubExposure->graph(1)->setData(selectedExposureX, selectedExposureY);

    QPen penExposureEnvelope;
    penExposureEnvelope.setWidth(1);
    // penExposureEnvelope.setColor(QColor(0, 180, 180));
    // On the black background need more contrast
    penExposureEnvelope.setColor(QColor(0, 220, 220));
    ui->qCustomPlotSubExposure->graph(0)->setPen(penExposureEnvelope);

    QPen penSelectedExposure;
    penSelectedExposure.setWidth(1);
    // penSelectedExposure.setColor(QColor(180, 0, 0));
    // On the black background need more contrast
    penSelectedExposure.setColor(QColor(240, 0, 0));

    ui->qCustomPlotSubExposure->graph(1)->setPen(penSelectedExposure);

    ui->qCustomPlotSubExposure->graph(1)->setScatterStyle(QCPScatterStyle::ssCircle);

    // extend the x-axis slightly so that the markers aren't hidden at the extreme edges
    ui->qCustomPlotSubExposure->xAxis->setRange(anOptimalSubExposureCalculator->getImagingCameraData().getGainMin() - 5,
            anOptimalSubExposureCalculator->getImagingCameraData().getGainMax() + 5);
    // force the y-axis to start at 0, (sometimes the auto rescale was making the y-axis range start a negative value
    ui->qCustomPlotSubExposure->yAxis->setRange(0, aCameraExposureEnvelope.getExposureTimeMax());

    ui->qCustomPlotSubExposure->graph()->rescaleAxes(true);
    ui->qCustomPlotSubExposure->replot();

}

void ExposureCalculatorDialog::initializeSubExposureCalculator(double aNoiseTolerance, double aSkyQualityValue,
        double aFocalRatioValue, double aFilterCompensationValue, QString aSelectedImagingCameraName)
{
    // qCInfo(KSTARS_EKOS_CAPTURE) << "initializeSubExposureComputer";
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taNoiseTolerance: " << aNoiseTolerance;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taSkyQualityValue: " << aSkyQualityValue;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taFocalRatioValue: " << aFocalRatioValue;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taFilterCompensation: " << aFilterCompensationValue;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taSelectedImagingCamera: " << aSelectedImagingCameraName;

    //    QVector<int> *aGainSelectionRange = new QVector<int>();

    //    QVector<OptimalExposure::CameraGainReadNoise> *aCameraGainReadNoiseVector
    //        = new QVector<OptimalExposure::CameraGainReadNoise>();

    //    QVector<OptimalExposure::CameraGainReadMode>  *aCameraGainReadModeVector
    //        = new QVector<OptimalExposure::CameraGainReadMode>();

    //    // Initialize with some default values before attempting to load from file

    //    anImagingCameraData = new OptimalExposure::ImagingCameraData(aSelectedImagingCameraName, OptimalExposure::SENSORTYPE_COLOR,
    //                OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *aGainSelectionRange, *aCameraGainReadModeVector);

    anImagingCameraData = new OptimalExposure::ImagingCameraData();
    // Load camera data from file
    OptimalExposure::FileUtilityCameraData::readCameraDataFile(aSelectedImagingCameraName, anImagingCameraData);

    // qCInfo(KSTARS_EKOS_CAPTURE) << "Loaded Imaging Camera Data for " + anImagingCameraData->getCameraId();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Gain Selection Type " +  QString::number(anImagingCameraData->getGainSelectionType());

    // qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Read Mode Vector Size " + QString::number(
    //  anImagingCameraData->getCameraGainReadModeVector().size());

    switch(anImagingCameraData->getSensorType())
    {
        case OptimalExposure::SENSORTYPE_COLOR:
            ui->SensorType->setText("Color");
            break;
        case OptimalExposure::SENSORTYPE_MONOCHROME:
            ui->SensorType->setText("Mono");
            break;
    }

    ui->cameraReadModeSelector->clear();
    foreach(OptimalExposure::CameraGainReadMode aReadMode, anImagingCameraData->getCameraGainReadModeVector())
    {
        QString readModeText = QString::number(aReadMode.getCameraGainReadModeNumber()) + " : " +
                               aReadMode.getCameraGainReadModeName();
        ui->cameraReadModeSelector->addItem(readModeText, aReadMode.getCameraGainReadModeNumber());
    }
    if(anImagingCameraData->getCameraGainReadModeVector().size() > 1)
    {
        ui->cameraReadModeSelector->setEnabled(true);
    }
    else
    {
        ui->cameraReadModeSelector->setEnabled(false);
    }


    // qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Gain Read-Noise Vector Size "
    // + QString::number(anImagingCameraData->getCameraGainReadNoiseVector().size());



    switch( anImagingCameraData->getGainSelectionType() )
    {
        case OptimalExposure::GAIN_SELECTION_TYPE_FIXED:
            // qCInfo(KSTARS_EKOS_CAPTURE) << "Gain Selection Type: GAIN_SELECTION_TYPE_FIXED";
            hideGainSelectionWidgets();
            showGainSelectionFixedWidgets();

            break;

        case OptimalExposure::GAIN_SELECTION_TYPE_ISO_DISCRETE:
            // qCInfo(KSTARS_EKOS_CAPTURE) << "Gain Selection Type: GAIN_SELECTION_TYPE_ISO_DISCRETE";
            hideGainSelectionWidgets();

            ui->gainISODiscreteSelector->clear();
            // Load the ISO Combo from the camera data
            foreach(int isoSetting, anImagingCameraData->getGainSelectionRange())
            {
                ui->gainISODiscreteSelector->addItem(QString::number(isoSetting));
            }
            ui->gainISODiscreteSelector->setCurrentIndex(0);

            // qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Data Gain min " +  QString::number(anImagingCameraData->getGainMin());
            // qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Data Gain max " +  QString::number(anImagingCameraData->getGainMax());

            showGainSelectionISODiscreteWidgets();

            break;

        case OptimalExposure::GAIN_SELECTION_TYPE_NORMAL:
            // qCInfo(KSTARS_EKOS_CAPTURE) << "Gain Selection Type: GAIN_SELECTION_TYPE_NORMAL";

            hideGainSelectionWidgets();
            showGainSelectionNormalWidgets();
            // qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Data Gain min " +  QString::number(anImagingCameraData->getGainMin());
            // qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Data Gain max " +  QString::number(anImagingCameraData->getGainMax());
            break;

    }

    anOptimalSubExposureCalculator = new OptimalExposure::OptimalSubExposureCalculator(aNoiseTolerance, aSkyQualityValue,
            aFocalRatioValue, aFilterCompensationValue, *anImagingCameraData);

    // qCInfo(KSTARS_EKOS_CAPTURE) << "Calculating... ";
    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Noise Tolerance " << anOptimalSubExposureCalculator->getANoiseTolerance();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Sky Quality " << anOptimalSubExposureCalculator->getASkyQuality();

    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Focal Ratio " << anOptimalSubExposureCalculator->getAFocalRatio();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Filter Compensation Value (ignored): " << anOptimalSubExposureCalculator->getAFilterCompensation();

    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Camera Gain Min " << anOptimalSubExposureCalculator->getImagingCameraData().getGainMin();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Camera Gain Max " << anOptimalSubExposureCalculator->getImagingCameraData().getGainMax();

}


void refreshStackTable(Ui::ExposureCalculatorDialog *ui,
                       OptimalExposure::OptimalExposureDetail *subExposureDetail)
{
    QTableWidget *resultStackTable = ui->exposureStackResult;

    int stackSummarySize =  subExposureDetail->getStackSummary().size();
    resultStackTable->setRowCount(stackSummarySize);

    for(int stackSummaryIndex = 0; stackSummaryIndex < stackSummarySize; stackSummaryIndex++)
    {
        OptimalExposure::OptimalExposureStack anOptimalExposureStack = subExposureDetail->getStackSummary()[stackSummaryIndex];

        resultStackTable->setItem(stackSummaryIndex, 0,
                                  new QTableWidgetItem(QString::number(anOptimalExposureStack.getPlannedTime())));
        resultStackTable->item(stackSummaryIndex, 0)->setTextAlignment(Qt::AlignCenter);

        resultStackTable->setItem(stackSummaryIndex, 1,
                                  new QTableWidgetItem(QString::number(anOptimalExposureStack.getExposureCount())));
        resultStackTable->item(stackSummaryIndex, 1)->setTextAlignment(Qt::AlignRight);

        resultStackTable->setItem(stackSummaryIndex, 2,
                                  new QTableWidgetItem(QString::number(anOptimalExposureStack.getStackTime())));
        resultStackTable->item(stackSummaryIndex, 2)->setTextAlignment(Qt::AlignRight);

        resultStackTable->setItem(stackSummaryIndex, 3,
                                  new QTableWidgetItem(QString::number(anOptimalExposureStack.getStackTotalNoise(), 'f', 2)));
        resultStackTable->item(stackSummaryIndex, 3)->setTextAlignment(Qt::AlignRight);

        double ratio = anOptimalExposureStack.getStackTime() / anOptimalExposureStack.getStackTotalNoise();
        resultStackTable->setItem(stackSummaryIndex, 4, new QTableWidgetItem(QString::number(ratio, 'f', 2)));
        resultStackTable->item(stackSummaryIndex, 4)->setTextAlignment(Qt::AlignRight);

        resultStackTable->setRowHeight(stackSummaryIndex, 22);
        /*
                qCInfo(KSTARS_EKOS_CAPTURE) << "Stack info: Hours: " << anOptimalExposureStack.getPlannedTime()
                                            << " Exposure Count: " << anOptimalExposureStack.getExposureCount()
                                            << " Stack Time: " << anOptimalExposureStack.getStackTime()
                                            << " Stack Total Noise: " << anOptimalExposureStack.getStackTotalNoise();
        */

        /*
             2023-10 Add plot of the ratio of Total Noise to Stack Integration Time into new plot widget qCustomPlotIntegrationNoise
        */

    }
    resultStackTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

}


void ExposureCalculatorDialog::calculateSubExposure(double aNoiseTolerance, double aSkyQualityValue,
        double aFocalRatioValue, double aFilterCompensationValue, int aSelectedReadMode, int aSelectedGainValue)
{

    anOptimalSubExposureCalculator->setANoiseTolerance(aNoiseTolerance);
    anOptimalSubExposureCalculator->setASkyQuality(aSkyQualityValue);
    anOptimalSubExposureCalculator->setAFocalRatio(aFocalRatioValue);
    anOptimalSubExposureCalculator->setAFilterCompensation(aFilterCompensationValue);
    anOptimalSubExposureCalculator->setASelectedCameraReadMode(aSelectedReadMode);
    anOptimalSubExposureCalculator->setASelectedGain(aSelectedGainValue);


    // qCInfo(KSTARS_EKOS_CAPTURE) << "initializeSubExposureComputer";
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taNoiseTolerance: " << aNoiseTolerance;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taSkyQualityValue: " << aSkyQualityValue;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taFocalRatioValue: " << aFocalRatioValue;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taFilterCompensation: (ignored) " << aFilterCompensationValue;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\taSelectedGainValue: " << aSelectedGainValue;

    anOptimalSubExposureCalculator->setAFilterCompensation(aFilterCompensationValue);

    // qCInfo(KSTARS_EKOS_CAPTURE) << "Calculating... ";
    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Noise Tolerance " << anOptimalSubExposureCalculator->getANoiseTolerance();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Sky Quality " << anOptimalSubExposureCalculator->getASkyQuality();

    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Focal Ratio " << anOptimalSubExposureCalculator->getAFocalRatio();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Filter Compensation Value (ignored): " << anOptimalSubExposureCalculator->getAFilterCompensation();

    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Camera Gain Min " << anOptimalSubExposureCalculator->getImagingCameraData().getGainMin();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "A Camera Gain Max " << anOptimalSubExposureCalculator->getImagingCameraData().getGainMax();


    OptimalExposure::CameraExposureEnvelope aCameraExposureEnvelope =
        anOptimalSubExposureCalculator->calculateCameraExposureEnvelope();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Envelope has a set of: " << aCameraExposureEnvelope.getASubExposureVector().size();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Envelope has a minimum Exposure Time of " << aCameraExposureEnvelope.getExposureTimeMin();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Envelope has a maximum Exposure Time of " << aCameraExposureEnvelope.getExposureTimeMax();


    //OptimalExposure::OptimalExposureDetail subExposureDetail = anOptimalSubExposureCalculator->calculateSubExposureDetail(
    //            aSelectedGainValue);


    aSubExposureDetail = anOptimalSubExposureCalculator->calculateSubExposureDetail();
    // Get the exposure details into the ui
    //ui->exposureCalculatonResult.

    plotSubExposureEnvelope(ui, anOptimalSubExposureCalculator, &aSubExposureDetail);

    if(ui->gainSelector->isEnabled())
    {
        // realignGainSlider();
        ui->gainSelector->setMaximum(anOptimalSubExposureCalculator->getImagingCameraData().getGainMax());
        ui->gainSelector->setMinimum(anOptimalSubExposureCalculator->getImagingCameraData().getGainMin());
    }

    ui->subExposureTime->setText(QString::number(aSubExposureDetail.getSubExposureTime(), 'f', 2));
    ui->subPollutionElectrons->setText(QString::number(aSubExposureDetail.getExposurePollutionElectrons(), 'f', 0));
    ui->subShotNoise->setText(QString::number(aSubExposureDetail.getExposureShotNoise(), 'f', 2));
    ui->subTotalNoise->setText(QString::number(aSubExposureDetail.getExposureTotalNoise(), 'f', 2));


    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Pollution Electrons: " << subExposureDetail.getExposurePollutionElectrons();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Shot Noise: " << subExposureDetail.getExposureShotNoise();
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Total Noise: " << subExposureDetail.getExposureTotalNoise();


    QTableWidget *resultStackTable = ui->exposureStackResult;
    resultStackTable->setColumnCount(5);
    resultStackTable->verticalHeader()->setVisible(false);

    QStringList stackDetailHeaders;
    stackDetailHeaders << "Planned Hours" << "Exposure Count" << "Stack Time" << "Noise" << "Ratio";
    resultStackTable->setHorizontalHeaderLabels(stackDetailHeaders);
    resultStackTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter | (Qt::Alignment)Qt::TextWordWrap);
    resultStackTable->horizontalHeader()->setFixedHeight(32);

    resultStackTable->horizontalHeaderItem(1)->setToolTip("Sub-exposure count in stacked image");
    resultStackTable->horizontalHeaderItem(2)->setToolTip("Integration time of stacked image (seconds)");
    resultStackTable->horizontalHeaderItem(3)->setToolTip("Total Noise in Stacked Image");
    resultStackTable->horizontalHeaderItem(4)->setToolTip("Integration time to noise ratio (potential quality)");

    /*
    double initializedTargetNoiseRatio = ceil((100.0 * aSubExposureDetail.getSubExposureTime()) /
                                         aSubExposureDetail.getExposureTotalNoise()) / 10.0;

    // Reinitialize the time/noise input in the stack calculator to produce a stack of 100 images.
    ui->targetNoiseRatio->setValue(initializedTargetNoiseRatio);

    */
    refreshStackTable(ui, &aSubExposureDetail);

    plotIntegratedNoise(ui, &aSubExposureDetail);

}



void ExposureCalculatorDialog::refreshCameraSelector(Ui::ExposureCalculatorDialog *ui,
        QStringList availableCameraFileNames, const QString aPreferredCameraId)
{
    // Present the aCameraId in a way that hopfully matches the cameraId from the driver
    // but set the full path in the combo box data as a QVariant
    // Retrievable as:
    // QString filePath = ui->imagingCameraSelector->itemData(index).toString();

    int preferredIndex = 0;
    ui->imagingCameraSelector->clear();

    /*
     *  2023-10-05 Added sorting to the filelist, but the full path is included in this
     *  list, and since camera data can come from either the applicaton folder, or a user local folder
     *  the sort result can produce two groupings of sorted camera ids.
     *  In Linux, files from the user local folder will appear first in the QCombo.
    */
    availableCameraFileNames.sort();

    foreach(QString filename, availableCameraFileNames)
    {
        QString aCameraId = OptimalExposure::FileUtilityCameraData::cameraDataFileNameToCameraId(filename);

        // qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Filename: " << filename << " Camera Id:" << aCameraId;

        ui->imagingCameraSelector->addItem(aCameraId, filename);
        if(aPreferredCameraId != nullptr && aPreferredCameraId.length() > 0)
        {
            if(aCameraId == aPreferredCameraId)
                preferredIndex = ui->imagingCameraSelector->count() - 1;
        }
    }

    ui->imagingCameraSelector->setCurrentIndex(preferredIndex);

}


ExposureCalculatorDialog::~ExposureCalculatorDialog()
{
    delete ui;
}

void ExposureCalculatorDialog::on_downloadCameraB_clicked()
{
    // User may want to add more camera files.
    FileUtilityCameraDataDialog aCameraDownloadDialog(this, aPreferredCameraId);
    aCameraDownloadDialog.setWindowModality(Qt::WindowModal);
    aCameraDownloadDialog.exec();

    // Using refresh is causing an error because the combobox->clear is
    // making the selection change.  Need to resolve this.
    // but for now, if a user adds more cameras they will be available
    // in the exposure calculator on the next start.
    // refreshCameraSelector(ui, aPreferredCameraId);

}


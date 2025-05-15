/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QLoggingCategory>
#include <cmath>
#include "optimalsubexposurecalculator.h"
#include "imagingcameradata.h"
#include "calculatedgainsubexposuretime.h"
#include "cameraexposureenvelope.h"
#include "optimalexposuredetail.h"
#include <ekos_capture_debug.h>

namespace OptimalExposure
{

/*
 *  For UI presentation provide two calculation methods.
 *  1) A method for the overall calculation for
 *  exposure times over the range of gain read-noise pairs.
 *      This method would be called when changes are made to:
 *          Camera selection, Focal Ratio, Sky Quality, Filter Compensation, or Noise Tolerance
 *      This method will return an obect (CameraExposureEnvelope) containing (for ui presentation)
 *          lightPollutionElectronBaseRate, lightPollutionForOpticFocalRatio,
 *          a vector of gain sub-exposure pairs, and int values the max and min exposure time.
 *      This method is intented to be used to present the graphical display of exposures for the camera,
 *      based on a specifc SQM.  The SQM can be adjusted in the UI, and an this method will be used to refresh ui.
 *
 *  2) A method for a calculation of an exposure time (OptimalExposureDetail) at a specific gain.
 *      This method is intented to be used to present a specific exposure calculation, and
 *      resulting shot and stack noise information.  This method will interpolate for a
 *      read-noise value (between camera gain read-noise pairs), and will be called when
 *      a change is made to selected Gain on the UI.
 *      This method will return an object that conains:
 *          Precise Exposure Time in (interpolated), Shot pollution electrons, Exposure shot noise, Exposure total noise
 *          a Vector for stacks of 1 to n hours, which includes Exposure Count, Stack Total Time, Stack total noise
 *
 */

/*
 *
 * More work is needed for the calculation of effect of a Filter on exposure noise and time.
 * Original effort (in Java) used an estimage of the spectrum bandwidth passed by a filter. For
 * example: on broadband filters for a one shot color camera:
 *      Optolong l-Pro apprears to pass about 165 nanometers.
 *      Optolong l-Enhance apprears to pass only about 33 nanometers.
 *
 * In this code the filter compensation has is applied as a reducer of the calulation of the
 * light pollution rate for the optic. For example, the filter compensation to the light pollution
 * would be 165 / 300 for an l-Pro filter.
 *
 * But this filter compensatoin approach is imprecise & and does not reflect reality. It might
 * be better to analyze a filter for its ability to block the distinct emmission lines found
 * in light pollution:
 *
 *      Hg	435.8, 546.1, 577, 578.1
 *      Na	598, 589.6, 615.4, 616.1
 *
 * And finally, the use of 300 nanometers (the bandwidth of the visible light spectrum) as the
 * basis for filter compensation may not be accurate. An unfiltered camera sensor may be able
 * to "see" a spectrum that is much more broad.  Is an unfiltered sensor collecting more noise
 * from beyond the range of the visible spectrum?  But other discussions on web forums regarding
 * Dr. Glovers calculations appear to use 300 as the basis for filter compensation.
 *
 */

OptimalSubExposureCalculator::OptimalSubExposureCalculator(
    double aNoiseTolerance, double aSkyQuality, double aFocalRatio, double aFilterCompensation,
    ImagingCameraData &anImagingCameraData) :
    aNoiseTolerance(aNoiseTolerance),
    aSkyQuality(aSkyQuality),
    aFocalRatio(aFocalRatio),
    aFilterCompensation(aFilterCompensation),
    anImagingCameraData(anImagingCameraData) {}



double OptimalSubExposureCalculator::getASkyQuality()
{
    return aSkyQuality;
}

void OptimalSubExposureCalculator::setASkyQuality(double newASkyQuality)
{
    aSkyQuality = newASkyQuality;
}

double OptimalSubExposureCalculator::getANoiseTolerance()
{
    return aNoiseTolerance;
}

void OptimalSubExposureCalculator::setANoiseTolerance(double newANoiseTolerance)
{
    aNoiseTolerance = newANoiseTolerance;
}

double OptimalSubExposureCalculator::getAFocalRatio()
{
    return aFocalRatio;
}

void OptimalSubExposureCalculator::setAFocalRatio(double newAFocalRatio)
{
    aFocalRatio = newAFocalRatio;
}

double OptimalSubExposureCalculator::getAFilterCompensation()
{
    return aFilterCompensation;
}

void OptimalSubExposureCalculator::setAFilterCompensation(double newAFilterCompensation)
{
    aFilterCompensation = newAFilterCompensation;
}

ImagingCameraData &OptimalSubExposureCalculator::getImagingCameraData()
{
    return anImagingCameraData;
}

void OptimalSubExposureCalculator::setImagingCameraData(ImagingCameraData &newanImagingCameraData)
{
    anImagingCameraData = newanImagingCameraData;
}

int OptimalSubExposureCalculator::getASelectedGain()
{
    return aSelectedGain;
}

void OptimalSubExposureCalculator::setASelectedGain(int newASelectedGain)
{
    aSelectedGain = newASelectedGain;
}

int OptimalSubExposureCalculator::getASelectedCameraReadMode() const
{
    return aSelectedCameraReadMode;
}

void OptimalSubExposureCalculator::setASelectedCameraReadMode(int newASelectedCameraReadMode)
{
    aSelectedCameraReadMode = newASelectedCameraReadMode;
}



QVector<CalculatedGainSubExposureTime> OptimalSubExposureCalculator::calculateGainSubExposureVector(double cFactor,
        double lightPollutionForOpticFocalRatio)
{
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Calculating gain sub-exposure vector: ";
    QVector<CalculatedGainSubExposureTime> aCalculatedGainSubExposureTimeVector;

    OptimalExposure::CameraGainReadMode aSelectedReadMode =
        anImagingCameraData.getCameraGainReadModeVector()[aSelectedCameraReadMode];
    // qCInfo(KSTARS_EKOS_CAPTURE) << "\t with camera read mode: "
    //  << aSelectedReadMode.getCameraGainReadModeName();

    QVector<OptimalExposure::CameraGainReadNoise> aCameraGainReadNoiseVector
        = aSelectedReadMode.getCameraGainReadNoiseVector();

    for(QVector<OptimalExposure::CameraGainReadNoise>::iterator rn = aCameraGainReadNoiseVector.begin();
            rn != aCameraGainReadNoiseVector.end(); ++rn)
    {
        int aGain = rn->getGain();
        // qCInfo(KSTARS_EKOS_CAPTURE) << "At a gain of: " << aGain;
        double aReadNoise = rn->getReadNoise();
        // qCInfo(KSTARS_EKOS_CAPTURE) << "\t camera read-noise is: " << aReadNoise;

        // initial sub exposure.
        double anOptimalSubExposure = 0.0;


        switch(anImagingCameraData.getSensorType())
        {
            case SENSORTYPE_MONOCHROME:
                anOptimalSubExposure =  cFactor * pow(aReadNoise, 2) / lightPollutionForOpticFocalRatio;
                break;

            case SENSORTYPE_COLOR:
                anOptimalSubExposure = (double)3.0 * cFactor * pow(aReadNoise, 2) / lightPollutionForOpticFocalRatio;
                break;
        }

        // qCInfo(KSTARS_EKOS_CAPTURE) << "anOptimalSubExposure is: " << anOptimalSubExposure;

        CalculatedGainSubExposureTime *aSubExposureTime = new CalculatedGainSubExposureTime(aGain, anOptimalSubExposure);

        aCalculatedGainSubExposureTimeVector.append(*aSubExposureTime);

    }

    return aCalculatedGainSubExposureTimeVector;
}
/*
    double OptimalSubExposureCalculator::calculateLightPolutionForOpticFocalRatio(double lightPollutionElectronBaseRate, double aFocalRatio) {
    // double lightPollutionRateForOptic =  lightPollutionElectronBaseRate * java.lang.Math.pow(anOptic.getFocalRatio(),-2);
        return((double) (lightPollutionElectronBaseRate * pow(aFocalRatio,-2)));
    }
*/

double OptimalSubExposureCalculator::calculateLightPolutionForOpticFocalRatio(double lightPollutionElectronBaseRate,
        double aFocalRatio, double aFilterCompensation)
{
    // double lightPollutionRateForOptic =
    //  lightPollutionElectronBaseRate
    //  * java.lang.Math.pow(anOptic.getFocalRatio(),-2);
    return(((double) (lightPollutionElectronBaseRate * pow(aFocalRatio, -2))) * aFilterCompensation);
}

double OptimalSubExposureCalculator::calculateCFactor(double aNoiseTolerance)
{
    // cFactor = 1/( (((100+allowableNoiseIncreasePercent)/100)^2) -1)

    return((double) (1 / ( pow((((double)100 + (double) aNoiseTolerance) / (double)100), (double)2) - 1)));

}

double OptimalSubExposureCalculator::calculateLightPollutionElectronBaseRate(double skyQuality)
{
	/* Old Methodology
    
    // Conversion curve fitted from Dr Glover data
    double base = std::stod("1.25286030612621E+27");
    double power = (double) -19.3234809465887;
    // New version of Dr Glover's function calculates the Electron rate at thet pixel level
	// and requires pixel size in microns and QE of sensor.
    // double baseV2 = 1009110388.7838
    // Calculation of an initial electron rate will
    // use a new fitted curve based upon a 1.0 micon pixel and 100% QE
    // curve appears to be f(sqm) = 1009110388.7838 exp (-0.921471189594521 * sqm)
    return(base * pow(skyQuality, power));
    
    */
    ImagingCameraData cameraData = getImagingCameraData();
    double pixelSize = cameraData.getCameraPixelSize();
	double QE = cameraData.getCameraQuantumEfficiency();

	return (double)(3.341e6 * QE * 300 * pow(10, -0.4*skyQuality) * pow(pixelSize, 2));
	
    
}

OptimalExposure::CameraExposureEnvelope OptimalSubExposureCalculator::calculateCameraExposureEnvelope()
{
    /*
        This method calculates the exposures for each gain setting found in the camera gain readnoise table.
        It is used to refresh the ui presentation, in prparation for a calculation of sub-exposure data with a
        specific (probably interpolated) read-noise value.
    */

    double lightPollutionElectronBaseRate = OptimalSubExposureCalculator::calculateLightPollutionElectronBaseRate(aSkyQuality);
    /*
    qCInfo(KSTARS_EKOS_CAPTURE) << "Calculating CameraExposureEnvelope..."
                                << "Using Sky Quality: " << aSkyQuality
                                << "\nConverted to lightPollutionElectronBaseRate: " << lightPollutionElectronBaseRate
                                << "Using an Optical Focal Ratio: " << aFocalRatio;
    */
    double lightPollutionForOpticFocalRatio = calculateLightPolutionForOpticFocalRatio(lightPollutionElectronBaseRate,
            aFocalRatio, aFilterCompensation);
    /*
    qCInfo(KSTARS_EKOS_CAPTURE)
            << "\tResulting in an Light Pollution Rate for the Optic of: "
            << lightPollutionForOpticFocalRatio;
    */
    // qCDebug(KSTARS_EKOS_CAPTURE) << "Using a camera Id: " << anImagingCameraData.getCameraId();
    OptimalExposure::CameraGainReadMode aSelectedReadMode =
        anImagingCameraData.getCameraGainReadModeVector()[aSelectedCameraReadMode];
    /*
    qCInfo(KSTARS_EKOS_CAPTURE) << "\t with camera read mode: "
                                << aSelectedReadMode.getCameraGainReadModeName()
                                << "\tWith a read-noise table of : "
                                << aSelectedReadMode.getCameraGainReadNoiseVector().size() << " values";
    */
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Using a Noise Tolerance of: " << aNoiseTolerance;

    double cFactor = calculateCFactor(aNoiseTolerance);
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Calculated CFactor is: " << cFactor;

    QVector<CalculatedGainSubExposureTime> aSubExposureTimeVector = calculateGainSubExposureVector(cFactor,
            lightPollutionForOpticFocalRatio);

    double exposureTimeMin = 99999999999.9;
    double exposureTimeMax = -1.0;
    // Iterate the times to capture and store the min and max exposure times.
    // (But the QCustomPlot can rescale, so this may be unnecessary
    for(QVector<OptimalExposure::CalculatedGainSubExposureTime>::iterator oe = aSubExposureTimeVector.begin();
            oe != aSubExposureTimeVector.end(); ++oe)
    {
        if(exposureTimeMin > oe->getSubExposureTime()) exposureTimeMin = oe->getSubExposureTime();
        if(exposureTimeMax < oe->getSubExposureTime()) exposureTimeMax = oe->getSubExposureTime();
    }

    CameraExposureEnvelope aCameraExposureEnvelope = CameraExposureEnvelope(
                lightPollutionElectronBaseRate,
                lightPollutionForOpticFocalRatio,
                aSubExposureTimeVector,
                exposureTimeMin,
                exposureTimeMax);

    return(aCameraExposureEnvelope);
}

// OptimalExposure::OptimalExposureDetail OptimalSubExposureCalculator::calculateSubExposureDetail(int aSelectedGainValue)
OptimalExposure::OptimalExposureDetail OptimalSubExposureCalculator::calculateSubExposureDetail()
{
    /*
        This method calculates some details for a sub-exposure. It will interpolate between
        points in the camera read-noise table, to find a reasonable appoximation of the read-noise
        for a non-listed gain value.
    */

    // qCDebug(KSTARS_EKOS_CAPTURE) << "aSelectedGainValue: " << aSelectedGainValue;

    int aSelectedGain = getASelectedGain();

    // Look for a matching gain from the camera gain read-noise table, or identify a bracket for interpolation.
    // Interpolation may result in slight errors when the read-noise data is curve. (there's probably a better way to code this)
    double aReadNoise = 100.0;  // defaulted high just to make an error in the interpolation obvious
    bool matched = false;
    int lowerReadNoiseIndex = 0;

    OptimalExposure::CameraGainReadMode aSelectedReadMode =
        anImagingCameraData.getCameraGainReadModeVector()[aSelectedCameraReadMode];

    QVector<OptimalExposure::CameraGainReadNoise> aCameraGainReadNoiseVector =
        aSelectedReadMode.getCameraGainReadNoiseVector();



    for(int readNoiseIndex = 0; readNoiseIndex < aSelectedReadMode.getCameraGainReadNoiseVector().size(); readNoiseIndex++)
    {
        if(!matched)
        {
            CameraGainReadNoise aCameraGainReadNoise = aSelectedReadMode.getCameraGainReadNoiseVector()[readNoiseIndex];
            if(aCameraGainReadNoise.getGain() == aSelectedGain)
            {
                matched = true;
                // qCInfo(KSTARS_EKOS_CAPTURE) << " matched a camera gain ";
                aReadNoise = aCameraGainReadNoise.getReadNoise();
                break;
            }
            if(aCameraGainReadNoise.getGain() < aSelectedGain)
            {
                lowerReadNoiseIndex = readNoiseIndex;
            }
        }
    }

    if(!matched)
    {
        int upperReadNoiseIndex = lowerReadNoiseIndex + 1;
        // qCInfo(KSTARS_EKOS_CAPTURE)
        //  << "Interpolating read noise gain bracket: " << lowerReadNoiseIndex
        //  << " and " << upperReadNoiseIndex;

        if (lowerReadNoiseIndex < aSelectedReadMode.getCameraGainReadNoiseVector().size() - 1)
        {

            CameraGainReadNoise aLowerIndexCameraReadNoise = aSelectedReadMode.getCameraGainReadNoiseVector()[lowerReadNoiseIndex];
            CameraGainReadNoise anUpperIndexCameraReadNoise = aSelectedReadMode.getCameraGainReadNoiseVector()[upperReadNoiseIndex];

            // interpolate a read-noise value
            double m = (anUpperIndexCameraReadNoise.getReadNoise() - aLowerIndexCameraReadNoise.getReadNoise()) /
                       (anUpperIndexCameraReadNoise.getGain() - aLowerIndexCameraReadNoise.getGain());
            aReadNoise = aLowerIndexCameraReadNoise.getReadNoise() + (m * (aSelectedGain - aLowerIndexCameraReadNoise.getGain()));
            // qCInfo(KSTARS_EKOS_CAPTURE)
            //        << "The camera read-noise for the selected gain value is interpolated to: " << aReadNoise;
        }
        else
        {
            // qCInfo(KSTARS_EKOS_CAPTURE) << " using max camera gain ";
            int maxIndex = aSelectedReadMode.getCameraGainReadNoiseVector().size() - 1;
            CameraGainReadNoise aMaxIndexCameraReadNoise = aSelectedReadMode.getCameraGainReadNoiseVector()[maxIndex];
            aReadNoise = aMaxIndexCameraReadNoise.getReadNoise();
        }
    }
    else
    {
        // qCInfo(KSTARS_EKOS_CAPTURE)
        // << "The camera read-noise for the selected gain value was matched: " << aReadNoise;
    }

    double anOptimalSubExposure = 0.0;

    double lightPollutionElectronBaseRate = OptimalSubExposureCalculator::calculateLightPollutionElectronBaseRate(aSkyQuality);
    double lightPollutionForOpticFocalRatio = calculateLightPolutionForOpticFocalRatio(lightPollutionElectronBaseRate,
            aFocalRatio, aFilterCompensation);
    double cFactor = calculateCFactor(aNoiseTolerance);

    switch(anImagingCameraData.getSensorType())
    {
        case SENSORTYPE_MONOCHROME:
            anOptimalSubExposure =  cFactor * pow(aReadNoise, 2) / lightPollutionForOpticFocalRatio;
            break;

        case SENSORTYPE_COLOR:
            anOptimalSubExposure = (double)3.0 * cFactor * pow(aReadNoise, 2) / lightPollutionForOpticFocalRatio;
            break;
    }

    // qCInfo(KSTARS_EKOS_CAPTURE) << "an Optimal Sub Exposure at gain: "
    //  << aSelectedGainValue << " is " << anOptimalSubExposure << " seconds";

    // Add the single exposure noise calcs to the response
    double anExposurePollutionElectrons = lightPollutionForOpticFocalRatio * anOptimalSubExposure;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Pollution Electrons: " << anExposurePollutionElectrons;

    double anExposureShotNoise = sqrt(lightPollutionForOpticFocalRatio * anOptimalSubExposure);
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Shot Noise: " << anExposureShotNoise;

    double anExposureTotalNoise = sqrt( pow(aReadNoise, 2)
                                        + lightPollutionForOpticFocalRatio * anOptimalSubExposure);
    // qCInfo(KSTARS_EKOS_CAPTURE) << "Exposure Total Noise: " << anExposureTotalNoise;

    // Need to build and populate the stack estimations
    QVector<OptimalExposureStack> aStackSummary;

    // Loop through planned sessions for hours of stacking
    for(int sessionHours = 1; sessionHours < 41; sessionHours++)
    {
        // rounding up to ensure that exposure count is at least "sessionHours" of data.
        int anExposureCount = (int) ceil( (double)(sessionHours * 3600) /
                                          anOptimalSubExposure );

        int aStackTime = anExposureCount * anOptimalSubExposure;
        double aStackTotalNoise = sqrt( (double)anExposureCount * pow(aReadNoise, 2)
                                        + lightPollutionForOpticFocalRatio * (anExposureCount * anOptimalSubExposure));

        OptimalExposureStack *anOptimalExposureStack
            = new OptimalExposureStack(sessionHours, anExposureCount, aStackTime, aStackTotalNoise);

        /*
                qCInfo(KSTARS_EKOS_CAPTURE) << sessionHours << "\t" << anExposureCount
                                            << "\t" << aStackTime << "\t" << aStackTotalNoise
                                            << "\t" << aStackTime / aStackTotalNoise;

                //  << aSelectedGainValue << " is " << anOptimalSubExposure << " seconds";
        */
        aStackSummary.push_back(*anOptimalExposureStack);


    }

    OptimalExposureDetail anOptimalExposureDetail = OptimalExposureDetail(getASelectedGain(), anOptimalSubExposure,
            anExposurePollutionElectrons, anExposureShotNoise,  anExposureTotalNoise, aStackSummary);

    return(anOptimalExposureDetail);
}


}


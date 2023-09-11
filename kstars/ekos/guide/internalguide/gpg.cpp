/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gpg.h"
#include <vector>

#include "guidestars.h"
#include "Options.h"
#include "ekos_guide_debug.h"

#include "calibration.h"

#include <QElapsedTimer>

namespace
{

// Don't have GPG model large errors. Instantaneous large errors are probably not due to
// periodic error, but rather one-offs.
constexpr double MAX_ARCSEC_ERROR = 5.0;

// If we skip this many samples, reset gpg.
constexpr int MAX_SKIPPED_SAMPLES = 4;

// Fills parameters from the KStars options.
void getGPGParameters(GaussianProcessGuider::guide_parameters *parameters)
{
    // Parameters from standard options.
    parameters->control_gain_                      = Options::rAProportionalGain();
    // We need a calibration to determine min move. This is re-set in computePulse() below.
    parameters->min_move_                          = 0.25;

    // Parameters from GPG-specific options.
    parameters->min_periods_for_inference_         = Options::gPGMinPeriodsForInference();
    parameters->SE0KLengthScale_                   = Options::gPGSE0KLengthScale();
    parameters->SE0KSignalVariance_                = Options::gPGSE0KSignalVariance();
    parameters->PKLengthScale_                     = Options::gPGPKLengthScale();
    parameters->PKPeriodLength_                    = Options::gPGPeriod();
    parameters->PKSignalVariance_                  = Options::gPGPKSignalVariance();
    parameters->SE1KLengthScale_                   = Options::gPGSE1KLengthScale();
    parameters->SE1KSignalVariance_                = Options::gPGSE1KSignalVariance();
    parameters->min_periods_for_period_estimation_ = Options::gPGMinPeriodsForPeriodEstimate();
    parameters->points_for_approximation_          = Options::gPGPointsForApproximation();
    parameters->prediction_gain_                   = Options::gPGpWeight();
    parameters->compute_period_                    = Options::gPGEstimatePeriod();
}

// Returns the SNR returned by guideStars, or if guideStars is null (e.g. we aren't
// running SEP MultiStar, then just 50, as this isn't a critical parameter.
double getSNR(const GuideStars *guideStars, const double raArcsecError)
{
    if (fabs(raArcsecError) > MAX_ARCSEC_ERROR)
        return 1.0;

    // If no SEP MultiStar, just fudge the SNR. Won't be a big deal.
    if (guideStars == nullptr)
        return 50.0;
    else
        return guideStars->getGuideStarSNR();
}

const QString directionStr(GuideDirection dir)
{
    switch (dir)
    {
        case RA_DEC_DIR:
            return "Decrease RA";
        case RA_INC_DIR:
            return "Increase RA";
        case DEC_DEC_DIR:
            return "Decrease DEC";
        case DEC_INC_DIR:
            return "Increase DEC";
        default:
            return "NO DIR";
    }
}

}  // namespace

double GPG::predictionContribution()
{
    if (gpg.get() == nullptr)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Nullptr in predictionContribution()";
        return 0.0;
    }
    return gpg->getPredictionContribution();
}

void GPG::updateParameters()
{
    // Parameters would be set when the gpg stars up.
    if (gpg.get() == nullptr) return;

    GaussianProcessGuider::guide_parameters parameters;
    getGPGParameters(&parameters);
    gpg->SetControlGain(parameters.control_gain_);
    gpg->SetPeriodLengthsInference(parameters.min_periods_for_inference_);
    gpg->SetMinMove(parameters.min_move_);
    gpg->SetPeriodLengthsPeriodEstimation(parameters.min_periods_for_period_estimation_);
    gpg->SetNumPointsForApproximation(parameters.points_for_approximation_);
    gpg->SetPredictionGain(parameters.prediction_gain_);
    gpg->SetBoolComputePeriod(parameters.compute_period_);

    // The GPG header really should be in a namespace so NumParameters
    // is not in a global namespace.
    std::vector<double> hyperparameters(NumParameters);
    hyperparameters[SE0KLengthScale]    = parameters.SE0KLengthScale_;
    hyperparameters[SE0KSignalVariance] = parameters.SE0KSignalVariance_;
    hyperparameters[PKLengthScale]      = parameters.PKLengthScale_;
    hyperparameters[PKSignalVariance]   = parameters.PKSignalVariance_;
    hyperparameters[SE1KLengthScale]    = parameters.SE1KLengthScale_;
    hyperparameters[SE1KSignalVariance] = parameters.SE1KSignalVariance_;
    hyperparameters[PKPeriodLength]     = parameters.PKPeriodLength_;
    gpg->SetGPHyperparameters(hyperparameters);
}


GPG::GPG()
{
    GaussianProcessGuider::guide_parameters parameters;
    getGPGParameters(&parameters);
    gpg.reset(new GaussianProcessGuider(parameters));
    reset();
}

void GPG::reset()
{
    gpgSamples = 0;
    gpgSkippedSamples = 0;
    gpg->reset();
    qCDebug(KSTARS_EKOS_GUIDE) << "Resetting GPG";
}

void GPG::startDithering(double dx, double dy, const Calibration &cal)
{
    // convert the x and y offsets to RA and DEC offsets.
    double raPixels, decPixels;
    cal.rotateToRaDec(dx, dy, &raPixels, &decPixels);

    // The 1000 in the denominator below converts gear-milliseconds into gear-seconds,
    // which is the unit GPG wants. Since calibration uses arcseconds, we convert using
    // arcseconds/pixel, though that's inexact when the pixels aren't square.
    // amount = (pixels * ms/pixel) / 1000;
    const double amount = raPixels * cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel() / 1000.0;

    qCDebug(KSTARS_EKOS_GUIDE) << "GPG Dither started. Gear-seconds =" << amount << "x,y was: " << dx << dy;
    gpg->GuidingDithered(amount, 1.0);
}

void GPG::ditheringSettled(bool success)
{
    gpg->GuidingDitherSettleDone(success);
    if (success)
        qCDebug(KSTARS_EKOS_GUIDE) << "GPG Dither done (success)";
    else
        qCDebug(KSTARS_EKOS_GUIDE) << "GPG Dither done (failed)";
}

void GPG::suspended(const GuiderUtils::Vector &guideStarPosition,
                    const GuiderUtils::Vector &reticlePosition,
                    GuideStars *guideStars,
                    const Calibration &cal)
{
    constexpr int MaxGpgSamplesForReset = 25;
    // We just reset the gpg if there's not enough samples to make
    // it worth our while to keep it going. We're probably worse off
    // to start it and then feed it bad samples.
    if (gpgSamples < MaxGpgSamplesForReset)
    {
        reset();
        return;
    }

    const GuiderUtils::Vector arc_star = cal.convertToArcseconds(guideStarPosition);
    const GuiderUtils::Vector arc_reticle = cal.convertToArcseconds(reticlePosition);
    const GuiderUtils::Vector star_drift =  cal.rotateToRaDec(arc_star - arc_reticle);

    double gpgInput = star_drift.x;
    if (guideStars != nullptr)
    {
        double multiStarRADrift, multiStarDECDrift;
        if (guideStars->getDrift(sqrt(star_drift.x * star_drift.x + star_drift.y * star_drift.y),
                                 reticlePosition.x, reticlePosition.y,
                                 &multiStarRADrift, &multiStarDECDrift))
            gpgInput = multiStarRADrift;
    }

    // Temporarily set the control & prediction gains to 0,
    // since we won't be guiding while suspended.
    double predictionGain = gpg->GetPredictionGain();
    gpg->SetPredictionGain(0.0);
    double controlGain = gpg->GetControlGain();
    gpg->SetControlGain(0.0);

    QElapsedTimer gpgTimer;
    gpgTimer.restart();
    const double gpgResult = gpg->result(gpgInput, getSNR(guideStars, gpgInput), Options::guideExposure());
    // Store the updated period length.
    std::vector<double> gpgParams = gpg->GetGPHyperparameters();
    Options::setGPGPeriod(gpgParams[PKPeriodLength]);
    // Not incrementing gpgSamples here, want real samples for that.
    const double gpgTime = gpgTimer.elapsed();
    qCDebug(KSTARS_EKOS_GUIDE) << QString("GPG(suspended): elapsed %1s. RA in %2, result: %3")
                               .arg(gpgTime / 1000.0)
                               .arg(gpgInput)
                               .arg(gpgResult);
    // Reset the gains that were zero'd earlier.
    gpg->SetPredictionGain(predictionGain);
    gpg->SetControlGain(controlGain);
}

bool GPG::computePulse(double raArcsecError, GuideStars *guideStars,
                       int *pulseLength, GuideDirection *pulseDir,
                       const Calibration &cal, Seconds timeStep)
{
    if (!Options::gPGEnabled())
        return false;

    if (fabs(raArcsecError) > MAX_ARCSEC_ERROR)
    {
        if (++gpgSkippedSamples > MAX_SKIPPED_SAMPLES)
        {
            qCDebug(KSTARS_EKOS_GUIDE) << "Resetting GPG because RA error = "
                                       << raArcsecError;
            reset();
        }
        else
            qCDebug(KSTARS_EKOS_GUIDE) << "Skipping GPG because RA error = "
                                       << raArcsecError;
        return false;
    }
    gpgSkippedSamples = 0;

    // I want to start off the gpg on the right foot.
    // If the initial guiding is messed up by an early focus,
    // wait until RA has settled down.
    if (gpgSamples == 0 && fabs(raArcsecError) > 1)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Delaying GPG startup. RA error = "
                                   << raArcsecError;
        reset();
        return false;
    }

    // GPG uses proportional gain and min-move from standard controls. Make sure they're using up-to-date values.
    gpg->SetControlGain(Options::rAProportionalGain());
    gpg->SetMinMove(Options::rAMinimumPulseArcSec());

    // GPG input is in RA arcseconds.
    QElapsedTimer gpgTimer;
    gpgTimer.restart();

    // Cast back to a raw double
    auto const rawTime = timeStep.count();

    const double gpgResult = gpg->result(raArcsecError, getSNR(guideStars, raArcsecError), rawTime);
    const double gpgTime = gpgTimer.elapsed();
    gpgSamples++;

    // GPG output is in RA arcseconds. pulseLength and pulseDir are set by convertCorrectionToPulseMilliseconds.
    const double gpgPulse = convertCorrectionToPulseMilliseconds(cal, pulseLength, pulseDir, gpgResult);

    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("GPG: elapsed %1s. RA in %2, result: %3 * %4 --> %5 : %6ms %7")
            .arg(gpgTime / 1000.0)
            .arg(raArcsecError, 0, 'f', 2)
            .arg(gpgResult, 0, 'f', 2)
            .arg(cal.raPulseMillisecondsPerArcsecond(), 0, 'f', 1)
            .arg(gpgPulse, 0, 'f', 1)
            .arg(*pulseLength)
            .arg(directionStr(*pulseDir));
    if (Options::gPGEstimatePeriod())
    {
        double period_length = gpg->GetGPHyperparameters()[PKPeriodLength];
        Options::setGPGPeriod(period_length);
    }
    return true;
}

double GPG::convertCorrectionToPulseMilliseconds(const Calibration &cal, int *pulseLength,
        GuideDirection *pulseDir, const double gpgResult)
{
    const double gpgPulse = gpgResult * cal.raPulseMillisecondsPerArcsecond();

    *pulseDir = gpgPulse > 0 ? RA_DEC_DIR : RA_INC_DIR;
    *pulseLength = fabs(gpgPulse);
    return gpgPulse;
}

bool GPG::darkGuiding(int *pulseLength, GuideDirection *pulseDir, const Calibration &cal,
                      Seconds timeStep)
{
    if (!Options::gPGEnabled() || !Options::gPGDarkGuiding())
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "dark guiding isn't enabled!";
        return false;
    }

    // GPG uses proportional gain and min-move from standard controls. Make sure they're using up-to-date values.
    gpg->SetControlGain(Options::rAProportionalGain());
    gpg->SetMinMove(Options::rAMinimumPulseArcSec());

    QElapsedTimer gpgTimer;
    gpgTimer.restart();
    const double gpgResult = gpg->deduceResult(timeStep.count());
    const double gpgTime = gpgTimer.elapsed();

    // GPG output is in RA arcseconds.
    const double gpgPulse = convertCorrectionToPulseMilliseconds(cal, pulseLength, pulseDir, gpgResult);

    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("GPG dark guiding: elapsed %1s. RA result: %2 * %3 --> %4 : %5ms %6")
            .arg(gpgTime / 1000.0)
            .arg(gpgResult)
            .arg(cal.raPulseMillisecondsPerArcsecond())
            .arg(gpgPulse)
            .arg(*pulseLength)
            .arg(directionStr(*pulseDir));
    return true;
}

/*
    SPDX-FileCopyrightText: 2019 Hy Murveit <hy-1@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusalgorithms.h"
#include "curvefit.h"

#include "polynomialfit.h"
#include <QVector>
#include "kstars.h"
#include "fitsviewer/fitsstardetector.h"
#include "focusstars.h"
#include <ekos_focus_debug.h>

namespace Ekos
{
/**
 * @class LinearFocusAlgorithm
 * @short Autofocus algorithm that linearly samples HFR values.
 *
 * @author Hy Murveit
 * @version 1.0
 */
class LinearFocusAlgorithm : public FocusAlgorithmInterface
{
    public:

        // Constructor initializes a linear autofocus algorithm, starting at some initial position,
        // sampling HFR values, decreasing the position by some step size, until the algorithm believe's
        // it's seen a minimum. It then either:
        // a) tries to find that minimum in a 2nd pass. This is the LINEAR algorithm, or
        // b) moves to the minimum. This is the LINEAR 1 PASS algorithm.
        LinearFocusAlgorithm(const FocusParams &params);

        // After construction, this should be called to get the initial position desired by the
        // focus algorithm. It returns the initial position passed to the constructor if
        // it has no movement request.
        int initialPosition() override
        {
            return requestedPosition;
        }

        // Pass in the measurement for the last requested position. Returns the position for the next
        // requested measurement, or -1 if the algorithm's done or if there's an error.
        // If stars is not nullptr, then the relativeHFR scheme is used to modify the HFR value.
        int newMeasurement(int position, double value, const double starWeight, const QList<Edge*> *stars) override;

        FocusAlgorithmInterface *Copy() override;

        void getMeasurements(QVector<int> *pos, QVector<double> *val, QVector<double> *sds) const override
        {
            *pos = positions;
            *val = values;
            *sds = weights;
        }

        void getPass1Measurements(QVector<int> *pos, QVector<double> *val, QVector<double> *sds, QVector<bool> *out) const override
        {
            *pos = pass1Positions;
            *val = pass1Values;
            *sds = pass1Weights;
            *out = pass1Outliers;
        }

        double latestValue() const override
        {
            if (values.size() > 0)
                return values.last();
            return -1;
        }

        bool isInFirstPass() const override
        {
            if (params.focusAlgorithm == Focus::FOCUS_LINEAR || params.focusAlgorithm == Focus::FOCUS_LINEAR1PASS)
                return inFirstPass;
            else
                return true;
        }

        QString getTextStatus(double R2 = 0) const override;

    private:

        // Called in newMeasurement. Sets up the next iteration.
        int completeIteration(int step, bool foundFit, double minPos, double minVal);

        // Does the bookkeeping for the final focus solution.
        int setupSolution(int position, double value, double sigma);

        // Perform the linear walk for FOCUS_WALK_FIXED_STEPS and FOCUS_WALK_CFZ_SHUFFLE
        int linearWalk(int position, double value, const double starWeight);

        // Calculate the curve fitting goal based on the algorithm parameters and progress
        CurveFitting::FittingGoal getGoal(int numSteps);

        // Get the minimum number of datapoints before attempting the first curve fit
        int getCurveMinPoints();

        // Calc the next step size for Linear1Pass for FOCUS_WALK_FIXED_STEPS and FOCUS_WALK_CFZ_SHUFFLE
        int getNextStepSize();

        // Called when we've found a solution, e.g. the HFR value is within tolerance of the desired value.
        // It it returns true, then it's decided tht we should try one more sample for a possible improvement.
        // If it returns false, then "play it safe" and use the current position as the solution.
        bool setupPendingSolution(int position);

        // Determines the desired focus position for the first sample.
        void computeInitialPosition();

        // Get the first step focus position
        int getFirstPosition();

        // Sets the internal state for re-finding the minimum, and returns the requested next
        // position to sample.
        // Called by Linear. L1P calls finishFirstPass instead
        int setupSecondPass(int position, double value, double margin = 2.0);

        // Called by L1P to finish off the first pass, setting state variables.
        int finishFirstPass(int position, double value);

        // Peirce's criterion
        // Returns the squared threshold error deviation for outlier identification
        // using Peirce's criterion based on Gould's methodology.
        // Arguments:
        // N = total number of observations
        // n = max number of outliers to be removed
        // m = number of model unknowns
        // Returns: squared error threshold (x2)
        double peirce_criterion(double N, double n, double m);

        // Used in the 2nd pass. Focus is getting worse. Requires several consecutive samples getting worse.
        bool gettingWorse();

        // If one of the last 2 samples are as good or better than the 2nd best so far, return true.
        bool bestSamplesHeuristic();

        // Adds to the debug log a line summarizing the result of running this algorithm.
        void debugLog();

        // Use the detailed star measurements to adjust the HFR value.
        // See comment above method definition.
        double relativeHFR(double origHFR, const QList<Edge*> *stars);

        // Calculate the standard deviation (=sigma) of the star HFRs
        double calculateStarSigma(const bool useWeights, const QList<Edge*> *stars);

        // Used to time the focus algorithm.
        QElapsedTimer stopWatch;

        // A vector containing the HFR values sampled by this algorithm so far.
        QVector<double> values;
        QVector<double> pass1Values;
        // A vector containing star weights
        QVector<double> weights;
        QVector<double> pass1Weights;
        // A vector containing the focus positions corresponding to the HFR values stored above.
        QVector<int> positions;
        QVector<int> pass1Positions;
        // A vector containing whether or not the datapoint has been identified as an outlier.
        QVector<bool> pass1Outliers;

        // Focus position requested by this algorithm the previous step.
        int requestedPosition;
        // The position where the first pass is begun. Usually requestedPosition unless there's a restart.
        int passStartPosition;
        // Number of iterations processed so far. Used to see it doesn't exceed params.maxIterations.
        int numSteps;
        // The best value in the first pass. The 2nd pass attempts to get within
        // tolerance of this value.
        double firstPassBestValue;
        // The position of the minimum found in the first pass.
        int firstPassBestPosition;
        // The sampling interval--the recommended number of focuser steps moved inward each iteration
        // of the first pass.
        int stepSize;
        // The minimum focus position to use. Computed from the focuser limits and maxTravel.
        int minPositionLimit;
        // The maximum focus position to use. Computed from the focuser limits and maxTravel.
        int maxPositionLimit;
        // Counter for the number of times a v-curve minimum above the current position was found,
        // which implies the initial focus sweep has passed the minimum and should be terminated.
        int numPolySolutionsFound;
        // Counter for the number of times a v-curve minimum above the passStartPosition was found,
        // which implies the sweep should restart at a higher position.
        int numRestartSolutionsFound;
        // The index (into values and positions) when the most recent 2nd pass started.
        int secondPassStartIndex;
        // True if performing the first focus sweep.
        bool inFirstPass;
        // True if the 2nd pass has found a solution, and it's now optimizing the solution.
        bool solutionPending;
        // When we're near done, but the HFR just got worse, we may retry the current position
        // in case the HFR value was noisy.
        int retryNumber = 0;

        // Keep the solution-pending position/value for the status messages.
        double solutionPendingValue = 0;
        int solutionPendingPosition = 0;

        // RelativeHFR variables
        bool relativeHFREnabled = false;
        QVector<FocusStars> starLists;
        double bestRelativeHFR = 0;
        int bestRelativeHFRIndex = 0;
};

// Copies the object. Used in testing to examine alternate possible inputs given
// the current state.
FocusAlgorithmInterface *LinearFocusAlgorithm::Copy()
{
    LinearFocusAlgorithm *alg = new LinearFocusAlgorithm(params);
    *alg = *this;
    return dynamic_cast<FocusAlgorithmInterface*>(alg);
}

FocusAlgorithmInterface *MakeLinearFocuser(const FocusAlgorithmInterface::FocusParams &params)
{
    return new LinearFocusAlgorithm(params);
}

LinearFocusAlgorithm::LinearFocusAlgorithm(const FocusParams &focusParams)
    : FocusAlgorithmInterface(focusParams)
{
    stopWatch.start();
    // These variables don't get reset if we restart the algorithm.
    numSteps = 0;
    maxPositionLimit = std::min(params.maxPositionAllowed, params.startPosition + params.maxTravel);
    minPositionLimit = std::max(params.minPositionAllowed, params.startPosition - params.maxTravel);
    computeInitialPosition();
}

QString LinearFocusAlgorithm::getTextStatus(double R2) const
{
    if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
    {
        if (done)
        {
            if (focusSolution > 0)
                return QString("Solution: %1").arg(focusSolution);
            else
                return "Failed";
        }
        if (solutionPending)
            return QString("Pending: %1,  %2").arg(solutionPendingPosition).arg(solutionPendingValue, 0, 'f', 2);
        if (inFirstPass)
            return "1st Pass";
        else
            return QString("2nd Pass. 1st: %1,  %2")
                   .arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 2);
    }
    else // Linear 1 Pass
    {
        QString text = "L1P";
        if (params.filterName != "")
            text.append(QString(" [%1]").arg(params.filterName));
        if (params.curveFit == CurveFitting::FOCUS_QUADRATIC)
            text.append(": Quadratic");
        else if (params.curveFit == CurveFitting::FOCUS_HYPERBOLA)
        {
            text.append(": Hyperbola");
            text.append(params.useWeights ? " (W)" : " (U)");
        }
        else if (params.curveFit == CurveFitting::FOCUS_PARABOLA)
        {
            text.append(": Parabola");
            text.append(params.useWeights ? " (W)" : " (U)");
        }

        if (inFirstPass)
            return text;
        else if (!done)
            return text.append(". Moving to Solution");
        else
        {
            if (focusSolution > 0)
            {
                if (params.curveFit == CurveFitting::FOCUS_QUADRATIC)
                    return text.append(" Solution: %1").arg(focusSolution);
                else
                    // Add R2 to 2 decimal places. Round down to be conservative
                    return text.append(" Solution: %1, R²=%2").arg(focusSolution).arg(trunc(R2 * 100.0) / 100.0, 0, 'f', 2);
            }
            else
                return text.append(" Failed");
        }
    }
}

void LinearFocusAlgorithm::computeInitialPosition()
{
    // These variables get reset if the algorithm is restarted.
    stepSize = params.initialStepSize;
    inFirstPass = true;
    solutionPending = false;
    retryNumber = 0;
    firstPassBestValue = -1;
    firstPassBestPosition = 0;
    numPolySolutionsFound = 0;
    numRestartSolutionsFound = 0;
    secondPassStartIndex = -1;

    qCDebug(KSTARS_EKOS_FOCUS)
            << QString("Linear: Travel %1 initStep %2 pos %3 min %4 max %5 maxIters %6 tolerance %7 minlimit %8 maxlimit %9 init#steps %10 algo %11 backlash %12 curvefit %13 useweights %14")
            .arg(params.maxTravel).arg(params.initialStepSize).arg(params.startPosition).arg(params.minPositionAllowed)
            .arg(params.maxPositionAllowed).arg(params.maxIterations).arg(params.focusTolerance).arg(minPositionLimit)
            .arg(maxPositionLimit).arg(params.initialOutwardSteps).arg(params.focusAlgorithm).arg(params.backlash)
            .arg(params.curveFit).arg(params.useWeights);

    requestedPosition = std::min(maxPositionLimit, getFirstPosition());
    passStartPosition = requestedPosition;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: initialPosition %1 sized %2")
                               .arg(requestedPosition).arg(params.initialStepSize);
}

// Calculate the first position relative to the start position
// Generally this is just the number of outward points * step size.
// For LINEAR1PASS most walks have a constant step size so again it is the number of outward points * step size.
// For LINEAR1PASS and FOCUS_WALK_CFZ_SHUFFLE the middle 3 or 4 points are separated by half step size
int LinearFocusAlgorithm::getFirstPosition()
{
    if (params.focusAlgorithm != Focus::FOCUS_LINEAR1PASS)
        return static_cast<int>(params.startPosition + params.initialOutwardSteps * params.initialStepSize);

    int firstPosition;
    double outSteps, numFullStepsOut, numHalfStepsOut;

    switch (params.focusWalk)
    {
        case Focus::FOCUS_WALK_CLASSIC:
            firstPosition = static_cast<int>(params.startPosition + params.initialOutwardSteps * params.initialStepSize);
            break;

        case Focus::FOCUS_WALK_FIXED_STEPS:
            outSteps = (params.numSteps - 1) / 2.0f;
            firstPosition = params.startPosition + (outSteps * params.initialStepSize);
            break;

        case Focus::FOCUS_WALK_CFZ_SHUFFLE:
            if (params.numSteps % 2)
            {
                // Odd number of steps
                numHalfStepsOut = 1.0f;
                numFullStepsOut = ((params.numSteps - 1) / 2.0f) - numHalfStepsOut;
            }
            else
            {
                // Even number of steps
                numHalfStepsOut = 1.5f;
                numFullStepsOut = (params.numSteps / 2.0f) - 2.0f;
            }
            firstPosition = params.startPosition + (numFullStepsOut * params.initialStepSize) + (numHalfStepsOut *
                            (params.initialStepSize / 2.0f));

            break;

        default:
            firstPosition = static_cast<int>(params.startPosition + params.initialOutwardSteps * params.initialStepSize);
            break;
    }

    return firstPosition;
}

// The Problem:
// The HFR values passed in to these focus algorithms are simply the mean or median HFR values
// for all stars detected (with some other constraints). However, due to randomness in the
// star detection scheme, two sets of star measurements may use different sets of actual stars to
// compute their mean HFRs. This adds noise to determining best focus (e.g. including a
// wider star in one set but not the other will tend to increase the HFR for that star set).
// relativeHFR tries to correct for this by comparing two sets of stars only using their common stars.
//
// The Solution:
// We maintain a reference set of stars, along with the HFR computed for those reference stars (bestRelativeHFR).
// We find the common set of stars between an input star set and those reference stars. We compute
// HFRs for the input stars in that common set, and for the reference common set as well.
// The ratio of those common-set HFRs multiply bestRelativeHFR to generate the relative HFR for this
// input star set--that is, it's the HFR "relative to the reference set". E.g. if the common-set HFR
// for the input stars is twice worse than that from the reference common stars, then the relative HFR
// would be 2 * bestRelativeHFR.
// The reference set is the best HFR set of stars seen so far, but only from the 1st pass.
// Thus, in the 2nd pass, the reference stars would be the best HFR set from the 1st pass.
//
// In unusual cases, e.g. when the common set can't be computed, we just return the input origHFR.
double LinearFocusAlgorithm::relativeHFR(double origHFR, const QList<Edge*> *stars)
{
    constexpr double minHFR = 0.3;
    if (origHFR < minHFR) return origHFR;

    const int currentIndex = values.size();
    const bool isFirstSample = (currentIndex == 0);
    double relativeHFR = origHFR;

    if (isFirstSample && stars != nullptr)
        relativeHFREnabled = true;

    if (relativeHFREnabled && stars == nullptr)
    {
        // Error--we have been getting relativeHFR stars, and now we're not.
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Error: Inconsistent relativeHFR, disabling.");
        relativeHFREnabled = false;
    }

    if (!relativeHFREnabled)
        return origHFR;

    if (starLists.size() != positions.size())
    {
        // Error--inconsistent data structures!
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Error: Inconsistent relativeHFR data structures, disabling.");
        relativeHFREnabled = false;
        return origHFR;
    }

    // Copy the input stars.
    // FocusStars enables us to measure HFR relatively consistently,
    // by using the same stars when comparing two measurements.
    constexpr int starDistanceThreshold = 5;  // Max distance (pixels) for two positions to be considered the same star.
    FocusStars fs(*stars, starDistanceThreshold);
    starLists.push_back(fs);
    auto &currentStars = starLists.back();

    if (isFirstSample)
    {
        relativeHFR = currentStars.getHFR();
        if (relativeHFR <= 0)
        {
            // Fall back to the simpler scheme.
            relativeHFREnabled = false;
            return origHFR;
        }
        // 1st measurement. By definition this is the best HFR.
        bestRelativeHFR = relativeHFR;
        bestRelativeHFRIndex = currentIndex;
    }
    else
    {
        // HFR computed relative to the best measured so far.
        auto &bestStars = starLists[bestRelativeHFRIndex];
        double hfr = currentStars.relativeHFR(bestStars, bestRelativeHFR);
        if (hfr > 0)
        {
            relativeHFR = hfr;
        }
        else
        {
            relativeHFR = currentStars.getHFR();
            if (relativeHFR <= 0)
                return origHFR;
        }

        // In the 1st pass we compute the current HFR relative to the best HFR measured yet.
        // In the 2nd pass we compute the current HFR relative to the best HFR in the 1st pass.
        if (inFirstPass && relativeHFR <= bestRelativeHFR)
        {
            bestRelativeHFR = relativeHFR;
            bestRelativeHFRIndex = currentIndex;
        }
    }

    qCDebug(KSTARS_EKOS_FOCUS) << QString("RelativeHFR: orig %1 computed %2 relative %3")
                               .arg(origHFR).arg(currentStars.getHFR()).arg(relativeHFR);

    return relativeHFR;
}

int LinearFocusAlgorithm::newMeasurement(int position, double value, const double starWeight, const QList<Edge*> *stars)
{
    if (params.focusAlgorithm == Focus::FOCUS_LINEAR1PASS && (params.focusWalk == Focus::FOCUS_WALK_FIXED_STEPS
            || params.focusWalk == Focus::FOCUS_WALK_CFZ_SHUFFLE))
        return linearWalk(position, value, starWeight);

    double minPos = -1.0, minVal = 0;
    bool foundFit = false;

    double origValue = value;
    // For QUADRATIC continue to use the relativeHFR functionality
    // For HYPERBOLA and PARABOLA the stars used for the HDR calculation and the sigma calculation
    // should be the same. For now, we will use the full set of stars and therefore not adjust the HFR
    if (params.curveFit == CurveFitting::FOCUS_QUADRATIC)
        value = relativeHFR(value, stars);

    // For LINEAR 1 PASS don't bother with a full 2nd pass just jump to the solution
    // Do the step out and back in to deal with backlash
    if (params.focusAlgorithm == Focus::FOCUS_LINEAR1PASS && !inFirstPass)
    {
        return setupSolution(position, value, starWeight);
    }

    int thisStepSize = stepSize;
    ++numSteps;
    if (inFirstPass)
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: step %1, newMeasurement(%2, %3 -> %4, %5)")
                                   .arg(numSteps).arg(position).arg(origValue).arg(value)
                                   .arg(stars == nullptr ? 0 : stars->size());
    else
        qCDebug(KSTARS_EKOS_FOCUS)
                << QString("Linear: step %1, newMeasurement(%2, %3 -> %4, %5) 1stBest %6 %7").arg(numSteps)
                .arg(position).arg(origValue).arg(value).arg(stars == nullptr ? 0 : stars->size())
                .arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 3);

    const int LINEAR_POSITION_TOLERANCE = params.initialStepSize;
    if (abs(position - requestedPosition) > LINEAR_POSITION_TOLERANCE)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: error didn't get the requested position");
        return requestedPosition;
    }
    // Have we already found a solution?
    if (focusSolution != -1)
    {
        doneString = i18n("Called newMeasurement after a solution was found.");
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: error %1").arg(doneString);
        debugLog();
        return -1;
    }

    // Store the sample values.
    positions.push_back(position);
    values.push_back(value);
    weights.push_back(starWeight);
    if (inFirstPass)
    {
        pass1Positions.push_back(position);
        pass1Values.push_back(value);
        pass1Weights.push_back(starWeight);
        pass1Outliers.push_back(false);
    }

    // If we're in the 2nd pass and either
    // - the current value is within tolerance, or
    // - we're optimizing because we've previously found a within-tolerance solution,
    // then setupPendingSolution decides whether to continue optimizing or to complete.
    if (solutionPending ||
            (!inFirstPass && (value < firstPassBestValue * (1.0 + params.focusTolerance))))
    {
        if (setupPendingSolution(position))
            // We can continue to look a little further.
            return completeIteration(retryNumber > 0 ? 0 : stepSize, false, -1.0f, -1.0f);
        else
            // Finish now
            return setupSolution(position, value, starWeight);
    }

    if (inFirstPass)
    {
        constexpr int kNumPolySolutionsRequired = 2;
        constexpr int kNumRestartSolutionsRequired = 3;
        //constexpr double kDecentValue = 3.0;

        if (values.size() >= getCurveMinPoints())
        {
            if (params.curveFit == CurveFitting::FOCUS_QUADRATIC)
            {
                PolynomialFit fit(2, positions, values);
                foundFit = fit.findMinimum(position, 0, params.maxPositionAllowed, &minPos, &minVal);
                if (!foundFit)
                {
                    // I've found that the first sample can be odd--perhaps due to backlash.
                    // Try again skipping the first sample, if we have sufficient points.
                    if (values.size() > getCurveMinPoints())
                    {
                        PolynomialFit fit2(2, positions.mid(1), values.mid(1));
                        foundFit = fit2.findMinimum(position, 0, params.maxPositionAllowed, &minPos, &minVal);
                        minPos = minPos + 1;
                    }
                }
            }
            else // Hyperbola or Parabola so use the LM solver
            {
                auto goal = getGoal(numSteps);
                params.curveFitting->fitCurve(goal, positions, values, weights, pass1Outliers, params.curveFit, params.useWeights,
                                              params.optimisationDirection);

                foundFit = params.curveFitting->findMinMax(position, 0, params.maxPositionAllowed, &minPos, &minVal,
                           static_cast<CurveFitting::CurveFit>(params.curveFit),
                           params.optimisationDirection);
            }

            if (foundFit)
            {
                const int distanceToMin = static_cast<int>(position - minPos);
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: fit(%1): %2 = %3 @ %4 distToMin %5")
                                           .arg(positions.size()).arg(minPos).arg(minVal).arg(position).arg(distanceToMin);
                if (distanceToMin >= 0)
                {
                    // The minimum is further inward.
                    numPolySolutionsFound = 0;
                    numRestartSolutionsFound = 0;
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Solutions reset %1 = %2").arg(minPos).arg(minVal);

                    // The next code block is an optimisation where the user starts focus off a long way from prime focus.
                    // The idea is that by detecting this situation we can warp quickly to the solution.
                    // However, when the solver is solving on a small number of points (e.g. 6 or 7) its possible that the
                    // best fit curve produces a solution much further in than what turns out to be the actual focus point.
                    // This code therefore results in inappropriate warping. So this code is now disabled.
                    /*if (value / minVal > kDecentValue)
                    {
                        // Only skip samples if the value is a long way from the minimum.
                        const int stepsToMin = distanceToMin / stepSize;
                        // Temporarily increase the step size if the minimum is very far inward.
                        if (stepsToMin >= 8)
                            thisStepSize = stepSize * 4;
                        else if (stepsToMin >= 4)
                            thisStepSize = stepSize * 2;
                    }*/
                }
                else if (!bestSamplesHeuristic())
                {
                    // We have potentially passed the bottom of the curve,
                    // but it's possible it is further back than the start of our sweep.
                    if (minPos > passStartPosition)
                    {
                        numRestartSolutionsFound++;
                        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: RESTART Solution #%1 %2 = %3 @ %4")
                                                   .arg(numRestartSolutionsFound).arg(minPos).arg(minVal).arg(position);
                    }
                    else
                    {
                        numPolySolutionsFound++;
                        numRestartSolutionsFound = 0;
                        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Solution #%1: %2 = %3 @ %4")
                                                   .arg(numPolySolutionsFound).arg(minPos).arg(minVal).arg(position);
                    }
                }

                // The LINEAR algo goes >2 steps past the minimum. The LINEAR 1 PASS algo uses the InitialOutwardSteps parameter
                // in order to have some user control over the number of steps past the minimum when fitting the curve.
                // With LINEAR 1 PASS setupSecondPass with a margin of 0.0 as no need to move the focuser further
                // This will result in a step out, past the solution of either "backlash" id set, or 5 * step size, followed
                // by a step in to the solution. By stepping out and in, backlash will be neutralised.
                int howFarToGo;
                if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
                    howFarToGo = kNumPolySolutionsRequired;
                else
                    howFarToGo = std::max(1, static_cast<int> (params.initialOutwardSteps) - 1);

                if (numPolySolutionsFound >= howFarToGo)
                {
                    // We found a minimum. Setup the 2nd pass. We could use either the polynomial min or the
                    // min measured star as the target HFR. I've seen using the polynomial minimum to be
                    // sometimes too conservative, sometimes too low. For now using the min sample.
                    double minMeasurement = *std::min_element(values.begin(), values.end());
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: 1stPass solution @ %1: pos %2 val %3, min measurement %4")
                                               .arg(position).arg(minPos).arg(minVal).arg(minMeasurement);

                    // For Linear setup the second pass with a margin of 2
                    if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
                        return setupSecondPass(static_cast<int>(minPos), minMeasurement, 2.0);
                    else
                        // For Linear 1 Pass do a round on the double minPos parameter before casting to an int
                        // as the cast will truncate and potentially change the position down by 1
                        // The code to display v-curve rounds so this keeps things consistent.
                        // Could make this change for Linear as well but this will then need testfocus to change
                        return finishFirstPass(static_cast<int>(round(minPos)), minMeasurement);
                }
                else if (numRestartSolutionsFound >= kNumRestartSolutionsRequired)
                {
                    // We need to restart--that is the error is rising and thus our initial position
                    // was already past the minimum. Restart the algorithm with a greater initial position.
                    // If the min position from the polynomial solution is not too far from the current start position
                    // use that, but don't let it go too far away.
                    const double highestStartPosition = params.startPosition + params.initialOutwardSteps * params.initialStepSize;
                    params.startPosition = std::min(minPos, highestStartPosition);
                    computeInitialPosition();
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Restart. Current pos %1, min pos %2, min val %3, re-starting at %4")
                                               .arg(position).arg(minPos).arg(minVal).arg(requestedPosition);
                    return requestedPosition;
                }

            }
            else
            {
                // Minimum failed indicating the 2nd-order polynomial is an inverted U--it has a maximum,
                // but no minimum.  This is, of course, not a sensible solution for the focuser, but can
                // happen with noisy data and perhaps small step sizes. We still might be able to take advantage,
                // and notice whether the polynomial is increasing or decreasing locally. For now, do nothing.
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: ******** No poly min: Poly must be inverted");
            }
        }
        else
        {
            // Don't have enough samples to reliably fit a polynomial.
            // Simply step the focus in one more time and iterate.
        }
    }
    else if (gettingWorse())
    {
        // Doesn't look like we'll find something close to the min. Retry the 2nd pass.
        // NOTE: this branch will only be executed for the 2 pass version of Linear
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: getting worse, re-running 2nd pass");
        return setupSecondPass(firstPassBestPosition, firstPassBestValue);
    }

    return completeIteration(thisStepSize, foundFit, minPos, minVal);
}

// Get the curve fitting goal, based on the "walk".
// Start with STANDARD but at the end of the pass use BEST
CurveFitting::FittingGoal LinearFocusAlgorithm::getGoal(int numSteps)
{
    if (params.focusWalk == Focus::FOCUS_WALK_CLASSIC)
    {
        if (numSteps > 2.0 * params.initialOutwardSteps)
            return CurveFitting::FittingGoal::BEST;
        else
            return CurveFitting::FittingGoal::STANDARD;
    }
    else // FOCUS_WALK_FIXED_STEPS or FOCUS_WALK_CFZ_SHUFFLE
    {
        if (numSteps >= params.numSteps)
            return CurveFitting::FittingGoal::BEST;
        else
            return CurveFitting::FittingGoal::STANDARD;
    }
}

// Get the minimum number of points to start fitting a curve
// The default is 5. Try to get past the minimum so the curve shape is hyperbolic/parabolic
int LinearFocusAlgorithm::getCurveMinPoints()
{
    if (params.focusAlgorithm != Focus::FOCUS_LINEAR1PASS)
        return 5;

    if (params.focusWalk == Focus::FOCUS_WALK_CLASSIC)
        return params.initialOutwardSteps + 2;

    return params.numSteps / 2 + 2;
}

// Process next step for LINEAR1PASS for walks: FOCUS_WALK_FIXED_STEPS and FOCUS_WALK_CFZ_SHUFFLE
int LinearFocusAlgorithm::linearWalk(int position, double value, const double starWeight)
{
    if (!inFirstPass)
    {
        return setupSolution(position, value, starWeight);
    }

    bool foundFit = false;
    double minPos = -1.0, minVal = 0;
    ++numSteps;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear1Pass: step %1, linearWalk %2, %3")
                               .arg(numSteps).arg(position).arg(value);

    // If we are within 1 tick of where we should be then fine, otherwise try again
    if (abs(position - requestedPosition) > 1)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear1Pass: linearWalk error didn't get the requested position");
        return requestedPosition;
    }

    // Store the sample values.
    positions.push_back(position);
    values.push_back(value);
    weights.push_back(starWeight);
    pass1Positions.push_back(position);
    pass1Values.push_back(value);
    pass1Weights.push_back(starWeight);
    pass1Outliers.push_back(false);

    if (values.size() < getCurveMinPoints())
    {
        // Don't have enough samples to reliably fit a curve.
        // Simply step the focus in one more time and iterate.
    }
    else
    {
        if (params.curveFit == CurveFitting::FOCUS_QUADRATIC)
            qCDebug(KSTARS_EKOS_FOCUS) << QString("linearWalk called incorrectly for FOCUS_QUADRATIC");
        else
        {
            // Hyperbola or Parabola so use the LM solver
            auto goal = getGoal(numSteps);
            params.curveFitting->fitCurve(goal, positions, values, weights, pass1Outliers, params.curveFit, params.useWeights,
                                          params.optimisationDirection);

            foundFit = params.curveFitting->findMinMax(position, 0, params.maxPositionAllowed, &minPos, &minVal,
                       static_cast<CurveFitting::CurveFit>(params.curveFit), params.optimisationDirection);

            if (numSteps >= params.numSteps)
            {
                // linearWalk has now completed either successfully (foundFit=true) or not
                if (foundFit)
                    return finishFirstPass(static_cast<int>(round(minPos)), minVal);
                else
                {
                    done = true;
                    doneString = i18n("Failed to fit curve to data.");
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearWalk: %1").arg(doneString);
                    debugLog();
                    return -1;
                }
            }
        }
    }

    int nextStepSize = getNextStepSize();
    return completeIteration(nextStepSize, foundFit, minPos, minVal);
}

// Function to calculate the next step size for LINEAR1PASS for walks: FOCUS_WALK_FIXED_STEPS and FOCUS_WALK_CFZ_SHUFFLE
int LinearFocusAlgorithm::getNextStepSize()
{
    int nextStepSize, lower, upper;

    switch (params.focusWalk)
    {
        case Focus::FOCUS_WALK_CLASSIC:
            nextStepSize = stepSize;
            break;
        case Focus::FOCUS_WALK_FIXED_STEPS:
            nextStepSize = stepSize;
            break;
        case Focus::FOCUS_WALK_CFZ_SHUFFLE:
            if (params.numSteps % 2)
            {
                // Odd number of steps
                lower = (params.numSteps - 3) / 2;
                upper = params.numSteps - lower;
            }
            else
            {
                // Evem number of steps
                lower = (params.numSteps - 4) / 2;
                upper = (params.numSteps - lower);
            }

            if (numSteps <= lower)
                nextStepSize = stepSize;
            else if (numSteps >= upper)
                nextStepSize = stepSize;
            else
                nextStepSize = stepSize / 2;
            break;
        default:
            nextStepSize = stepSize;
            break;
    }

    return nextStepSize;
}

int LinearFocusAlgorithm::setupSolution(int position, double value, double weight)
{
    focusSolution = position;
    focusValue = value;
    done = true;
    doneString = i18n("Solution found.");
    if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: solution @ %1 = %2 (best %3)")
                                   .arg(position).arg(value).arg(firstPassBestValue);
    else // Linear 1 Pass
    {
        double delta = fabs(value - firstPassBestValue);
        QString str("Linear: solution @ ");
        str.append(QString("%1 = %2 (expected %3) delta=%4").arg(position).arg(value).arg(firstPassBestValue).arg(delta));

        if (params.useWeights && weight > 0.0)
        {
            // TODO fix this for weights
            double numSigmas = delta * weight;
            str.append(QString(" or %1 sigma %2 than expected")
                       .arg(numSigmas).arg(value <= firstPassBestValue ? "better" : "worse"));
            if (value <= firstPassBestValue || numSigmas < 1)
                str.append(QString(". GREAT result"));
            else if (numSigmas < 3)
                str.append(QString(". OK result"));
            else
                str.append(QString(". POOR result. If this happens repeatedly it may be a sign of poor backlash compensation."));
        }

        qCInfo(KSTARS_EKOS_FOCUS) << str;
    }
    debugLog();
    return -1;
}

int LinearFocusAlgorithm::completeIteration(int step, bool foundFit, double minPos, double minVal)
{
    if (params.focusAlgorithm == Focus::FOCUS_LINEAR && numSteps == params.maxIterations - 2)
    {
        // If we're close to exceeding the iteration limit, retry this pass near the old minimum position.
        // For Linear 1 Pass we are still in the first pass so no point retrying the 2nd pass. Just carry on and
        // fail in 2 more datapoints if necessary.
        const int minIndex = static_cast<int>(std::min_element(values.begin(), values.end()) - values.begin());
        return setupSecondPass(positions[minIndex], values[minIndex], 0.5);
    }
    else if (numSteps > params.maxIterations)
    {
        // Fail. Exceeded our alloted number of iterations.
        done = true;
        doneString = i18n("Too many steps.");
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: error %1").arg(doneString);
        debugLog();
        return -1;
    }

    // Setup the next sample.
    requestedPosition = requestedPosition - step;

    // Make sure the next sample is within bounds.
    if (requestedPosition < minPositionLimit)
    {
        // The position is too low. Pick the min value and go to (or retry) a 2nd iteration.
        if (params.focusAlgorithm == Focus::FOCUS_LINEAR)
        {
            const int minIndex = static_cast<int>(std::min_element(values.begin(), values.end()) - values.begin());
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: reached end without Vmin. Restarting %1 pos %2 value %3")
                                       .arg(minIndex).arg(positions[minIndex]).arg(values[minIndex]);
            return setupSecondPass(positions[minIndex], values[minIndex]);
        }
        else // Linear 1 Pass
        {
            if (foundFit && minPos >= minPositionLimit)
                // Although we ran out of room, we have got a viable, although not perfect, solution
                return finishFirstPass(static_cast<int>(round(minPos)), minVal);
            else
            {
                // We can't travel far enough to find a solution so fail as focuser parameters require user attention
                done = true;
                doneString = i18n("Solution lies outside max travel.");
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: error %1").arg(doneString);
                debugLog();
                return -1;
            }
        }
    }
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: requesting position %1").arg(requestedPosition);
    return requestedPosition;
}

// This is called in the 2nd pass, when we've found a sample point that's within tolerance
// of the 1st pass' best value. Since it's within tolerance, the 2nd pass might finish, using
// the current value as the final solution. However, this method checks to see if we might go
// a bit further. Once solutionPending is true, it's committed to finishing soon.
// It goes further if:
// - the current HFR value is an improvement on the previous 2nd-pass value,
// - the number of steps taken so far is not close the max number of allowable steps.
// If instead the HFR got worse, we retry the current position a few times to see if it might
// get better before giving up.
bool LinearFocusAlgorithm::setupPendingSolution(int position)
{
    const int length = values.size();
    const int secondPassIndex = length - secondPassStartIndex;
    const double thisValue = values[length - 1];

    // ReferenceValue is the value used in the notGettingWorse computation.
    // Basically, it's the previous value, or the value before retries.
    double referenceValue = (secondPassIndex <= 1) ? 1e6 : values[length - 2];
    if (retryNumber > 0 && length - (2 + retryNumber) >= 0)
        referenceValue = values[length - (2 + retryNumber)];

    // NotGettingWorse: not worse than the previous (non-repeat) 2nd pass sample, or the 1st 2nd pass sample.
    const bool notGettingWorse = (secondPassIndex <= 1) || (thisValue <= referenceValue);

    // CouldGoFurther: Not near a boundary in position or number of steps.
    const bool couldGoFather = (numSteps < params.maxIterations - 2) && (position - stepSize > minPositionLimit);

    // Allow passing the 1st pass' minimum HFR position, but take smaller steps and don't retry as much.
    int maxNumRetries = 3;
    if (position - stepSize / 2 < firstPassBestPosition)
    {
        stepSize = std::max(2, params.initialStepSize / 4);
        maxNumRetries = 1;
    }

    if (notGettingWorse && couldGoFather)
    {
        qCDebug(KSTARS_EKOS_FOCUS)
                << QString("Linear: %1: Position(%2) & HFR(%3) -- Pass1: %4 %5, solution pending, searching further")
                .arg(length).arg(position).arg(thisValue, 0, 'f', 3).arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 3);
        solutionPending = true;
        solutionPendingPosition = position;
        solutionPendingValue = thisValue;
        retryNumber = 0;
        return true;
    }
    else if (solutionPending && couldGoFather && retryNumber < maxNumRetries &&
             (secondPassIndex > 1) && (thisValue >= referenceValue))
    {
        qCDebug(KSTARS_EKOS_FOCUS)
                << QString("Linear: %1: Position(%2) & HFR(%3) -- Pass1: %4 %5, solution pending, got worse, retrying")
                .arg(length).arg(position).arg(thisValue, 0, 'f', 3).arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 3);
        // Try this poisition again.
        retryNumber++;
        return true;
    }
    qCDebug(KSTARS_EKOS_FOCUS)
            << QString("Linear: %1: Position(%2) & HFR(%3) -- Pass1: %4 %5, finishing, can't go further")
            .arg(length).arg(position).arg(thisValue, 0, 'f', 3).arg(firstPassBestPosition).arg(firstPassBestValue, 0, 'f', 3);
    retryNumber = 0;
    return false;
}

void LinearFocusAlgorithm::debugLog()
{
    QString str("Linear: points=[");
    for (int i = 0; i < positions.size(); ++i)
    {
        str.append(QString("(%1, %2, %3)").arg(positions[i]).arg(values[i]).arg(weights[i]));
        if (i < positions.size() - 1)
            str.append(", ");
    }
    str.append(QString("];iterations=%1").arg(numSteps));
    str.append(QString(";duration=%1").arg(stopWatch.elapsed() / 1000));
    str.append(QString(";solution=%1").arg(focusSolution));
    str.append(QString(";value=%1").arg(focusValue));
    str.append(QString(";filter='%1'").arg(params.filterName));
    str.append(QString(";temperature=%1").arg(params.temperature));
    str.append(QString(";focusalgorithm=%1").arg(params.focusAlgorithm));
    str.append(QString(";backlash=%1").arg(params.backlash));
    str.append(QString(";curvefit=%1").arg(params.curveFit));
    str.append(QString(";useweights=%1").arg(params.useWeights));

    qCDebug(KSTARS_EKOS_FOCUS) << str;
}

// Called by Linear to set state for the second pass
int LinearFocusAlgorithm::setupSecondPass(int position, double value, double margin)
{
    firstPassBestPosition = position;
    firstPassBestValue = value;
    inFirstPass = false;
    solutionPending = false;
    secondPassStartIndex = values.size();

    // Arbitrarily go back "margin" steps above the best position.
    // Could be a problem if backlash were worse than that many steps.
    requestedPosition = std::min(static_cast<int>(firstPassBestPosition + stepSize * margin), maxPositionLimit);
    stepSize = params.initialStepSize / 2;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: 2ndPass starting at %1 step %2").arg(requestedPosition).arg(stepSize);
    return requestedPosition;
}

// Called by L1P to finish off the first pass by setting state variables
int LinearFocusAlgorithm::finishFirstPass(int position, double value)
{
    firstPassBestPosition = position;
    firstPassBestValue = value;
    inFirstPass = false;
    requestedPosition = position;

    if (params.refineCurveFit)
    {
        // We've completed pass1 so, if required, reprocess all the datapoints excluding outliers
        // First, we need the distance of each datapoint from the curve
        std::vector<std::pair<int, double>> curveDeltas;

        params.curveFitting->calculateCurveDeltas(params.curveFit, curveDeltas);

        // Check for outliers if there are enough datapoints
        if (curveDeltas.size() > 6)
        {
            // Build a vector of the square of the curve deltas
            std::vector<double> curveDeltasX2Vec;
            for (int i = 0; i < curveDeltas.size(); i++)
                curveDeltasX2Vec.push_back(pow(curveDeltas[i].second, 2.0));

            auto curveDeltasX2Mean = Mathematics::RobustStatistics::ComputeLocation(Mathematics::RobustStatistics::LOCATION_MEAN,
                                     curveDeltasX2Vec);

            // Order the curveDeltas, highest first, then check against the limit
            // Remove points over the limit, but don't remove too many points to compromise the curve
            double maxOutliers;
            if (curveDeltas.size() < 7)
                maxOutliers = 1;
            else if (curveDeltas.size() < 11)
                maxOutliers = 2;
            else
                maxOutliers = 3;

            double modelUnknowns = params.curveFit == CurveFitting::FOCUS_PARABOLA ? 3.0 : 4.0;

            // Use Peirce's Criterion to get the outlier threshold
            // Note this operates on the square of the curve deltas
            double pc = peirce_criterion(static_cast<double> (curveDeltas.size()), maxOutliers, modelUnknowns);
            double pc_threshold = sqrt(pc * curveDeltasX2Mean);

            // Sort the curve deltas, largest first
            std::sort(curveDeltas.begin(), curveDeltas.end(),
                      [](const std::pair<int, double> &a, const std::pair<int, double> &b)
            {
                return ( a.second > b.second );
            });

            // Save off pass1 variables in case they need to be reset later
            auto bestPass1Outliers = pass1Outliers;

            // Work out how many outliers to exclude
            int outliers = 0;
            for (int i = 0; i < curveDeltas.size(); i++)
            {
                if(curveDeltas[i].second <= pc_threshold)
                    break;
                else
                {
                    pass1Outliers[curveDeltas[i].first] = true;
                    outliers++;
                }
                if (outliers >= maxOutliers)
                    break;
            }

            // If there are any outliers then try to improve the curve fit by removing these
            // datapoints and reruning the curve fitting process. Note that this may not improve
            // the fit so we need to be prepared to reinstate the original solution.
            QVector<double> currentCoefficients;
            if (outliers > 0 && params.curveFitting->getCurveParams(params.curveFit, currentCoefficients))
            {
                double currentR2 = params.curveFitting->calculateR2(static_cast<CurveFitting::CurveFit>(params.curveFit));

                // Try another curve fit on the data without outliers
                params.curveFitting->fitCurve(CurveFitting::FittingGoal::BEST, pass1Positions, pass1Values, pass1Weights,
                                              pass1Outliers, params.curveFit, params.useWeights, params.optimisationDirection);
                double minPos, minVal;
                bool foundFit = params.curveFitting->findMinMax(position, 0, params.maxPositionAllowed, &minPos, &minVal,
                                static_cast<CurveFitting::CurveFit>(params.curveFit),
                                params.optimisationDirection);
                if (foundFit)
                {
                    double newR2 = params.curveFitting->calculateR2(static_cast<CurveFitting::CurveFit>(params.curveFit));
                    if (newR2 > currentR2)
                    {
                        // We have a better solution so we'll use it
                        requestedPosition = minPos;
                        qCDebug(KSTARS_EKOS_FOCUS) << QString("Refine Curve Fit improved focus from %1 to %2. R2 improved from %3 to %4")
                                                   .arg(position).arg(minPos).arg(currentR2).arg(newR2);
                    }
                    else
                    {
                        // New solution is not better than previous one so go with the previous one
                        qCDebug(KSTARS_EKOS_FOCUS) <<
                                                   QString("Refine Curve Fit could not improve on the original solution");
                        pass1Outliers = bestPass1Outliers;
                        params.curveFitting->setCurveParams(params.curveFit, currentCoefficients);
                    }
                }
                else
                {
                    qCDebug(KSTARS_EKOS_FOCUS) <<
                                               QString("Refine Curve Fit failed to fit curve whilst refining curve fit. Running with original solution");
                    pass1Outliers = bestPass1Outliers;
                    params.curveFitting->setCurveParams(params.curveFit, currentCoefficients);
                }
            }
        }
    }
    return requestedPosition;
}

// Peirce's criterion based on Gould's methodology for outlier identification
// See https://en.wikipedia.org/wiki/Peirce%27s_criterion for details incl Peirce's original paper
// and Gould's paper both referenced in the notes. The wikipedia entry also includes some code fragments
// that form the basis of this function.
//
// Arguments:
// N = total number of observations
// n = number of outliers to be removed
// m = number of model unknowns
// Returns: squared error threshold (x2)
double LinearFocusAlgorithm::peirce_criterion(double N, double n, double m)
{
    double x2 = 0.0;
    if (N > 1)
    {
        // Calculate Q (Nth root of Gould's equation B):
        double Q = (pow(n, (n / N)) * pow((N - n), ((N - n) / N))) / N;

        // Initialize R values
        double r_new = 1.0;
        double r_old = 0.0;

        // Start iteration to converge on R:
        while (abs(r_new - r_old) > (N * 2.0e-16))
        {
            // Calculate Lamda
            // (1 / (N - n) th root of Gould's equation B
            double ldiv = pow(r_new, n);
            if (ldiv == 0)
                ldiv = 1.0e-6;

            double Lamda = pow((pow(Q, N) / (ldiv)), (1.0 / (N - n)));

            // Calculate x -squared(Gould's equation C)
            x2 = 1.0 + (N - m - n) / n * (1.0 - pow(Lamda, 2.0));

            if (x2 < 0.0)
            {
                // If x2 goes negative, return 0
                x2 = 0.0;
                r_old = r_new;
            }
            else
            {
                // Use x-squared to update R(Gould 's equation D)
                r_old = r_new;
                r_new = exp((x2 - 1) / 2.0) * gsl_sf_erfc(sqrt(x2) / sqrt(2.0));
            }
        }
    }

    return x2;
}

// Return true if one of the 2 recent samples is among the best 2 samples so far.
bool LinearFocusAlgorithm::bestSamplesHeuristic()
{
    const int length = values.size();
    if (length < 5) return true;
    QVector<double> tempValues = values;
    std::nth_element(tempValues.begin(), tempValues.begin() + 2, tempValues.end());
    double secondBest = tempValues[1];
    if ((values[length - 1] <= secondBest) || (values[length - 2] <= secondBest))
        return true;
    return false;
}

// Return true if there are "streak" consecutive values which are successively worse.
bool LinearFocusAlgorithm::gettingWorse()
{
    // Must have this many consecutive values getting worse.
    constexpr int streak = 3;
    const int length = values.size();
    if (secondPassStartIndex < 0)
        return false;
    if (length < streak + 1)
        return false;
    // This insures that all the values we're checking are in the latest 2nd pass.
    if (length - secondPassStartIndex < streak + 1)
        return false;
    for (int i = length - 1; i >= length - streak; --i)
        if (values[i] <= values[i - 1])
            return false;
    return true;
}

}


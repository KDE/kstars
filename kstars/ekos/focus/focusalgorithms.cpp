/*
    SPDX-FileCopyrightText: 2019 Hy Murveit <hy-1@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusalgorithms.h"

#include "polynomialfit.h"
#include <QVector>
#include "kstars.h"

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
        // it's seen a minimum. It then tries to find that minimum in a 2nd pass.
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
        int newMeasurement(int position, double value) override;

        FocusAlgorithmInterface *Copy() override;

    private:

        // Called in newMeasurement. Sets up the next iteration.
        int completeIteration(int step);

        // Does the bookkeeping for the final focus solution.
        int setupSolution(int position, double value);

        // Called when we've found a solution, e.g. the HFR value is within tolerance of the desired value.
        // It it returns true, then it's decided tht we should try one more sample for a possible improvement.
        // If it returns false, then "play it safe" and use the current position as the solution.
        bool setupPendingSolution(int position, int step);

        // Determines the desired focus position for the first sample.
        void computeInitialPosition();

        // Sets the internal state for re-finding the minimum, and returns the requested next
        // position to sample.
        int setupSecondPass(int position, double value, double margin = 2.0);

        // Used in the 2nd pass. Focus is getting worse. Requires several consecutive samples getting worse.
        bool gettingWorse();

        // If one of the last 2 samples are as good or better than the 2nd best so far, return true.
        bool bestSamplesHeuristic();

        // Adds to the debug log a line summarizing the result of running this algorithm.
        void debugLog();

        // Used to time the focus algorithm.
        QTime stopWatch;

        // A vector containing the HFR values sampled by this algorithm so far.
        QVector<double> values;
        // A vector containing the focus positions corresponding to the HFR values stored above.
        QVector<int> positions;

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

void LinearFocusAlgorithm::computeInitialPosition()
{
    // These variables get reset if the algorithm is restarted.
    stepSize = params.initialStepSize;
    inFirstPass = true;
    solutionPending = false;
    firstPassBestValue = -1;
    firstPassBestPosition = 0;
    numPolySolutionsFound = 0;
    numRestartSolutionsFound = 0;
    secondPassStartIndex = -1;

    qCDebug(KSTARS_EKOS_FOCUS)
            << QString("Linear: v3.3. 1st pass. Travel %1 initStep %2 pos %3 min %4 max %5 maxIters %6 tolerance %7 minlimit %8 maxlimit %9 init#steps %10")
            .arg(params.maxTravel).arg(params.initialStepSize).arg(params.startPosition).arg(params.minPositionAllowed)
            .arg(params.maxPositionAllowed).arg(params.maxIterations).arg(params.focusTolerance).arg(minPositionLimit).arg(
                maxPositionLimit)
            .arg(params.initialOutwardSteps);

    requestedPosition = std::min(maxPositionLimit,
                                 static_cast<int>(params.startPosition + params.initialOutwardSteps * params.initialStepSize));
    passStartPosition = requestedPosition;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: initialPosition %1 sized %2")
                               .arg(requestedPosition).arg(params.initialStepSize);
}

int LinearFocusAlgorithm::newMeasurement(int position, double value)
{
    int thisStepSize = stepSize;
    ++numSteps;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: step %1, newMeasurement(%2, %3)").arg(numSteps).arg(position).arg(value);

    // Not sure how to get a general value for this. Skip this check?
    constexpr int LINEAR_POSITION_TOLERANCE = 25;
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
    values.push_back(value);
    positions.push_back(position);

    // If we've already found a pretty good solution and we're just optimizing, then either
    // continue optimizing or complete.
    if (solutionPending)
    {
        if (setupPendingSolution(position, thisStepSize))
            // We can continue to look a little further.
            return completeIteration(thisStepSize);
        else
            // Finish now
            return setupSolution(position, value);
    }

    if (inFirstPass)
    {
        constexpr int kMinPolynomialPoints = 5;
        constexpr int kNumPolySolutionsRequired = 2;
        constexpr int kNumRestartSolutionsRequired = 3;
        constexpr double kDecentValue = 2.5;

        if (values.size() >= kMinPolynomialPoints)
        {
            PolynomialFit fit(2, positions, values);
            double minPos, minVal;
            bool foundFit = fit.findMinimum(position, 0, 100000, &minPos, &minVal);
            if (!foundFit)
            {
                // I've found that the first sample can be odd--perhaps due to backlash.
                // Try again skipping the first sample, if we have sufficient points.
                if (values.size() > kMinPolynomialPoints)
                {
                    PolynomialFit fit2(2, positions.mid(1), values.mid(1));
                    foundFit = fit2.findMinimum(position, 0, 100000, &minPos, &minVal);
                    minPos = minPos + 1;
                }
            }
            if (foundFit)
            {
                const int distanceToMin = static_cast<int>(position - minPos);
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: poly fit(%1): %2 = %3 @ %4 distToMin %5")
                                           .arg(positions.size()).arg(minPos).arg(minVal).arg(position).arg(distanceToMin);
                if (distanceToMin >= 0)
                {
                    // The minimum is further inward.
                    numPolySolutionsFound = 0;
                    numRestartSolutionsFound = 0;
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Solutions reset %1 = %2").arg(minPos).arg(minVal);
                    if (value > kDecentValue)
                    {
                        // Only skip samples if the HFV values aren't very good.
                        const int stepsToMin = distanceToMin / stepSize;
                        // Temporarily increase the step size if the minimum is very far inward.
                        if (stepsToMin >= 8)
                            thisStepSize = stepSize * 4;
                        else if (stepsToMin >= 4)
                            thisStepSize = stepSize * 2;
                    }
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

                if (numPolySolutionsFound >= kNumPolySolutionsRequired)
                {
                    // We found a minimum. Setup the 2nd pass. We could use either the polynomial min or the
                    // min measured star as the target HFR. I've seen using the polynomial minimum to be
                    // sometimes too conservative, sometimes too low. For now using the min sample.
                    double minMeasurement = *std::min_element(values.begin(), values.end());
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: 1stPass solution @ %1: pos %2 val %3, min measurement %4")
                                               .arg(position).arg(minPos).arg(minVal).arg(minMeasurement);
                    return setupSecondPass(static_cast<int>(minPos), minMeasurement);
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
    else
    {
        // In a 2nd pass looking to recreate the 1st pass' minimum.

        // If the current HFR is good-enough to complete.
        if (value < firstPassBestValue * (1.0 + params.focusTolerance))
        {
            if (setupPendingSolution(position, thisStepSize))
                // We could finish now, but let's look a little further.
                return completeIteration(thisStepSize);
            else
                // We're done.
                return setupSolution(position, value);
        }
        else if (gettingWorse())
        {
            // Doesn't look like we'll find something close to the min. Retry the 2nd pass.
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: getting worse, re-running 2nd pass");
            return setupSecondPass(firstPassBestPosition, firstPassBestValue);
        }
    }

    return completeIteration(thisStepSize);
}

int LinearFocusAlgorithm::setupSolution(int position, double value)
{
    focusSolution = position;
    focusHFR = value;
    done = true;
    doneString = i18n("Solution found.");
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: 2ndPass solution @ %1 = %2 (best %3)")
                               .arg(position).arg(value).arg(firstPassBestValue);
    debugLog();
    return -1;
}

int LinearFocusAlgorithm::completeIteration(int step)
{
    if (numSteps == params.maxIterations - 2)
    {
        // If we're close to exceeding the iteration limit, retry this pass near the old minimum position.
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
        const int minIndex = static_cast<int>(std::min_element(values.begin(), values.end()) - values.begin());
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: reached end without Vmin. Restarting %1 pos %2 value %3")
                                   .arg(minIndex).arg(positions[minIndex]).arg(values[minIndex]);
        return setupSecondPass(positions[minIndex], values[minIndex]);
    }
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: requesting position %1").arg(requestedPosition);
    return requestedPosition;
}

// This is called in the 2nd pass, when we've found a sample point that's within tolerance of the 1st pass'
// best value. The 2nd pass could simply finish, using this value as the final solution.
// However, this method checks to see if we might go a bit further. It does so if
// - the current value is an improvement on the previous 2nd-pass value (or if this is the 1st 2nd-pass value),
// - the current position is greater than the min of the v-curve computed by the 1st pass,
// - the number of steps taken so far is not close the max number of allowable steps.
bool LinearFocusAlgorithm::setupPendingSolution(int position, int step)
{
    const int length = values.size();
    const int secondPassIndex = length - secondPassStartIndex;
    // This is either the first sample in the 2nd pass, or an improvement over the previous sample.
    bool notGettingWorse = (secondPassIndex <= 1) || (values[length - 1] < values[length - 2]);
    if (notGettingWorse &&
            position - step > firstPassBestPosition &&
            numSteps < params.maxIterations - 2 &&
            position - step > minPositionLimit
       )
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Position(%1) & HFR(%2) good, but searching further")
                                   .arg(position).arg(values[length - 1]);
        solutionPending = true;
        return true;
    }
    return false;
}

void LinearFocusAlgorithm::debugLog()
{
    QString str("Linear: points=[");
    for (int i = 0; i < positions.size(); ++i)
    {
        str.append(QString("(%1, %2)").arg(positions[i]).arg(values[i]));
        if (i < positions.size() - 1)
            str.append(", ");
    }
    str.append(QString("];iterations=%1").arg(numSteps));
    str.append(QString(";duration=%1").arg(stopWatch.elapsed() / 1000));
    str.append(QString(";solution=%1").arg(focusSolution));
    str.append(QString(";HFR=%1").arg(focusHFR));
    str.append(QString(";filter='%1'").arg(params.filterName));
    str.append(QString(";temperature=%1").arg(params.temperature));

    qCDebug(KSTARS_EKOS_FOCUS) << str;
}

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


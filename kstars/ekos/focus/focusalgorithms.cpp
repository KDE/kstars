/*  Ekos
    Copyright (C) 2019 Hy Murveit <hy-1@murveit.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
    int initialPosition() override { return requestedPosition; }

    // Pass in the measurement for the last requested position. Returns the position for the next
    // requested measurement, or -1 if the algorithm's done or if there's an error.
    int newMeasurement(int position, double value) override;

private:

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
};

FocusAlgorithmInterface *MakeLinearFocuser(const FocusAlgorithmInterface::FocusParams& params)
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
    firstPassBestValue = -1;
    numPolySolutionsFound = 0;
    numRestartSolutionsFound = 0;
    secondPassStartIndex = -1;

    qCDebug(KSTARS_EKOS_FOCUS)
            << QString("Linear: v3.1. 1st pass. Travel %1 initStep %2 pos %3 min %4 max %5 maxIters %6 tolerance %7 minlimit %8 maxlimit %9")
               .arg(params.maxTravel).arg(params.initialStepSize).arg(params.startPosition).arg(params.minPositionAllowed)
               .arg(params.maxPositionAllowed).arg(params.maxIterations).arg(params.focusTolerance).arg(minPositionLimit).arg(maxPositionLimit);

    const int position = params.startPosition;
    int start, end;

    // If the bounds allow, set the focus to half-travel above the current position
    // and sample focusing in down-to half-travel below the current position.
    if (position + params.maxTravel <= maxPositionLimit && position - params.maxTravel >= minPositionLimit)
    {
        start = position + params.maxTravel;
        end = position - params.maxTravel;
    }
    else if (position + params.maxTravel > maxPositionLimit)
    {
        // If the above hits the focus-out bound, start from the highest focus position possible
        // and sample down the travel amount.
        start = maxPositionLimit;
        end = std::max(minPositionLimit, start - params.maxTravel);
    }
    else
    {
        // If the range above hits the focus-in bound, try to start from max-travel above the min position
        // and sample down to the min position.
        start = std::min(minPositionLimit + params.maxTravel, maxPositionLimit);
        end = minPositionLimit;
    }

    // Now that the start and end of the sampling interval is set,
    // check to see if the params were reasonably set up.
    // If too many steps (more than half the allotment) are required, honor stepSize over maxTravel.
    const int nSteps = (start - end) / params.initialStepSize;
    if (nSteps > params.maxIterations/2)
    {
        const int newStart = position + params.initialStepSize * (params.maxIterations/6);
        start = std::min(newStart, maxPositionLimit);
    }
    requestedPosition = start;
    passStartPosition = requestedPosition;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: initialPosition %1 sized %2")
                                  .arg(start).arg(params.initialStepSize);
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

    if (inFirstPass)
    {
        constexpr int kMinPolynomialPoints = 5;
        constexpr int kNumPolySolutionsRequired = 3;
        constexpr int kNumRestartSolutionsRequired = 3;
        constexpr double kDecentValue = 2.5;

        if (values.size() >= kMinPolynomialPoints)
        {
            PolynomialFit fit(2, positions, values);
            double minPos, minVal;
            if (fit.findMinimum(position, 0, 100000, &minPos, &minVal))
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
                    params.startPosition = static_cast<int>(minPos);
                    computeInitialPosition();
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: Restart @ %1: %1 = %2, start at %3")
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
        if (value < firstPassBestValue * (1.0 + params.focusTolerance))
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
        else if (gettingWorse())
        {
            // Doesn't look like we'll find something close to the min. Retry the 2nd pass.
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Linear: getting worse, re-running 2nd pass");
            return setupSecondPass(firstPassBestPosition, firstPassBestValue);
        }
    }

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
    requestedPosition = requestedPosition - thisStepSize;

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

void LinearFocusAlgorithm::debugLog()
{
    QString str("Linear: points=[");
    for (int i = 0; i < positions.size(); ++i)
    {
        str.append(QString("(%1, %2)").arg(positions[i]).arg(values[i]));
        if (i < positions.size()-1)
            str.append(", ");
    }
    str.append(QString("];iterations=%1").arg(numSteps));
    str.append(QString(";duration=%1").arg(stopWatch.elapsed()/1000));
    str.append(QString(";solution=%1").arg(focusSolution));
    str.append(QString(";HFR=%1").arg(focusHFR));
    str.append(QString(";filter='%1'").arg(params.filterName));

    qCDebug(KSTARS_EKOS_FOCUS) << str;
}

int LinearFocusAlgorithm::setupSecondPass(int position, double value, double margin)
{
    firstPassBestPosition = position;
    firstPassBestValue = value;
    inFirstPass = false;
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
    if ((values[length-1] <= secondBest) || (values[length-2] <= secondBest))
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
    if (length < streak+1)
        return false;
    // This insures that all the values we're checking are in the latest 2nd pass.
    if (length - secondPassStartIndex < streak + 1)
        return false;
    for (int i = length-1; i >= length-streak; --i)
        if (values[i] <= values[i-1])
            return false;
    return true;
}

}


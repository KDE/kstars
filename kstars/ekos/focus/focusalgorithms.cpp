/*  Ekos
    Copyright (C) 2019 Hy Murveit <hy-1@murveit.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "focusalgorithms.h"

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

    // Returns a score evaluating whether index is the location of a minumum in the
    // HFR vector. -1 means no. A positive integer indicates yes, but can be overruled
    // by another index with a better score.
    int evaluateMinimum(int index, bool lastSample);

    // Sets the internal state for re-finding the minimum, and returns the requested next
    // position to sample.
    int setupSecondPass(int minIndex, double margin = 2.0);

    // Used in the 2nd pass. Focus is getting worse. Requires several consecutive samples getting worse.
    bool gettingWorse();

    // A vector containing the HFR values sampled by this algorithm so far.
    QVector<double> values;
    // A vector containing the focus positions corresponding to the HFR values stored above.
    QVector<int> positions;

    // Focus position requested by this algorithm the previous step.
    int requestedPosition;
    // Number of iterations processed so far.
    int numSteps;
    // The best value in the first pass. The 2nd pass attempts to get within
    // tolerance of this value.
    double minValue;
    // The sampling interval--the number of focuser steps reduced each iteration.
    int stepSize;
    // The minimum focus position to use. Computed from the focuser limits and maxTravel.
    int minPositionLimit;
    // The maximum focus position to use. Computed from the focuser limits and maxTravel.
    int maxPositionLimit;
    // The index of the minimum found in the first pass.
    int firstPassMinIndex;
    // True if the first iteration has already found a minimum.
    bool minimumFound;
    // True if the 2nd pass re-found the minimum, and thus the algorithm is done.
    bool minimumReFound;
    // True if the algorithm failed, and thus is done.
    bool searchFailed;
};

FocusAlgorithmInterface *MakeLinearFocuser(const FocusAlgorithmInterface::FocusParams& params)
{
    return new LinearFocusAlgorithm(params);
}

LinearFocusAlgorithm::LinearFocusAlgorithm(const FocusParams &focusParams)
    : FocusAlgorithmInterface(focusParams)
{
    requestedPosition = params.startPosition;
    stepSize = params.initialStepSize;
    minimumFound = false;
    numSteps = 0;
    minimumReFound = false;
    searchFailed = false;
    minValue = 0;

    maxPositionLimit = std::min(params.maxPositionAllowed, params.startPosition + params.maxTravel);
    minPositionLimit = std::max(params.minPositionAllowed, params.startPosition - params.maxTravel);
    qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: Travel %1 initStep %2 pos %3 min %4 max %5 maxIters %6 tolerance %7 minlimit %8 maxlimit %9")
                                  .arg(params.maxTravel).arg(params.initialStepSize).arg(params.startPosition).arg(params.minPositionAllowed)
                                  .arg(params.maxPositionAllowed).arg(params.maxIterations).arg(params.focusTolerance).arg(minPositionLimit).arg(maxPositionLimit);
    computeInitialPosition();
}

void LinearFocusAlgorithm::computeInitialPosition()
{
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
        const int topSize = start - position;
        const int bottomSize = position - end;
        if (topSize <= bottomSize)
        {
            const int newStart = position + params.initialStepSize * (params.maxIterations/4);
            start = std::min(newStart, maxPositionLimit);
            end = start - params.initialStepSize * (params.maxIterations/2);
        }
        else
        {
            const int newEnd = position - params.initialStepSize * (params.maxIterations/4);
            end = std::max(newEnd, minPositionLimit);
            start = end + params.initialStepSize * (params.maxIterations/2);
        }
    }
    requestedPosition = start;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: initialPosition %1 end %2 steps %3 sized %4")
                                  .arg(start).arg(end).arg((start-end)/params.initialStepSize).arg(params.initialStepSize);
}

int LinearFocusAlgorithm::newMeasurement(int position, double value)
{
    ++numSteps;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: step %1, newMeasurement(%2, %3)").arg(numSteps).arg(position).arg(value);

    // Not sure how to get a general value for this. Skip this check?
    constexpr int LINEAR_POSITION_TOLERANCE = 25;
    if (abs(position - requestedPosition) > LINEAR_POSITION_TOLERANCE)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: error didn't get the requested position");
        return requestedPosition;
    }
    // Have we already found a solution?
    if (focusSolution != -1)
    {
        doneString = i18n("Called newMeasurement after a solution was found.");
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: error %1").arg(doneString);
        return -1;
    }

    // Store the sample values.
    values.push_back(value);
    positions.push_back(position);

    if (!minimumFound)
    {
        // The first pass. We're looking for the minimum of a v-curve.
        // Check all possible indices to see if we have a V-curve minimum position, and,
        // if so, which is the best.
        int bestScore = -1;
        int bestIndex = -1;
        // Is this the last sample before we bump into a limit.
        bool lastSample = requestedPosition - stepSize < minPositionLimit;
        for (int i = 0; i < values.size(); ++i) {
            const int score = evaluateMinimum(i, lastSample);
            if (score > 0) {
                minimumFound = true;
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = i;
                }
            }
        }
        if (minimumFound)
        {
            qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: Minimum found at index %1").arg(bestIndex);
            return setupSecondPass(bestIndex);
        }
    }
    else
    {
        // We previously found the v-curve minimum. We're now in a 2nd pass looking to recreate it.
        if (value < minValue * (1.0 + params.focusTolerance))
        {
            focusSolution = position;
            minimumReFound = true;
            done = true;
            doneString = i18n("Solution found.");
            qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: solution at position %1 value %2 (best %3)").arg(position).arg(value).arg(minValue);
            return -1;
        }
        else
        {
            qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: %1 %2 not a solution, not < %3").arg(position).arg(value).arg(minValue * (1.0+params.focusTolerance));
            if (gettingWorse())
            {
                // Doesn't look like we'll find something close to the min. Retry the 2nd pass.
                qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: getting worse, re-running 2nd pass");
                return setupSecondPass(firstPassMinIndex);
            }
        }
    }

    if (numSteps == params.maxIterations - 1)
    {
        // If we're close to exceeding the iteration limit, retry this pass near the old minimum position.
        const int minIndex = static_cast<int>(std::min_element(values.begin(), values.end()) - values.begin());
        return setupSecondPass(minIndex, 0.5);
    }
    else if (numSteps > params.maxIterations)
    {
        // Fail. Exceeded our alloted number of iterations.
        searchFailed = true;
        done = true;
        doneString = i18n("Too many steps.");
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: error %1").arg(doneString);
        return -1;
    }

    // Setup the next sample.
    requestedPosition = requestedPosition - stepSize;

    // Make sure the next sample is within bounds.
    if (requestedPosition < minPositionLimit)
    {
        // The position is too low. Pick the min value and go to (or retry) a 2nd iteration.
        const int minIndex = static_cast<int>(std::min_element(values.begin(), values.end()) - values.begin());
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: reached end without Vmin. Restarting %1 pos %2 value %3")
                                      .arg(minIndex).arg(positions[minIndex]).arg(values[minIndex]);
        return setupSecondPass(minIndex);
    }
    qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: requesting position %1").arg(requestedPosition);
    return requestedPosition;
}

int LinearFocusAlgorithm::setupSecondPass(int minIndex, double margin)
{
    firstPassMinIndex = minIndex;
    minimumFound = true;

    int bestPosition = positions[minIndex];
    minValue = values[minIndex];
    // Arbitrarily go back "margin" steps above the best position.
    // Could be a problem if backlash were worse than that many steps.
    requestedPosition = std::min(static_cast<int>(bestPosition + stepSize * margin), maxPositionLimit);
    stepSize = params.initialStepSize / 2;
    qCDebug(KSTARS_EKOS_FOCUS) << QString("LinearFocuser: 2ndPass starting at %1 step %2").arg(requestedPosition).arg(stepSize);
    return requestedPosition;
}

// Given the values in the two vectors (values & positions), Evaluates whether index
// is the position of a "V". If a negative number is returned, the answer is no.
// A positive return value is a score. Best score wins.
//
// Implemented as simple heuristic. A V point must have:
// - at least 2 points on both sides that have higher values
// - for both sides: # higher values - # lower values > 2
// Return value is the number of higher values minus the number of lower values
// counted on both sides.
// Note: could have just returned positive for only the minimum value, but this
// allows for others to be possible minima as well, if the min is too close to the edge.

int LinearFocusAlgorithm::evaluateMinimum(int index, bool lastSample)
{
    const double indexValue = values[index];
    const int threshold = 2;

    if (index < threshold)
        return -1;
    if (!lastSample && index >= values.size() - threshold)
        return -1;

    // Left means lower focus position (focused in), right means higher position.
    int right_num_higher = 0, right_num_lower = 0;
    int left_num_higher = 0, left_num_lower = 0;
    for (int i = 0; i < index; ++i)
    {
        if (values[i] >= indexValue) ++right_num_higher;
        else ++right_num_lower;
    }
    for (int i = index+1; i < values.size(); ++i)
    {
        if (values[i] >= indexValue) ++left_num_higher;
        else ++left_num_lower;
    }
    const int right_difference = right_num_higher - right_num_lower;
    const int left_difference = left_num_higher - left_num_lower;
    if (right_difference >= threshold && left_difference >= threshold)
        return right_difference + left_difference;
    return -1;
}

// Return true if there are "streak" consecutive values which are successively worse.
bool LinearFocusAlgorithm::gettingWorse()
{
    // Must have this many consecutive values getting worse.
    constexpr int streak = 3;
    const int length = values.size();
    if (length < streak+1)
        return false;
    for (int i = length-1; i >= length-streak; --i)
        if (values[i] <= values[i-1])
            return false;
    return true;
}

}


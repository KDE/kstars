/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/focus/focusalgorithms.h"

#include <QTest>
#include <memory>

#include <QObject>

// At this point, only the methods in focusalgorithms.h are tested.

class TestFocus : public QObject
{
        Q_OBJECT

    public:
        /** @short Constructor */
        TestFocus();

        /** @short Destructor */
        ~TestFocus() override = default;

    private slots:
        void basicTest();
        void restartTest();
        void L1PHyperbolaTest();
        void L1PParabolaTest();
        void L1PQuadraticTest();
};

#include "testfocus.moc"

using Ekos::FocusAlgorithmInterface;

FocusAlgorithmInterface::FocusParams makeParams()
{
    const int maxTravel = 100000;
    const int initialStepSize = 25;
    const int startPosition = 10000;
    const int minPositionAllowed = 0;
    const int maxPositionAllowed = 1000000;
    const int maxIterations = 30;
    const double focusTolerance = 0.05;
    const QString filterName = "Red";
    const double temperature = 20.0;
    const double initialOutwardSteps = 5;
    const int numSteps = 11;
    const Ekos::Focus::Algorithm focusAlgorithm = Ekos::Focus::FOCUS_LINEAR;
    const int backlash = 0;
    const Ekos::CurveFitting::CurveFit curveFit = Ekos::CurveFitting::FOCUS_QUADRATIC;
    const bool useWeights = false;
    const Ekos::Focus::StarMeasure starMeasure = Ekos::Focus::FOCUS_STAR_HFR;
    const Ekos::Focus::StarPSF starPSF = Ekos::Focus::FOCUS_STAR_GAUSSIAN;
    const bool refineCurveFit = false;
    const Ekos::Focus::FocusWalk focusWalk = Ekos::Focus::FOCUS_WALK_CLASSIC;
    auto const optimisationDirection = Ekos::CurveFitting::OPTIMISATION_MINIMISE;
    auto const weightCalculation = Mathematics::RobustStatistics::SCALE_QESTIMATOR;

    const FocusAlgorithmInterface::FocusParams params(nullptr,
            maxTravel, initialStepSize, startPosition, minPositionAllowed,
            maxPositionAllowed, maxIterations, focusTolerance, filterName,
            temperature, initialOutwardSteps, numSteps, focusAlgorithm, backlash,
            curveFit, useWeights, starMeasure, starPSF, refineCurveFit, focusWalk,
            optimisationDirection, weightCalculation);

    return params;
}

FocusAlgorithmInterface::FocusParams makeL1PHyperbolaParams()
{
    const int maxTravel = 100000;
    const int initialStepSize = 25;
    const int startPosition = 10000;
    const int minPositionAllowed = 0;
    const int maxPositionAllowed = 1000000;
    const int maxIterations = 30;
    const double focusTolerance = 0.05;
    const QString filterName = "Green";
    const double temperature = 20.0;
    const double initialOutwardSteps = 5;
    const int numSteps = 11;
    const Ekos::Focus::Algorithm focusAlgorithm = Ekos::Focus::FOCUS_LINEAR1PASS;
    const int backlash = 300;
    const Ekos::CurveFitting::CurveFit curveFit = Ekos::CurveFitting::FOCUS_HYPERBOLA;
    const bool useWeights = true;
    const Ekos::Focus::StarMeasure starMeasure = Ekos::Focus::FOCUS_STAR_HFR;
    const Ekos::Focus::StarPSF starPSF = Ekos::Focus::FOCUS_STAR_GAUSSIAN;
    const bool refineCurveFit = false;
    const Ekos::Focus::FocusWalk focusWalk = Ekos::Focus::FOCUS_WALK_CLASSIC;
    auto const optimisationDirection = Ekos::CurveFitting::OPTIMISATION_MINIMISE;
    auto const weightCalculation = Mathematics::RobustStatistics::SCALE_QESTIMATOR;

    const FocusAlgorithmInterface::FocusParams params(nullptr,
            maxTravel, initialStepSize, startPosition, minPositionAllowed,
            maxPositionAllowed, maxIterations, focusTolerance, filterName,
            temperature, initialOutwardSteps, numSteps, focusAlgorithm, backlash,
            curveFit, useWeights, starMeasure, starPSF, refineCurveFit, focusWalk,
            optimisationDirection, weightCalculation);

    return params;
}

FocusAlgorithmInterface::FocusParams makeL1PParabolaParams()
{
    const int maxTravel = 100000;
    const int initialStepSize = 25;
    const int startPosition = 10000;
    const int minPositionAllowed = 0;
    const int maxPositionAllowed = 1000000;
    const int maxIterations = 30;
    const double focusTolerance = 0.05;
    const QString filterName = "Blue";
    const double temperature = 20.0;
    const double initialOutwardSteps = 5;
    const int numSteps = 11;
    const Ekos::Focus::Algorithm focusAlgorithm = Ekos::Focus::FOCUS_LINEAR1PASS;
    const int backlash = 200;
    const Ekos::CurveFitting::CurveFit curveFit = Ekos::CurveFitting::FOCUS_PARABOLA;
    const bool useWeights = false;
    const Ekos::Focus::StarMeasure starMeasure = Ekos::Focus::FOCUS_STAR_HFR;
    const Ekos::Focus::StarPSF starPSF = Ekos::Focus::FOCUS_STAR_GAUSSIAN;
    const bool refineCurveFit = false;
    const Ekos::Focus::FocusWalk focusWalk = Ekos::Focus::FOCUS_WALK_CLASSIC;
    auto const optimisationDirection = Ekos::CurveFitting::OPTIMISATION_MINIMISE;
    auto const weightCalculation = Mathematics::RobustStatistics::SCALE_QESTIMATOR;

    const FocusAlgorithmInterface::FocusParams params(nullptr,
            maxTravel, initialStepSize, startPosition, minPositionAllowed,
            maxPositionAllowed, maxIterations, focusTolerance, filterName,
            temperature, initialOutwardSteps, numSteps, focusAlgorithm, backlash,
            curveFit, useWeights, starMeasure, starPSF, refineCurveFit, focusWalk,
            optimisationDirection, weightCalculation);

    return params;
}

FocusAlgorithmInterface::FocusParams makeL1PQuadraticParams()
{
    const int maxTravel = 100000;
    const int initialStepSize = 25;
    const int startPosition = 10000;
    const int minPositionAllowed = 0;
    const int maxPositionAllowed = 1000000;
    const int maxIterations = 30;
    const double focusTolerance = 0.05;
    const QString filterName = "Lum";
    const double temperature = 20.0;
    const double initialOutwardSteps = 5;
    const int numSteps = 11;
    const Ekos::Focus::Algorithm focusAlgorithm = Ekos::Focus::FOCUS_LINEAR1PASS;
    const int backlash = 0;
    const Ekos::CurveFitting::CurveFit curveFit = Ekos::CurveFitting::FOCUS_QUADRATIC;
    const bool useWeights = false;
    const Ekos::Focus::StarMeasure starMeasure = Ekos::Focus::FOCUS_STAR_HFR;
    const Ekos::Focus::StarPSF starPSF = Ekos::Focus::FOCUS_STAR_GAUSSIAN;
    const bool refineCurveFit = false;
    const Ekos::Focus::FocusWalk focusWalk = Ekos::Focus::FOCUS_WALK_CLASSIC;
    auto const optimisationDirection = Ekos::CurveFitting::OPTIMISATION_MINIMISE;
    auto const weightCalculation = Mathematics::RobustStatistics::SCALE_QESTIMATOR;

    const FocusAlgorithmInterface::FocusParams params(nullptr,
            maxTravel, initialStepSize, startPosition, minPositionAllowed,
            maxPositionAllowed, maxIterations, focusTolerance, filterName,
            temperature, initialOutwardSteps, numSteps, focusAlgorithm, backlash,
            curveFit, useWeights, starMeasure, starPSF, refineCurveFit, focusWalk,
            optimisationDirection, weightCalculation);

    return params;
}

TestFocus::TestFocus() : QObject()
{
}

#define CompareFloat(d1,d2) QVERIFY(fabs((d1) - (d2)) < .0001)

void TestFocus::basicTest()
{
    // Test initial position. Since we set it up so that the min/max PositionAllowed
    // are not near the start position, we should start out with
    // initial position +
    auto params = makeParams();
    std::unique_ptr<FocusAlgorithmInterface> focuser(MakeLinearFocuser(params));
    int position = focuser->initialPosition();
    // The first position should be initialPosition + stepSize * initialOutwardSteps,
    // unless maxPositionAllowed or maxTravel doesn't allow that.
    QCOMPARE(position, static_cast <int> (params.startPosition + params.initialOutwardSteps * params.initialStepSize));

    params.maxTravel = 4 * params.initialStepSize;
    focuser.reset(MakeLinearFocuser(params));
    position = focuser->initialPosition();
    QCOMPARE(position, params.startPosition + params.maxTravel);

    params.maxPositionAllowed = params.startPosition + 3 * params.initialStepSize;
    focuser.reset(MakeLinearFocuser(params));
    position = focuser->initialPosition();
    QCOMPARE(position, params.maxPositionAllowed);

    // go back to the default params
    params = makeParams();
    focuser.reset(MakeLinearFocuser(params));
    int currentPosition = focuser->initialPosition();

    // Here we run the algorithm, feeding it a v-curve, and watching it solve.
    // First pass: Should see the position reducing by initialStepSize
    position = focuser->newMeasurement(currentPosition, 5, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 4, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 3, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    // Level off and then increase.

    position = focuser->newMeasurement(currentPosition, 1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 3, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    // At the point the focuser should end the first pass and start a new 2nd pass.
    // The position to start the 2nd pass is hardcoded here (it is a polynomial
    // curve-fit of the above data).
    int secondPassStart = 10064;
    position = focuser->newMeasurement(currentPosition, 4, 1);
    QCOMPARE(position, secondPassStart);
    currentPosition = position;

    // The 2nd pass steps down by half step-size, and should terminate when we
    // pass in an HFR within tolerance of the best HFR in the first pass (which was 1.0).

    position = focuser->newMeasurement(currentPosition, 1.4, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    // Save the state from here for testing the retry logic below.
    std::unique_ptr<FocusAlgorithmInterface> focuserRetry(focuser->Copy());
    int retryPosition = currentPosition;

    position = focuser->newMeasurement(currentPosition, 1.3, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.2, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    // Making a copy of the focuser state at this point, for testing a scenario below
    // where the system does not get close enough to the desired HFR.
    std::unique_ptr<FocusAlgorithmInterface> focuserMissTolerance(focuser->Copy());
    int missTolerancePosition = currentPosition;

    // 1.04 is within tolerance of 1.0
    // It would complete, but we now cross the 1st-pass-best position (10014)
    // and use even smaller steps.
    position = focuser->newMeasurement(currentPosition, 1.04, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 4);
    QVERIFY(!focuser->isDone());
    currentPosition = position;

    // Now let it finally complete.
    position = focuser->newMeasurement(currentPosition, 1.3, 1);
    QCOMPARE(position, currentPosition);
    position = focuser->newMeasurement(currentPosition, 1.3, 1);
    // Since the focuser is past the 1st pass min, it only retries once then finishes.
    QCOMPARE(position, -1);
    QVERIFY(focuser->isDone());
    QCOMPARE(focuser->solution(), currentPosition);

    // Test the focuser retry logic. When the focuser gets within its tolerance of the solution
    // in its 2nd pass, but hasn't yet reached the 1st pass' minimum, it will continue to iterate
    // and move its position inward as the HFRs it measures continue to improve.
    // However, if the HFR sample got worse, it will resample at the same position a few times,
    // in case there was noise in the sample.

    focuser.swap(focuserRetry);
    position = focuser->newMeasurement(retryPosition, 1.04, 1);
    QCOMPARE(position, retryPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.05, 1);
    // HFR got worse, we're within tolderance of the solution. It should retry the same position.
    QCOMPARE(position, currentPosition);

    position = focuser->newMeasurement(currentPosition, 1.10, 1);
    QCOMPARE(position, currentPosition);

    position = focuser->newMeasurement(currentPosition, 1.10, 1);
    QCOMPARE(position, currentPosition);

    // Making a copy of the focuser state at this point, for later testing.
    std::unique_ptr<FocusAlgorithmInterface> focuserRetry2(focuser->Copy());
    int retry2Position = currentPosition;

    // Try 2 scenarious from this position.

    // 1. HFR is worse again compared to the best value above (1.04), give up.
    position = focuser->newMeasurement(retry2Position, 1.05, 1);
    QCOMPARE(position, -1);
    QVERIFY(focuser->isDone());
    QCOMPARE(focuser->solution(), currentPosition);

    // 2. HFR gets better. Continue moving toward the pass1 solution.
    focuser.swap(focuserRetry2);
    position = focuser->newMeasurement(retry2Position, 1.03, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.02, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    // Finally we're close enough to the 1st pass best to finish.
    // Since we crossed the 1st pass best position, we use a smaller step size.
    position = focuser->newMeasurement(currentPosition, 1.01, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 4);
    currentPosition = position;

    // Fail a few times trying to get better
    position = focuser->newMeasurement(currentPosition, 1.05, 1);
    QCOMPARE(position, currentPosition);
    position = focuser->newMeasurement(currentPosition, 1.05, 1);
    // Since the focuser is past the 1st pass min, it only retries once then finishes.
    QCOMPARE(position, -1);
    QVERIFY(focuser->isDone());
    QCOMPARE(focuser->solution(), currentPosition);

    // Retry testing is done.

    // We pick up from above just before the system reached the desired tolerance.
    // In this case, it misses the desired HFR value, and continues processing.
    focuser.swap(focuserMissTolerance);

    // Sample HFR=1.06 instead of the 1.04 above--not within tolerance.
    position = focuser->newMeasurement(missTolerancePosition, 1.06, 1);
    QCOMPARE(position, missTolerancePosition - params.initialStepSize / 2);
    currentPosition = position;

    // HFRs continue to get worse (the min on this v-curve missed the threshold).
    position = focuser->newMeasurement(currentPosition, 1.1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.5, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    // At this point, the focuser realizes it missed the minimum and resets.
    // The value it goes back to is a polynomial fit, and hardcoded here.
    int returnPosition = 10038;
    position = focuser->newMeasurement(currentPosition, 2.0, 1);
    QCOMPARE(position, returnPosition);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.5, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    // Making a copy of the focuser state at this point, for later testing.
    std::unique_ptr<FocusAlgorithmInterface> focuser2(focuser->Copy());
    int currentPosition2 = currentPosition;

    // This time we get within tolerance,
    // and we've crossed the 1st-pass min so smaller step size.
    position = focuser->newMeasurement(currentPosition, 1.03, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 4);
    currentPosition = position;

    // Start getting worse and eventually finish.
    position = focuser->newMeasurement(currentPosition, 1.10, 1);
    QCOMPARE(position, currentPosition);
    position = focuser->newMeasurement(currentPosition, 1.10, 1);
    // Since the focuser is past the 1st pass min, it only retries once then finishes.
    QCOMPARE(position, -1);
    QVERIFY(focuser->isDone());
    QCOMPARE(focuser->solution(), currentPosition);

    // Alternative reality.
    focuser.swap(focuser2);

    // We miss the tolerance again.
    position = focuser->newMeasurement(currentPosition2, 1.06, 1);
    QCOMPARE(position, currentPosition2 - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.2, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    // It should notice we're getting worse (3 consecutive) and reset again.
    int returnPosition2 = 10038;
    position = focuser->newMeasurement(currentPosition, 1.3, 1);
    QCOMPARE(position, returnPosition2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition2, 1.06, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition2, 1.1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition2, 1.2, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    // Again it notices 3 bad in a row
    int returnPosition3 = 10038;
    position = focuser->newMeasurement(currentPosition, 1.4, 1);
    QCOMPARE(position, returnPosition3);

    // Now we're within 3 of the max number of iterations.
    // It resets again, trying to place us near the best position seen.
    int returnPosition4 = 10031;
    position = focuser->newMeasurement(currentPosition2, 1.06, 1);
    QCOMPARE(position, returnPosition4);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition2, 1.2, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition2, 1.3, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize / 2);
    currentPosition = position;

    // And at this point we exceed the allowed number of steps (30) and fail.
    position = focuser->newMeasurement(currentPosition2, 1.4, 1);
    QCOMPARE(position, -1);
    QVERIFY(focuser->isDone());
    QCOMPARE(focuser->solution(), -1);
}

// This test replays a situation from a log where the focuser started "too low"
// and restarts a bit higher.
void TestFocus::restartTest()
{
    auto params = makeParams();
    params.startPosition = 11592;
    std::unique_ptr<FocusAlgorithmInterface> focuser(MakeLinearFocuser(params));

    int currentPosition = focuser->initialPosition();
    QCOMPARE(currentPosition, 11717);

    int position = focuser->newMeasurement(currentPosition, 1.24586, 1);
    QCOMPARE(position, 11692);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.22075, 1);
    QCOMPARE(position, 11667);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.47694, 1);
    QCOMPARE(position, 11642);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.71925, 1);
    QCOMPARE(position, 11617);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.73312, 1);
    QCOMPARE(position, 11592);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1.90926, 1);
    QCOMPARE(position, 11567);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2.09901, 1);

    // At this point it should restart with a higher initial position, but not too high.
    QCOMPARE(position, 11842);
}

void TestFocus::L1PHyperbolaTest()
{
    // Linear 1 Pass hyperbola curve test.
    auto params = makeL1PHyperbolaParams();
    std::unique_ptr<FocusAlgorithmInterface> focuser(MakeLinearFocuser(params));
    int position = focuser->initialPosition();
    // The first position should be initialPosition + stepSize * initialOutwardSteps,
    // unless maxPositionAllowed or maxTravel doesn't allow that.
    QCOMPARE(position, static_cast <int> (params.startPosition + params.initialOutwardSteps * params.initialStepSize));

    params.maxTravel = 4 * params.initialStepSize;
    focuser.reset(MakeLinearFocuser(params));
    position = focuser->initialPosition();
    QCOMPARE(position, params.startPosition + params.maxTravel);

    params.maxPositionAllowed = params.startPosition + 3 * params.initialStepSize;
    focuser.reset(MakeLinearFocuser(params));
    position = focuser->initialPosition();
    QCOMPARE(position, params.maxPositionAllowed);

    // go back to the default params
    params = makeL1PHyperbolaParams();
    focuser.reset(MakeLinearFocuser(params));
    int currentPosition = focuser->initialPosition();

    // Here we run the algorithm, feeding it a v-curve, and watching it solve.
    // Should see the position reducing by initialStepSize
    position = focuser->newMeasurement(currentPosition, 5.01, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 4.0067, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2.981, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2.03, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 0.996, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 0.9, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    // Level off and then increase.

    position = focuser->newMeasurement(currentPosition, 1.1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2.03, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2.987, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 4.006, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    // Send in the last datapoint to complete the first pass. The focuser will move to the solution
    // found by the curve fitting process.
    int solution = 10000;
    position = focuser->newMeasurement(currentPosition, 5, 1);
    QCOMPARE(position, solution);

    // Check the latestHDR method
    QCOMPARE(focuser->latestValue(), 5.0);

    // Check the getMeasurements method
    QVector<int> pos;
    QVector<double> hfrs;
    QVector<double> weights;
    focuser->getMeasurements(&pos, &hfrs, &weights);
    QVERIFY(pos.size() == 11);
    QVERIFY(hfrs.size() == 11);
    QVERIFY(weights.size() == 11);
    QCOMPARE(pos.last(), currentPosition);
    QCOMPARE(hfrs.last(), 5.0);
    QCOMPARE(weights.last(), 1.0);
    currentPosition = position;

    // Check the getPass1Measurements method - for L1P this is the same as getMeasurement
    QVector<int> posPass1;
    QVector<double> hfrsPass1;
    QVector<double> weightsPass1;
    QVector<bool> outliersPass1;
    focuser->getPass1Measurements(&posPass1, &hfrsPass1, &weightsPass1, &outliersPass1);
    QCOMPARE(pos.last(), posPass1.last());
    QCOMPARE(hfrs.last(), hfrsPass1.last());
    QCOMPARE(weights.last(), weightsPass1.last());

    // Check the getTextStatus method
    double R2 = 0.8;
    QCOMPARE(focuser->getTextStatus(R2), "L1P [Green]: Hyperbola (W). Moving to Solution");

    // Now let it complete.
    position = focuser->newMeasurement(currentPosition, 0.91, 1);
    // Focuser returns a position of -1 to indicate its done.
    QCOMPARE(position, -1);
    // Check the isDone method
    QVERIFY(focuser->isDone());
    //Check the solution method
    QCOMPARE(focuser->solution(), currentPosition);
    // Check the doneReason method
    QCOMPARE(focuser->doneReason(), "Solution found.");
}

void TestFocus::L1PParabolaTest()
{
    // Linear 1 Pass parabola curve test.
    auto params = makeL1PParabolaParams();
    std::unique_ptr<FocusAlgorithmInterface> focuser(MakeLinearFocuser(params));
    int position = focuser->initialPosition();
    // The first position should be initialPosition + stepSize * initialOutwardSteps,
    // unless maxPositionAllowed or maxTravel doesn't allow that.
    QCOMPARE(position, static_cast <int> (params.startPosition + params.initialOutwardSteps * params.initialStepSize));

    params.maxTravel = 4 * params.initialStepSize;
    focuser.reset(MakeLinearFocuser(params));
    position = focuser->initialPosition();
    QCOMPARE(position, params.startPosition + params.maxTravel);

    params.maxPositionAllowed = params.startPosition + 3 * params.initialStepSize;
    focuser.reset(MakeLinearFocuser(params));
    position = focuser->initialPosition();
    QCOMPARE(position, params.maxPositionAllowed);

    // go back to the default params
    params = makeL1PParabolaParams();
    focuser.reset(MakeLinearFocuser(params));
    int currentPosition = focuser->initialPosition();

    // Here we run the algorithm, feeding it a v-curve, and watching it solve.
    // Should see the position reducing by initialStepSize
    position = focuser->newMeasurement(currentPosition, 5.01, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 4.0067, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2.981, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2.03, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 0.996, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 0.9, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    // Level off and then increase.

    position = focuser->newMeasurement(currentPosition, 1.1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2.03, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2.987, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 4.006, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    // Send in the last datapoint to complete the first pass. The focuser will move to the solution
    // found by the curve fitting process.
    int solution = 10000;
    position = focuser->newMeasurement(currentPosition, 5, 1);
    QCOMPARE(position, solution);

    // Check the latestHDR method
    QCOMPARE(focuser->latestValue(), 5.0);

    // Check the getMeasurements method
    QVector<int> pos;
    QVector<double> hfrs;
    QVector<double> weights;
    focuser->getMeasurements(&pos, &hfrs, &weights);
    QVERIFY(pos.size() == 11);
    QVERIFY(hfrs.size() == 11);
    QVERIFY(weights.size() == 11);
    QCOMPARE(pos.last(), currentPosition);
    QCOMPARE(hfrs.last(), 5.0);
    QCOMPARE(weights.last(), 1.0);
    currentPosition = position;

    // Check the getPass1Measurements method - for L1P this is the same as getMeasurement
    QVector<int> posPass1;
    QVector<double> hfrsPass1;
    QVector<double> weightsPass1;
    QVector<bool> outliersPass1;
    focuser->getPass1Measurements(&posPass1, &hfrsPass1, &weightsPass1, &outliersPass1);
    QCOMPARE(pos.last(), posPass1.last());
    QCOMPARE(hfrs.last(), hfrsPass1.last());
    QCOMPARE(weights.last(), weightsPass1.last());

    // Check the getTextStatus method
    double R2 = 0.8;
    QCOMPARE(focuser->getTextStatus(R2), "L1P [Blue]: Parabola (U). Moving to Solution");

    // Now let it complete.
    position = focuser->newMeasurement(currentPosition, 0.91, 1);
    // Focuser returns a position of -1 to indicate its done.
    QCOMPARE(position, -1);
    // Check the isDone method
    QVERIFY(focuser->isDone());
    //Check the solution method
    QCOMPARE(focuser->solution(), currentPosition);
    // Check the doneReason method
    QCOMPARE(focuser->doneReason(), "Solution found.");
}

void TestFocus::L1PQuadraticTest()
{
    // Linear 1 Pass quadratic curve test - should be the same as parabola
    auto params = makeL1PQuadraticParams();
    std::unique_ptr<FocusAlgorithmInterface> focuser(MakeLinearFocuser(params));
    int position = focuser->initialPosition();
    // The first position should be initialPosition + stepSize * initialOutwardSteps,
    // unless maxPositionAllowed or maxTravel doesn't allow that.
    QCOMPARE(position, static_cast <int> (params.startPosition + params.initialOutwardSteps * params.initialStepSize));

    params.maxTravel = 4 * params.initialStepSize;
    focuser.reset(MakeLinearFocuser(params));
    position = focuser->initialPosition();
    QCOMPARE(position, params.startPosition + params.maxTravel);

    params.maxPositionAllowed = params.startPosition + 3 * params.initialStepSize;
    focuser.reset(MakeLinearFocuser(params));
    position = focuser->initialPosition();
    QCOMPARE(position, params.maxPositionAllowed);

    // go back to the default params
    params = makeL1PQuadraticParams();
    focuser.reset(MakeLinearFocuser(params));
    int currentPosition = focuser->initialPosition();

    // Here we run the algorithm, feeding it a v-curve, and watching it solve.
    // Should see the position reducing by initialStepSize
    position = focuser->newMeasurement(currentPosition, 5, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 4, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 3, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 0.9, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    // Level off and then increase.

    position = focuser->newMeasurement(currentPosition, 1.1, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 2, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 3, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    position = focuser->newMeasurement(currentPosition, 4, 1);
    QCOMPARE(position, currentPosition - params.initialStepSize);
    currentPosition = position;

    // Send in the last datapoint to complete the first pass. The focuser will move to the solution
    // found by the curve fitting process.
    int solution = 10000;
    position = focuser->newMeasurement(currentPosition, 5, 1);
    QCOMPARE(position, solution);

    // Check the latestHDR method
    QCOMPARE(focuser->latestValue(), 5.0);

    // Check the getMeasurements method
    QVector<int> pos;
    QVector<double> hfrs;
    QVector<double> weights;
    focuser->getMeasurements(&pos, &hfrs, &weights);
    QVERIFY(pos.size() == 11);
    QVERIFY(hfrs.size() == 11);
    QVERIFY(weights.size() == 11);
    QCOMPARE(pos.last(), currentPosition);
    QCOMPARE(hfrs.last(), 5.0);
    QCOMPARE(weights.last(), 1.0);
    currentPosition = position;

    // Check the getPass1Measurements method - for L1P this is the same as getMeasurement
    QVector<int> posPass1;
    QVector<double> hfrsPass1;
    QVector<double> weightsPass1;
    QVector<bool> outliersPass1;
    focuser->getPass1Measurements(&posPass1, &hfrsPass1, &weightsPass1, &outliersPass1);
    QCOMPARE(pos.last(), posPass1.last());
    QCOMPARE(hfrs.last(), hfrsPass1.last());
    QCOMPARE(weights.last(), weightsPass1.last());

    // Check the getTextStatus method
    double R2 = 0.8;
    QCOMPARE(focuser->getTextStatus(R2), "L1P [Lum]: Quadratic. Moving to Solution");

    // Now let it complete.
    position = focuser->newMeasurement(currentPosition, 0.91, 1);
    // Focuser returns a position of -1 to indicate its done.
    QCOMPARE(position, -1);
    // Check the isDone method
    QVERIFY(focuser->isDone());
    //Check the solution method
    QCOMPARE(focuser->solution(), currentPosition);
    // Check the doneReason method
    QCOMPARE(focuser->doneReason(), "Solution found.");
}

QTEST_GUILESS_MAIN(TestFocus)

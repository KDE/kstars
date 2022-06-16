/*
    SPDX-FileCopyrightText: 2019 Hy Murveit <hy-1@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QList>
#include "focus.h"
#include "curvefit.h"

class Edge;

namespace Ekos
{

/**
 * @class FocusAlgorithmInterface
 * @short Interface intender for autofocus algorithms.
 *
 * @author Hy Murveit
 * @version 1.0
 */
class FocusAlgorithmInterface
{
    public:
        // Invalid HFR result - this value (-1) is compatible with graph renderings
        static double constexpr IGNORED_HFR = -1;

    public:
        struct FocusParams
        {
            // Maximum movement from current position allowed for the algorithm.
            int maxTravel;
            // Initial sampling interval for the algorithm.
            int initialStepSize;
            // Absolute position of the focuser when the algorithm starts.
            int startPosition;
            // Minimum position the focuser is allowed to reach.
            int minPositionAllowed;
            // Maximum position the focuser is allowed to reach.
            int maxPositionAllowed;
            // Maximum number of iterations (captures) the focuser may try.
            int maxIterations;
            // The focus algorithm may terminate if it gets within this fraction of the best focus, e.g. 0.10.
            double focusTolerance;
            // The name of the filter used, if any.
            QString filterName;
            // The temperature measured when starting the focus (from focuser or observatory).
            double temperature;
            // The number of outward steps taken at the start of the algorithm.
            double initialOutwardSteps;
            // The focus algo
            Focus::FocusAlgorithm focusAlgorithm;
            // The user defined focuser backlash value
            // The value does not need to be exact but needs to be >= focuser backlash
            int backlash;
            // Curve fit is the type of curve to fit to the data
            CurveFitting::CurveFit curveFit;
            // Whether we want to use weightings of datapoints in the curve fitting process
            bool useWeights;

            FocusParams(int _maxTravel, int _initialStepSize, int _startPosition,
                        int _minPositionAllowed, int _maxPositionAllowed,
                        int _maxIterations, double _focusTolerance, const QString &filterName_,
                        double _temperature, double _initialOutwardSteps, Focus::FocusAlgorithm _focusAlgorithm,
                        int _backlash, CurveFitting::CurveFit _curveFit, bool _useWeights) :
                maxTravel(_maxTravel), initialStepSize(_initialStepSize),
                startPosition(_startPosition), minPositionAllowed(_minPositionAllowed),
                maxPositionAllowed(_maxPositionAllowed), maxIterations(_maxIterations),
                focusTolerance(_focusTolerance), filterName(filterName_),
                temperature(_temperature), initialOutwardSteps(_initialOutwardSteps),
                focusAlgorithm(_focusAlgorithm), backlash(_backlash), curveFit(_curveFit),
                useWeights(_useWeights) {}
        };

        // Constructor initializes an autofocus algorithm from the input params.
        FocusAlgorithmInterface(const FocusParams &_params) : params(_params) {}
        virtual ~FocusAlgorithmInterface() {}

        // After construction, this should be called to get the initial position desired by the
        // focus algorithm. It returns the start position passed to the constructor if
        // it has no movement request.
        virtual int initialPosition() = 0;

        // Pass in the recent measurement. Returns the position for the next measurement,
        // or -1 if the algorithms done or if there's an error.
        // If stars is not nullptr, then the they may be used to modify the HFR value.
        virtual int newMeasurement(int position, double value, const QList<Edge*> *stars = nullptr) = 0;

        // Returns true if the algorithm has terminated either successfully or in error.
        bool isDone() const
        {
            return done;
        }

        // Returns the best position. Should be called after isDone() returns true.
        // Returns -1 if there's an error.
        int solution() const
        {
            return focusSolution;
        }

        // Returns human-readable extra error information about why the algorithm is done.
        QString doneReason() const
        {
            return doneString;
        }

        // Returns the params used to construct this object.
        const FocusParams &getParams() const
        {
            return params;
        }

        virtual double latestHFR() const = 0;

        virtual void getMeasurements(QVector<int> *positions, QVector<double> *hfrs,
                                     QVector<double> *sigmas) const = 0;
        virtual void getPass1Measurements(QVector<int> *positions, QVector<double> *hfrs,
                                          QVector<double> *sigmas) const = 0;

        virtual QString getTextStatus(double R2 = 0) const = 0;

        // Curve fitting object
        CurveFitting curveFit;

        // For testing.
        virtual FocusAlgorithmInterface *Copy() = 0;

    protected:
        FocusParams params;
        bool done = false;
        int focusSolution = -1;
        double focusHFR = -1;
        QString doneString;
};

// Creates a LinearFocuser. Caller responsible for the memory.
FocusAlgorithmInterface *MakeLinearFocuser(const FocusAlgorithmInterface::FocusParams &params);
}


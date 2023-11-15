/*
    SPDX-FileCopyrightText: 2019 Hy Murveit <hy-1@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QList>
#include "focus.h"
#include "curvefit.h"
#include "../../auxiliary/robuststatistics.h"
#include "../../auxiliary/gslhelpers.h"
#include <gsl/gsl_sf_erf.h>

class Edge;

namespace Ekos
{

/**
 * @class FocusAlgorithmInterface
 * @short Interface intender for autofocus algorithms.
 *
 * @author Hy Murveit
 * @version 1.1
 */
class FocusAlgorithmInterface
{
    public:
        struct FocusParams
        {
            // Curve Fitting object ptr. Create object if nullptr passed in
            CurveFitting *curveFitting;
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
            // The number of steps
            int numSteps;
            // The focus algo
            Focus::Algorithm focusAlgorithm;
            // The user defined focuser backlash value
            // The value does not need to be exact but needs to be >= focuser backlash
            int backlash;
            // Curve fit is the type of curve to fit to the data
            CurveFitting::CurveFit curveFit;
            // Whether we want to use weightings of datapoints in the curve fitting process
            bool useWeights;
            // Which star measure to use, e.g. HFR, FWHM, etc.
            Focus::StarMeasure starMeasure;
            // If using FWHM, which PSF to use.
            Focus::StarPSF starPSF;
            // After pass1 re-evaluate the curve fit to remove outliers
            bool refineCurveFit;
            // The type of focus walk
            Focus::FocusWalk focusWalk;
            // Whether we want to minimise or maximise the focus measurement statistic
            CurveFitting::OptimisationDirection optimisationDirection;
            // How to assign weights to focus measurements
            Mathematics::RobustStatistics::ScaleCalculation scaleCalculation;

            FocusParams(CurveFitting *_curveFitting, int _maxTravel, int _initialStepSize, int _startPosition,
                        int _minPositionAllowed, int _maxPositionAllowed, int _maxIterations,
                        double _focusTolerance, const QString &filterName_, double _temperature,
                        double _initialOutwardSteps, int _numSteps, Focus::Algorithm _focusAlgorithm, int _backlash,
                        CurveFitting::CurveFit _curveFit, bool _useWeights, Focus::StarMeasure _starMeasure,
                        Focus::StarPSF _starPSF, bool _refineCurveFit, Focus::FocusWalk _focusWalk,
                        CurveFitting::OptimisationDirection _optimisationDirection,
                        Mathematics::RobustStatistics::ScaleCalculation _scaleCalculation) :
                curveFitting(_curveFitting), maxTravel(_maxTravel), initialStepSize(_initialStepSize),
                startPosition(_startPosition), minPositionAllowed(_minPositionAllowed),
                maxPositionAllowed(_maxPositionAllowed), maxIterations(_maxIterations),
                focusTolerance(_focusTolerance), filterName(filterName_),
                temperature(_temperature), initialOutwardSteps(_initialOutwardSteps), numSteps(_numSteps),
                focusAlgorithm(_focusAlgorithm), backlash(_backlash), curveFit(_curveFit),
                useWeights(_useWeights), starMeasure(_starMeasure), starPSF(_starPSF),
                refineCurveFit(_refineCurveFit), focusWalk(_focusWalk),
                optimisationDirection(_optimisationDirection), scaleCalculation(_scaleCalculation) {}
        };

        // Constructor initializes an autofocus algorithm from the input params.
        FocusAlgorithmInterface(const FocusParams &_params) : params(_params)
        {
            // Either a curve fitting object ptr is passed in or the constructor will create its own
            if (params.curveFitting == nullptr)
                params.curveFitting = new CurveFitting();
        }
        virtual ~FocusAlgorithmInterface() {}

        // After construction, this should be called to get the initial position desired by the
        // focus algorithm. It returns the start position passed to the constructor if
        // it has no movement request.
        virtual int initialPosition() = 0;

        // Pass in the recent measurement. Returns the position for the next measurement,
        // or -1 if the algorithms done or if there's an error.
        // If stars is not nullptr, then the they may be used to modify the HFR value.
        virtual int newMeasurement(int position, double value, const double starWeight, const QList<Edge*> *stars = nullptr) = 0;

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

        // Returns the value for best solution. Should be called after isDone() returns true.
        // Returns -1 if there's an error.
        double solutionValue() const
        {
            return focusValue;
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

        virtual double latestValue() const = 0;

        virtual void getMeasurements(QVector<int> *positions, QVector<double> *values, QVector<double> *scale) const = 0;
        virtual void getPass1Measurements(QVector<int> *positions, QVector<double> *values, QVector<double> *scale,
                                          QVector<bool> *out) const = 0;

        virtual QString getTextStatus(double R2 = 0) const = 0;

        // For Linear and L1P returns whether focuser is inFirstPass, other algos return true
        virtual bool isInFirstPass() const = 0;

        // For testing.
        virtual FocusAlgorithmInterface *Copy() = 0;

    protected:
        FocusParams params;
        bool done = false;
        int focusSolution = -1;
        double focusValue = -1;
        QString doneString;
};

// Creates a LinearFocuser. Caller responsible for the memory.
FocusAlgorithmInterface *MakeLinearFocuser(const FocusAlgorithmInterface::FocusParams &params);
}


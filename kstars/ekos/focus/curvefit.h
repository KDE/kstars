#pragma once

#include <QVector>
#include <qcustomplot.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_min.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_multifit_nlinear.h>
#include <gsl/gsl_blas.h>

namespace Ekos
{
// The curve fitting class provides curve fitting algorithms using the Lehvensberg-Marquart (LM)
// solver as provided the the Gnu Science Library (GSL). See the comments at the start of curvefit.cpp
// for more details.
//
// 2 curves, hyperbola and parabola are provided.
// For compatibility with existing Ekos functionality a Quadratic option using the exising Ekos linear
// least squares solver (again provided by GSL) is supported. The Quadratic and Parabola curves are
// the same thing mathematically but Parabola uses the non-linear least squares LM solver whilst Quadratic
// uses the original Ekos linear least squares solver.
//
// Users of CurveFitting operate on focuser position and HFR. Within CurveFitting the curve uses the more
// usual mathematical notation of x, y
//
// Furture releases may migrate all curve fitting to the LM solver.
class CurveFitting
{
    public:
        typedef enum { FOCUS_QUADRATIC, FOCUS_HYPERBOLA, FOCUS_PARABOLA } CurveFit;

        // Data structures to hold datapoints for the class
        struct DataPT
        {
            double x;            // focuser position
            double y;            // star HFR
        };

        struct DataPointT        // This is the data strcture passed into GSL LM routines
        {
            bool useWeights;     // Are we fitting the curve with or without weights (using sigma or not)
            QVector<DataPT> dps; // Vector of DataPT

            // Helper function to operate on the data structure
            void push_back(double x, double y)
            {
                dps.push_back({x, y});
            }
        };

        // Constructor just initialises the object
        CurveFitting();
        // fitCurve takes in the vectors with the position, hfr and sigma (standard deviation in HFR) values
        // along with the type of curve to use and whether or not to sigmas in the calculation
        // It fits the curve and solves for the coefficients.
        void fitCurve(const QVector<int> &position, const QVector<double> &hfr, const QVector<double> &sigma,
                      const CurveFit curveFit,
                      const bool useWeights);

        // Returns the minimum position and value in the pointers for the solved curve.
        // Returns false if the curve couldn't be solved.
        bool findMin(double expected, double minPosition, double maxPosition, double *position, double *value, CurveFit curveFit);
        // f calculates the value of y (hfr) for a given x (position) using the appropriate curve algorithm
        double f(double x);
        // Calculates the R-squared which is a measure of how well the curve fits the datapoints
        double calculateR2(CurveFit curveFit);

    private:
        // TODO: This function will likely go when Linear and L1P merge to be closer.
        // Calculates the value of the polynomial at x. Params will be cast to a CurveFit*.
        static double curveFunction(double x, void *params);

        // TODO: This function will likely go when Linear and L1P merge to be closer.
        QVector<double> polynomial_fit(const double *const data_x, const double *const data_y, const int n, const int order);
        QVector<double> hyperbola_fit(const QVector<double> data_x, const QVector<double> data_y, const QVector<double> data_sigma,
                                      bool useWeights);
        QVector<double> parabola_fit(const QVector<double> data_x, const QVector<double> data_y, const QVector<double> data_sigma,
                                     bool useWeights);

        bool minimumQuadratic(double expected, double minPosition, double maxPosition, double *position, double *value);
        bool minimumHyperbola(double expected, double minPosition, double maxPosition, double *position, double *value);
        bool minimumParabola(double expected, double minPosition, double maxPosition, double *position, double *value);

        void hypMakeGuess(const QVector<double> inX, const QVector<double> inY, gsl_vector * guess);
        void parMakeGuess(const QVector<double> inX, const QVector<double> inY, gsl_vector * guess);

        // Calculation engine for the R-squared which is a measure of how well the curve fits the datapoints
        double calcR2(QVector<double> dataPoints, QVector<double> curvePoints);

        // Type of curve
        CurveFit m_CurveType;
        // The data values.
        QVector<double> m_x, m_y, m_sigma;
        // The solved parameters.
        QVector<double> m_coefficients;
        // State variables used by the LM solver. These variables provide a way of optimising the starting
        // point for the solver by using the solution found by the previous run providing the relevant
        // solver parameters are consistent between runs.
        bool firstSolverRun;
        CurveFit lastCurveType;
        QVector<double> lastCoefficients;
};

} //namespace

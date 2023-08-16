#pragma once

#include "../../auxiliary/robuststatistics.h"

#include <QVector>
#include <qcustomplot.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_min.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_multifit_nlinear.h>
#include <gsl/gsl_blas.h>
#include <ekos_focus_debug.h>

namespace Ekos
{
// The curve fitting class provides curve fitting algorithms using the Lehvensberg-Marquart (LM)
// solver as provided the Gnu Science Library (GSL). See the comments at the start of curvefit.cpp
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
        typedef enum { FOCUS_QUADRATIC, FOCUS_HYPERBOLA, FOCUS_PARABOLA, FOCUS_GAUSSIAN } CurveFit;
        typedef enum { OPTIMISATION_MINIMISE, OPTIMISATION_MAXIMISE } OptimisationDirection;
        typedef enum { STANDARD, BEST, BEST_RETRY } FittingGoal;

        // Data structures to hold datapoints for the class
        struct DataPT
        {
            double x;            // focuser position
            double y;            // focus measurement, e.g. HFR
            double weight;       // the measurement weight, e.g. inverse variance
        };

        struct DataPointT        // This is the data strcture passed into GSL LM routines
        {
            bool useWeights;     // Are we fitting the curve with or without weights
            QVector<DataPT> dps; // Vector of DataPT
            OptimisationDirection dir = OPTIMISATION_MINIMISE; // Are we minimising or maximising?
            Mathematics::RobustStatistics::ScaleCalculation weightCalculation =
                Mathematics::RobustStatistics::SCALE_VARIANCE; // How to compute weights

            // Helper functions to operate on the data structure
            void push_back(double x, double y, double weight = 1)
            {
                dps.push_back({x, y, weight});
            }
        };

        struct DataPT3D
        {
            double x;            // x position
            double y;            // y position
            int z;               // value
            double weight;       // the measurement weight, e.g. inverse variance
        };

        struct DataPoint3DT      // This is the data strcture passed into GSL LM routines
        {
            bool useWeights;     // Are we fitting the curve with or without weights
            QVector<DataPT3D> dps; // Vector of DataPT3D
            OptimisationDirection dir = OPTIMISATION_MAXIMISE; // Are we minimising or maximising?
            Mathematics::RobustStatistics::ScaleCalculation weightCalculation =
                Mathematics::RobustStatistics::SCALE_VARIANCE; // How to compute weights

            // Helper function to operate on the data structure
            void push_back(double x, double y, int z, double weight = 1)
            {
                dps.push_back({x, y, z, weight});
            }
        };

        // Data structure to hold star parameters
        struct StarParams
        {
            double background;
            double peak;
            double centroid_x;
            double centroid_y;
            double HFR;
            double theta;
            double FWHMx;
            double FWHMy;
            double FWHM;
        };

        // Constructor just initialises the object
        CurveFitting();

        // A constructor that reconstructs a previously built object.
        // Does not implement getting the original data points.
        CurveFitting(const QString &serialized);

        // fitCurve takes in the vectors with the position, hfr and weight (e.g. variance in HFR) values
        // along with the type of curve to use and whether or not to use weights in the calculation
        // It fits the curve and solves for the coefficients.
        void fitCurve(const FittingGoal goal, const QVector<int> &position, const QVector<double> &hfr,
                      const QVector<double> &weights, const QVector<bool> &outliers,
                      const CurveFit curveFit, const bool useWeights, const OptimisationDirection optDir);

        // fitCurve3D 3-Dimensional version of fitCurve.
        // Data is passed in in imageBuffer - a 2D array of width x height
        // Approx star information is passed in to seed the LM solver initial parameters.
        // Start and end define the x,y coordinates of a box around the star start is top left corner, end is bottom right
        template <typename T>
        void fitCurve3D(const T *imageBuffer, const int imageWidth, const QPair<int, int> start, const QPair<int, int> end,
                        const StarParams &starParams, const CurveFit curveFit, const bool useWeights)
        {
            if (imageBuffer == nullptr)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("CurveFitting::fitCurve3D null image ptr");
                m_FirstSolverRun = true;
                return;
            }

            if (imageWidth <= 0)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("CurveFitting::fitCurve3D imageWidth=%1").arg(imageWidth);
                m_FirstSolverRun = true;
                return;
            }

            if (useWeights)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("CurveFitting::fitCurve3D called with useWeights. Not yet implemented");
                m_FirstSolverRun = true;
                return;
            }

            m_dataPoints.dps.clear();
            m_dataPoints.useWeights = useWeights;

            // Load up the data structures for the solver.
            // The pixel reference x, y refers to the top left corner of the pizel so add 0.5 to x and y to reference the
            // centre of the pixel.
            int width = end.first - start.first;
            int height = end.second - start.second;

            for (int j = 0; j < height; j++)
                for (int i = 0; i < width; i++)
                    m_dataPoints.push_back(i + 0.5, j + 0.5, imageBuffer[start.first + i + ((start.second + j) * imageWidth)], 1.0);

            m_CurveType = curveFit;
            switch (m_CurveType)
            {
                case FOCUS_GAUSSIAN :
                    m_coefficients = gaussian_fit(m_dataPoints, starParams);
                    break;
                default :
                    // Something went wrong, log an error and reset state so solver starts from scratch if called again
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("CurveFitting::fitCurve3D called with curveFit=%1").arg(curveFit);
                    m_FirstSolverRun = true;
                    return;
            }
            m_LastCoefficients = m_coefficients;
            m_LastCurveType    = m_CurveType;
            m_FirstSolverRun   = false;
        }

        // Returns the minimum position and value in the pointers for the solved curve.
        // Returns false if the curve couldn't be solved
        bool findMinMax(double expected, double minPosition, double maxPosition, double *position, double *value, CurveFit curveFit,
                        const OptimisationDirection optDir);
        // getStarParams returns the star parameters for the solved star
        bool getStarParams(const CurveFit curveFit, StarParams *starParams);

        // getCurveParams gets the coefficients of a curve solve
        // setCurveParams sets the coefficients of a curve solve
        // using get and set returns the solver to its state as it was when get was called
        // This allows functions like "f" to be called to calculate curve values for a
        // prior curve fit solution.
        bool getCurveParams(const CurveFit curveType, QVector<double> &coefficients)
        {
            if (curveType != m_CurveType)
                return false;
            coefficients = m_coefficients;
            return true;
        }

        bool setCurveParams(const CurveFit curveType, const QVector<double> coefficients)
        {
            if (curveType != m_CurveType)
                return false;
            m_coefficients = coefficients;
            return true;
        }

        // f calculates the value of y for a given x using the appropriate curve algorithm
        double f(double x);
        // f calculates the value of z for a given x and y using the appropriate curve algorithm
        double f3D(double x, double y);
        // Calculates the R-squared which is a measure of how well the curve fits the datapoints
        double calculateR2(CurveFit curveFit);
        // Calculate the deltas of each datapoint from the curve
        void calculateCurveDeltas(CurveFit curveFit, std::vector<std::pair<int, double>> &curveDeltas);

        // Returns a QString which can be used to construct an identical object.
        // Does not implement getting the original data points.
        QString serialize() const;

    private:
        // TODO: This function will likely go when Linear and L1P merge to be closer.
        // Calculates the value of the polynomial at x. Params will be cast to a CurveFit*.
        static double curveFunction(double x, void *params);

        // TODO: This function will likely go when Linear and L1P merge to be closer.
        QVector<double> polynomial_fit(const double *const data_x, const double *const data_y, const int n, const int order);

        QVector<double> hyperbola_fit(FittingGoal goal, const QVector<double> data_x, const QVector<double> data_y,
                                      const QVector<double> weights, bool useWeights, const OptimisationDirection optDir);
        QVector<double> parabola_fit(FittingGoal goal, const QVector<double> data_x, const QVector<double> data_y,
                                     const QVector<double> data_weights,
                                     bool useWeights, const OptimisationDirection optDir);
        QVector<double> gaussian_fit(DataPoint3DT data, const StarParams &starParams);

        bool minimumQuadratic(double expected, double minPosition, double maxPosition, double *position, double *value);
        bool minMaxHyperbola(double expected, double minPosition, double maxPosition, double *position, double *value,
                             const OptimisationDirection optDir);
        bool minMaxParabola(double expected, double minPosition, double maxPosition, double *position, double *value,
                            const OptimisationDirection optDir);
        bool getGaussianParams(StarParams *starParams);

        void hypMakeGuess(const int attempt, const QVector<double> inX, const QVector<double> inY,
                          const OptimisationDirection optDir, gsl_vector * guess);
        void hypSetupParams(FittingGoal goal, gsl_multifit_nlinear_parameters *params, int *numIters, double *xtol, double *gtol,
                            double *ftol);

        void parMakeGuess(const int attempt, const QVector<double> inX, const QVector<double> inY,
                          const OptimisationDirection optDir,
                          gsl_vector * guess);
        void parSetupParams(FittingGoal goal, gsl_multifit_nlinear_parameters *params, int *numIters, double *xtol, double *gtol,
                            double *ftol);
        void gauMakeGuess(const int attempt, const StarParams &starParams, gsl_vector * guess);
        void gauSetupParams(gsl_multifit_nlinear_parameters *params, int *numIters, double *xtol, double *gtol, double *ftol);

        // Get the reason code from the passed in info
        QString getLMReasonCode(int info);

        // Calculation engine for the R-squared which is a measure of how well the curve fits the datapoints
        double calcR2(const QVector<double> dataPoints, const QVector<double> curvePoints, const QVector<double> scale,
                      const bool useWeights);

        // Used in the QString constructor.
        bool recreateFromQString(const QString &serialized);

        // Type of curve
        CurveFit m_CurveType;
        // The data values.
        QVector<double> m_x, m_y, m_scale;
        // Use weights or not
        bool m_useWeights;
        DataPoint3DT m_dataPoints;
        // The solved parameters.
        QVector<double> m_coefficients;
        // State variables used by the LM solver. These variables provide a way of optimising the starting
        // point for the solver by using the solution found by the previous run providing the relevant
        // solver parameters are consistent between runs.
        bool m_FirstSolverRun;
        CurveFit m_LastCurveType;
        QVector<double> m_LastCoefficients;
};

} //namespace

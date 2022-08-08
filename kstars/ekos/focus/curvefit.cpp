#include "curvefit.h"
#include "ekos/ekos.h"
#include <ekos_focus_debug.h>

// Constants used to identify the number of parameters used for different curve types
constexpr int NUM_HYPERBOLA_PARAMS = 4;
constexpr int NUM_PARABOLA_PARAMS = 3;
// Parameters for the solver
// MAX_ITERATIONS is used to limit the number of iterations for the solver.
// A value needs to be entered that allows a solution to be found without iterating unnecessarily
// There is a relationship with the tolerance parameters that follow.
constexpr int MAX_ITERATIONS = 500;
// The next 2 parameters are used as tolerance for convergence
// convergence is achieved if for each datapoint i
//     dx_i < INEPSABS + (INEPSREL * x_i)
constexpr double INEPSABS = 1e-5;
const double INEPSREL = pow(GSL_DBL_EPSILON, 1.0 / 3.0);

// The functions here fit a number of different curves to the incoming data points using the Lehvensberg-Marquart
// solver with geodesic acceleration as provided the the Gnu Science Library (GSL). The following sources of information are useful:
// www.wikipedia.org/wiki/Levenberg–Marquardt_algorithm - overview of the mathematics behind the algo
// www.gnu.org/software/gsl/doc/html/nls.html - GSL manual describing Nonlinear Least-Squares fitting.
//
// Levensberg-Marquart (LM)
// ------------------------
// The Levensberg-Marquart algorithm is a non-linear least-squares solver and thus suitable for mnay
// different equations. The basic is idea is to adjust the equation y = f(x,P) so that the computed
// y values are as close as possible to the y values of the datapoints provided, so that the resultant
// curve fits the data as best as it can. P is a set of parameters that are varied by the solver in order
// to find the best fit. The solver measures how far away the curve is at each data point, squares the
// result and adds them all up. This is the number to be minimised, lets call is S.
//
// The solver is supplied with an initial guess for the parameters, P. It calculates S, makes an adjustment
// P and calculates a new S1. Provided S1 < S the solver has moved in the right direction and reduced S.
// It iterates through this procedure until S1 - S is:
// 1. less than a supplied limit (convergence has been reached), or
// 2. The maximum number of iterations has been reached, or
// 3. The solver encountered an error.
//
// The solver is capable of solving either an unweighted or weighted set of datapoints. In essence, an
// unweighted set of data gives equal weight to each datapoint when trying to fit a curve. An alternative is to
// weight each datapoint with a measure that corresponds to how accurate the measurement of the datapoint
// actually is. Given we are calculating HFRs of stars we can calculate the variance (standard deviation
// squared) in the measurement of HFR and use 1 / SD^2 as weighting. What this means is that if we have a
// datapoint where the SD in HFR measurements is small we would give this datapoint a relatively high
// weighting when fitting the curve, and if there was a datapoint with a higher SD in HFR measurements
// it would receive a lower weighting when trying to fit curve to that point.
//
// There are several optimisations to LM that speed up convergence for certain types of equations. This
// code uses the LM solver with geodesic acceleration; a technique to accelerate convergence by using the
// second derivative of the fitted function. An optimisation that has been implemented is that since, in normal
// operation, the solver is run multiple times with the same or similar parameters, the initial guess for
// a solver run is set the solution from the previous run.
//
// The documents referenced provide the maths of the LM algorithm. What is required is the function to be
// used f(x) and the derivative of f(x) with respect to the parameters of f(x). This partial derivative forms
// a matrix called the Jacobian, J(x). LM then finds a minimum. Mathematically it will find a minimum local
// to the initial guess, but not necessarily the global minimum but for the equations used this is not a
// problem under normal operation as there will only be 1 minimum. If the data is poor, however, the
// solver may find an "n" solution rather than a "u" especially at the start of the process where there
// are a small number of points and the error in measurement is high. We'll ignore and continue and this
// should correct itself.
//
// The original algorithm used in Ekos is a quadratic equation, which represents a parabolic curve. In order
// not to disturb historic code the original solution will be called Quadratic. It uses a linear least-squares
// model, not LM. The LM solver has been applied to a hyperbolic and a parabolic curve.
//
// Hyperbola
// ---------
// Equation y = f(x) = b * sqrt(1 + ((x - c) / a) ^2) + d
// This can be re-written as:
// f(x) = b * phi + d, where phi = sqrt(1 + ((x - c) / a) ^2)
// Jacobian J = {df/da, df/db, df/dc, df/db}
// df/da      = b * (1 / 2) * (1 / phi) * -2 * ((x - c) ^2) / (a ^3)
//            = -b * ((x - c) ^2) / ((a ^ 3) * phi)
// df/db      = phi
// df/dc      = b * (1 / 2) * (1 / phi) * 2 * (x - c) / (a ^2) * (-1)
//            = -b * (x - c) / ((a ^2) * phi)
// df/dd      = 1
//
// For a valid solution:
// 1. c must be > 0 and within the range of travel of the focuser.
// 2. b must be > 0 for a V shaped curve (b < 0 for an inverted V and b = 0 for a horizontal line).
// 3. b + d > 0. The minimum solution when x = c gives f(x) = b + d which must be > 0.
//
// Parabola
// --------
// Equation y = f(x) = a + b((x - c) ^2)
// Jacobian J = {df/da, df/db, df/dc}
// df/da      = 1
// df/db      = (x - c) ^2
// df/dc      = -2 * b * (x - c)
//
// For a valid solution:
// 1. c must be > 0 and within the range of travel of the focuser.
// 2. b must be > 0 for a V shaped curve (b < 0 for an inverted V and b = 0 for a horizontal line).
// 3. a > 0. The minimum solution when x = c gives f(x) = a which must be > 0.
//
// Convergence
// -----------
// There following are key parameters that drive the LM solver. Currently these settings are coded into
// the program. It would be better to store them in a configuration file somewhere, but they aren't the
// sort of parameters users should have ready access to.
// MAX_ITERATIONS - This is how many iterations to do before aborting. The game here is to set a
//                  reasonable number to allow for convergence. In my testing with good data and a
//                  good start point <10 iterations are required. With good data and a poor start point
//                  <200 iterations are required. With poor data and a tight "tolerance" convergence
//                  may need >1000 iterations. Setting to 500 seems a good compromise.
// tolerance      - Conceptually tolerance could be done in 2 ways:
//                  - check the gradient, would be 0 at the curve minimum
//                  - check the residuals (and their gradient), will minimise at the curve minimum
//                  Currently we will check on residuals.
//                  This is supported by 2 parameters INEPSABS and INEPSREL.
//                      convergence is achieved if for each datapoint i, where:
//                        dx_i < INEPSABS + (INEPSREL * x_i)
//                  Setting a slack tolerance will mean a range of x (focus positions) that are deemed
//                  valid solutions. Not good.
//                  Setting a tighter tolerance will require more iterations to solve, but setting too
//                  tight a tolerance is just wasting time if it doesn't improve the focus position, and
//                  if too tight the solver may not be able to find the solution, so a balance needs to
//                  be struck. For now lets just make these values constant and see where this gets to.
//                  If this turns out not to work for some equipment, the next step would be to adjust
//                  these parameters based on the equipment profile, or to adapt the parameters starting
//                  with a loose tolerance and tightening as the curve gets nearer to a complete solution.
//                  If we inadvertently overtighten the tolerance and fail to converge, the tolerance
//                  could be slackened or more iterations used.
//                  I have found that the following work well on my equipment and the simulator
//                  for both hyperbola and parabola. The advice in the GSL documentation is to start
//                  with an absolute tolerance of 10^-d where d is the number of digits required in the
//                  solution precision of x (focuser position). The gradient tolerance starting point is
//                  (machine precision)^1/3 which we're using a relative tolerance.
//                      INEPSABS = 1e-5
//                      INEPSREL = GSL_DBL_EPSILON ^ 1/3

namespace Ekos
{

namespace
{
// Constants used to index m_coefficient arrays
enum { A_IDX = 0, B_IDX, C_IDX, D_IDX };

// hypPhi() is a repeating part of the function calculations for Hyperbolas.
inline double hypPhi(double x, double a, double c)
{
    return sqrt(1.0 + pow(((x - c) / a), 2.0));
}

// Function to calculate f(x) for a hyperbola
// y = b * hypPhi(x, a, c) + d
double hypfx(double x, double a, double b, double c, double d)
{
    return b * hypPhi(x, a, c) + d;
}

// Calculates F(x) for each data point on the hyperbola
int hypFx(const gsl_vector * X, void * inParams, gsl_vector * outResultVec)
{
    CurveFitting::DataPointT * DataPoints = ((CurveFitting::DataPointT *)inParams);

    double a = gsl_vector_get (X, A_IDX);
    double b = gsl_vector_get (X, B_IDX);
    double c = gsl_vector_get (X, C_IDX);
    double d = gsl_vector_get (X, D_IDX);

    for(int i = 0; i < DataPoints->dps.size(); ++i)
    {
        // Hyperbola equation
        double yi = hypfx(DataPoints->dps[i].x, a, b, c, d);

        // TODO: Need to understand this a bit more
        gsl_vector_set(outResultVec, i, (yi - DataPoints->dps[i].y));
    }

    return GSL_SUCCESS;
}

// Calculates the Jacobian (derivative) matrix for the hyperbola
int hypJx(const gsl_vector * X, void * inParams, gsl_matrix * J)
{
    CurveFitting::DataPointT * DataPoints = ((struct CurveFitting::DataPointT *)inParams);

    // Store current coefficients
    double a = gsl_vector_get(X, A_IDX);
    double b = gsl_vector_get(X, B_IDX);
    double c = gsl_vector_get(X, C_IDX);

    // Store non-changing calculations
    const double a2 = a * a;
    const double a3 = a * a2;

    for(int i = 0; i < DataPoints->dps.size(); ++i)
    {
        // Calculate the Jacobian Matrix
        const double x = DataPoints->dps[i].x;
        const double x_minus_c = x - c;

        gsl_matrix_set(J, i, A_IDX, -b * (x_minus_c * x_minus_c) / (a3 * hypPhi(x, a, c)));
        gsl_matrix_set(J, i, B_IDX, hypPhi(x, a, c));
        gsl_matrix_set(J, i, C_IDX, -b * x_minus_c / (a2 * hypPhi(x, a, c)));
        gsl_matrix_set(J, i, D_IDX, 1);
    }

    return GSL_SUCCESS;
}

// Function to calculate f(x) for a parabola.
double parfx(double x, double a, double b, double c)
{
    return a + b * pow((x - c), 2.0);
}

// Calculates f(x) for each data point in the parabola.
int parFx(const gsl_vector * X, void * inParams, gsl_vector * outResultVec)
{
    CurveFitting::DataPointT * DataPoint = ((struct CurveFitting::DataPointT *)inParams);

    double a = gsl_vector_get (X, A_IDX);
    double b = gsl_vector_get (X, B_IDX);
    double c = gsl_vector_get (X, C_IDX);

    for(int i = 0; i < DataPoint->dps.size(); ++i)
    {
        // Parabola equation
        double yi = parfx(DataPoint->dps[i].x, a, b, c);

        // TODO: Need to understand this a bit more
        gsl_vector_set(outResultVec, i, (yi - DataPoint->dps[i].y));
    }

    return GSL_SUCCESS;
}

// Calculates the Jacobian (derivative) matrix for the parabola equation f(x) = a + b*(x-c)^2
// dy/da = 1
// dy/db = (x - c)^2
// dy/dc = -2b * (x - c)
int parJx(const gsl_vector * X, void * inParams, gsl_matrix * J)
{
    CurveFitting::DataPointT * DataPoint = ((struct CurveFitting::DataPointT *)inParams);

    // Store current coefficients
    double b = gsl_vector_get(X, B_IDX);
    double c = gsl_vector_get(X, C_IDX);

    for(int i = 0; i < DataPoint->dps.size(); ++i)
    {
        // Calculate the Jacobian Matrix
        const double x = DataPoint->dps[i].x;

        gsl_matrix_set(J, i, 0, 1);
        gsl_matrix_set(J, i, 1, (x - c) * (x - c));
        gsl_matrix_set(J, i, 2, -b * (x - c));
    }

    return GSL_SUCCESS;
}

}  // namespace

CurveFitting::CurveFitting()
{
    // Constructor just initialises variables
    firstSolverRun = true;
}

void CurveFitting::fitCurve(const QVector<int> &x_, const QVector<double> &y_, const QVector<double> &sigma_,
                            const CurveFit curveFit, const bool useWeights)
{
    if ((x_.size() != y_.size()) || (x_.size() != sigma_.size()))
        qCDebug(KSTARS_EKOS_FOCUS) << QString("CurveFitting::CurveFitting inconsistent parameters. x=%1, y=%2, sigma=%3")
                                   .arg(x_.size()).arg(y_.size()).arg(sigma_.size());

    m_x.clear();
    for (int i = 0; i < x_.size(); ++i)
        m_x.push_back(static_cast<double>(x_[i]));
    m_y = y_;
    m_sigma = sigma_;

    m_CurveType = curveFit;
    switch (m_CurveType)
    {
        case FOCUS_QUADRATIC :
            m_coefficients = polynomial_fit(m_x.data(), m_y.data(), m_x.count(), 2);
            break;
        case FOCUS_HYPERBOLA :
            m_coefficients = hyperbola_fit(m_x, m_y, m_sigma, useWeights);
            break;
        case FOCUS_PARABOLA :
            m_coefficients = parabola_fit(m_x, m_y, m_sigma, useWeights);
            break;
        default :
            // Something went wrong, log an error and reset state so solver starts from scratch if called again
            qCDebug(KSTARS_EKOS_FOCUS) << QString("CurveFitting::CurveFitting called with curveFit=%1").arg(curveFit);
            firstSolverRun = true;
            return;
    }
    lastCoefficients = m_coefficients;
    lastCurveType    = m_CurveType;
    firstSolverRun   = false;
}

double CurveFitting::curveFunction(double x, void *params)
{
    CurveFitting *instance = static_cast<CurveFitting *>(params);

    if (instance && !instance->m_coefficients.empty())
        return instance->f(x);
    else
        return -1;
}

double CurveFitting::f(double x)
{
    const int order = m_coefficients.size() - 1;
    double y = 0;
    if (m_CurveType == FOCUS_QUADRATIC)
    {
        for (int i = 0; i <= order; ++i)
            y += m_coefficients[i] * pow(x, i);
    }
    else if (m_CurveType == FOCUS_HYPERBOLA && m_coefficients.size() == NUM_HYPERBOLA_PARAMS)
        y = hypfx(x, m_coefficients[A_IDX], m_coefficients[B_IDX], m_coefficients[C_IDX], m_coefficients[D_IDX]);
    else if (m_CurveType == FOCUS_PARABOLA && m_coefficients.size() == NUM_PARABOLA_PARAMS)
        y = parfx(x, m_coefficients[A_IDX], m_coefficients[B_IDX], m_coefficients[C_IDX]);
    return y;
}

QVector<double> CurveFitting::polynomial_fit(const double *const data_x, const double *const data_y, const int n,
        const int order)
{
    int status = 0;
    double chisq = 0;
    QVector<double> vc;
    gsl_vector *y, *c;
    gsl_matrix *X, *cov;
    y   = gsl_vector_alloc(n);
    c   = gsl_vector_alloc(order + 1);
    X   = gsl_matrix_alloc(n, order + 1);
    cov = gsl_matrix_alloc(order + 1, order + 1);

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < order + 1; j++)
        {
            gsl_matrix_set(X, i, j, pow(data_x[i], j));
        }
        gsl_vector_set(y, i, data_y[i]);
    }

    // Must turn off error handler or it aborts on error
    gsl_set_error_handler_off();

    gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(n, order + 1);
    status                              = gsl_multifit_linear(X, y, c, cov, &chisq, work);

    if (status != GSL_SUCCESS)
        qDebug() << Q_FUNC_INFO << "GSL multifit error:" << gsl_strerror(status);
    else
    {
        gsl_multifit_linear_free(work);

        for (int i = 0; i < order + 1; i++)
        {
            vc.push_back(gsl_vector_get(c, i));
        }
    }

    gsl_vector_free(y);
    gsl_vector_free(c);
    gsl_matrix_free(X);
    gsl_matrix_free(cov);

    return vc;
}

QVector<double> CurveFitting::hyperbola_fit(const QVector<double> data_x, const QVector<double> data_y,
        const QVector<double> data_sigma,
        const bool useWeights)
{
    QVector<double> vc;
    DataPointT dataPoints;

    qCDebug(KSTARS_EKOS_FOCUS) <<
                               QString("Starting Levenberg-Marquardt solver, fit=hyperbola, Iterations= %1, Precision abs/rel=%2/%3...")
                               .arg(MAX_ITERATIONS).arg(INEPSABS).arg(INEPSREL);
    auto weights = gsl_vector_alloc(data_sigma.size());
    // Fill in the data to which the curve will be fitted
    dataPoints.useWeights = useWeights;
    for (int i = 0; i < data_x.size(); i++)
        dataPoints.push_back(data_x[i], data_y[i]);

    // Set the gsl error handler off as it aborts the program on error.
    gsl_set_error_handler_off();

    gsl_multifit_nlinear_parameters params = gsl_multifit_nlinear_default_parameters();
    params.trs = gsl_multifit_nlinear_trs_lmaccel;
    params.solver = gsl_multifit_nlinear_solver_qr;
    gsl_multifit_nlinear_workspace* w = gsl_multifit_nlinear_alloc (gsl_multifit_nlinear_trust, &params, data_x.size(), NUM_HYPERBOLA_PARAMS);

    // Fill in function info
    gsl_multifit_nlinear_fdf fdf;
    fdf.f = hypFx;
    fdf.df = hypJx;
    fdf.fvv = nullptr;
    fdf.n = data_x.size();
    fdf.p = NUM_HYPERBOLA_PARAMS;
    fdf.params = &dataPoints;

    // Allocate the guess vector
    gsl_vector * guess = gsl_vector_alloc(NUM_HYPERBOLA_PARAMS);
    // Make initial guesses - here we just set all parameters to 1.0
    hypMakeGuess(data_x, data_y, guess);
    if (useWeights)
    {

        for (int i = 0; i < data_sigma.size(); i++)
        {
            gsl_vector_set(weights,i,data_sigma[i]);
        }
        gsl_multifit_nlinear_winit(guess, weights, &fdf, w);
    }
    else
    {
        gsl_multifit_nlinear_init(guess, &fdf, w);
    }

    int info = 0;
    int status = gsl_multifit_nlinear_driver(MAX_ITERATIONS,INEPSABS, INEPSREL, INEPSABS, nullptr,nullptr,&info,w);

    if (status != 0)
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LM solver (Hyperbola): Failed with status=%1 [%2] after %3/%4 iterations")
                                   .arg(status).arg(gsl_strerror(status)).arg(gsl_multifit_nlinear_niter(w)).arg(MAX_ITERATIONS);
    else
    {
        // All good so store the results - parameters A, B, C and D
        auto solution = gsl_multifit_nlinear_position(w);
        for (int j = 0; j < NUM_HYPERBOLA_PARAMS; j++)
        {
            vc.push_back(gsl_vector_get(solution, j));
        }
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LM Solver (Hyperbola): Solution found after %1 iterations. A=%2 B=%3, C=%4, D=%5")
                                   .arg(gsl_multifit_nlinear_niter(w)).arg(vc[A_IDX]).arg(vc[B_IDX]).arg(vc[C_IDX]).arg(vc[D_IDX]);
    }

    // Free GSL memory
    gsl_multifit_nlinear_free(w);
    gsl_vector_free(guess);
    gsl_vector_free(weights);

    return vc;
}

// Initialise parameters before starting the solver
void CurveFitting::hypMakeGuess(const QVector<double> inX, const QVector<double> inY, gsl_vector * guess)
{
    if (inX.size() < 1)
        return;

    if (!firstSolverRun && (lastCurveType == FOCUS_HYPERBOLA) && (lastCoefficients.size() == NUM_HYPERBOLA_PARAMS))
    {
        // Last run of the solver was a Hyperbola and the solution was good, so use that solution
        gsl_vector_set(guess, A_IDX, lastCoefficients[A_IDX]);
        gsl_vector_set(guess, B_IDX, lastCoefficients[B_IDX]);
        gsl_vector_set(guess, C_IDX, lastCoefficients[C_IDX]);
        gsl_vector_set(guess, D_IDX, lastCoefficients[D_IDX]);
    }
    else
    {
        // Find min HFD -> good start value for c. b > 0 and b + d > 0
        int i;
        double minX, minY;

        minX = inX[0];
        minY = inY[0];
        for(i = 0; i < inX.size(); i++)
        {
            if(inY[i] < minY)
            {
                minX = inX[i];
                minY = inY[i];
            }
        };
        gsl_vector_set(guess, A_IDX, 1.0);
        gsl_vector_set(guess, B_IDX, 1.0);
        gsl_vector_set(guess, C_IDX, minX);
        gsl_vector_set(guess, D_IDX, 1.0);
    }
}

QVector<double> CurveFitting::parabola_fit(const QVector<double> data_x, const QVector<double> data_y,
        const QVector<double> data_sigma,
        bool useWeights)
{
    QVector<double> vc;
    DataPointT dataPoints;

    qCDebug(KSTARS_EKOS_FOCUS) <<
                               QString("Starting Levenberg-Marquardt solver, fit=parabola, Iterations= %1, Precision abs/rel=%2/%3...")
                               .arg(MAX_ITERATIONS).arg(INEPSABS).arg(INEPSREL);

    auto weights = gsl_vector_alloc(data_sigma.size());
    // Fill in the data to which the curve will be fitted
    dataPoints.useWeights = useWeights;
    for (int i = 0; i < data_x.size(); i++)
        dataPoints.push_back(data_x[i], data_y[i]);

    // Set the gsl error handler off as it aborts the program on error.
    gsl_set_error_handler_off();

    gsl_multifit_nlinear_parameters params = gsl_multifit_nlinear_default_parameters();
    params.trs = gsl_multifit_nlinear_trs_lmaccel;
    params.solver = gsl_multifit_nlinear_solver_qr;
    gsl_multifit_nlinear_workspace* w = gsl_multifit_nlinear_alloc (gsl_multifit_nlinear_trust, &params, data_x.size(), NUM_PARABOLA_PARAMS);

    // Fill in function info
    gsl_multifit_nlinear_fdf fdf;
    fdf.f = parFx;
    fdf.df = parJx;
    fdf.fvv = nullptr;
    fdf.n = data_x.size();
    fdf.p = NUM_PARABOLA_PARAMS;
    fdf.params = &dataPoints;

    // Allocate the guess vector
    gsl_vector * guess = gsl_vector_alloc(NUM_PARABOLA_PARAMS);
    // Make initial guesses - here we just set all parameters to 1.0
    parMakeGuess(data_x, data_y, guess);
    if (useWeights)
    {

        for (int i = 0; i < data_sigma.size(); i++)
        {
            gsl_vector_set(weights,i,data_sigma[i]);
        }
        gsl_multifit_nlinear_winit(guess, weights, &fdf, w);
    }
    else
    {
        gsl_multifit_nlinear_init(guess, &fdf, w);
    }


    int info = 0;
    int status = gsl_multifit_nlinear_driver(MAX_ITERATIONS,INEPSABS, INEPSREL, INEPSABS, nullptr,nullptr,&info,w);

    if (status != 0)
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LM solver (Parabola): Failed with status=%1 [%2] after %3/%4 iterations")
                                   .arg(status).arg(gsl_strerror(status)).arg(gsl_multifit_nlinear_niter(w)).arg(MAX_ITERATIONS);
    else
    {
        // All good so store the results - parameters A, B, and C
        auto solution = gsl_multifit_nlinear_position(w);
        for (int j = 0; j < NUM_PARABOLA_PARAMS; j++)
        {
            vc.push_back(gsl_vector_get(solution, j));
        }
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LM Solver (Parabola): Solution found after %1 iterations. A=%2 B=%3, C=%4")
                                   .arg(gsl_multifit_nlinear_niter(w)).arg(vc[A_IDX]).arg(vc[B_IDX]).arg(vc[C_IDX]);
    }

    // Free GSL memory
    gsl_multifit_nlinear_free(w);
    gsl_vector_free(guess);
    gsl_vector_free(weights);

    return vc;
}

// Initialise parameters before starting the solver
void CurveFitting::parMakeGuess(const QVector<double> inX, const QVector<double> inY, gsl_vector * guess)
{
    if (inX.size() < 1)
        return;

    if (!firstSolverRun && (lastCurveType == FOCUS_PARABOLA) && (lastCoefficients.size() == NUM_PARABOLA_PARAMS))
    {
        // Last run of the solver was a Parabola and that solution was good, so use that solution
        gsl_vector_set(guess, A_IDX, lastCoefficients[A_IDX]);
        gsl_vector_set(guess, B_IDX, lastCoefficients[B_IDX]);
        gsl_vector_set(guess, C_IDX, lastCoefficients[C_IDX]);
    }
    else
    {
        // Find min HFD -> good start value for c. a and b need to be positive.
        int i;
        double minX, minY;

        minX = inX[0];
        minY = inY[0];
        for(i = 0; i < inX.size(); i++)
        {
            if(inY[i] < minY)
            {
                minX = inX[i];
                minY = inY[i];
            }
        };
        gsl_vector_set(guess, A_IDX, 1.0);
        gsl_vector_set(guess, B_IDX, 1.0);
        gsl_vector_set(guess, C_IDX, minX);
    }
}

bool CurveFitting::findMin(double expected, double minPosition, double maxPosition, double *position, double *value,
                           CurveFit curveFit)
{
    bool foundFit;
    switch (curveFit)
    {
        case FOCUS_QUADRATIC :
            foundFit = minimumQuadratic(expected, minPosition, maxPosition, position, value);
            break;
        case FOCUS_HYPERBOLA :
            foundFit = minimumHyperbola(expected, minPosition, maxPosition, position, value);
            break;
        case FOCUS_PARABOLA :
            foundFit = minimumParabola(expected, minPosition, maxPosition, position, value);
            break;
        default :
            foundFit = false;
            break;
    }
    if (!foundFit)
        // If we couldn't fit a curve then something's wrong so reset coefficients which will force the next run of the LM solver to start from scratch
        lastCoefficients.clear();
    return foundFit;
}

bool CurveFitting::minimumQuadratic(double expected, double minPosition, double maxPosition, double *position,
                                    double *value)
{
    int status;
    int iter = 0, max_iter = 100;
    const gsl_min_fminimizer_type *T;
    gsl_min_fminimizer *s;
    double m = expected;

    gsl_function F;
    F.function = &CurveFitting::curveFunction;
    F.params   = this;

    // Must turn off error handler or it aborts on error
    gsl_set_error_handler_off();

    T      = gsl_min_fminimizer_brent;
    s      = gsl_min_fminimizer_alloc(T);
    status = gsl_min_fminimizer_set(s, &F, expected, minPosition, maxPosition);

    if (status != GSL_SUCCESS)
    {
        qCWarning(KSTARS_EKOS_FOCUS) << "Focus GSL error:" << gsl_strerror(status);
        return false;
    }

    do
    {
        iter++;
        status = gsl_min_fminimizer_iterate(s);

        m = gsl_min_fminimizer_x_minimum(s);
        minPosition = gsl_min_fminimizer_x_lower(s);
        maxPosition = gsl_min_fminimizer_x_upper(s);

        status = gsl_min_test_interval(minPosition, maxPosition, 0.01, 0.0);

        if (status == GSL_SUCCESS)
        {
            *position = m;
            *value    = curveFunction(m, this);
        }
    }
    while (status == GSL_CONTINUE && iter < max_iter);

    gsl_min_fminimizer_free(s);
    return (status == GSL_SUCCESS);
}

bool CurveFitting::minimumHyperbola(double expected, double minPosition, double maxPosition, double *position,
                                    double *value)
{
    Q_UNUSED(expected);
    if (m_coefficients.size() != NUM_HYPERBOLA_PARAMS)
    {
        if (m_coefficients.size() != 0)
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::minimumHyperbola coefficients.size()=%1").arg(
                                           m_coefficients.size());
        return false;
    }

    double b = m_coefficients[B_IDX];
    double c = m_coefficients[C_IDX];
    double d = m_coefficients[D_IDX];

    // We need to check that the solution found is in the correct form.
    // Check 1: The hyperbola minimum (=c) lies within the bounds of the focuser (and is > 0)
    // Check 2: At the minimum position (=c), the value of f(x) (which is the HFR) given by b+d is > 0
    // Check 3: We have a "u" shaped curve, not an "n" shape. b > 0.
    if ((c >= minPosition) && (c <= maxPosition) && (b + d > 0.0) && (b > 0.0))
    {
        *position = c;
        *value = b + d;
        return true;
    }
    else
        return false;
}

bool CurveFitting::minimumParabola(double expected, double minPosition, double maxPosition, double *position, double *value)
{
    Q_UNUSED(expected);
    if (m_coefficients.size() != NUM_PARABOLA_PARAMS)
    {
        if (m_coefficients.size() != 0)
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::minimumParabola coefficients.size()=%1").arg(
                                           m_coefficients.size());
        return false;
    }

    double a = m_coefficients[A_IDX];
    double b = m_coefficients[B_IDX];
    double c = m_coefficients[C_IDX];

    // We need to check that the solution found is in the correct form.
    // Check 1: The parabola minimum (=c) lies within the bounds of the focuser (and is > 0)
    // Check 2: At the minimum position (=c), the value of f(x) (which is the HFR) given by a is > 0
    // Check 3: We have a "u" shaped curve, not an "n" shape. b > 0.
    if ((c >= minPosition) && (c <= maxPosition) && (a > 0.0) && (b > 0.0))
    {
        *position = c;
        *value = a;
        return true;
    }
    else
        return false;
}

// R2 (R squared) is the coefficient of determination gives a measure of how well the curve fits the datapoints.
// It lies between 0 and 1. 1 means that all datapoints will lie on the curve which therefore exactly describes the
// datapoints. 0 means that there is no correlation between the curve and the datapoints.
// See www.wikipedia.org/wiki/Coefficient_of_determination for more details.
double CurveFitting::calculateR2(CurveFit curveFit)
{
    double R2 = 0.0;
    QVector<double> curvePoints;
    int i;
    switch (curveFit)
    {
        case FOCUS_QUADRATIC :
            // Not implemented
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateR2 called for Quadratic");
            break;

        case FOCUS_HYPERBOLA :
            // Calculate R2 for the hyperbola
            if (m_coefficients.size() != NUM_HYPERBOLA_PARAMS)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateR2 hyperbola coefficients.size()=").arg(
                                               m_coefficients.size());
                break;
            }

            for (i = 0; i < m_y.size(); i++)
                // Load up the curvePoints vector
                curvePoints.push_back(hypfx(m_x[i], m_coefficients[A_IDX], m_coefficients[B_IDX], m_coefficients[C_IDX],
                                            m_coefficients[D_IDX]));
            // Do the actual R2 calculation
            R2 = calcR2(m_y, curvePoints);
            break;

        case FOCUS_PARABOLA :
            // Calculate R2 for the parabola
            if (m_coefficients.size() != NUM_PARABOLA_PARAMS)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateR2 parabola coefficients.size()=").arg(
                                               m_coefficients.size());
                break;
            }

            for (i = 0; i < m_y.size(); i++)
                // Load up the curvePoints vector
                curvePoints.push_back(parfx(m_x[i], m_coefficients[A_IDX], m_coefficients[B_IDX], m_coefficients[C_IDX]));

            // Do the actual R2 calculation
            R2 = calcR2(m_y, curvePoints);
            break;

        default :
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateR2 curveFit=%1").arg(curveFit);
            break;
    }
    // R2 for linear function range 0<=R2<=1. For non-linear functions it is possible
    // for R2 to be negative. This doesn't really mean anything so force 0 in these situations.
    return std::max(R2, 0.0);
}

// Do the maths to calculate R2 - how well the curve fits the datapoints
double CurveFitting::calcR2(QVector<double> dataPoints, QVector<double> curvePoints)
{
    double R2 = 0.0, chisq = 0.0, sum = 0.0, totalSumSquares = 0.0, average;
    int i;

    // Calculate R2 for the hyperbola
    for (i = 0; i < dataPoints.size(); i++)
    {
        sum += dataPoints[i];
        chisq += pow((dataPoints[i] - curvePoints[i]), 2.0);
    }
    average = sum / dataPoints.size();

    for (i = 0; i < dataPoints.size(); i++)
    {
        totalSumSquares += pow((dataPoints[i] - average), 2.0);
    }

    if (totalSumSquares > 0.0)
        R2 = 1 - (chisq / totalSumSquares);
    else
    {
        R2 = 0.0;
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calcR2 tss=%1").arg(totalSumSquares);
    }
    // R2 for linear function range 0<=R2<=1. For non-linear functions it is possible
    // for R2 to be negative. This doesn't really mean anything so force 0 in these situations.
    return std::max(R2, 0.0);
}
}  // namespace

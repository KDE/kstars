#include "curvefit.h"
#include "ekos/ekos.h"
#include <ekos_focus_debug.h>

// Constants used to identify the number of parameters used for different curve types
constexpr int NUM_HYPERBOLA_PARAMS = 4;
constexpr int NUM_PARABOLA_PARAMS = 3;
//constexpr int NUM_GAUSSIAN_PARAMS = 7;
constexpr int NUM_GAUSSIAN_PARAMS = 7;
// Parameters for the solver
// MAX_ITERATIONS is used to limit the number of iterations for the solver.
// A value needs to be entered that allows a solution to be found without iterating unnecessarily
// There is a relationship with the tolerance parameters that follow.
constexpr int MAX_ITERATIONS_CURVE = 5000;
constexpr int MAX_ITERATIONS_STARS = 1000;
// The next 3 parameters are used as tolerance for convergence
// convergence is achieved if for each datapoint i
//     dx_i < INEPSABS + (INEPSREL * x_i)
constexpr double INEPSXTOL = 1e-5;
const double INEPSGTOL = pow(GSL_DBL_EPSILON, 1.0 / 3.0);
constexpr double INEPSFTOL = 1e-5;

// The functions here fit a number of different curves to the incoming data points using the Lehvensberg-Marquart
// solver with geodesic acceleration as provided the Gnu Science Library (GSL). The following sources of information are useful:
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
// In addition, the solver has been applied to a 3D Gaussian in order to fit star profiles and calculate FWHM
//
// Hyperbola
// ---------
// Equation y = f(x) = b * sqrt(1 + ((x - c) / a) ^2) + d
// This can be re-written as:
//
// f(x) = b * phi + d
// where phi = sqrt(1 + ((x - c) / a) ^2)
//
// Jacobian J = {df/da, df/db, df/dc, df/db}
// df/da      = b * (1 / 2) * (1 / phi) * -2 * ((x - c) ^2) / (a ^3)
//            = -b * ((x - c) ^2) / ((a ^ 3) * phi)
// df/db      = phi
// df/dc      = b * (1 / 2) * (1 / phi) * 2 * (x - c) / (a ^2) * (-1)
//            = -b * (x - c) / ((a ^2) * phi)
// df/dd      = 1
//
// Second directional derivative:
// {
//   {ddf/dada,ddf/dadb,ddf/dadc,ddf/dadd},
//   {ddf/dbda,ddf/dbdb,ddf/dbdc,ddf/dbdd},
//   {ddf/dcda,ddf/dcdb,ddf/dcdc,ddf/dcdd},
//   {ddf/ddda,ddf/dddb,ddf/dddc,ddf/dddd}
// }
// Note the matrix is symmetric, as ddf/dxdy = ddf/dydx.
//
// ddf/dada = (b*(c-x)^2*(3*a^2+a*(c-x)^2))/(a^4*(a^2 + (c-x)^2)^3/2))
// ddf/dadb = -((x - c) ^2) / ((a ^ 3) * phi)
// ddf/dadc = -(b*(c-x)*(2a^2+(c-x)^2))/(a^3*(a^2 + (c-x)^2)^3/2))
// ddf/dadd = 0
// ddf/dbdb = 0
// ddf/dbdc = -(x-c)/(a^2*phi)
// ddf/dbdd = 0
// ddf/dcdc = b/((a^2+(c-x)^2) * phi)
// ddf/dcdd = 0
// ddf/dddd = 0
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
// Gaussian
// --------
// Generalised equation for a 2D gaussian.
// Equation z = f(x,y) = b + a.exp-(A((x-x0)^2) + 2B(x-x0)(y-y0) + 2C((y-y0)^2))

// with parameters
// b          = background
// a          = Peak Intensity
// x0         = Star centre in x dimension
// y0         = Star centre in y dimension
// A, B, C    = relate to the shape of the elliptical gaussian and the orientation of the major and
//              minor ellipse axes wrt x and y
//
// Jacobian J = {df/db, df/da, df/dx0, df/dy0, df/dA, df/dB, df/dC}
// Let phi    = exp-(A((x-x0)^2) + 2B(x-x0)(y-y0) + C((y-y0)^2))
// df/db      = 1
// df/da      = phi
// df/dx0     = [2A(x-x0)+2B(y-y0)].a.phi
// df/dy0     = [2B(x-x0)+2C(y-y0)].a.phi
// df/dA      = -(x-x0)^2.a.phi
// df/DB      = -2(x-x0)(y-y0).a.phi
// df/dC      = -(y-y0)^2.a.phi
//
// 2nd derivatives
// {
//   {ddf/dbdb,ddf/dbda,ddf/dbdx0,ddf/dbdy0,ddf/dbdA,ddf/dbdB,ddf/dbdC},
//   {ddf/dadb,ddf/dada,ddf/dadx0,ddf/dady0,ddf/dadA,ddf/dadB,ddf/dadC},
//   {ddf/dx0db,ddf/dx0da,ddf/dx0dx0,ddf/dx0dy0,ddf/dx0dA,ddf/dx0dB,ddf/dx0dC},
//   {ddf/dy0db,ddf/dy0da,ddf/dy0dx0,ddf/dy0dy0,ddf/dy0dA,ddf/dy0dB,ddf/dy0dC},
//   {ddf/dAdb,ddf/dAda,ddf/dAdx0,ddf/dAdy0,ddf/dAdA,ddf/dAdB,ddf/dAdC},
//   {ddf/dBdb,ddf/dBda,ddf/dBdx0,ddf/dBdy0,ddf/dBdA,ddf/dBdB,ddf/dBdC},
//   {ddf/dCdb,ddf/dCda,ddf/dCdx0,ddf/dCdy0,ddf/dCdA,ddf/dCdB,ddf/dCdC},
// }
// The matrix is symmetric, as ddf/dParm1dParm2 = ddf/dParm2dParm1 so we only need to differentiate
// one half (above or below the diagonal) of the matrix elements.
// ddf/dbdb   = 0
// ddf/dbda   = 0
// ddf/dbdx0  = 0
// ddf/dbdy0  = 0
// ddf/dbdA   = 0
// ddf/dbdB   = 0
// ddf/dbdC   = 0
//
// ddf/dada   = 0
// ddf/dadx0  = [2A(x-x0) + 2B(y-y0)].phi
// ddf/dady0  = [2B(x-x0) + 2C(y-y0)].phi
// ddf/dadA   = -(x-x0)^2.phi
// ddf/dadB   = -2(x-x0)(y-y0).phi
// ddf/dadC   = -Cy-y0)^2.phi
//
// ddf/dxodx0 = -2A.a.phi + [2A(x-x0)+2B(y-y0)]^2.a.phi
// ddf/dx0dy0 = -2B.a.phi - [2A(x-x0)+2B(y-y0)].[2B(x-x0)+2C(y-y0)].a.phi
// ddf/dx0dA  = 2(x-x0).a.phi - [2A(x-x0)+2B(y-y0)].(x-x0)^2.a.phi
// ddf/dx0dB  = 2(y-y0).a.phi - [2A(x-x0)+2B(y-y0)].2(x-x0).(y-y0).a.phi
// ddf/dx0dC  = -[2A(x-x0)+2B(y-y0)].2(y-y0)^2.a.phi
//
// ddf/dy0dy0 = -2c.a.phi + [2B(x-x0)+2C(y-y0)]^2.a.phi
// ddf/dy0dA  = -[2B(x-x0)+2C(y-y0)].(x-x0)^2.a.phi
// ddf/dy0dB  = 2(x-x0).a.phi - [2B(x-x0)+2C(y-y0)].2(x-x0)(y-y0).a.phi
// ddf/dy0dC  = 2(y-y0).a.phi - [2B(x-x0)+2C(y-y0)].(y-y0)^2.a.phi
//
// ddf/dAdA   = (x-x0)^4.a.phi
// ddf/dAdB   = 2(x-x0)^3.(y-y0).a.phi
// ddf/dAdC   = (x-x0)^2.(y-y0)^2.a.phi
//
// ddf/dBdB   = 4(x-x0)^2.(y-y0)^2.a.phi
// ddf/dBdC   = 2(x-x0).(y-y0)^3.a.phi
//
// ddf/dCdC   = (y-y0)^4.a.phi
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
enum { A_IDX = 0, B_IDX, C_IDX, D_IDX, E_IDX, F_IDX, G_IDX };

// hypPhi() is a repeating part of the function calculations for Hyperbolas.
double hypPhi(double x, double a, double c)
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


// ddf/dada = (b*(c-x)^2*(3*a^2+a*(c-x)^2))/(a^4*(a^2 + (c-x)^2)*phi)
// ddf/dadb = -((x - c) ^2) / ((a ^ 3) * phi)
// ddf/dadc = -(b*(c-x)*(2a^2+(c-x)^2))/(a^3*(a^2 + (c-x)^2)*phi))
// ddf/dadd = 0
// ddf/dbdb = 0
// ddf/dbdc = -(x-c)/(a^2*phi)
// ddf/dbdd = 0
// ddf/dcdc = b/((a^2+(c-x)^2) * phi)
// ddf/dcdd = 0
// ddf/dddd = 0

// ddf/dada = (b*(c-x)^2*/(a^4*phi).[3 - (x-c)^2/(a * phi)^2]
// ddf/dadb = -((x - c)^2) / ((a^3) * phi)
// ddf/dadc = -(b*(x-c)/((a * phi)^3).[2 - (x-c)^2/((a * phi)^2)]
// ddf/dadd = 0
// ddf/dbdb = 0
// ddf/dbdc = -(x-c)/(a^2*phi)
// ddf/dbdd = 0
// ddf/dcdc = b/(phi^3 * a^2).[1 - ((x - c)^2)/((a * phi)^2)]
// ddf/dcdd = 0
// ddf/dddd = 0
int hypFxx(const gsl_vector* X,  const gsl_vector* v, void* inParams, gsl_vector* fvv)
{
    CurveFitting::DataPointT * DataPoint = ((struct CurveFitting::DataPointT *)inParams);

    // Store current coefficients
    const double a = gsl_vector_get(X, A_IDX);
    const double b = gsl_vector_get(X, B_IDX);
    const double c = gsl_vector_get(X, C_IDX);

    const double a2 = pow(a, 2);
    const double a4 = pow(a2, 2);

    const double va = gsl_vector_get(v, A_IDX);
    const double vb = gsl_vector_get(v, B_IDX);
    const double vc = gsl_vector_get(v, C_IDX);

    for(int i = 0; i < DataPoint->dps.size(); ++i)
    {
        const double x = DataPoint->dps[i].x;
        const double xmc   = x - c;
        const double xmc2  = pow(xmc, 2);
        const double phi   = hypPhi(x, a, c);
        const double aphi  = a * phi;
        const double aphi2 = pow(aphi, 2);

        const double Daa = b * xmc2 * (3 - xmc2 / aphi2) / (a4 * phi);
        const double Dab = -xmc2 / (a2 * aphi);
        const double Dac = b * xmc * (2 - (xmc2 / aphi2)) / (aphi2 * aphi);
        const double Dbc = -xmc / (a2 * phi);
        const double Dcc = b * (1 - (xmc2 / aphi2)) / (phi * aphi2);

        double sum = va * va * Daa + 2 * (va * vb * Dab + va * vc * Dac + vb * vc * Dbc) + vc * vc * Dcc;

        gsl_vector_set(fvv, i, sum);

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
        const double xmc = x - c;

        gsl_matrix_set(J, i, A_IDX, 1);
        gsl_matrix_set(J, i, B_IDX, xmc * xmc);
        gsl_matrix_set(J, i, C_IDX, -2 * b * xmc);
    }

    return GSL_SUCCESS;
}
// Calculates the second directional derivative vector for the parabola equation f(x) = a + b*(x-c)^2
int parFxx(const gsl_vector* X,  const gsl_vector* v, void* inParams, gsl_vector* fvv)
{
    CurveFitting::DataPointT * DataPoint = ((struct CurveFitting::DataPointT *)inParams);

    const double b = gsl_vector_get(X, B_IDX);
    const double c = gsl_vector_get(X, C_IDX);

    const double vb = gsl_vector_get(v, B_IDX);
    const double vc = gsl_vector_get(v, C_IDX);

    for(int i = 0; i < DataPoint->dps.size(); ++i)
    {
        const double x = DataPoint->dps[i].x;
        double Dbc = -2 * (x - c);
        double Dcc = 2 * b;
        double sum = 2 * vb * vc * Dbc + vc * vc * Dcc;

        gsl_vector_set(fvv, i, sum);

    }

    return GSL_SUCCESS;
}

// Function to calculate f(x,y) for a 2-D gaussian.
double gaufxy(double x, double y, double a, double x0, double y0, double A, double B, double C, double b)
{
    return b + a * exp(-(A * (pow(x - x0, 2.0)) + 2.0 * B * (x - x0) * (y - y0) + C * (pow(y - y0, 2.0))));
}

// Calculates f(x,y) for each data point in the gaussian.
int gauFxy(const gsl_vector * X, void * inParams, gsl_vector * outResultVec)
{
    CurveFitting::DataPoint3DT * DataPoint = ((struct CurveFitting::DataPoint3DT *)inParams);

    double a  = gsl_vector_get (X, A_IDX);
    double x0  = gsl_vector_get (X, B_IDX);
    double y0 = gsl_vector_get (X, C_IDX);
    double A = gsl_vector_get (X, D_IDX);
    double B  = gsl_vector_get (X, E_IDX);
    double C  = gsl_vector_get (X, F_IDX);
    double b  = gsl_vector_get (X, G_IDX);

    for(int i = 0; i < DataPoint->dps.size(); ++i)
    {
        // Gaussian equation
        double zij = gaufxy(DataPoint->dps[i].x, DataPoint->dps[i].y, a, x0, y0, A, B, C, b);
        gsl_vector_set(outResultVec, i, (zij - DataPoint->dps[i].z));
    }

    return GSL_SUCCESS;
}

// Calculates the Jacobian (derivative) matrix for the gaussian
// Jacobian J = {df/db, df/da, df/dx0, df/dy0, df/dA, df/dB, df/dC}
// Let phi    = exp-(A((x-x0)^2) + 2B(x-x0)(y-y0) + C((y-y0)^2))
// df/db      = 1
// df/da      = phi
// df/dx0     = [2A(x-x0)+2B(y-y0)].a.phi
// df/dy0     = [2B(x-x0)+2C(y-y0)].a.phi
// df/dA      = -(x-x0)^2.a.phi
// df/DB      = -2(x-x0)(y-y0).a.phi
// df/dC      = -(y-y0)^2.a.phi
int gauJxy(const gsl_vector * X, void * inParams, gsl_matrix * J)
{
    CurveFitting::DataPointT * DataPoint = ((struct CurveFitting::DataPointT *)inParams);

    // Get current coefficients
    const double a  = gsl_vector_get (X, A_IDX);
    const double x0  = gsl_vector_get (X, B_IDX);
    const double y0 = gsl_vector_get (X, C_IDX);
    const double A = gsl_vector_get (X, D_IDX);
    const double B  = gsl_vector_get (X, E_IDX);
    const double C  = gsl_vector_get (X, F_IDX);
    // b is not used ... const double b  = gsl_vector_get (X, G_IDX);

    for(int i = 0; i < DataPoint->dps.size(); ++i)
    {
        // Calculate the Jacobian Matrix
        const double x = DataPoint->dps[i].x;
        const double xmx0 = x - x0;
        const double xmx02 = xmx0 * xmx0;
        const double y = DataPoint->dps[i].y;
        const double ymy0 = y - y0;
        const double ymy02 = ymy0 * ymy0;
        const double phi = exp(-((A * xmx02) + (2.0 * B * xmx0 * ymy0) + (C * ymy02)));
        const double aphi = a * phi;

        gsl_matrix_set(J, i, A_IDX, phi);
        gsl_matrix_set(J, i, B_IDX, 2.0 * aphi * ((A * xmx0) + (B * ymy0)));
        gsl_matrix_set(J, i, C_IDX, 2.0 * aphi * ((B * xmx0) + (C * ymy0)));
        gsl_matrix_set(J, i, D_IDX, -1.0 * aphi * xmx02);
        gsl_matrix_set(J, i, E_IDX, -2.0 * aphi * xmx0 * ymy0);
        gsl_matrix_set(J, i, F_IDX, -1.0 * aphi * ymy02);
        gsl_matrix_set(J, i, G_IDX, 1.0);
    }

    return GSL_SUCCESS;
}

// Calculates the second directional derivative vector for the gaussian equation
// The matrix is symmetric, as ddf/dParm1dParm2 = ddf/dParm2dParm1 so we only need to differentiate
// one half (above or below the diagonal) of the matrix elements.
// ddf/dbdb   = 0
// ddf/dbda   = 0
// ddf/dbdx0  = 0
// ddf/dbdy0  = 0
// ddf/dbdA   = 0
// ddf/dbdB   = 0
// ddf/dbdC   = 0
//
// ddf/dada   = 0
// ddf/dadx0  = [2A(x-x0) + 2B(y-y0)].phi
// ddf/dady0  = [2B(x-x0) + 2C(y-y0)].phi
// ddf/dadA   = -(x-x0)^2.phi
// ddf/dadB   = -2(x-x0)(y-y0).phi
// ddf/dadC   = -Cy-y0)^2.phi
//
// ddf/dxodx0 = -2A.a.phi + [2A(x-x0)+2B(y-y0)]^2.a.phi
// ddf/dx0dy0 = -2B.a.phi - [2A(x-x0)+2B(y-y0)].[2B(x-x0)+2C(y-y0)].a.phi
// ddf/dx0dA  = 2(x-x0).a.phi - [2A(x-x0)+2B(y-y0)].(x-x0)^2.a.phi
// ddf/dx0dB  = 2(y-y0).a.phi - [2A(x-x0)+2B(y-y0)].2(x-x0).(y-y0).a.phi
// ddf/dx0dC  = -[2A(x-x0)+2B(y-y0)].2(y-y0)^2.a.phi
//
// ddf/dy0dy0 = -2c.a.phi + [2B(x-x0)+2C(y-y0)]^2.a.phi
// ddf/dy0dA  = -[2B(x-x0)+2C(y-y0)].(x-x0)^2.a.phi
// ddf/dy0dB  = 2(x-x0).a.phi - [2B(x-x0)+2C(y-y0)].2(x-x0)(y-y0).a.phi
// ddf/dy0dC  = 2(y-y0).a.phi - [2B(x-x0)+2C(y-y0)].(y-y0)^2.a.phi
//
// ddf/dAdA   = (x-x0)^4.a.phi
// ddf/dAdB   = 2(x-x0)^3.(y-y0).a.phi
// ddf/dAdC   = (x-x0)^2.(y-y0)^2.a.phi
//
// ddf/dBdB   = 4(x-x0)^2.(y-y0)^2.a.phi
// ddf/dBdC   = 2(x-x0).(y-y0)^3.a.phi
//
// ddf/dCdC   = (y-y0)^4.a.phi
//
int gauFxyxy(const gsl_vector* X,  const gsl_vector* v, void* inParams, gsl_vector* fvv)
{
    CurveFitting::DataPointT * DataPoint = ((struct CurveFitting::DataPointT *)inParams);

    // Get current coefficients
    const double a  = gsl_vector_get (X, A_IDX);
    const double x0 = gsl_vector_get (X, B_IDX);
    const double y0 = gsl_vector_get (X, C_IDX);
    const double A  = gsl_vector_get (X, D_IDX);
    const double B  = gsl_vector_get (X, E_IDX);
    const double C  = gsl_vector_get (X, F_IDX);
    // b not used ... const double b  = gsl_vector_get (X, G_IDX);

    const double va  = gsl_vector_get(v, A_IDX);
    const double vx0 = gsl_vector_get(v, B_IDX);
    const double vy0 = gsl_vector_get(v, C_IDX);
    const double vA  = gsl_vector_get(v, D_IDX);
    const double vB  = gsl_vector_get(v, E_IDX);
    const double vC  = gsl_vector_get(v, F_IDX);
    // vb not used ... const double vb  = gsl_vector_get(v, G_IDX);

    for(int i = 0; i < DataPoint->dps.size(); ++i)
    {
        double x = DataPoint->dps[i].x;
        double xmx0 = x - x0;
        double xmx02 = xmx0 * xmx0;
        double y = DataPoint->dps[i].y;
        double ymy0 = y - y0;
        double ymy02 = ymy0 * ymy0;
        double phi = exp(-((A * xmx02) + (2.0 * B * xmx0 * ymy0) + (C * ymy02)));
        double aphi = a * phi;
        double AB = 2.0 * ((A * xmx0) + (B * ymy0));
        double BC = 2.0 * ((B * xmx0) + (C * ymy0));

        double Dax0 = AB * phi;
        double Day0 = BC * phi;
        double DaA  = -xmx02 * phi;
        double DaB  = -2.0 * xmx0 * ymy0 * phi;
        double DaC  = -ymy02 * phi;

        double Dx0x0 = aphi * ((-2.0 * A) + (AB * AB));
        double Dx0y0 = -aphi * ((2.0 * B) + (AB * BC));
        double Dx0A  = aphi * ((2.0 * xmx0) - (AB * xmx02));
        double Dx0B  = 2.0 * aphi * (ymy0 - (AB * xmx0 * ymy0));
        double Dx0C  = -2.0 * aphi * AB * ymy02;

        double Dy0y0 = aphi * ((-2.0 * C) + (BC * BC));
        double Dy0A  = -aphi * BC * xmx02;
        double Dy0B  = 2.0 * aphi * (xmx0 - (BC * xmx0 * ymy0));
        double Dy0C  = aphi * ((2.0 * ymy0) - (BC * ymy02));

        double DAA   = aphi * xmx02 * xmx02;
        double DAB   = 2.0 * aphi * xmx02 * xmx0 * ymy0;
        double DAC   = aphi * xmx02 * ymy02;

        double DBB   = 4.0 * aphi * xmx02 * ymy02;
        double DBC   = 2.0 * aphi * xmx0 * ymy02 * ymy0;

        double DCC   = aphi * ymy02 * ymy02;

        double sum = 2 * va * ((vx0 * Dax0) + (vy0 * Day0) + (vA * DaA) + (vB * DaB) + (vC * DaC)) + // a diffs
                     vx0 * ((vx0 * Dx0x0) + 2 * ((vy0 * Dx0y0) + (vA * Dx0A) + (vB * Dx0B) + (vC * Dx0C))) + // x0 diffs
                     vy0 * ((vy0 * Dy0y0) + 2 * ((vA * Dy0A) + (vB * Dy0B) + (vC * Dy0C))) + // y0 diffs
                     vA * ((vA * DAA) + 2 * ((vB * DAB) + (vC * DAC))) + // A diffs
                     vB * ((vB * DBB) + 2 * (vC * DBC)) + // B diffs
                     vC * vC * DCC; // C diffs

        gsl_vector_set(fvv, i, sum);
    }

    return GSL_SUCCESS;
}
}  // namespace

CurveFitting::CurveFitting()
{
    // Constructor just initialises variables
    m_FirstSolverRun = true;
}

CurveFitting::CurveFitting(const QString &serialized)
{
    recreateFromQString(serialized);
}

void CurveFitting::fitCurve(const FittingGoal goal, const QVector<int> &x_, const QVector<double> &y_,
                            const QVector<double> &weight_, const QVector<bool> &outliers_,
                            const CurveFit curveFit, const bool useWeights, const OptimisationDirection optDir)
{
    if ((x_.size() != y_.size()) || (x_.size() != weight_.size()) || (x_.size() != outliers_.size()))
        qCDebug(KSTARS_EKOS_FOCUS) << QString("CurveFitting::fitCurve inconsistent parameters. x=%1, y=%2, weight=%3, outliers=%4")
                                   .arg(x_.size()).arg(y_.size()).arg(weight_.size()).arg(outliers_.size());

    m_x.clear();
    m_y.clear();
    m_scale.clear();
    for (int i = 0; i < x_.size(); ++i)
    {
        if (!outliers_[i])
        {
            m_x.push_back(static_cast<double>(x_[i]));
            m_y.push_back(y_[i]);
            m_scale.push_back(weight_[i]);
        }
    }

    m_useWeights = useWeights;
    m_CurveType = curveFit;

    switch (m_CurveType)
    {
        case FOCUS_QUADRATIC :
            m_coefficients = polynomial_fit(m_x.data(), m_y.data(), m_x.count(), 2);
            break;
        case FOCUS_HYPERBOLA :
            m_coefficients = hyperbola_fit(goal, m_x, m_y, m_scale, useWeights, optDir);
            break;
        case FOCUS_PARABOLA :
            m_coefficients = parabola_fit(goal, m_x, m_y, m_scale, useWeights, optDir);
            break;
        default :
            // Something went wrong, log an error and reset state so solver starts from scratch if called again
            qCDebug(KSTARS_EKOS_FOCUS) << QString("CurveFitting::fitCurve called with curveFit=%1").arg(curveFit);
            m_FirstSolverRun = true;
            return;
    }
    m_LastCoefficients = m_coefficients;
    m_LastCurveType    = m_CurveType;
    m_FirstSolverRun   = false;
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
    else
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Error: CurveFitting::f called with m_CurveType = %1 m_coefficients.size = %2")
                                   .arg(m_CurveType).arg(m_coefficients.size());

    return y;
}

double CurveFitting::f3D(double x, double y)
{
    double z = 0;
    if (m_CurveType == FOCUS_GAUSSIAN && m_coefficients.size() == NUM_GAUSSIAN_PARAMS)
        z = gaufxy(x, y, m_coefficients[A_IDX], m_coefficients[B_IDX], m_coefficients[C_IDX], m_coefficients[D_IDX],
                   m_coefficients[E_IDX], m_coefficients[F_IDX], m_coefficients[G_IDX]);
    else
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Error: CurveFitting::f3D called with m_CurveType = %1 m_coefficients.size = %2")
                                   .arg(m_CurveType).arg(m_coefficients.size());

    return z;
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

QVector<double> CurveFitting::hyperbola_fit(FittingGoal goal, const QVector<double> data_x, const QVector<double> data_y,
        const QVector<double> data_weights, const bool useWeights, const OptimisationDirection optDir)
{
    QVector<double> vc;
    DataPointT dataPoints;

    auto weights = gsl_vector_alloc(data_weights.size());
    // Fill in the data to which the curve will be fitted
    dataPoints.useWeights = useWeights;
    for (int i = 0; i < data_x.size(); i++)
        dataPoints.push_back(data_x[i], data_y[i], data_weights[i]);

    // Set the gsl error handler off as it aborts the program on error.
    auto const oldErrorHandler = gsl_set_error_handler_off();

    // Setup variables to be used by the solver
    gsl_multifit_nlinear_parameters params = gsl_multifit_nlinear_default_parameters();
    gsl_multifit_nlinear_workspace *w = gsl_multifit_nlinear_alloc(gsl_multifit_nlinear_trust, &params, data_x.size(),
                                        NUM_HYPERBOLA_PARAMS);
    gsl_multifit_nlinear_fdf fdf;
    gsl_vector *guess = gsl_vector_alloc(NUM_HYPERBOLA_PARAMS);
    int numIters;
    double xtol, gtol, ftol;

    // Fill in function info
    fdf.f = hypFx;
    fdf.df = hypJx;
    fdf.fvv = hypFxx;
    fdf.n = data_x.size();
    fdf.p = NUM_HYPERBOLA_PARAMS;
    fdf.params = &dataPoints;

    // This is the callback used by the LM solver to allow some introspection of each iteration
    // Useful for debugging but clogs up the log
    // To activate, uncomment the callback lambda and change the call to gsl_multifit_nlinear_driver
    /*auto callback = [](const size_t iter, void* _params, const auto * w)
    {
        gsl_vector *f = gsl_multifit_nlinear_residual(w);
        gsl_vector *x = gsl_multifit_nlinear_position(w);

        // compute reciprocal condition number of J(x)
        double rcond;
        gsl_multifit_nlinear_rcond(&rcond, w);

        // ratio of accel component to velocity component
        double avratio = gsl_multifit_nlinear_avratio(w);

        qCDebug(KSTARS_EKOS_FOCUS) << QString("iter %1: A=%2, B=%3, C=%4, D=%5 rcond(J)=%6, avratio=%7, |f(x)|=%8")
                                   .arg(iter)
                                   .arg(gsl_vector_get(x, A_IDX))
                                   .arg(gsl_vector_get(x, B_IDX))
                                   .arg(gsl_vector_get(x, C_IDX))
                                   .arg(gsl_vector_get(x, D_IDX))
                                   .arg(rcond)
                                   .arg(avratio)
                                   .arg(gsl_blas_dnrm2(f));
    };*/

    // Start a timer to see how long the solve takes.
    QElapsedTimer timer;
    timer.start();

    // We can sometimes have several attempts to solve based on "goal" and why the solver failed.
    // If the goal is STANDARD and we fail to solve then so be it. If the goal is BEST, then retry
    // with different parameters to really try and get a solution. A special case is if the solver
    // fails on its first step where we will always retry after adjusting parameters. It helps with
    // a situation where the solver gets "stuck" failing on first step repeatedly.
    for (int attempt = 0; attempt < 5; attempt++)
    {
        // Make initial guesses
        hypMakeGuess(attempt, data_x, data_y, optDir, guess);

        // Load up the weights and guess vectors
        if (useWeights)
        {
            for (int i = 0; i < data_weights.size(); i++)
                gsl_vector_set(weights, i, data_weights[i]);
            gsl_multifit_nlinear_winit(guess, weights, &fdf, w);
        }
        else
            gsl_multifit_nlinear_init(guess, &fdf, w);

        // Tweak solver parameters from default values
        hypSetupParams(goal, &params, &numIters, &xtol, &gtol, &ftol);

        qCDebug(KSTARS_EKOS_FOCUS) << QString("Starting LM solver, fit=hyperbola, solver=%1, scale=%2, trs=%3, iters=%4, xtol=%5,"
                                              "gtol=%6, ftol=%7")
                                   .arg(params.solver->name).arg(params.scale->name).arg(params.trs->name).arg(numIters)
                                   .arg(xtol).arg(gtol).arg(ftol);

        int info = 0;
        int status = gsl_multifit_nlinear_driver(numIters, xtol, gtol, ftol, NULL, NULL, &info, w);

        if (status != 0)
        {
            // Solver failed so determine whether a retry is required.
            bool retry = false;
            if (goal == BEST)
            {
                // Pull out all the stops to get a solution
                retry = true;
                goal = BEST_RETRY;
            }
            else if (status == GSL_EMAXITER && info == GSL_ENOPROG && gsl_multifit_nlinear_niter(w) <= 1)
                // This is a special case where the solver can't take a first step
                // So, perturb the initial conditions and have another go.
                retry = true;

            qCDebug(KSTARS_EKOS_FOCUS) <<
                                       QString("LM solver (Hyperbola): Failed after %1ms iters=%2 [attempt=%3] with status=%4 [%5] and info=%6 [%7], retry=%8")
                                       .arg(timer.elapsed()).arg(gsl_multifit_nlinear_niter(w)).arg(attempt + 1).arg(status).arg(gsl_strerror(status))
                                       .arg(info).arg(gsl_strerror(info)).arg(retry);
            if (!retry)
                break;
        }
        else
        {
            // All good so store the results - parameters A, B, C and D
            auto solution = gsl_multifit_nlinear_position(w);
            for (int j = 0; j < NUM_HYPERBOLA_PARAMS; j++)
                vc.push_back(gsl_vector_get(solution, j));

            qCDebug(KSTARS_EKOS_FOCUS) <<
                                       QString("LM Solver (Hyperbola): Solution found after %1ms %2 iters (%3). A=%4, B=%5, C=%6, D=%7")
                                       .arg(timer.elapsed()).arg(gsl_multifit_nlinear_niter(w)).arg(getLMReasonCode(info)).arg(vc[A_IDX]).arg(vc[B_IDX])
                                       .arg(vc[C_IDX]).arg(vc[D_IDX]);
            break;
        }
    }

    // Free GSL memory
    gsl_multifit_nlinear_free(w);
    gsl_vector_free(guess);
    gsl_vector_free(weights);

    // Restore old GSL error handler
    gsl_set_error_handler(oldErrorHandler);

    return vc;
}

QString CurveFitting::getLMReasonCode(int info)
{
    QString reason;

    if (info == 1)
        reason = QString("small step size");
    else if(info == 2)
        reason = QString("small gradient");
    else
        reason = QString("unknown reason");

    return reason;
}

// Setup the parameters for hyperbola curve fitting
void CurveFitting::hypSetupParams(FittingGoal goal, gsl_multifit_nlinear_parameters *params, int *numIters, double *xtol,
                                  double *gtol, double *ftol)
{
    // Trust region subproblem
    // - gsl_multifit_nlinear_trs_lm is the Levenberg-Marquardt algorithm without acceleration
    // - gsl_multifit_nlinear_trs_lmaccel is the Levenberg-Marquardt algorithm with acceleration
    // - gsl_multilarge_nlinear_trs_dogleg is the dogleg algorithm
    // - gsl_multifit_nlinear_trs_ddogleg is the double dogleg algorithm
    // - gsl_multifit_nlinear_trs_subspace2D is the 2D subspace algorithm
    params->trs = gsl_multifit_nlinear_trs_lmaccel;

    // Scale
    // - gsl_multifit_nlinear_scale_more uses. More strategy. Good for problems with parameters with widely different scales
    // - gsl_multifit_nlinear_scale_levenberg. Levensberg strategy. Can be good but not scale invariant.
    // - gsl_multifit_nlinear_scale_marquardt. Marquardt strategy. Considered inferior to More and Levensberg
    params->scale = gsl_multifit_nlinear_scale_more;

    // Solver
    // - gsl_multifit_nlinear_solver_qr produces reliable results but needs more iterations than Cholesky
    // - gsl_multifit_nlinear_solver_cholesky fast but not as stable as QR
    // - gsl_multifit_nlinear_solver_mcholesky modified Cholesky more stable than Cholesky

    // avmax is the max allowed ratio of 2nd order term (acceleration, a) to the 1st order term (velocity, v)
    // GSL defaults to 0.75, but suggests reducing it in the case of convergence problems
    switch (goal)
    {
        case STANDARD:
            params->solver = gsl_multifit_nlinear_solver_qr;
            params->avmax = 0.75;

            *numIters = MAX_ITERATIONS_CURVE;
            *xtol = INEPSXTOL;
            *gtol = INEPSGTOL;
            *ftol = INEPSFTOL;
            break;
        case BEST:
            params->solver = gsl_multifit_nlinear_solver_cholesky;
            params->avmax = 0.75;

            *numIters = MAX_ITERATIONS_CURVE * 2.0;
            *xtol = INEPSXTOL / 10.0;
            *gtol = INEPSGTOL / 10.0;
            *ftol = INEPSFTOL / 10.0;
            break;
        case BEST_RETRY:
            params->solver = gsl_multifit_nlinear_solver_qr;
            params->avmax = 0.5;

            *numIters = MAX_ITERATIONS_CURVE * 2.0;
            *xtol = INEPSXTOL;
            *gtol = INEPSGTOL;
            *ftol = INEPSFTOL;
            break;
        default:
            break;
    }
}

// Initialise parameters before starting the solver. Its important to start with a guess as near to the solution as possible
// If we found a solution before and we're just adding more datapoints use the last solution as the guess.
// If we don't have a solution use the datapoints to approximate. Work out the min and max points and use these
// to find "close" values for the starting point parameters
void CurveFitting::hypMakeGuess(const int attempt, const QVector<double> inX, const QVector<double> inY,
                                const OptimisationDirection optDir, gsl_vector * guess)
{
    if (inX.size() < 1 || inX.size() != inY.size())
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Bad call to hypMakeGuess: inX.size=%1, inY.size=%2").arg(inX.size()).arg(inY.size());
        return;
    }

    // If we are retrying then perturb the initial conditions. The hope is that by doing this the solver
    // will be nudged to find a solution this time
    double perturbation = 1.0 + pow(-1, attempt) * (attempt * 0.1);

    if (!m_FirstSolverRun && (m_LastCurveType == FOCUS_HYPERBOLA) && (m_LastCoefficients.size() == NUM_HYPERBOLA_PARAMS))
    {
        // Last run of the solver was a Hyperbola and the solution was good, so use that solution
        gsl_vector_set(guess, A_IDX, m_LastCoefficients[A_IDX] * perturbation);
        gsl_vector_set(guess, B_IDX, m_LastCoefficients[B_IDX] * perturbation);
        gsl_vector_set(guess, C_IDX, m_LastCoefficients[C_IDX] * perturbation);
        gsl_vector_set(guess, D_IDX, m_LastCoefficients[D_IDX] * perturbation);
    }
    else
    {
        double minX = inX[0];
        double minY = inY[0];
        double maxX = minX;
        double maxY = minY;
        for(int i = 0; i < inX.size(); i++)
        {
            if (inY[i] <= 0.0)
                continue;
            if(minY <= 0.0 || inY[i] < minY)
            {
                minX = inX[i];
                minY = inY[i];
            }
            if(maxY <= 0.0 || inY[i] > maxY)
            {
                maxX = inX[i];
                maxY = inY[i];
            }
        }
        double A, B, C, D;
        if (optDir == OPTIMISATION_MAXIMISE)
        {
            // Hyperbola equation: y = f(x) = b * sqrt(1 + ((x - c) / a) ^2) + d
            // For a maximum: c = maximum x = x(max)
            //                b < 0 and b + d > 0
            // For the array of data, set:
            // => c = x(max)
            // Now assume maximum x is near the real curve maximum, so y = b + d
            // Set b = -d/2. So y(max) = -d/2 + d = d/2.
            // => d = 2.y(max)
            // => b = -y(max)
            // Now look at the minimum y value in the array of datapoints
            // y(min) = b * sqrt(1 + ((x(min) - c) / a) ^2) + d
            // (y(min) - d) / b) ^ 2) = 1 + ((x(min) - c) / a) ^2
            // sqrt((((y(min) - d) / b) ^ 2) - 1) = (x(min) - c) / a
            // a = (x(min) - c) / sqrt((((y(min) - d) / b) ^ 2) - 1)
            // use the values for b, c, d obtained above to get:
            // => a = (x(min) - x(max)) / sqrt((((y(min) - (2.y(max)) / (-y(max))) ^ 2) - 1)
            double num = minX - maxX;
            double denom = sqrt(pow((2.0 * maxY - minY) / maxY, 2.0) - 1.0);
            if(denom <= 0.0)
                denom = 1.0;
            A = num / denom * perturbation;
            B = -maxY * perturbation;
            C = maxX * perturbation;
            D = 2.0 * maxY * perturbation;
        }
        else
        {
            // For a minimum: c = minimum x; b > 0 and b + d > 0
            // For the array of data, set:
            // => c = x(min)
            // Now assume minimum x is near the real curve minimum, so y = b + d
            // => Set b = d = y(min) / 2
            // Now look at the maximum y value in the array of datapoints
            // y(max) = b * sqrt(1 + ((x(max) - c) / a) ^2) + d
            // ((y(max) - d) / b) ^2 = 1 + ((x(max) - c) / a) ^2
            // a = (x(max) - c) / sqrt((((y(max) - d) / b) ^ 2) - 1)
            // use the values for b, c, d obtained above to get:
            // a = (x(max) - x(min)) / sqrt((((y(max) - (y(min) / 2)) / (y(min) / 2)) ^ 2) - 1)
            double minYdiv2 = (minY / 2.0 <= 0.0) ? 1.0 : minY / 2.0;
            double num = maxX - minX;
            double denom = sqrt(pow((maxY - minYdiv2) / minYdiv2, 2.0) - 1.0);
            if(denom <= 0.0)
                denom = 1.0;
            A = num / denom * perturbation;
            B = minYdiv2 * perturbation;
            C = minX * perturbation;
            D = B;
        }
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LM solver (hyp) initial params: perturbation=%1, A=%2, B=%3, C=%4, D=%5")
                                   .arg(perturbation).arg(A).arg(B).arg(C).arg(D);

        gsl_vector_set(guess, A_IDX, A);
        gsl_vector_set(guess, B_IDX, B);
        gsl_vector_set(guess, C_IDX, C);
        gsl_vector_set(guess, D_IDX, D);
    }
}

QVector<double> CurveFitting::parabola_fit(FittingGoal goal, const QVector<double> data_x, const QVector<double> data_y,
        const QVector<double> data_weights, bool useWeights, const OptimisationDirection optDir)
{
    QVector<double> vc;
    DataPointT dataPoints;

    auto weights = gsl_vector_alloc(data_weights.size());
    // Fill in the data to which the curve will be fitted
    dataPoints.useWeights = useWeights;
    for (int i = 0; i < data_x.size(); i++)
        dataPoints.push_back(data_x[i], data_y[i], data_weights[i]);

    // Set the gsl error handler off as it aborts the program on error.
    auto const oldErrorHandler = gsl_set_error_handler_off();

    // Setup variables to be used by the solver
    gsl_multifit_nlinear_parameters params = gsl_multifit_nlinear_default_parameters();
    gsl_multifit_nlinear_workspace* w = gsl_multifit_nlinear_alloc (gsl_multifit_nlinear_trust, &params, data_x.size(),
                                        NUM_PARABOLA_PARAMS);
    gsl_multifit_nlinear_fdf fdf;
    gsl_vector * guess = gsl_vector_alloc(NUM_PARABOLA_PARAMS);
    int numIters;
    double xtol, gtol, ftol;

    // Fill in function info
    fdf.f = parFx;
    fdf.df = parJx;
    fdf.fvv = parFxx;
    fdf.n = data_x.size();
    fdf.p = NUM_PARABOLA_PARAMS;
    fdf.params = &dataPoints;

    // This is the callback used by the LM solver to allow some introspection of each iteration
    // Useful for debugging but clogs up the log
    // To activate, uncomment the callback lambda and change the call to gsl_multifit_nlinear_driver
    /*auto callback = [](const size_t iter, void* _params, const auto * w)
    {
        gsl_vector *f = gsl_multifit_nlinear_residual(w);
        gsl_vector *x = gsl_multifit_nlinear_position(w);

        // compute reciprocal condition number of J(x)
        double rcond;
        gsl_multifit_nlinear_rcond(&rcond, w);

        // ratio of accel component to velocity component
        double avratio = gsl_multifit_nlinear_avratio(w);

        qCDebug(KSTARS_EKOS_FOCUS) << QString("iter %1: A=%2, B=%3, C=%4, rcond(J)=%5, avratio=%6 |f(x)|=%7")
                                   .arg(iter)
                                   .arg(gsl_vector_get(x, A_IDX))
                                   .arg(gsl_vector_get(x, B_IDX))
                                   .arg(gsl_vector_get(x, C_IDX))
                                   .arg(rcond)
                                   .arg(gsl_blas_dnrm2(f));
    };*/

    // Start a timer to see how long the solve takes.
    QElapsedTimer timer;
    timer.start();

    // We can sometimes have several attempts to solve based on "goal" and why the solver failed.
    // If the goal is STANDARD and we fail to solve then so be it. If the goal is BEST, then retry
    // with different parameters to really try and get a solution. A special case is if the solver
    // fails on its first step where we will always retry after adjusting parameters. It helps with
    // a situation where the solver gets "stuck" failing on first step repeatedly.
    for (int attempt = 0; attempt < 5; attempt++)
    {
        // Make initial guesses - here we just set all parameters to 1.0
        parMakeGuess(attempt, data_x, data_y, optDir, guess);

        // Load up the weights and guess vectors
        if (useWeights)
        {
            for (int i = 0; i < data_weights.size(); i++)
                gsl_vector_set(weights, i, data_weights[i]);
            gsl_multifit_nlinear_winit(guess, weights, &fdf, w);
        }
        else
            gsl_multifit_nlinear_init(guess, &fdf, w);

        // Tweak solver parameters from default values
        parSetupParams(goal, &params, &numIters, &xtol, &gtol, &ftol);

        qCDebug(KSTARS_EKOS_FOCUS) << QString("Starting LM solver, fit=parabola, solver=%1, scale=%2, trs=%3, iters=%4, xtol=%5,"
                                              "gtol=%6, ftol=%7")
                                   .arg(params.solver->name).arg(params.scale->name).arg(params.trs->name).arg(numIters)
                                   .arg(xtol).arg(gtol).arg(ftol);

        int info = 0;
        int status = gsl_multifit_nlinear_driver(numIters, xtol, gtol, ftol, NULL, NULL, &info, w);

        if (status != 0)
        {
            // Solver failed so determine whether a retry is required.
            bool retry = false;
            if (goal == BEST)
            {
                // Pull out all the stops to get a solution
                retry = true;
                goal = BEST_RETRY;
            }
            else if (status == GSL_EMAXITER && info == GSL_ENOPROG && gsl_multifit_nlinear_niter(w) <= 1)
                // This is a special case where the solver can't take a first step
                // So, perturb the initial conditions and have another go.
                retry = true;

            qCDebug(KSTARS_EKOS_FOCUS) <<
                                       QString("LM solver (Parabola): Failed after %1ms iters=%2 [attempt=%3] with status=%4 [%5] and info=%6 [%7], retry=%8")
                                       .arg(timer.elapsed()).arg(gsl_multifit_nlinear_niter(w)).arg(attempt + 1).arg(status).arg(gsl_strerror(status))
                                       .arg(info).arg(gsl_strerror(info)).arg(retry);
            if (!retry)
                break;
        }
        else
        {
            // All good so store the results - parameters A, B, and C
            auto solution = gsl_multifit_nlinear_position(w);
            for (int j = 0; j < NUM_PARABOLA_PARAMS; j++)
                vc.push_back(gsl_vector_get(solution, j));

            qCDebug(KSTARS_EKOS_FOCUS) << QString("LM Solver (Parabola): Solution found after %1ms %2 iters (%3). A=%4, B=%5, C=%6")
                                       .arg(timer.elapsed()).arg(gsl_multifit_nlinear_niter(w)).arg(getLMReasonCode(info)).arg(vc[A_IDX]).arg(vc[B_IDX])
                                       .arg(vc[C_IDX]);
            break;
        }
    }

    // Free GSL memory
    gsl_multifit_nlinear_free(w);
    gsl_vector_free(guess);
    gsl_vector_free(weights);

    // Restore old GSL error handler
    gsl_set_error_handler(oldErrorHandler);

    return vc;
}

// Setup the parameters for parabola curve fitting
void CurveFitting::parSetupParams(FittingGoal goal, gsl_multifit_nlinear_parameters *params, int *numIters, double *xtol,
                                  double *gtol, double *ftol)
{
    // Trust region subproblem
    // - gsl_multifit_nlinear_trs_lm is the Levenberg-Marquardt algorithm without acceleration
    // - gsl_multifit_nlinear_trs_lmaccel is the Levenberg-Marquardt algorithm with acceleration
    // - gsl_multilarge_nlinear_trs_dogleg is the dogleg algorithm
    // - gsl_multifit_nlinear_trs_ddogleg is the double dogleg algorithm
    // - gsl_multifit_nlinear_trs_subspace2D is the 2D subspace algorithm
    params->trs = gsl_multifit_nlinear_trs_lmaccel;

    // Scale
    // - gsl_multifit_nlinear_scale_more uses. More strategy. Good for problems with parameters with widely different scales
    // - gsl_multifit_nlinear_scale_levenberg. Levensberg strategy. Can be good but not scale invariant.
    // - gsl_multifit_nlinear_scale_marquardt. Marquardt strategy. Considered inferior to More and Levensberg
    params->scale = gsl_multifit_nlinear_scale_more;

    // Solver
    // - gsl_multifit_nlinear_solver_qr produces reliable results but needs more iterations than Cholesky
    // - gsl_multifit_nlinear_solver_cholesky fast but not as stable as QR
    // - gsl_multifit_nlinear_solver_mcholesky modified Cholesky more stable than Cholesky

    // avmax is the max allowed ratio of 2nd order term (acceleration, a) to the 1st order term (velocity, v)
    // GSL defaults to 0.75, but suggests reducing it in the case of convergence problems
    switch (goal)
    {
        case STANDARD:
            params->solver = gsl_multifit_nlinear_solver_cholesky;
            params->avmax = 0.75;

            *numIters = MAX_ITERATIONS_CURVE;
            *xtol = INEPSXTOL;
            *gtol = INEPSGTOL;
            *ftol = INEPSFTOL;
            break;
        case BEST:
            params->solver = gsl_multifit_nlinear_solver_cholesky;
            params->avmax = 0.75;

            *numIters = MAX_ITERATIONS_CURVE * 2.0;
            *xtol = INEPSXTOL / 10.0;
            *gtol = INEPSGTOL / 10.0;
            *ftol = INEPSFTOL / 10.0;
            break;
        case BEST_RETRY:
            params->solver = gsl_multifit_nlinear_solver_qr;
            params->avmax = 0.5;

            *numIters = MAX_ITERATIONS_CURVE * 2.0;
            *xtol = INEPSXTOL;
            *gtol = INEPSGTOL;
            *ftol = INEPSFTOL;
            break;
        default:
            break;
    }
}

// Initialise parameters before starting the solver. Its important to start with a guess as near to the solution as possible
// If we found a solution before and we're just adding more datapoints use the last solution as the guess.
// If we don't have a solution use the datapoints to approximate. Work out the min and max points and use these
// to find "close" values for the starting point parameters
void CurveFitting::parMakeGuess(const int attempt, const QVector<double> inX, const QVector<double> inY,
                                const OptimisationDirection optDir, gsl_vector * guess)
{
    if (inX.size() < 1 || inX.size() != inY.size())
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Bad call to parMakeGuess: inX.size=%1, inY.size=%2").arg(inX.size()).arg(inY.size());
        return;
    }

    // If we are retrying then perturb the initial conditions. The hope is that by doing this the solver
    // will be nudged to find a solution this time
    double perturbation = 1.0 + pow(-1, attempt) * (attempt * 0.1);

    if (!m_FirstSolverRun && (m_LastCurveType == FOCUS_PARABOLA) && (m_LastCoefficients.size() == NUM_PARABOLA_PARAMS))
    {
        // Last run of the solver was a Parabola and that solution was good, so use that solution
        gsl_vector_set(guess, A_IDX, m_LastCoefficients[A_IDX] * perturbation);
        gsl_vector_set(guess, B_IDX, m_LastCoefficients[B_IDX] * perturbation);
        gsl_vector_set(guess, C_IDX, m_LastCoefficients[C_IDX] * perturbation);
    }
    else
    {
        double minX = inX[0];
        double minY = inY[0];
        double maxX = minX;
        double maxY = minY;
        for(int i = 0; i < inX.size(); i++)
        {
            if (inY[i] <= 0.0)
                continue;
            if(minY <= 0.0 || inY[i] < minY)
            {
                minX = inX[i];
                minY = inY[i];
            }
            if(maxY <= 0.0 || inY[i] > maxY)
            {
                maxX = inX[i];
                maxY = inY[i];
            }
        }
        double A, B, C;
        if (optDir == OPTIMISATION_MAXIMISE)
        {
            // Equation y = f(x) = a + b((x - c) ^2)
            // For a maximum b < 0 and a > 0
            // At the maximum, Xmax = c, Ymax = a
            // Far from the maximum, b = (Ymin - a) / ((Xmin - c) ^2)
            A = maxY * perturbation;
            B = ((minY - maxY) / pow(minX - maxX, 2.0)) * perturbation;
            C = maxX * perturbation;
        }
        else
        {
            // For a minimum b > 0 and a > 0
            // At the minimum, Xmin = c, Ymin = a
            // Far from the minimum, b = (Ymax - a) / ((Xmax - c) ^2)
            A = minY * perturbation;
            B = ((maxY - minY) / pow(maxX - minX, 2.0)) * perturbation;
            C = minX * perturbation;
        }
        qCDebug(KSTARS_EKOS_FOCUS) << QString("LM solver (par) initial params: perturbation=%1, A=%2, B=%3, C=%4")
                                   .arg(perturbation).arg(A).arg(B).arg(C);

        gsl_vector_set(guess, A_IDX, A);
        gsl_vector_set(guess, B_IDX, B);
        gsl_vector_set(guess, C_IDX, C);
    }
}

QVector<double> CurveFitting::gaussian_fit(DataPoint3DT data, const StarParams &starParams)
{
    QVector<double> vc;

    // Set the gsl error handler off as it aborts the program on error.
    auto const oldErrorHandler = gsl_set_error_handler_off();

    // Setup variables to be used by the solver
    gsl_multifit_nlinear_parameters params = gsl_multifit_nlinear_default_parameters();
    gsl_multifit_nlinear_workspace* w = gsl_multifit_nlinear_alloc (gsl_multifit_nlinear_trust, &params, data.dps.size(),
                                        NUM_GAUSSIAN_PARAMS);
    gsl_multifit_nlinear_fdf fdf;
    int numIters;
    double xtol, gtol, ftol;

    // Fill in function info
    fdf.f = gauFxy;
    fdf.df = gauJxy;
    fdf.fvv = gauFxyxy;
    fdf.n = data.dps.size();
    fdf.p = NUM_GAUSSIAN_PARAMS;
    fdf.params = &data;

    // Allocate the guess vector
    gsl_vector * guess = gsl_vector_alloc(NUM_GAUSSIAN_PARAMS);
    // Allocate weights vector
    auto weights = gsl_vector_alloc(data.dps.size());

    // Setup a timer to see how long the solve takes
    QElapsedTimer timer;
    timer.start();

    // Setup for multiple solve attempts. We won't worry too much if the solver fails as there should be
    // plenty of stars, but if the solver fails on its first step then adjust parameters and retry as this
    // is a fast thing to do.
    for (int attempt = 0; attempt < 5; attempt++)
    {
        // Make initial guesses
        gauMakeGuess(attempt, starParams, guess);

        // If using weights load up the GSL vector
        if (data.useWeights)
        {
            QVectorIterator<DataPT3D> dp(data.dps);
            int i = 0;
            while (dp.hasNext())
                gsl_vector_set(weights, i++, dp.next().weight);

            gsl_multifit_nlinear_winit(guess, weights, &fdf, w);
        }
        else
            gsl_multifit_nlinear_init(guess, &fdf, w);

        // This is the callback used by the LM solver to allow some introspection of each iteration
        // Useful for debugging but clogs up the log
        // To activate, uncomment the callback lambda and change the call to gsl_multifit_nlinear_driver
        /*auto callback = [](const size_t iter, void* _params, const gsl_multifit_nlinear_workspace * w)
        {
            gsl_vector *f = gsl_multifit_nlinear_residual(w);
            gsl_vector *x = gsl_multifit_nlinear_position(w);
            double rcond;

            // compute reciprocal condition number of J(x)
            gsl_multifit_nlinear_rcond(&rcond, w);

            // ratio of accel component to velocity component
            double avratio = gsl_multifit_nlinear_avratio(w);

            qCDebug(KSTARS_EKOS_FOCUS) <<
                                   QString("iter %1: A = %2, B = %3, C = %4, D = %5, E = %6, F = %7, G = %8 "
                                           "rcond(J) = %9, avratio = %10, |f(x)| = %11")
                                   .arg(iter)
                                   .arg(gsl_vector_get(x, A_IDX))
                                   .arg(gsl_vector_get(x, B_IDX))
                                   .arg(gsl_vector_get(x, C_IDX))
                                   .arg(gsl_vector_get(x, D_IDX))
                                   .arg(gsl_vector_get(x, E_IDX))
                                   .arg(gsl_vector_get(x, F_IDX))
                                   .arg(gsl_vector_get(x, G_IDX))
                                   //.arg(1.0 / rcond)
                                   .arg(rcond)
                                   .arg(avratio)
                                   .arg(gsl_blas_dnrm2(f));
        };*/

        gauSetupParams(&params, &numIters, &xtol, &gtol, &ftol);

        qCDebug(KSTARS_EKOS_FOCUS) << QString("Starting LM solver, fit=gaussian, solver=%1, scale=%2, trs=%3, iters=%4, xtol=%5,"
                                              "gtol=%6, ftol=%7")
                                   .arg(params.solver->name).arg(params.scale->name).arg(params.trs->name).arg(numIters)
                                   .arg(xtol).arg(gtol).arg(ftol);

        int info = 0;
        int status = gsl_multifit_nlinear_driver(numIters, xtol, gtol, ftol, NULL, NULL, &info, w);

        if (status != 0)
        {
            qCDebug(KSTARS_EKOS_FOCUS) <<
                                       QString("LM solver (Gaussian): Failed after %1ms iters=%2 [attempt=%3] with status=%4 [%5] and info=%6 [%7]")
                                       .arg(timer.elapsed()).arg(gsl_multifit_nlinear_niter(w)).arg(attempt + 1).arg(status).arg(gsl_strerror(status))
                                       .arg(info).arg(gsl_strerror(info));
            if (status != GSL_EMAXITER || info != GSL_ENOPROG || gsl_multifit_nlinear_niter(w) > 1)
                break;
        }
        else
        {
            // All good so store the results - parameters A, B, C, D, E, F, G
            auto solution = gsl_multifit_nlinear_position(w);
            for (int j = 0; j < NUM_GAUSSIAN_PARAMS; j++)
                vc.push_back(gsl_vector_get(solution, j));

            qCDebug(KSTARS_EKOS_FOCUS) << QString("LM Solver (Gaussian): Solution found after %1ms %2 iters (%3). A=%4, B=%5, C=%6, "
                                                  "D=%7, E=%8, F=%9, G=%10").arg(timer.elapsed()).arg(gsl_multifit_nlinear_niter(w)).arg(getLMReasonCode(info))
                                       .arg(vc[A_IDX]).arg(vc[B_IDX]).arg(vc[C_IDX]).arg(vc[D_IDX]).arg(vc[E_IDX])
                                       .arg(vc[F_IDX]).arg(vc[G_IDX]);
            break;
        }
    }

    // Free GSL memory
    gsl_multifit_nlinear_free(w);
    gsl_vector_free(guess);
    gsl_vector_free(weights);

    // Restore old GSL error handler
    gsl_set_error_handler(oldErrorHandler);

    return vc;
}

// Initialise parameters before starting the solver. Its important to start with a guess as near to the solution as possible
// Since we have already run some HFR calcs on the star, use these values to calculate the guess
void CurveFitting::gauMakeGuess(const int attempt, const StarParams &starParams, gsl_vector * guess)
{
    // If we are retrying then perturb the initial conditions. The hope is that by doing this the solver
    // will be nudged to find a solution this time
    double perturbation = 1.0 + pow(-1, attempt) * (attempt * 0.1);

    // Default from the input star details
    const double a  = std::max(starParams.peak, 0.0) * perturbation;       // Peak value
    const double x0 = std::max(starParams.centroid_x, 0.0) * perturbation; // Centroid x
    const double y0 = std::max(starParams.centroid_y, 0.0) * perturbation; // Centroid y
    const double b  = std::max(starParams.background, 0.0) * perturbation; // Background

    double A = 1.0, B = 0.0, C = 1.0;
    if (starParams.HFR > 0.0)
    {
        // Use 2*HFR value as FWHM along with theta to calc A, B, C
        // FWHM = 2.sqrt(2.ln(2)).sigma
        // Assume circular symmetry so B = 0
        const double sigma2 = pow(starParams.HFR / (sqrt(2.0 * log(2.0))), 2.0);
        const double costheta2 = pow(cos(starParams.theta), 2.0);
        const double sintheta2 = pow(sin(starParams.theta), 2.0);

        A = C = (costheta2 + sintheta2) / (2 * sigma2) * perturbation;
    }

    qCDebug(KSTARS_EKOS_FOCUS) <<
                               QString("LM Solver (Gaussian): Guess perturbation=%1, A=%2, B=%3, C=%4, D=%5, E=%6, F=%7, G=%8")
                               .arg(perturbation).arg(a).arg(x0).arg(y0).arg(A).arg(B).arg(C).arg(b);

    gsl_vector_set(guess, A_IDX, a);
    gsl_vector_set(guess, B_IDX, x0);
    gsl_vector_set(guess, C_IDX, y0);
    gsl_vector_set(guess, D_IDX, A);
    gsl_vector_set(guess, E_IDX, B);
    gsl_vector_set(guess, F_IDX, C);
    gsl_vector_set(guess, G_IDX, b);
}

// Setup the parameters for gaussian curve fitting
void CurveFitting::gauSetupParams(gsl_multifit_nlinear_parameters *params, int *numIters, double *xtol, double *gtol,
                                  double *ftol)
{
    // Trust region subproblem
    // - gsl_multifit_nlinear_trs_lm is the Levenberg-Marquardt algorithm without acceleration
    // - gsl_multifit_nlinear_trs_lmaccel is the Levenberg-Marquardt algorithm with acceleration
    // - gsl_multilarge_nlinear_trs_dogleg is the dogleg algorithm
    // - gsl_multifit_nlinear_trs_ddogleg is the double dogleg algorithm
    // - gsl_multifit_nlinear_trs_subspace2D is the 2D subspace algorithm
    params->trs = gsl_multifit_nlinear_trs_lmaccel;

    // avmax is the max allowed ratio of 2nd order term (acceleration, a) to the 1st order term (velocity, v)
    // GSL defaults to 0.75
    params->avmax = 0.75;

    // Scale
    // - gsl_multifit_nlinear_scale_more uses. More strategy. Good for problems with parameters with widely different scales
    // - gsl_multifit_nlinear_scale_levenberg. Levensberg strategy. Can be good but not scale invariant.
    // - gsl_multifit_nlinear_scale_marquardt. Marquardt strategy. Considered inferior to More and Levensberg
    params->scale = gsl_multifit_nlinear_scale_more;

    // Solver
    // - gsl_multifit_nlinear_solver_qr produces reliable results but needs more iterations than Cholesky
    // - gsl_multifit_nlinear_solver_cholesky fast but not as stable as QR
    // - gsl_multifit_nlinear_solver_mcholesky modified Cholesky more stable than Cholesky
    params->solver = gsl_multifit_nlinear_solver_qr;

    *numIters = MAX_ITERATIONS_STARS;
    *xtol = 1e-5;
    *gtol = INEPSGTOL;
    *ftol = 1e-5;
}

bool CurveFitting::findMinMax(double expected, double minPosition, double maxPosition, double *position, double *value,
                              CurveFit curveFit, const OptimisationDirection optDir)
{
    bool foundFit;
    switch (curveFit)
    {
        case FOCUS_QUADRATIC :
            foundFit = minimumQuadratic(expected, minPosition, maxPosition, position, value);
            break;
        case FOCUS_HYPERBOLA :
            foundFit = minMaxHyperbola(expected, minPosition, maxPosition, position, value, optDir);
            break;
        case FOCUS_PARABOLA :
            foundFit = minMaxParabola(expected, minPosition, maxPosition, position, value, optDir);
            break;
        default :
            foundFit = false;
            break;
    }
    if (!foundFit)
        // If we couldn't fit a curve then something's wrong so reset coefficients which will force the next run of the LM solver to start from scratch
        m_LastCoefficients.clear();
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

bool CurveFitting::minMaxHyperbola(double expected, double minPosition, double maxPosition, double *position,
                                   double *value, const OptimisationDirection optDir)
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
    // Check 3: For a minimum: we have a "u" shaped curve, not an "n" shape. b > 0.
    //                maximum: we have an "n" shaped curve, not a "U" shape. b < 0;
    if ((c >= minPosition) && (c <= maxPosition) && (b + d > 0.0) &&
            ((optDir == OPTIMISATION_MINIMISE && b > 0.0) || (optDir == OPTIMISATION_MAXIMISE && b < 0.0)))
    {
        *position = c;
        *value = b + d;
        return true;
    }
    else
        return false;
}

bool CurveFitting::minMaxParabola(double expected, double minPosition, double maxPosition, double *position,
                                  double *value, const OptimisationDirection optDir)
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
    // Check 3: For a minimum: we have a "u" shaped curve, not an "n" shape. b > 0.
    //                maximum: we have an "n" shaped curve, not a "U" shape. b < 0;
    if ((c >= minPosition) && (c <= maxPosition) && (a > 0.0) &&
            ((optDir == OPTIMISATION_MINIMISE && b > 0.0) || (optDir == OPTIMISATION_MAXIMISE && b < 0.0)))
    {
        *position = c;
        *value = a;
        return true;
    }
    else
        return false;
}

bool CurveFitting::getStarParams(const CurveFit curveFit, StarParams *starParams)
{
    bool foundFit;
    switch (curveFit)
    {
        case FOCUS_GAUSSIAN :
            foundFit = getGaussianParams(starParams);
            break;
        default :
            foundFit = false;
            break;
    }
    if (!foundFit)
        // If we couldn't fit a curve then something's wrong so reset coefficients which will force the next run of the LM solver to start from scratch
        m_LastCoefficients.clear();
    return foundFit;
}

// Having solved for a Gaussian return params
bool CurveFitting::getGaussianParams(StarParams *starParams)
{
    if (m_coefficients.size() != NUM_GAUSSIAN_PARAMS)
    {
        if (m_coefficients.size() != 0)
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::getGaussianParams coefficients.size()=%1").arg(
                                           m_coefficients.size());
        return false;
    }

    const double a  = m_coefficients[A_IDX];
    const double x0 = m_coefficients[B_IDX];
    const double y0 = m_coefficients[C_IDX];
    const double A  = m_coefficients[D_IDX];
    const double B  = m_coefficients[E_IDX];
    const double C  = m_coefficients[F_IDX];
    const double b  = m_coefficients[G_IDX];

    // Sanity check the coefficients in case the solver produced a bad solution
    if (a <= 0.0 || b <= 0.0 || x0 <= 0.0 || y0 <= 0.0)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::getGaussianParams a=%1 b=%2 x0=%3 y0=%4").arg(a).arg(b)
                                   .arg(x0).arg(y0);
        return false;
    }

    const double AmC = A - C;
    double theta;
    abs(AmC) < 1e-10 ? theta = 0.0 : theta = 0.5 * atan(2 * B / AmC);
    const double costheta = cos(theta);
    const double costheta2 = costheta * costheta;
    const double sintheta = sin(theta);
    const double sintheta2 = sintheta * sintheta;

    double sigmax2 = 0.5 / ((A * costheta2) + (2 * B * costheta * sintheta) + (C * sintheta2));
    double sigmay2 = 0.5 / ((A * costheta2) - (2 * B * costheta * sintheta) + (C * sintheta2));

    double FWHMx = 2 * pow(2 * log(2) * sigmax2, 0.5);
    double FWHMy = 2 * pow(2 * log(2) * sigmay2, 0.5);
    double FWHM  = (FWHMx + FWHMy) / 2.0;

    if (FWHM < 0.0)
    {
        qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::getGaussianParams FWHM=%1").arg(FWHM);
        return false;
    }

    starParams->background = b;
    starParams->peak = a;
    starParams->centroid_x = x0;
    starParams->centroid_y = y0;
    starParams->theta = theta;
    starParams->FWHMx = FWHMx;
    starParams->FWHMy = FWHMy;
    starParams->FWHM = FWHM;

    return true;
}

// R2 (R squared) is the coefficient of determination gives a measure of how well the curve fits the datapoints.
// It lies between 0 and 1. 1 means that all datapoints will lie on the curve which therefore exactly describes the
// datapoints. 0 means that there is no correlation between the curve and the datapoints.
// See www.wikipedia.org/wiki/Coefficient_of_determination for more details.
double CurveFitting::calculateR2(CurveFit curveFit)
{
    double R2 = 0.0;
    QVector<double> dataPoints, curvePoints;
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
            R2 = calcR2(m_y, curvePoints, m_scale, m_useWeights);
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
            R2 = calcR2(m_y, curvePoints, m_scale, m_useWeights);
            break;

        case FOCUS_GAUSSIAN :
            // Calculate R2 for the gaussian
            if (m_coefficients.size() != NUM_GAUSSIAN_PARAMS)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateR2 gaussian coefficients.size()=").arg(
                                               m_coefficients.size());
                break;
            }

            for (i = 0; i < m_dataPoints.dps.size(); i++)
            {
                // Load up the curvePoints vector
                curvePoints.push_back(gaufxy(m_dataPoints.dps[i].x, m_dataPoints.dps[i].y, m_coefficients[A_IDX],
                                             m_coefficients[B_IDX], m_coefficients[C_IDX], m_coefficients[D_IDX],
                                             m_coefficients[E_IDX], m_coefficients[F_IDX], m_coefficients[G_IDX]));
                dataPoints.push_back(static_cast <double> (m_dataPoints.dps[i].z));
            }

            // Do the actual R2 calculation
            R2 = calcR2(dataPoints, curvePoints, m_scale, m_dataPoints.useWeights);
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
double CurveFitting::calcR2(const QVector<double> dataPoints, const QVector<double> curvePoints,
                            const QVector<double> scale, const bool useWeights)
{
    double R2 = 0.0, chisq = 0.0, sum = 0.0, totalSumSquares = 0.0, weight, average;

    for (int i = 0; i < dataPoints.size(); i++)
    {
        sum += dataPoints[i];
        useWeights ? weight = scale[i] : weight = 1.0;
        chisq += weight * pow((dataPoints[i] - curvePoints[i]), 2.0);
    }
    average = sum / dataPoints.size();

    for (int i = 0; i < dataPoints.size(); i++)
    {
        useWeights ? weight = scale[i] : weight = 1.0;
        totalSumSquares += weight * pow((dataPoints[i] - average), 2.0);
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

void CurveFitting::calculateCurveDeltas(CurveFit curveFit, std::vector<std::pair<int, double>> &curveDeltas)
{
    curveDeltas.clear();

    switch (curveFit)
    {
        case FOCUS_QUADRATIC :
            // Not implemented
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateCurveDeltas called for Quadratic");
            break;

        case FOCUS_HYPERBOLA :
            if (m_coefficients.size() != NUM_HYPERBOLA_PARAMS)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateCurveDeltas hyperbola coefficients.size()=").arg(
                                               m_coefficients.size());
                break;
            }

            for (int i = 0; i < m_y.size(); i++)
                curveDeltas.push_back(std::make_pair(i, abs(m_y[i] - hypfx(m_x[i], m_coefficients[A_IDX], m_coefficients[B_IDX],
                                                     m_coefficients[C_IDX],
                                                     m_coefficients[D_IDX]))));
            break;

        case FOCUS_PARABOLA :
            if (m_coefficients.size() != NUM_PARABOLA_PARAMS)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateCurveDeltas parabola coefficients.size()=").arg(
                                               m_coefficients.size());
                break;
            }

            for (int i = 0; i < m_y.size(); i++)
                curveDeltas.push_back(std::make_pair(i, abs(m_y[i] - parfx(m_x[i], m_coefficients[A_IDX], m_coefficients[B_IDX],
                                                     m_coefficients[C_IDX]))));
            break;

        case FOCUS_GAUSSIAN :
            // Not implemented
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateCurveDeltas called for Gaussian");
            break;

        default :
            qCDebug(KSTARS_EKOS_FOCUS) << QString("Error in CurveFitting::calculateCurveDeltas curveFit=%1").arg(curveFit);
            break;
    }
}

namespace
{
QString serializeDoubleVector(const QVector<double> &vector)
{
    QString str = QString("%1").arg(vector.size());
    for (const double v : vector)
        str.append(QString(";%1").arg(v, 0, 'g', 12));
    return str;
}

bool decodeDoubleVector(const QString serialized, QVector<double> *vector)
{
    vector->clear();
    const QStringList parts = serialized.split(';');
    if (parts.size() == 0) return false;
    bool ok;
    int size = parts[0].toInt(&ok);
    if (!ok || (size != parts.size() - 1)) return false;
    for (int i = 0; i < size; ++i)
    {
        const double val = parts[i + 1].toDouble(&ok);
        if (!ok) return false;
        vector->append(val);
    }
    return true;
}
}

QString CurveFitting::serialize() const
{
    QString serialized = "";
    if (m_FirstSolverRun) return serialized;
    serialized = QString("%1").arg(int(m_CurveType));
    serialized.append(QString("|%1").arg(serializeDoubleVector(m_x)));
    serialized.append(QString("|%1").arg(serializeDoubleVector(m_y)));
    serialized.append(QString("|%1").arg(serializeDoubleVector(m_scale)));
    serialized.append(QString("|%1").arg(m_useWeights ? "T" : "F"));
    // m_dataPoints not implemented. Not needed for graphing.
    serialized.append(QString("|m_dataPoints not implemented"));
    serialized.append(QString("|%1").arg(serializeDoubleVector(m_coefficients)));
    serialized.append(QString("|%1").arg(int(m_LastCurveType)));
    serialized.append(QString("|%1").arg(serializeDoubleVector(m_LastCoefficients)));
    return serialized;
}

bool CurveFitting::recreateFromQString(const QString &serialized)
{
    QStringList parts = serialized.split('|');
    bool ok = false;
    if (parts.size() != 9) return false;

    int val = parts[0].toInt(&ok);
    if (!ok) return false;
    m_CurveType = static_cast<CurveFit>(val);

    if (!decodeDoubleVector(parts[1], &m_x)) return false;
    if (!decodeDoubleVector(parts[2], &m_y)) return false;
    if (!decodeDoubleVector(parts[3], &m_scale)) return false;

    if (parts[4] == "T") m_useWeights = true;
    else if (parts[4] == "F") m_useWeights = false;
    else return false;

    // parts[5]: m_dataPoints not implemented.

    if (!decodeDoubleVector(parts[6], &m_coefficients)) return false;

    val = parts[7].toInt(&ok);
    if (!ok) return false;
    m_LastCurveType = static_cast<CurveFit>(val);

    if (!decodeDoubleVector(parts[8], &m_LastCoefficients)) return false;

    m_FirstSolverRun = false;
    return true;
}

}  // namespace

#include "polynomialfit.h"

#include "ekos/ekos.h"
#include <gsl/gsl_fit.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_min.h>
#include <ekos_focus_debug.h>

namespace Ekos
{

PolynomialFit::PolynomialFit(int degree_, const QVector<double>& x_, const QVector<double>& y_)
    : degree(degree_), x(x_), y(y_)
{
    Q_ASSERT(x_.size() == y_.size());
    solve(x, y);
}

PolynomialFit::PolynomialFit(int degree_, const QVector<int>& x_, const QVector<double>& y_)
    : degree(degree_), y(y_)
{
    Q_ASSERT(x_.size() == y_.size());
    for (int i = 0; i < x_.size(); ++i)
    {
        x.push_back(static_cast<double>(x_[i]));
    }
    solve(x, y);
}

void PolynomialFit::solve(const QVector<double>& x, const QVector<double>& y)
{
    double chisq = 0;
    coefficients = gsl_polynomial_fit(x.data(), y.data(), x.count(), degree, chisq);
}

double PolynomialFit::polynomialFunction(double x, void *params)
{
    PolynomialFit *instance = static_cast<PolynomialFit *>(params);

    if (instance && !instance->coefficients.empty())
        return instance->f(x);
    else
        return -1;
}

double PolynomialFit::f(double x)
{
    const int order = coefficients.size() - 1;
    double sum = 0;
    for (int i = 0; i <= order; ++i)
        sum += coefficients[i] * pow(x, i);
    return sum;
}

bool PolynomialFit::findMinimum(double expected, double minPosition, double maxPosition, double *position, double *value)
{
    int status;
    int iter = 0, max_iter = 100;
    const gsl_min_fminimizer_type *T;
    gsl_min_fminimizer *s;
    double m = expected;

    gsl_function F;
    F.function = &PolynomialFit::polynomialFunction;
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
            *value    = polynomialFunction(m, this);
        }
    }
    while (status == GSL_CONTINUE && iter < max_iter);

    gsl_min_fminimizer_free(s);
    return (status == GSL_SUCCESS);
}
}  // namespace


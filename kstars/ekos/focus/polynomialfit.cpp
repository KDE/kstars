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
    {
        const int order = instance->coefficients.size() - 1;
        double sum = 0;
        for (int i = 0; i <= order; ++i)
            sum += instance->coefficients[i] * pow(x, i);
        return sum;
    }
    return -1;
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

void PolynomialFit::drawPolynomial(QCustomPlot *plot, QCPGraph *graph)
{
    graph->data()->clear();
    QCPRange range = plot->xAxis->range();
    double interval = range.size() / 20.0;

    for(double x = range.lower ; x < range.upper ; x += interval)
    {
        double y = polynomialFunction(x, this);
        graph->addData(x, y);
    }
    plot->replot();
}

void PolynomialFit::drawMinimum(QCustomPlot *plot, QCPGraph *solutionGraph,
                              double solutionPosition, double solutionValue, const QFont& font)
{
    solutionGraph->data()->clear();
    solutionGraph->addData(solutionPosition, solutionValue);
    QCPItemText *textLabel = new QCPItemText(plot);
    textLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    textLabel->position->setType(QCPItemPosition::ptPlotCoords);
    textLabel->position->setCoords(solutionPosition, solutionValue / 2);
    textLabel->setColor(Qt::red);
    textLabel->setPadding(QMargins(0, 0, 0, 0));
    textLabel->setBrush(Qt::white);
    textLabel->setPen(Qt::NoPen);
    textLabel->setFont(QFont(font.family(), 8));
    textLabel->setText(QString::number(solutionPosition));
}
}  // namespace


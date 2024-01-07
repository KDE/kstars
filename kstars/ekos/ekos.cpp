/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos.h"

#include <gsl/gsl_fit.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_multifit.h>

#include <QDebug>

namespace Ekos
{
const QString getGuideStatusString(GuideState state, bool translated)
{
    return translated ? i18n(guideStates[state]) : guideStates[state];
}
const QString getCaptureStatusString(CaptureState state, bool translated)
{
    return translated ? i18n(captureStates[state]) : captureStates[state];
}
const QString getFocusStatusString(FocusState state, bool translated)
{
    return translated ? i18n(focusStates[state]) : focusStates[state];
}
const QString getAlignStatusString(AlignState state, bool translated)
{
    return translated ? i18n(alignStates[state]) : alignStates[state];
}
const QString getFilterStatusString(FilterState state, bool translated)
{
    return translated ? i18n(filterStates[state]) : filterStates[state];
}
const QString getSchedulerStatusString(SchedulerState state, bool translated)
{
    return translated ? i18n(schedulerStates[state]) : schedulerStates[state];
}

/* Taken from https://codereview.stackexchange.com/questions/71300/wrapper-function-to-do-polynomial-fits-with-gsl */
std::vector<double> gsl_polynomial_fit(const double *const data_x, const double *const data_y, const int n,
                                       const int order, double &chisq)
{
    int status = 0;
    std::vector<double> vc;
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
    {
        qDebug() << Q_FUNC_INFO << "GSL multifit error:" << gsl_strerror(status);
        return vc;
    }

    gsl_multifit_linear_free(work);

    for (int i = 0; i < order + 1; i++)
    {
        vc.push_back(gsl_vector_get(c, i));
    }

    gsl_vector_free(y);
    gsl_vector_free(c);
    gsl_matrix_free(X);
    gsl_matrix_free(cov);

    return vc;
}
}

QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::CommunicationStatus &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::CommunicationStatus &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<Ekos::CommunicationStatus>(a);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::CaptureState &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::CaptureState &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<Ekos::CaptureState>(a);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::FocusState &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::FocusState &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<Ekos::FocusState>(a);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::GuideState &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::GuideState &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<Ekos::GuideState>(a);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::AlignState &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::AlignState &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<Ekos::AlignState>(a);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::SchedulerState &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::SchedulerState &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<Ekos::SchedulerState>(a);
    return argument;
}

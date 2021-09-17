/*
    SPDX-FileCopyrightText: 2019 Hy Murveit <hy-1@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QVector>
#include <qcustomplot.h>

namespace Ekos
{

class PolynomialFit
{
public:
    // Constructor. Pass in the degree of the desired polynomial fit, and a vector with the x and y values.
    // The constructor solves for the polynomial coefficients.
    PolynomialFit(int degree, const QVector<double>& x, const QVector<double>& y);
    PolynomialFit(int degree, const QVector<int>& x, const QVector<double>& y);

    // Returns the minimum position and value in the pointers for the solved polynomial.
    // Returns false if the polynomial couldn't be solved.
    bool findMinimum(double expected, double minPosition, double maxPosition, double *position, double *value);

    /**
     * @brief Polynomial function f(x)
     * @param x argument
     * @return f(x)
     */
    double f(double x);


private:
    // Solves for the polynomial coefficients.
    void solve(const QVector<double>& positions, const QVector<double>& values);

    // Calculates the value of the polynomial at x.
    // Params will be cast to a PolynomialFit*.
    static double polynomialFunction(double x, void *params);

    // Polynomial degree.
    int degree;
    // The data values.
    QVector<double> x, y;
    // The solved polynomial coefficients.
    std::vector<double> coefficients;
};
}

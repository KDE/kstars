/*  Ekos polynomial fit utilities Algorithms
    Copyright (C) 2019 Hy Murveit <hy-1@murveit.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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

    // Draws the polynomial on the plot's graph.
    void drawPolynomial(QCustomPlot *plot, QCPGraph *graph);
    // Annotate's the plot's solution graph with the solution position.
    void drawMinimum(QCustomPlot *plot, QCPGraph *solutionGraph,
                     double solutionPosition, double solutionValue, const QFont& font);

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

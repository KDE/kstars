/*
    SPDX-FileCopyrightText: 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars:
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "vect.h"

namespace GuiderUtils
{
class Matrix
{
    public:
        double x[4][4];
        Matrix();
        explicit Matrix(double);
        Matrix &operator+=(const Matrix &);
        Matrix &operator-=(const Matrix &);
        Matrix &operator*=(const Matrix &);
        Matrix &operator*=(double);
        Matrix &operator/=(double);
        void Invert();
        void Transpose();
        friend Matrix operator+(const Matrix &, const Matrix &);
        friend Matrix operator-(const Matrix &, const Matrix &);
        friend Matrix operator*(const Matrix &, double);
        friend Matrix operator*(const Matrix &, const Matrix &);
        friend GuiderUtils::Vector operator*(const GuiderUtils::Vector &, const Matrix &);
};

Matrix Translate(const GuiderUtils::Vector &);
Matrix Scale(const GuiderUtils::Vector &);
Matrix RotateX(double);
Matrix RotateY(double);
Matrix RotateZ(double);
Matrix Rotate(const GuiderUtils::Vector &v, double angle);
Matrix Transform(const GuiderUtils::Vector &v1, const GuiderUtils::Vector &v2, const GuiderUtils::Vector &v3);
Matrix MirrorX();
Matrix MirrorY();
Matrix MirrorZ();
}  // namespace GuiderUtils


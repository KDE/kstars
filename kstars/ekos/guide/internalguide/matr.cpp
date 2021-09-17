/*
    SPDX-FileCopyrightText: 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars:
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "matr.h"

#include "vect.h"

#include <cmath>

namespace GuiderUtils
{
Matrix ::Matrix(double v)
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            x[i][j] = (i == j) ? v : 0.0;
    x[3][3] = 1;
}

Matrix ::Matrix()
{
    for (auto &row : x)
        for (double &item : row)
        {
            item = 0.0;
        }

    x[3][3] = 1;
}

void Matrix ::Invert()
{
    Matrix Out(1);
    for (int i = 0; i < 4; i++)
    {
        double d = x[i][i];
        if (d != 1.0)
        {
            for (int j = 0; j < 4; j++)
            {
                Out.x[i][j] /= d;
                x[i][j] /= d;
            }
        }

        for (int j = 0; j < 4; j++)
        {
            if (j != i)
            {
                if (x[j][i] != 0.0)
                {
                    double mulby = x[j][i];
                    for (int k = 0; k < 4; k++)
                    {
                        x[j][k] -= mulby * x[i][k];
                        Out.x[j][k] -= mulby * Out.x[i][k];
                    }
                }
            }
        }
    }
    *this = Out;
}

void Matrix ::Transpose()
{
    double t;
    for (int i = 0; i < 4; i++)
        for (int j = i; j < 4; j++)
            if (i != j)
            {
                t       = x[i][j];
                x[i][j] = x[j][i];
                x[j][i] = t;
            }
}

Matrix &Matrix ::operator+=(const Matrix &A)
{
    if (this == &A)
        return *this;

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            x[i][j] += A.x[i][j];

    return *this;
}

Matrix &Matrix ::operator-=(const Matrix &A)
{
    if (this == &A)
        return *this;

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            x[i][j] -= A.x[i][j];

    return *this;
}

Matrix &Matrix ::operator*=(double v)
{
    for (auto &row : x)
        for (double &item : row)
        {
            item *= v;
        }

    return *this;
}

Matrix &Matrix ::operator*=(const Matrix &A)
{
    if (this == &A)
        return *this;

    Matrix res = *this;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            double sum = 0;
            for (int k = 0; k < 4; k++)
                sum += res.x[i][k] * A.x[k][j];

            x[i][j] = sum;
        }
    return *this;
}

Matrix operator+(const Matrix &A, const Matrix &B)
{
    Matrix res;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            res.x[i][j] = A.x[i][j] + B.x[i][j];

    return res;
}

Matrix operator-(const Matrix &A, const Matrix &B)
{
    Matrix res;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            res.x[i][j] = A.x[i][j] - B.x[i][j];

    return res;
}

Matrix operator*(const Matrix &A, const Matrix &B)
{
    Matrix res;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            double sum = 0;
            for (int k = 0; k < 4; k++)
                sum += A.x[i][k] * B.x[k][j];

            res.x[i][j] = sum;
        }

    return res;
}

Matrix operator*(const Matrix &A, double v)
{
    Matrix res;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            res.x[i][j] = A.x[i][j] * v;

    return res;
}

GuiderUtils::Vector operator*(const GuiderUtils::Vector &v, const Matrix &M)
{
    GuiderUtils::Vector res;

    res.x = v.x * M.x[0][0] + v.y * M.x[0][1] + v.z * M.x[0][2] + M.x[0][3];
    res.y = v.x * M.x[1][0] + v.y * M.x[1][1] + v.z * M.x[1][2] + M.x[1][3];
    res.z = v.x * M.x[2][0] + v.y * M.x[2][1] + v.z * M.x[2][2] + M.x[2][3];
    /*
     res.x = v.x*M.x[0][0] + v.y*M.x[1][0] + v.z*M.x[2][0] + M.x[3][0];
     res.y = v.x*M.x[0][1] + v.y*M.x[1][1] + v.z*M.x[2][1] + M.x[3][1];
     res.z = v.x*M.x[0][2] + v.y*M.x[1][2] + v.z*M.x[2][2] + M.x[3][2];

     double denom = v.x*M.x[0][3] + v.y*M.x[1][3] + v.z*M.x[2][3] + M.x[3][3];
     if( denom != 1.0 )
     {
         res /= denom;
         ShowMessage("denom="+FloatToStr(denom));
     }
    */
    //  ShowMessage( FloatToStr(v.x)+"\n" + FloatToStr(v.x)+"\n" + FloatToStr(v.x) );
    //  ShowMessage("res.x="+FloatToStr(res.x)+"\nres.y="+FloatToStr(res.y));
    /*
      ShowMessage( FloatToStr(M.x[0][0])+", " + FloatToStr(M.x[1][0])+", " + FloatToStr(M.x[2][0])+", " + FloatToStr(M.x[3][0])+"\n" +
                   FloatToStr(M.x[0][1])+", " + FloatToStr(M.x[1][1])+", " + FloatToStr(M.x[2][1])+", " + FloatToStr(M.x[3][1])+"\n" +
                   FloatToStr(M.x[0][2])+", " + FloatToStr(M.x[1][2])+", " + FloatToStr(M.x[2][2])+", " + FloatToStr(M.x[3][2])+"\n" +
                   FloatToStr(M.x[0][3])+", " + FloatToStr(M.x[1][3])+", " + FloatToStr(M.x[2][3])+", " + FloatToStr(M.x[3][3])+"\n"
                 );
    */
    return res;
}

Matrix Translate(const GuiderUtils::Vector &Loc)
{
    Matrix res(1);
    res.x[0][3] = Loc.x;
    res.x[1][3] = Loc.y;
    res.x[2][3] = Loc.z;
    /*
     res.x[3][0] = Loc.x;
     res.x[3][1] = Loc.y;
     res.x[3][2] = Loc.z;
    */
    return res;
}

Matrix Scale(const GuiderUtils::Vector &v)
{
    Matrix res(1);
    res.x[0][0] = v.x;
    res.x[1][1] = v.y;
    res.x[2][2] = v.z;

    return res;
}

Matrix RotateX(double Angle)
{
    Matrix res(1);
    double Cosine = cos(Angle);
    double Sine   = sin(Angle);

    res.x[1][1] = Cosine;
    res.x[2][1] = Sine;
    res.x[1][2] = -Sine;
    res.x[2][2] = Cosine;
    /*
     res.x[1][1] = Cosine;
     res.x[2][1] = -Sine;
     res.x[1][2] = Sine;
     res.x[2][2] = Cosine;
    */
    return res;
}

Matrix RotateY(double Angle)
{
    Matrix res(1);
    double Cosine = cos(Angle);
    double Sine   = sin(Angle);

    res.x[0][0] = Cosine;
    res.x[2][0] = Sine;  //-
    res.x[0][2] = -Sine; //+
    res.x[2][2] = Cosine;

    return res;
}

Matrix RotateZ(double Angle)
{
    Matrix res(1);
    double Cosine = cos(Angle);
    double Sine   = sin(Angle);

    // ShowMessage( "sin="+FloatToStr(Sine)+"\ncos="+FloatToStr(Cosine) );

    res.x[0][0] = Cosine;
    res.x[1][0] = Sine;  //-
    res.x[0][1] = -Sine; //+
    res.x[1][1] = Cosine;

    return res;
}

Matrix Rotate(const GuiderUtils::Vector &axis, double angle)
{
    Matrix res(1);
    double Cosine = cos(angle);
    double Sine   = sin(angle);

    res.x[0][0] = axis.x * axis.x + (1 - axis.x * axis.x) * Cosine;
    res.x[0][1] = axis.x * axis.y * (1 - Cosine) + axis.z * Sine;
    res.x[0][2] = axis.x * axis.z * (1 - Cosine) - axis.y * Sine;
    res.x[0][3] = 0;

    res.x[1][0] = axis.x * axis.y * (1 - Cosine) - axis.z * Sine;
    res.x[1][1] = axis.y * axis.y + (1 - axis.y * axis.y) * Cosine;
    res.x[1][2] = axis.y * axis.z * (1 - Cosine) + axis.x * Sine;
    res.x[1][3] = 0;

    res.x[2][0] = axis.x * axis.z * (1 - Cosine) + axis.y * Sine;
    res.x[2][1] = axis.y * axis.z * (1 - Cosine) - axis.x * Sine;
    res.x[2][2] = axis.z * axis.z + (1 - axis.z * axis.z) * Cosine;
    res.x[2][3] = 0;

    res.x[3][0] = 0;
    res.x[3][1] = 0;
    res.x[3][2] = 0;
    res.x[3][3] = 1;

    return res;
}
// Transforms V into coord sys. v1, v2, v3
Matrix Transform(const GuiderUtils::Vector &v1, const GuiderUtils::Vector &v2, const GuiderUtils::Vector &v3)
{
    Matrix res(1);

    // Flipped columns & rows vs prev version
    res.x[0][0] = v1.x;
    res.x[0][1] = v1.y;
    res.x[0][2] = v1.z;
    res.x[0][3] = 0;

    res.x[1][0] = v2.x;
    res.x[1][1] = v2.y;
    res.x[1][2] = v2.z;
    res.x[1][3] = 0;

    res.x[2][0] = v3.x;
    res.x[2][1] = v3.y;
    res.x[2][2] = v3.z;
    res.x[2][3] = 0;

    res.x[3][0] = 0;
    res.x[3][1] = 0;
    res.x[3][2] = 0;
    res.x[3][3] = 1;

    return res;
}

Matrix MirrorX()
{
    Matrix res(1);
    res.x[0][0] = -1;
    return res;
}

Matrix MirrorY()
{
    Matrix res(1);
    res.x[1][1] = -1;
    return res;
}

Matrix MirrorZ()
{
    Matrix res(1);
    res.x[2][2] = -1;
    return res;
}
}  // namespace GuiderUtils

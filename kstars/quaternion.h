/***************************************************************************
                          quaternion.h  -  K Desktop Planetarium
                             -------------------

    copyright            : (C) 2004 by Torsten Rahn
    email                : tackat@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QUATERNION_H
#define QUATERNION_H

enum
{
    Q_X = 0,
    Q_Y = 1,
    Q_Z = 2,
    Q_W = 3
};

typedef float matrix[3][4];

class Quaternion {
public:
    Quaternion();
    Quaternion(float w, float x, float y, float z);
    Quaternion(float alpha, float beta);

    // Operators
    Quaternion operator*(const Quaternion &q) const;
    bool operator==(const Quaternion &q) const;
    void operator*=(const Quaternion &q);

    void set(float w, float x, float y, float z);
    void normalize();

    Quaternion inverse() const;

    void createFromEuler(float pitch, float yaw, float roll);
    void display() const;

    void rotateAroundAxis(const Quaternion &q);

    void getSpherical(float &alpha, float &beta);

    void scalar(float mult);

    void toMatrix(matrix &m);
    void rotateAroundAxis(const matrix &m);

    // TODO: Better add accessors...
    float v[4];
};

#endif

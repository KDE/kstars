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

#ifdef __GNUC__
  typedef int xmmint[4] __attribute__ ((aligned (16)));
  typedef float xmmfloat[4] __attribute__ ((aligned (16)));
#else
  typedef int xmmint[4];
  typedef float xmmfloat[4];
#endif

typedef xmmfloat matrix[3];

static const xmmfloat fsgn_pmpp = { 1.0f, -1.0f, 1.0f, 1.0f };
static const xmmfloat fsgn_ppmp = { 1.0f, 1.0f, -1.0f, 1.0f };
static const xmmfloat fsgn_pppm = { 1.0f, 1.0f, 1.0f, -1.0f };

static const xmmint sgn_pmpp = { 0, 1 << 31, 0, 0 };
static const xmmint sgn_ppmp = { 0, 0, 1 << 31, 0 };
static const xmmint sgn_pppm = { 0, 0, 0, 1 << 31 };


class Quaternion {
public:
	Quaternion();
	Quaternion(float w, float x, float y, float z);
	Quaternion(float alpha, float beta);
	virtual ~Quaternion(){ }

	// Operators
	Quaternion operator*(const Quaternion &q) const;
	bool operator==(const Quaternion &q) const;
	void operator*=(const Quaternion &q);

	void set(float w, float x, float y, float z);
	void normalize();

	Quaternion inverse() const;

	void createFromEuler(float pitch, float yaw, float roll);
	void display() const;

	virtual void rotateAroundAxis(const Quaternion &q);

	void getSpherical(float &alpha, float &beta);

	void scalar(float mult);

	void toMatrix(matrix &m);
	void rotateAroundAxis(const matrix &m);

	// TODO: Better add accessors...
	xmmfloat v;
};

class QuaternionSSE : public Quaternion {
public:
	void rotateAroundAxis(const Quaternion &q);
};

#endif

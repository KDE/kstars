//
// C++ Interface: texcolorizer
//
// Description: Quaternions 

// Quaternions provides a class that deals with quaternion operations.
//
// Author: Torsten Rahn <tackat@kde.org>, (C) 2004
//
// Copyright: See COPYING file that comes with this distribution

#ifndef QUATERNION_H
#define QUATERNION_H

enum
{
	Q_X = 0,
	Q_Y = 1,
	Q_Z = 2,
	Q_W = 3,
};

typedef int xmmint[4] __attribute__ ((aligned (16)));
typedef float xmmfloat[4] __attribute__ ((aligned (16)));

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

#include "quaternion.h"
#include <QString>
#include <cmath>
#include <stdlib.h>
#include <QDebug>
// #include <xmmintrin.h>

#define quatNorm (v[Q_W] * v[Q_W] + v[Q_X] * v[Q_X] + v[Q_Y] * v[Q_Y] + v[Q_Z] * v[Q_Z])

Quaternion::Quaternion(){
//	qDebug("Quaternion NoSSE");
	v[Q_W] = 1.0f; v[Q_X] = 0.0f; v[Q_Y] = 0.0f; v[Q_Z] = 0.0f;
}

Quaternion::Quaternion(float w, float x, float y, float z) {
	v[Q_W] = w; v[Q_X] = x; v[Q_Y] = y; v[Q_Z] = z;
}

Quaternion::Quaternion(float alpha, float beta) {
	v[Q_W] = 0.0f;

	const float cosBeta = cos(beta);
	v[Q_X] = -cosBeta * sin(alpha);
	v[Q_Y] = -sin(beta);
	v[Q_Z] = cosBeta * cos(alpha);
}

void Quaternion::set(float w, float x, float y, float z) {
	v[Q_W] = w; v[Q_X] = x; v[Q_Y] = y; v[Q_Z] = z;
}

void Quaternion::normalize() {
	scalar(1.0f / sqrt(quatNorm));
}

void Quaternion::scalar(float mult) {
	v[Q_W] *= mult; v[Q_X] *= mult; v[Q_Y] *= mult; v[Q_Z] *= mult;
}

Quaternion Quaternion::inverse() const {
	Quaternion inverse(v[Q_W], -v[Q_X], -v[Q_Y], -v[Q_Z]);
	inverse.normalize();
	return inverse;
}

void Quaternion::createFromEuler(float pitch, float yaw, float roll) {
	float cX, cY, cZ, sX, sY, sZ, cYcZ, sYsZ, sYcZ, cYsZ;

	pitch *= 0.5f;
	yaw *= 0.5f;
	roll *= 0.5f;

	cX = cosf(pitch);
	cY = cosf(yaw);
	cZ = cosf(roll);

	sX = sinf(pitch);
	sY = sinf(yaw);
	sZ = sinf(roll);

	cYcZ = cY * cZ;
	sYsZ = sY * sZ;
	sYcZ = sY * cZ;
	cYsZ = cY * sZ;

	v[Q_W] = cX * cYcZ + sX * sYsZ;
	v[Q_X] = sX * cYcZ - cX * sYsZ;
	v[Q_Y] = cX * sYcZ + sX * cYsZ;
	v[Q_Z] = cX * cYsZ - sX * sYcZ;
}

void Quaternion::display() const {
	QString quatdisplay = QString("Quaternion: w= %1, x= %2, y= %3, z= %4, |q|= %5" )
						  .arg(v[Q_W]).arg(v[Q_X]).arg(v[Q_Y]).arg(v[Q_Z]).arg(quatNorm);

	qDebug(quatdisplay.toLatin1());
}

void Quaternion::operator*=(const Quaternion &q) {
	float x, y, z, w;

	w = v[Q_W] * q.v[Q_W] - v[Q_X] * q.v[Q_X] - v[Q_Y] * q.v[Q_Y] - v[Q_Z] * q.v[Q_Z];
	x = v[Q_W] * q.v[Q_X] + v[Q_X] * q.v[Q_W] + v[Q_Y] * q.v[Q_Z] - v[Q_Z] * q.v[Q_Y];
	y = v[Q_W] * q.v[Q_Y] - v[Q_X] * q.v[Q_Z] + v[Q_Y] * q.v[Q_W] + v[Q_Z] * q.v[Q_X];
	z = v[Q_W] * q.v[Q_Z] + v[Q_X] * q.v[Q_Y] - v[Q_Y] * q.v[Q_X] + v[Q_Z] * q.v[Q_W];

	v[Q_W] = w; v[Q_X] = x; v[Q_Y] = y; v[Q_Z] = z;
}
bool Quaternion::operator==(const Quaternion &q) const {

	if ( v[Q_W] == q.v[Q_W] && v[Q_X] == q.v[Q_X] && v[Q_Y] == q.v[Q_Y] && v[Q_Z] == q.v[Q_Z] )	
		return true;
	else
		return false;
}

Quaternion Quaternion::operator*(const Quaternion &q) const {
	return Quaternion(
		v[Q_W] * q.v[Q_W] - v[Q_X] * q.v[Q_X] - v[Q_Y] * q.v[Q_Y] - v[Q_Z] * q.v[Q_Z],
		v[Q_W] * q.v[Q_X] + v[Q_X] * q.v[Q_W] + v[Q_Y] * q.v[Q_Z] - v[Q_Z] * q.v[Q_Y],
		v[Q_W] * q.v[Q_Y] - v[Q_X] * q.v[Q_Z] + v[Q_Y] * q.v[Q_W] + v[Q_Z] * q.v[Q_X],
		v[Q_W] * q.v[Q_Z] + v[Q_X] * q.v[Q_Y] - v[Q_Y] * q.v[Q_X] + v[Q_Z] * q.v[Q_W]
	);
}

void Quaternion::rotateAroundAxis(const Quaternion &q) {
	float _x, _y, _z, _w;

	_w = + v[Q_X] * q.v[Q_X] + v[Q_Y] * q.v[Q_Y] + v[Q_Z] * q.v[Q_Z];
	_x = + v[Q_X] * q.v[Q_W] - v[Q_Y] * q.v[Q_Z] + v[Q_Z] * q.v[Q_Y];
	_y = + v[Q_X] * q.v[Q_Z] + v[Q_Y] * q.v[Q_W] - v[Q_Z] * q.v[Q_X];
	_z = - v[Q_X] * q.v[Q_Y] + v[Q_Y] * q.v[Q_X] + v[Q_Z] * q.v[Q_W];

	v[Q_W] = 0.0f;
//	v[Q_W] = q.v[Q_W] * _w - q.v[Q_X] * _x - q.v[Q_Y] * _y - q.v[Q_Z] * _z;
	v[Q_X] = q.v[Q_W] * _x + q.v[Q_X] * _w + q.v[Q_Y] * _z - q.v[Q_Z] * _y;
	v[Q_Y] = q.v[Q_W] * _y - q.v[Q_X] * _z + q.v[Q_Y] * _w + q.v[Q_Z] * _x;
	v[Q_Z] = q.v[Q_W] * _z + q.v[Q_X] * _y - q.v[Q_Y] * _x + q.v[Q_Z] * _w;

}

void QuaternionSSE::rotateAroundAxis(const Quaternion &q) {

	Quaternion::rotateAroundAxis(q);
/*
	__asm__ ( 

		"movl %0, %%esi \n\t"
		"movups (%%esi), %%xmm0 \n\t" // load this into xmm0

		"movups (%1), %%xmm1 \n\t" // load q into xmm1
		"movaps (%c2), %%xmm5 \n\t"
		"movaps (%c3), %%xmm6 \n\t"
		"movaps (%4), %%xmm7 \n\t"

		"xorps %%xmm2, %%xmm2 \n\t" // result register: xmm2

		"movaps %%xmm0, %%xmm4 \n\t"
		"shufps $0x55, %%xmm4, %%xmm4  \n\t" // put const multiplier into xmm4
		"mulps %%xmm1, %%xmm4 \n\t" // this[1] * q into xmm1
		"shufps $0xB1, %%xmm4, %%xmm4 \n\t" // q: xwzy 
		"xorps %%xmm7, %%xmm4  \n\t"
//		"mulps %%xmm7, %%xmm4  \n\t"
		"addps %%xmm4, %%xmm2 \n\t" // add to total result

		"movaps %%xmm0, %%xmm4 \n\t"
		"shufps $0xAA, %%xmm4, %%xmm4  \n\t" // put const multiplier into xmm4
		"mulps %%xmm1, %%xmm4 \n\t" // this[1] * q into xmm1
 		"shufps $0x4E, %%xmm4, %%xmm4 \n\t" // q: zxyw 
		"xorps %%xmm5, %%xmm4  \n\t"
//		"mulps %%xmm5, %%xmm4  \n\t"
		"addps %%xmm4, %%xmm2 \n\t" // add to total result

		"shufps $0xFF, %%xmm0, %%xmm0  \n\t" // put const multiplier into xmm4
		"mulps %%xmm1, %%xmm0 \n\t" // this[1] * q into xmm1
		"shufps $0x1B, %%xmm0, %%xmm0 \n\t" // q: zxyw 
		"xorps %%xmm6, %%xmm0  \n\t"
//		"mulps %%xmm6, %%xmm0  \n\t"
		"addps %%xmm0, %%xmm2 \n\t" // add to total result

		"movaps %%xmm2, %%xmm0 \n\t" // load new _q into xmm0, q is in xmm1 already
		"xorps %%xmm2, %%xmm2 \n\t" // reset result register: xmm2

		"movss %%xmm1, %%xmm4 \n\t" // FIXME: movaps -> movss (-1 Cylce)
		"shufps $0x0, %%xmm4, %%xmm4  \n\t" // put const multiplier into xmm4
		"mulps %%xmm0, %%xmm4 \n\t" // q * _q into xmm4
		"addps %%xmm4, %%xmm2 \n\t" // add to total result

		"movaps %%xmm1, %%xmm4 \n\t"
		"shufps $0x55, %%xmm4, %%xmm4  \n\t" // put const multiplier into xmm4
		"xorps %%xmm6, %%xmm4 \n\t" 
//		"mulps %%xmm6, %%xmm4 \n\t" 
		"movaps %%xmm0, %%xmm3 \n\t"
		"shufps $0xB1, %%xmm3, %%xmm3 \n\t" // distribute q-components correctly
		"mulps  %%xmm3, %%xmm4 \n\t" // q * _q into xmm4
		"addps %%xmm4, %%xmm2 \n\t" // add to total result

		"movaps %%xmm1, %%xmm4 \n\t"
		"shufps $0xAA, %%xmm4, %%xmm4  \n\t" // put const multiplier into xmm4
		"xorps %%xmm7, %%xmm4 \n\t" 
//		"mulps %%xmm7, %%xmm4 \n\t" 
		"movaps %%xmm0, %%xmm3 \n\t"
		"shufps $0x4E, %%xmm3, %%xmm3 \n\t" // distribute q-components correctly
		"mulps  %%xmm3, %%xmm4 \n\t" // q * _q into xmm4
		"addps %%xmm4, %%xmm2 \n\t" // add to total result

		"movaps %%xmm1, %%xmm4 \n\t"
		"shufps $0xFF, %%xmm4, %%xmm4  \n\t" // put const multiplier into xmm4
		"xorps %%xmm5, %%xmm4 \n\t"
//		"mulps %%xmm5, %%xmm4 \n\t"
		"shufps $0x1B, %%xmm0, %%xmm0 \n\t" // distribute q-components correctly
		"mulps  %%xmm0, %%xmm4 \n\t" // q * _q into xmm4
		"addps %%xmm4, %%xmm2 \n\t" // add to total result

		"movups %%xmm2, (%%esi) \n\t" // return

		: 
		: "D"(v), "r"(q.v), "%ir"( sgn_pmpp), "%ir"( sgn_ppmp ), "r"( sgn_pppm )
		: "%esi", "memory" );
*/
}

void Quaternion::getSpherical(float &alpha, float &beta) {
	if(fabs(v[Q_Y]) > 1.0f) {
		if(v[Q_Y] > 1.0f) v[Q_Y] = 1.0f;
		if(v[Q_Y] < -1.0f) v[Q_Y] = -1.0f;
	}

	beta = -asin(v[Q_Y]);

	if(v[Q_X] * v[Q_X] + v[Q_Z] * v[Q_Z] > 0.00005f) 
		alpha = atan2(v[Q_X], v[Q_Z]);
	else
		alpha = 0.0f;
}

void Quaternion::toMatrix(matrix &m) {

	float xy = v[Q_X] * v[Q_Y], xz = v[Q_X] * v[Q_Z];
	float yy = v[Q_Y] * v[Q_Y], yw = v[Q_Y] * v[Q_W];
	float zw = v[Q_Z] * v[Q_W], zz = v[Q_Z] * v[Q_Z];

	m[0][0] = 1.0f - 2.0f * (yy + zz);
	m[0][1] = 2.0f * (xy + zw);
	m[0][2] = 2.0f * (xz - yw);
	m[0][3] = 0.0f;

	float xx = v[Q_X] * v[Q_X], xw = v[Q_X] * v[Q_W], yz = v[Q_Y] * v[Q_Z];

	m[1][0] = 2.0f * (xy - zw);
	m[1][1] = 1.0f - 2.0f * (xx + zz);
	m[1][2] = 2.0f * (yz + xw);
	m[1][3] = 0.0f;

	m[2][0] = 2.0f * (xz + yw);
	m[2][1] = 2.0f * (yz - xw);
	m[2][2] = 1.0f - 2.0f * (xx + yy);
	m[2][3] = 0.0f;
}

void Quaternion::rotateAroundAxis(const matrix &m) {
	float x, y, z;

	x =  m[0][0] * v[Q_X] + m[1][0] * v[Q_Y] + m[2][0] * v[Q_Z];
	y =  m[0][1] * v[Q_X] + m[1][1] * v[Q_Y] + m[2][1] * v[Q_Z];
	z =  m[0][2] * v[Q_X] + m[1][2] * v[Q_Y] + m[2][2] * v[Q_Z];

	v[Q_X] = x; v[Q_Y] = y; v[Q_Z] = z; v[Q_W] = 1;
}

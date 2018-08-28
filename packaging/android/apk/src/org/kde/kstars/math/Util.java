package org.kde.kstars.math;

import android.util.FloatMath;

public class Util {
	/**
	 * Converts a double to a value between 0 and 360
	 * 
	 * @param x
	 * @return double in 0-360 range
	 */
	public static float floatrev(double x) {
		return (float) (x - Math.floor(x / 360.0f) * 360.0f);
	}

	/**
	 * Derive distance between 3d vector a,b
	 * 
	 * @param a
	 * @param b
	 * @return
	 */
	public static float calcDistance(Vector3 a, Vector3 b) {
		return FloatMath.sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
	}

	/**
	 * calculates the encompassing circle radius for sides a,b,c of a triangle
	 * 
	 * @param a
	 * @param b
	 * @param c
	 * @return
	 */
	public static float calcRadius(float a, float b, float c) {
		return (a * b * c) / FloatMath.sqrt((a + b + c) * (a - b + c) * (a + b - c) * (b + c - a));
	}

	/**
	 * Calculates the angle A given length a and circle radius r, according to
	 * the law of sines ([a/sin(A) = 2R], thus [A = arcsin(a/2r)])
	 * 
	 * @param a
	 * @param r
	 * @return angle A in radians
	 */
	public static float calcAngle(float a, float r) {
		return (float) Math.asin(a / (2 * r));
	}

	/**
	 * Calculates the angle A given length a and circle radius r, according to
	 * the law of sines ([a/sin(A) = 2R], thus [A = arcsin(a/2r)])
	 * 
	 * @param a
	 * @param r
	 * @return angle A in radians
	 */
	public static float calcAngleClamp(float a, float r) {
		return (float) Math.asin(Math.min(1, Math.max(-1, a / (2 * r))));
	}
}

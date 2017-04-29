package org.kde.kstars.math;

/**
 * Derived from Vector3 from libgdx - encapsulates a 4 component vector for
 * representing Homogeneous Coordinates
 */
public final class Vector4 {
	/** the x-component of this vector **/
	public float x;
	/** the x-component of this vector **/
	public float y;
	/** the x-component of this vector **/
	public float z;

	public float w;

	private static Vector4 tmp = new Vector4();
	private static Vector4 tmp2 = new Vector4();
	private static Vector4 tmp3 = new Vector4();

	/**
	 * Multiplies the vector by the given matrix.
	 * 
	 * @param matrix
	 *            The matrix
	 * @return This vector for chaining
	 */
	public Vector4 mul(Matrix4 matrix) {
		float l_mat[] = matrix.val;
		return this.set(x * l_mat[Matrix4.M00] + y * l_mat[Matrix4.M01] + z * l_mat[Matrix4.M02] + w * l_mat[Matrix4.M03], x * l_mat[Matrix4.M10] + y * l_mat[Matrix4.M11] + z * l_mat[Matrix4.M12] + w * l_mat[Matrix4.M13], x * l_mat[Matrix4.M20] + y
				* l_mat[Matrix4.M21] + z * l_mat[Matrix4.M22] + w * l_mat[Matrix4.M23], x * l_mat[Matrix4.M30] + y * l_mat[Matrix4.M31] + z * l_mat[Matrix4.M32] + w * l_mat[Matrix4.M33]);
	}

	/**
	 * Constructs a vector at (0,0,0)
	 */
	public Vector4() {
	}

	/**
	 * Creates a vector with the given components
	 * 
	 * @param x
	 *            The x-component
	 * @param y
	 *            The y-component
	 * @param z
	 *            The z-component
	 */
	public Vector4(float x, float y, float z, float w) {
		this.set(x, y, z, w);
	}

	/**
	 * Creates a vector from the given vector
	 * 
	 * @param vector
	 *            The vector
	 */
	public Vector4(Vector4 vector) {
		this.set(vector);
	}

	/**
	 * Creates a vector from the given array. The array must have at least 3
	 * elements.
	 * 
	 * @param values
	 *            The array
	 */
	public Vector4(float[] values) {
		this.set(values[0], values[1], values[2], values[3]);
	}

	/**
	 * Sets the vector to the given components
	 * 
	 * @param x
	 *            The x-component
	 * @param y
	 *            The y-component
	 * @param z
	 *            The z-component
	 * @return this vector for chaining
	 */
	public Vector4 set(float x, float y, float z, float w) {
		this.x = x;
		this.y = y;
		this.z = z;
		this.w = w;
		return this;
	}

	/**
	 * Sets the components of the given vector
	 * 
	 * @param vector
	 *            The vector
	 * @return This vector for chaining
	 */
	public Vector4 set(Vector4 vector) {
		return this.set(vector.x, vector.y, vector.z, vector.w);
	}

	/**
	 * Sets the components from the array. The array must have at least 3
	 * elements
	 * 
	 * @param values
	 *            The array
	 * @return this vector for chaining
	 */
	public Vector4 set(float[] values) {
		return this.set(values[0], values[1], values[2], values[3]);
	}

	/**
	 * @return a copy of this vector
	 */
	public Vector4 cpy() {
		return new Vector4(this);
	}

	/**
	 * NEVER EVER SAVE THIS REFERENCE!
	 * 
	 * @return
	 */
	public Vector4 tmp() {
		return tmp.set(this);
	}

	/**
	 * NEVER EVER SAVE THIS REFERENCE!
	 * 
	 * @return
	 */
	public Vector4 tmp2() {
		return tmp2.set(this);
	}

	/**
	 * NEVER EVER SAVE THIS REFERENCE!
	 * 
	 * @return
	 */
	Vector4 tmp3() {
		return tmp3.set(this);
	}

	/**
	 * Adds the given vector to this vector
	 * 
	 * @param vector
	 *            The other vector
	 * @return This vector for chaining
	 */
	public Vector4 add(Vector4 vector) {
		return this.add(vector.x, vector.y, vector.z, vector.w);
	}

	/**
	 * Adds the given vector to this component
	 * 
	 * @param x
	 *            The x-component of the other vector
	 * @param y
	 *            The y-component of the other vector
	 * @param z
	 *            The z-component of the other vector
	 * @return This vector for chaining.
	 */
	public Vector4 add(float x, float y, float z, float w) {
		return this.set(this.x + x, this.y + y, this.z + z, this.w + w);
	}

	/**
	 * Adds the given value to all three components of the vector.
	 * 
	 * @param values
	 *            The value
	 * @return This vector for chaining
	 */
	public Vector4 add(float values) {
		return this.set(this.x + values, this.y + values, this.z + values, this.w + values);
	}

	/**
	 * Subtracts the given vector from this vector
	 * 
	 * @param a_vec
	 *            The other vector
	 * @return This vector for chaining
	 */
	public Vector4 sub(Vector4 a_vec) {
		return this.sub(a_vec.x, a_vec.y, a_vec.z, a_vec.w);
	}

	/**
	 * Subtracts the other vector from this vector.
	 * 
	 * @param x
	 *            The x-component of the other vector
	 * @param y
	 *            The y-component of the other vector
	 * @param z
	 *            The z-component of the other vector
	 * @return This vector for chaining
	 */
	public Vector4 sub(float x, float y, float z, float w) {
		return this.set(this.x - x, this.y - y, this.z - z, this.w - w);
	}

	/**
	 * Subtracts the given value from all components of this vector
	 * 
	 * @param value
	 *            The value
	 * @return This vector for chaining
	 */
	public Vector4 sub(float value) {
		return this.set(this.x - value, this.y - value, this.z - value, this.w - value);
	}

	/**
	 * Multiplies all components of this vector by the given value
	 * 
	 * @param value
	 *            The value
	 * @return This vector for chaining
	 */
	public Vector4 mul(float value) {
		return this.set(this.x * value, this.y * value, this.z * value, this.w * value);
	}

	/**
	 * Divides all components of this vector by the given value
	 * 
	 * @param value
	 *            The value
	 * @return This vector for chaining
	 */
	public Vector4 div(float value) {
		float d = 1 / value;
		return this.set(this.x * d, this.y * d, this.z * d, this.w * d);
	}

	/**
	 * @return The euclidian length
	 */
	public float len() {
		return (float) Math.sqrt(x * x + y * y + z * z + w * w);
	}

	/**
	 * @return The squared euclidian length
	 */
	public float len2() {
		return x * x + y * y + z * z + w * w;
	}

	/**
	 * @param vector
	 *            The other vector
	 * @return Wether this and the other vector are equal
	 */
	public boolean idt(Vector4 vector) {
		return x == vector.x && y == vector.y && z == vector.z && w == vector.w;
	}

	/**
	 * @param vector
	 *            The other vector
	 * @return The euclidian distance between this and the other vector
	 */
	public float dst(Vector4 vector) {
		float a = vector.x - x;
		float b = vector.y - y;
		float c = vector.z - z;
		float d = vector.w - w;
		a *= a;
		b *= b;
		c *= c;
		d *= d;

		return (float) Math.sqrt(a + b + c + d);
	}

	/**
	 * @param vector
	 *            The other vector
	 * @return The squared euclidian distance between this and the other vector
	 */
	public float dist2(Vector4 vector) {
		float a = vector.x - x;
		float b = vector.y - y;
		float c = vector.z - z;
		float d = vector.w - w;
		return a * a + b * b + c * c + d * d;
	}

	/**
	 * Normalizes this vector to unit length
	 * 
	 * @return This vector for chaining
	 */
	public Vector4 nor() {
		if (x == 0 && y == 0 && z == 0 && w == 0)
			return this;
		else
			return this.div(this.len());
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		final int prime = 31;
		int result = 1;
		result = prime * result + Float.floatToIntBits(x);
		result = prime * result + Float.floatToIntBits(y);
		result = prime * result + Float.floatToIntBits(z);
		result = prime * result + Float.floatToIntBits(w);
		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean equals(Object obj) {
		if (this == obj)
			return true;
		if (obj == null)
			return false;
		if (getClass() != obj.getClass())
			return false;
		Vector4 other = (Vector4) obj;
		if (Float.floatToIntBits(x) != Float.floatToIntBits(other.x))
			return false;
		if (Float.floatToIntBits(y) != Float.floatToIntBits(other.y))
			return false;
		if (Float.floatToIntBits(z) != Float.floatToIntBits(other.z))
			return false;
		if (Float.floatToIntBits(w) != Float.floatToIntBits(other.w))
			return false;
		return true;
	}
}
package org.kde.kstars.rotation;

import android.annotation.SuppressLint;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

/**
 * Uses the new (Android 2.3+) Rotation Vector virtual sensor, which is a sensor
 * fusion of the magnetometer, accelerometer, and gyroscope (if present) Note:
 * this does not seem to function correctly for Samsung Galaxy Tab 10.1, in that
 * North does not point North
 * 
 * When it receives an event it will notify the constructor-specified delegate.
 * 
 * @author Adam
 * 
 */
public class RotationVectorListener implements SensorEventListener {
	private float[] mRotationM = new float[16];
	private RotationUpdateDelegate mRotationUpdateDelegate;
	private final float mFilterFactor = 1.0f;
	private final float mFilterFactorInv = 1.0f - mFilterFactor;

	private boolean firstSensor = true;

	private float[] mVectorVals = new float[] { 0f, 0f, 0f };

	public RotationVectorListener(RotationUpdateDelegate rotationUpdateDelegate) {
		mRotationUpdateDelegate = rotationUpdateDelegate;
	}

	@SuppressLint("NewApi")
	@Override
	public void onSensorChanged(SensorEvent event) {
		// smooth(event.values, mVectorVals, mVectorVals);

		SensorManager.getRotationMatrixFromVector(mRotationM, event.values);
		mRotationUpdateDelegate.onRotationUpdate(mRotationM);
	}

	@Override
	public void onAccuracyChanged(Sensor sensor, int accuracy) {
	}

	private void smooth(float[] inv, float prevv[], float outv[]) {
		outv[0] = prevv[0] + mFilterFactor * (inv[0] - prevv[0]);
		outv[1] = prevv[1] + mFilterFactor * (inv[1] - prevv[1]);
		outv[2] = prevv[2] + mFilterFactor * (inv[2] - prevv[2]);
	}
}

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

	public RotationVectorListener(RotationUpdateDelegate rotationUpdateDelegate) {
		mRotationUpdateDelegate = rotationUpdateDelegate;
	}

	@SuppressLint("NewApi")
	@Override
	public void onSensorChanged(SensorEvent event) {
		SensorManager.getRotationMatrixFromVector(mRotationM, event.values);
		mRotationUpdateDelegate.onRotationUpdate(mRotationM);
	}

	@Override
	public void onAccuracyChanged(Sensor sensor, int accuracy) {
	}
}

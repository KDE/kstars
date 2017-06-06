package org.kde.kstars.rotation;

import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

/**
 * Magnetometer / Accelerometer sensor fusion Smoothed by means of simple high
 * pass filter
 * 
 * When it receives an event it will notify the constructor-specified delegate.
 * 
 * @author Adam
 * 
 */
public class MagAccelListener implements SensorEventListener {
	private float[] mRotationM = new float[16];
	// smoothing factor - tune to taste
	private final float mFilterFactor = 0.1f;
	private final float mFilterFactorInv = 1.0f - mFilterFactor;
	private boolean mIsReady = false;
	// smoothed accelerometer values
	private float[] mAccelVals = new float[] { 0f, 0f, 9.8f };
	// smoothed magnetometer values
	private float[] mMagVals = new float[] { 0.5f, 0f, 0f };
	private RotationUpdateDelegate mRotationUpdateDelegate;

	public MagAccelListener(RotationUpdateDelegate rotationUpdateDelegate) {
		mRotationUpdateDelegate = rotationUpdateDelegate;
	}

	@Override
	public void onSensorChanged(SensorEvent event) {
		switch (event.sensor.getType()) {
		case Sensor.TYPE_ACCELEROMETER:
			smooth(event.values, mAccelVals, mAccelVals);
			break;
		case Sensor.TYPE_MAGNETIC_FIELD:
			smooth(event.values, mMagVals, mMagVals);
			mIsReady = true;
			break;
		default:
			break;
		}
		// wait until we have both a new accelerometer and magnetometer sample
		if (mIsReady) {
			mIsReady = false;
			fuseValues();
		}
	}

	private void fuseValues() {
		SensorManager.getRotationMatrix(mRotationM, null, mAccelVals, mMagVals);
		mRotationUpdateDelegate.onRotationUpdate(mRotationM);
	}

	private void smooth(float[] inv, float prevv[], float outv[]) {
		outv[0] = inv[0] * mFilterFactor + prevv[0] * (mFilterFactorInv);
		outv[1] = inv[1] * mFilterFactor + prevv[1] * (mFilterFactorInv);
		outv[2] = inv[2] * mFilterFactor + prevv[2] * (mFilterFactorInv);
	}

	@Override
	public void onAccuracyChanged(Sensor sensor, int accuracy) {
	}
}
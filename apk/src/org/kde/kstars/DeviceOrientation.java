package org.kde.kstars;

import org.qtproject.qt5.android.bindings.QtActivity;
import android.app.Activity;
import android.os.Bundle;
import android.view.Surface;
import android.view.Window;
import android.view.WindowManager;
import android.os.Build;
import android.content.Context;
import android.view.Display;

import android.hardware.Sensor;
import android.hardware.SensorManager;

import org.kde.kstars.math.Matrix4;

import org.kde.kstars.rotation.MagAccelListener;
import org.kde.kstars.rotation.RotationUpdateDelegate;
import org.kde.kstars.rotation.RotationVectorListener;

import org.kde.kstars.math.Matrix4;
import org.kde.kstars.math.Util;
import org.kde.kstars.math.Vector3;
import org.kde.kstars.math.Vector4;

import android.util.FloatMath;

import java.util.ArrayList;
import java.util.List;

//import org.kde.kstars.OrientationCalculator;

public class DeviceOrientation extends QtActivity implements RotationUpdateDelegate
{
    private SensorManager mSensorManager;
    private int mDisplayRotation;
    private Matrix4 mRotationMatrix = new Matrix4();
    private MagAccelListener mMagAccel;
    private RotationVectorListener mRotationVector;
    private boolean mUseRotationVector = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD);
    private float[] m_orientation = new float[4];

    public float Pitch = 0.f;
    public float Azimuth = 0.f;
    public float Roll = 0.f;

    public float getAzimuth() {
        return Azimuth;
    }

    public float getPitch() {
        return Pitch;
    }

    public float getRoll() {
        return Roll;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            // setup window decorations
//            getWindow().requestFeature(Window.FEATURE_NO_TITLE);
            final Display display = ((WindowManager) getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
            mDisplayRotation = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.FROYO) ? display.getRotation() : display.getOrientation();

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
                getWindow().setFlags(WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED, WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED);
            } else if (Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD) {

            }

        // sensor listeners
        mMagAccel = new MagAccelListener(this);
        mRotationVector = new RotationVectorListener(this);
        mSensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);

        mOrthographicProjectionMatrix.setToOrtho2D(0, 0, 1, -1);

        for (int i = 0; i < NUM_POINTS; i++) {
                mVertices.add(new Vector3());
        }
        mOrthographicVertexBatch.addAll(mVertices);
        mOrthographicVertexBatch.add(mSphereTop);
        mOrthographicVertexBatch.add(mSphereBottom);
        mOrthographicVertexBatch.add(mNorthReference);
    }

    // RotationUpdateDelegate methods
    @Override
    public void onRotationUpdate(float[] newMatrix) {
            // remap matrix values according to display rotation, as in
            // SensorManager documentation.
    	    final Display display = ((WindowManager) getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
    	    mDisplayRotation = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.FROYO) ? display.getRotation() : display.getOrientation();

            switch (mDisplayRotation) {
            case Surface.ROTATION_0:
            case Surface.ROTATION_180:
                    break;
            case Surface.ROTATION_90:
                    SensorManager.remapCoordinateSystem(newMatrix, SensorManager.AXIS_Y, SensorManager.AXIS_MINUS_X, newMatrix);
                    break;
            case Surface.ROTATION_270:
                    SensorManager.remapCoordinateSystem(newMatrix, SensorManager.AXIS_MINUS_Y, SensorManager.AXIS_X, newMatrix);
                    break;
            default:
                    break;
            }
            mRotationMatrix.set(newMatrix);
            getOrientation(mRotationMatrix, mDisplayRotation, m_orientation);
            Azimuth = m_orientation[0];
            Pitch = m_orientation[1];
            Roll = m_orientation[2];
    }

    // Other Activity life-cycle methods
    @Override
    protected void onPause() {
            super.onPause();
            mSensorManager.unregisterListener(mMagAccel);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
                    mSensorManager.unregisterListener(mRotationVector);
            }
    }

    private void applySensors(boolean useRV) {
        mSensorManager.unregisterListener(mMagAccel);
        mSensorManager.unregisterListener(mRotationVector);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD && useRV) {
                mSensorManager.registerListener(mRotationVector, mSensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR), SensorManager.SENSOR_DELAY_GAME);
        } else {
                mSensorManager.registerListener(mMagAccel, mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER), SensorManager.SENSOR_DELAY_GAME);
                mSensorManager.registerListener(mMagAccel, mSensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD), SensorManager.SENSOR_DELAY_GAME);
        }
    }

    @Override
    protected void onResume() {
            super.onResume();
            applySensors(mUseRotationVector);
    }

    //For use from C++
    public void startSensors() {
        applySensors(mUseRotationVector);
    }

    public void stopSensors() {
        mSensorManager.unregisterListener(mMagAccel);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
                mSensorManager.unregisterListener(mRotationVector);
        }
    }

    //Orientation calculation
    private static final float DEGREES_TO_RADIANS = (float) (Math.PI / 180.0f);
    private static final float RADIANS_TO_DEGREES = (float) (180.0f / Math.PI);

    // determines the number of points of the sphere
    private static final int POINTS_PER_SEGMENT = 72;
    private static final int NUM_SEGMENTS = 11;
    private static final int NUM_POINTS = POINTS_PER_SEGMENT * NUM_SEGMENTS;
    private static final int ORTHO_RESOLUTION = 1000;

    private ArrayList<Vector3> mVertices = new ArrayList<Vector3>(NUM_POINTS + 1);

    private Vector3 mRollTopAbsolute = new Vector3(), mRollBottomAbsolute = new Vector3(), mOriginPoint = new Vector3(), mReticlePoint = new Vector3();
    private Vector3 mSphereTop = new Vector3(), mSphereBottom = new Vector3(), mNorthReference = new Vector3();
    private Vector3 mNorthAbsolute = new Vector3(), mSouthAbsolute = new Vector3(), mWestAbsolute = new Vector3(), mEastAbsolute = new Vector3();

    private Matrix4 mOrthographicProjectionMatrix = new Matrix4();
    private Matrix4 mModelViewMatrix = new Matrix4();

    private List<Vector3> mOrthographicVertexBatch = new ArrayList<Vector3>();
    private Vector4 vTemp = new Vector4();

    public void getOrientation(Matrix4 rotationMatrix, int screenRotation, float[] out) {
            rotatePoints(rotationMatrix, screenRotation);

            Vector3 neighborPoint;
            float dist = 0;
            float dist2 = 0;
            float closestPointDist = Float.MAX_VALUE;
            float distLR = 0;
            float angle = 0;
            float neighborDist = 0, distToR = 0, distToL = 0;
            float xScale = ORTHO_RESOLUTION;
            float yScale = ORTHO_RESOLUTION;
            float distN, distS, distW, distE;
            float deviceRoll, deviceAltitude, deviceBearing;

            // ALITTUDE
            // altitude - very simple, triangulate between top and bottom point
            mReticlePoint.set(0.5f * xScale, 0.5f * yScale, -xScale);
            mOriginPoint.set(0.5f * xScale, 0.5f * yScale, 0);
            dist = Util.calcDistance(mReticlePoint, mSphereBottom);
            dist2 = Util.calcDistance(mReticlePoint, mSphereTop);
            distToL = Util.calcDistance(mOriginPoint, mSphereBottom);
            distToR = Util.calcDistance(mOriginPoint, mReticlePoint);
            distLR = Util.calcDistance(mSphereBottom, mReticlePoint);
            float altitude = RADIANS_TO_DEGREES * Util.calcAngle(distLR, Util.calcRadius(distToL, distToR, distLR)) - 90;

            // flip quadrant if closer to top of globe
            if (dist > dist2) {
                    altitude = -altitude;
            }

            deviceAltitude = altitude;
            if (Float.isNaN(deviceAltitude)) {
                    deviceAltitude = 0;
            }

            // BEARING - if held flat, we calculate one way, if not, we calculate
            // another
            mReticlePoint.set(0.5f * xScale, 0.5f * yScale, -xScale);
            if (Math.abs(deviceAltitude) < 75) {
                    int closestPoint = 0, pot1 = 0, pot2 = 0, neighbor = 0, left = 0;
                    int pointSize = NUM_POINTS;
                    // find closest point
                    for (int i = 0; i < pointSize; i++) {
                            Vector3 v = mVertices.get(i);
                            if (v.z < 0) {
                                    dist = Util.calcDistance(mReticlePoint, v);
                                    if (dist < closestPointDist) {
                                            closestPointDist = dist;
                                            closestPoint = i;
                                    }
                            }
                    }

                    // potential neighbors for azimuth
                    if (closestPoint % POINTS_PER_SEGMENT == 0) {
                            pot1 = closestPoint + 1;
                            pot2 = closestPoint + POINTS_PER_SEGMENT - 1;
                    } else if ((closestPoint + 1) % POINTS_PER_SEGMENT == 0) {
                            pot1 = closestPoint - 1;
                            pot2 = closestPoint - POINTS_PER_SEGMENT - 1;
                    } else {
                            pot1 = closestPoint + 1;
                            pot2 = closestPoint - 1;
                    }

                    // bounds check
                    if (pot1 >= 0 && pot2 >= 0 && pot1 < pointSize && pot2 < pointSize) {
                            dist = Util.calcDistance(mReticlePoint, mVertices.get(pot1));
                            dist2 = Util.calcDistance(mReticlePoint, mVertices.get(pot2));

                            if (dist < dist2) {
                                    neighbor = pot1;
                                    neighborDist = dist;
                            } else {
                                    neighbor = pot2;
                                    neighborDist = dist2;
                            }
                            // boundary cases:
                            // closest is 345, right is 0
                            // closest is 0, left is 345
                            if (neighbor < closestPoint) {
                                    // if we're 345, and point to right is 0, left should be
                                    // 345,
                                    // not 0
                                    if ((closestPoint + 1) % POINTS_PER_SEGMENT == 0 && neighbor == pot2) {
                                            left = closestPoint;
                                            distToL = closestPointDist;
                                            distToR = neighborDist;
                                    } else {
                                            left = neighbor;
                                            distToL = neighborDist;
                                            distToR = closestPointDist;
                                    }
                            } else {
                                    if (closestPoint % POINTS_PER_SEGMENT == 0 && neighbor == pot2) {
                                            left = neighbor;
                                            distToL = neighborDist;
                                            distToR = closestPointDist;
                                    } else {
                                            left = closestPoint;
                                            distToL = closestPointDist;
                                            distToR = neighborDist;
                                    }
                            }
                    }

                    if (neighbor <= NUM_POINTS - 1 && neighbor >= 0) {
                            neighborPoint = mVertices.get(neighbor);
                    } else {
                            if (neighbor < 0) {
                                    neighborPoint = mSphereBottom;
                            } else {
                                    neighborPoint = mSphereTop;
                            }
                    }

                    float angleIncrement = (360.0f / POINTS_PER_SEGMENT);
                    angle = left % POINTS_PER_SEGMENT * angleIncrement;
                    distLR = Util.calcDistance(mVertices.get(closestPoint), neighborPoint);

                    deviceBearing = Util.floatrev(angle + angleIncrement * ((FloatMath.cos(Util.calcAngleClamp(distToR, Util.calcRadius(distToL, distToR, distLR))) * distToL) / distLR) - 180);
            } else {
                    // calc current N Point distance from original compass points
                    mNorthAbsolute.set(0.5f * xScale, 0.2f * xScale + (yScale - xScale) / 2, 0);
                    mSouthAbsolute.set(0.5f * xScale, 0.8f * xScale + (yScale - xScale) / 2, 0);
                    mWestAbsolute.set(0.2f * xScale, 0.5f * yScale, 0);
                    mEastAbsolute.set(0.8f * xScale, 0.5f * yScale, 0);

                    distN = Util.calcDistance(mNorthReference, mNorthAbsolute);
                    distS = Util.calcDistance(mNorthReference, mSouthAbsolute);
                    distW = Util.calcDistance(mNorthReference, mWestAbsolute);
                    distE = Util.calcDistance(mNorthReference, mEastAbsolute);

                    distToL = Util.calcDistance(mOriginPoint, mNorthReference);
                    distToR = Util.calcDistance(mOriginPoint, mNorthAbsolute);
                    distLR = Util.calcDistance(mNorthReference, mNorthAbsolute);

                    float bearing = RADIANS_TO_DEGREES * -Util.calcAngle(distLR, Util.calcRadius(distToL, distToR, distLR));

                    if (distN < distS) {
                            if (distW < distE) {
                                    bearing = 360 - bearing;
                            }
                    } else {
                            if (distW < distE) {
                                    bearing += 180;
                            } else {
                                    bearing = 180 - bearing;
                            }
                    }

                    if (deviceAltitude > 0) {
                            bearing = 180 - bearing;
                    }
                    deviceBearing = Util.floatrev(bearing);
            }

            if (Float.isNaN(deviceBearing)) {
                    deviceBearing = 0;
            }

            // ROLL - calculate only when not held flat, ignore when altitude
            // (pitch) is less than 15
            if (Math.abs(deviceAltitude) < 75) {
                    mRollTopAbsolute.set(0.5f * xScale, 0.2f * yScale, 0);
                    mRollBottomAbsolute.set(0.5f * xScale, 0.8f * yScale, 0);

                    Vector3 upDown;
                    Vector3 topBot;
                    boolean altAbove = true;

                    if (deviceAltitude < 0) {
                            altAbove = false;
                            upDown = mRollBottomAbsolute;
                            topBot = mSphereTop;
                    } else {
                            upDown = mRollTopAbsolute;
                            topBot = mSphereBottom;
                    }

                    float adist = FloatMath.sqrt((upDown.x - mOriginPoint.x) * (upDown.x - mOriginPoint.x) + (upDown.y - mOriginPoint.y) * (upDown.y - mOriginPoint.y));
                    float bdist = FloatMath.sqrt((upDown.x - topBot.x) * (upDown.x - topBot.x) + (upDown.y - topBot.y) * (upDown.y - topBot.y));
                    float cdist = FloatMath.sqrt((mOriginPoint.x - topBot.x) * (mOriginPoint.x - topBot.x) + (mOriginPoint.y - topBot.y) * (mOriginPoint.y - topBot.y));

                    float val = ((adist * adist) + (cdist * cdist) - (bdist * bdist)) / (2 * adist * cdist);
                    if (val < 1) {
                            float atheta = (float) Math.acos(((adist * adist) + (cdist * cdist) - (bdist * bdist)) / (2 * adist * cdist)) * RADIANS_TO_DEGREES;
                            if (altAbove) {
                                    if (upDown.x - topBot.x < 0) {
                                            atheta = -atheta;
                                    }
                            } else {
                                    if (upDown.x - topBot.x > 0) {
                                            atheta = -atheta;
                                    }
                            }
                            atheta = Util.floatrev(atheta);
                            // restrict the roll to increments of 0.5 degrees - we don't
                            // need the precision here, also steady within 3 degrees
                            if (atheta <= 3 || atheta >= 357) {
                                    atheta = 0;
                            } else {
                                    atheta = 0.5f * Math.round(atheta / 0.5);
                            }
                            deviceRoll = atheta;
                    } else {
                            deviceRoll = 0;
                    }
            } else {
                    deviceRoll = 0;
            }
            if (Float.isNaN(deviceRoll)) {
                    deviceRoll = 0;
            }
            out[0] = deviceBearing;
            out[1] = deviceAltitude;
            out[2] = deviceRoll;
    }

    private void resetPoints() {
            mSphereTop.set(0, 0, 1);
            mSphereBottom.set(0, 0, -1);

            for (int j = 0; j < NUM_SEGMENTS; j++) {
                    int idx = j - 5;
                    float jCosVal = FloatMath.cos(DEGREES_TO_RADIANS * (float) (idx * 15));
                    float jCosValInv = FloatMath.cos(DEGREES_TO_RADIANS * (float) (90 - idx * 15));
                    for (int i = 0; i < POINTS_PER_SEGMENT; i++) {
                            float sinVal = FloatMath.sin(DEGREES_TO_RADIANS * (float) (i * (360 / POINTS_PER_SEGMENT)));
                            float cosVal = FloatMath.cos(DEGREES_TO_RADIANS * (float) (i * (360 / POINTS_PER_SEGMENT)));
                            mVertices.get(i + (POINTS_PER_SEGMENT * j)).set(sinVal * jCosVal * 1, -cosVal * jCosVal * 1, jCosValInv * 1);
                    }
            }

            // a reference to the N point on the sphere.
            mNorthReference.set(mVertices.get(POINTS_PER_SEGMENT * 5));
    }

    /**
     * With rotationMatrix, rotate the view components
     *
     * @param rotationMatrix
     */
    public void rotatePoints(Matrix4 rotationMatrix, int screenRotation) {
            resetPoints();
            final int width = ORTHO_RESOLUTION;
            final int height = ORTHO_RESOLUTION;
            final float orthoScale = 1.0f;
            mModelViewMatrix.idt().mul(mOrthographicProjectionMatrix).mul(rotationMatrix);
            switch (screenRotation) {
            case Surface.ROTATION_0:
            case Surface.ROTATION_90:
            case Surface.ROTATION_270:
                    for (Vector3 v : mOrthographicVertexBatch) {
                            vTemp.set(v.x, -v.y, -v.z, 0);
                            vTemp.mul(mModelViewMatrix);
                            v.x = (vTemp.x) * 0.5f * width * orthoScale + width / 2;
                            v.y = (vTemp.y) * 0.5f * width * orthoScale + width / 2 + (height - width) / 2;
                            v.z = vTemp.z * 0.5f * width * orthoScale;
                    }
                    break;
            // For 180, we have to reflect x and y values
            case Surface.ROTATION_180:
                    for (Vector3 v : mOrthographicVertexBatch) {
                            vTemp.set(v.x, -v.y, -v.z, 0);
                            vTemp.mul(mModelViewMatrix);
                            // reflect x and y axes...
                            v.x = (-vTemp.x) * 0.5f * width + width / 2;
                            v.y = (-vTemp.y) * 0.5f * width + width / 2 + (height - width) / 2;
                            v.z = vTemp.z * 0.5f * width;
                    }
                    break;
            }
    }
}

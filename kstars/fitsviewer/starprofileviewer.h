/*  StarProfileViewer
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    Based on the QT Surface Example http://doc.qt.io/qt-5.9/qtdatavisualization-surface-example.html
    and the QT Bars Example https://doc-snapshots.qt.io/qt5-5.9/qtdatavisualization-bars-example.html

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef STARPROFILEVIEWER_H
#define STARPROFILEVIEWER_H
#include <QtDataVisualization/qbar3dseries.h>
#include <QtDataVisualization/qbardataproxy.h>
#include <QtDataVisualization/q3dbars.h>
#include <QtDataVisualization/QCustom3DLabel>
#include <QtWidgets/QSlider>
#include <QDialog>

#include <QtWidgets/QWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtGui/QPainter>
#include <QtGui/QScreen>

#include <QtDataVisualization/QValue3DAxis>
#include <QtDataVisualization/Q3DTheme>
#include <QtDataVisualization/qabstract3dseries.h>
#include <QtGui/QImage>
#include <QtCore/qmath.h>
#include <QMessageBox>
#include "fitsdata.h"

using namespace QtDataVisualization;

class StarProfileViewer : public QDialog
{
    Q_OBJECT
public:
    explicit StarProfileViewer(QWidget *parent = nullptr);
    ~StarProfileViewer();

    void setBlackToYellowGradient();
    void setGreenToRedGradient();

    void loadData(FITSData *imageData, QRect sub, QList<Edge *> starCenters);
    float getImageDataValue(int x, int y);
    void toggleSlice();
    void updateVerticalAxis();
    void updateHFRandPeakSelection();
    void updateDisplayData();
    void updateScale();
    void enableTrackingBox(bool enable);
    void changeSelection();
    void updateSelectorBars(QPoint position);
    void toggleCutoffEnabled(bool enable);

public slots:
    void changeSelectionType(int type);
    void zoomViewTo(int where);
    void updateSampleSize(const QString &text);
    void updateColor(int selection);


signals:
    void sampleSizeUpdated(int size);
private:
    Q3DBars *m_graph;
    QValue3DAxis *m_pixelValueAxis;
    QCategory3DAxis *m_xPixelAxis;
    QCategory3DAxis *m_yPixelAxis;
    QBar3DSeries *m_3DPixelSeries;

    QBarDataArray *dataSet = nullptr;

    template <typename T>
    float getImageDataValue(int x, int y);
    void getSubFrameMinMax(float *subFrameMin, float *subFrameMax, double *dataMin, double *dataMax);

    QPushButton *HFRReport;
    QLabel *reportBox;
    QPushButton *showPeakValues;
    QCheckBox *autoScale;
    QPushButton *showScaling;
    QComboBox *sampleSize;
    QComboBox *selectionType;
    QComboBox *zoomView;
    QCheckBox *exploreMode;
    QLabel *pixelReport;
    QLabel *maxValue;
    QLabel *minValue;
    QLabel *cutoffValue;
    QPushButton *sliceB;
    FITSData * imageData;
    QRect subFrame;

    QSlider *blackPointSlider;
    QSlider *whitePointSlider;
    QSlider *cutoffSlider;
    QSlider *verticalSelector;
    QSlider *horizontalSelector;
    QList<Edge *> starCenters;

    bool cutOffEnabled;

    int convertToSliderValue(float value);
    float convertFromSliderValue(int value);
    void updatePixelReport();

};

#endif // STARPROFILEVIEWER_H

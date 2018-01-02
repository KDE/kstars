/*  StarProfileViewer
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    Based on the QT Surface Example http://doc.qt.io/qt-5.9/qtdatavisualization-surface-example.html

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef STARPROFILEVIEWER_H
#define STARPROFILEVIEWER_H
#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QtDataVisualization/QHeightMapSurfaceDataProxy>
#include <QtDataVisualization/QSurface3DSeries>
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
#include <QtGui/QImage>
#include <QtCore/qmath.h>
#include <QMessageBox>

using namespace QtDataVisualization;

class StarProfileViewer : public QDialog
{
    Q_OBJECT
public:
    explicit StarProfileViewer(QWidget *parent = nullptr);
    ~StarProfileViewer();

    void enableHeightMapModel(bool enable);

    //! [0]
    void toggleModeNone() { m_graph->setSelectionMode(QAbstract3DGraph::SelectionNone); }
    void toggleModeItem() { m_graph->setSelectionMode(QAbstract3DGraph::SelectionItem); }
    void toggleModeSliceRow() { m_graph->setSelectionMode(QAbstract3DGraph::SelectionItemAndRow
                                                          | QAbstract3DGraph::SelectionSlice); }
    void toggleModeSliceColumn() { m_graph->setSelectionMode(QAbstract3DGraph::SelectionItemAndColumn
                                                             | QAbstract3DGraph::SelectionSlice); }
    //! [0]

    void setBlackToYellowGradient();
    void setGreenToRedGradient();

    void loadData(QImage heightMapImage, double HFR);

private:
    Q3DSurface *m_graph;
    QHeightMapSurfaceDataProxy *m_heightMapProxy;
    QSurface3DSeries *m_heightMapSeries;

    QLabel *HFRReport;
    int m_heightMapWidth;
    int m_heightMapHeight;
};

#endif // STARPROFILEVIEWER_H

/*  StarProfileViewer
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    Based on the QT Surface Example http://doc.qt.io/qt-5.9/qtdatavisualization-surface-example.html

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "starprofileviewer.h"

using namespace QtDataVisualization;

StarProfileViewer::StarProfileViewer(QWidget *parent) : QDialog(parent)
{

#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    m_graph = new Q3DSurface();
    QWidget *container = QWidget::createWindowContainer(m_graph);

    if (!m_graph->hasContext()) {
        QMessageBox msgBox;
        msgBox.setText("Couldn't initialize the OpenGL context.");
        msgBox.exec();
        return;
    }

    QSize screenSize = m_graph->screen()->size();
    container->setMinimumSize(QSize(500, 300));
    container->setMaximumSize(screenSize);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    container->setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *vLayout = new QVBoxLayout(this);
    QHBoxLayout *hLayout = new QHBoxLayout();

    vLayout->addWidget(container, 1);
    vLayout->addLayout(hLayout);
    hLayout->setAlignment(Qt::AlignLeft);

    this->setWindowTitle(QStringLiteral("View Star Profile"));

    QRadioButton *modeNoneRB = new QRadioButton(this);
    modeNoneRB->setText(QStringLiteral("No selection"));
    modeNoneRB->setChecked(false);

    QRadioButton *modeItemRB = new QRadioButton(this);
    modeItemRB->setText(QStringLiteral("Item"));
    modeItemRB->setChecked(false);

    QRadioButton *modeSliceRowRB = new QRadioButton(this);
    modeSliceRowRB->setText(QStringLiteral("Row Slice"));
    modeSliceRowRB->setChecked(false);

    QRadioButton *modeSliceColumnRB = new QRadioButton(this);
    modeSliceColumnRB->setText(QStringLiteral("Column Slice"));
    modeSliceColumnRB->setChecked(false);

    QLabel *HFRReportLabel = new QLabel(this);
    HFRReportLabel->setText("HFR:");
    HFRReport = new QLabel(this);
    HFRReport->setText("0");

    hLayout->addWidget(modeNoneRB);
    hLayout->addWidget(modeItemRB);
    hLayout->addWidget(modeSliceRowRB);
    hLayout->addWidget(modeSliceColumnRB);
    hLayout->addWidget(HFRReportLabel);
    hLayout->addWidget(HFRReport);

    QObject::connect(modeNoneRB, &QRadioButton::toggled,
                     this, &StarProfileViewer::toggleModeNone);
    QObject::connect(modeItemRB,  &QRadioButton::toggled,
                     this, &StarProfileViewer::toggleModeItem);
    QObject::connect(modeSliceRowRB,  &QRadioButton::toggled,
                     this, &StarProfileViewer::toggleModeSliceRow);
    QObject::connect(modeSliceColumnRB,  &QRadioButton::toggled,
                     this, &StarProfileViewer::toggleModeSliceColumn);

    modeItemRB->setChecked(true);

    m_graph->setAxisX(new QValue3DAxis);
    m_graph->setAxisY(new QValue3DAxis);
    m_graph->setAxisZ(new QValue3DAxis);
    m_graph->activeTheme()->setType(Q3DTheme::Theme(3)); //Stone Moss

    m_graph->scene()->activeCamera()->setCameraPosition(0, 10);

    show();
}

StarProfileViewer::~StarProfileViewer()
{
    delete m_graph;
}

void StarProfileViewer::loadData(QImage heightMapImage, double HFR)
{
    HFRReport->setText(QString::number(HFR, 'f', 2));
    m_heightMapProxy = new QHeightMapSurfaceDataProxy(heightMapImage);
    m_heightMapSeries = new QSurface3DSeries(m_heightMapProxy);
    m_heightMapSeries->setItemLabelFormat(QStringLiteral("(@xLabel, @zLabel): @yLabel"));
    m_heightMapProxy->setValueRanges(0, heightMapImage.width(), 0, heightMapImage.height());
    m_heightMapWidth = heightMapImage.width();
    m_heightMapHeight = heightMapImage.height();

        m_heightMapSeries->setDrawMode(QSurface3DSeries::DrawSurface);
        m_heightMapSeries->setFlatShadingEnabled(false);

        m_graph->axisX()->setLabelFormat("%.1f");
        m_graph->axisZ()->setLabelFormat("%.1f");
        m_graph->axisX()->setRange(0.0f, m_heightMapWidth);
        m_graph->axisY()->setAutoAdjustRange(true);
        m_graph->axisZ()->setRange(0.0f, m_heightMapHeight);

        m_graph->axisX()->setTitle(QStringLiteral("X Pixels"));
        m_graph->axisY()->setTitle(QStringLiteral("Pixel Value"));
        m_graph->axisZ()->setTitle(QStringLiteral("Y Pixels"));

        if(m_graph->seriesList().size()>0)
            m_graph->removeSeries(m_graph->seriesList().at(0));
        m_graph->addSeries(m_heightMapSeries);
        setGreenToRedGradient();

}

void StarProfileViewer::setBlackToYellowGradient()
{
    //! [7]
    QLinearGradient gr;
    gr.setColorAt(0.0, Qt::black);
    gr.setColorAt(0.33, Qt::blue);
    gr.setColorAt(0.67, Qt::red);
    gr.setColorAt(1.0, Qt::yellow);

    m_graph->seriesList().at(0)->setBaseGradient(gr);
    m_graph->seriesList().at(0)->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    //! [7]
}

void StarProfileViewer::setGreenToRedGradient()
{
    QLinearGradient gr;
    gr.setColorAt(0.0, Qt::darkGreen);
    gr.setColorAt(0.5, Qt::yellow);
    gr.setColorAt(0.8, Qt::red);
    gr.setColorAt(1.0, Qt::darkRed);

    m_graph->seriesList().at(0)->setBaseGradient(gr);
    m_graph->seriesList().at(0)->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
}


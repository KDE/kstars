/*
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    Based on the QT Surface Example https://doc.qt.io/qt-5.9/qtdatavisualization-surface-example.html
    and the QT Bars Example https://doc-snapshots.qt.io/qt5-5.9/qtdatavisualization-bars-example.html

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "starprofileviewer.h"
#include <KLocalizedString>

using namespace QtDataVisualization;

StarProfileViewer::StarProfileViewer(QWidget *parent) : QDialog(parent)
{

#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    m_graph = new Q3DBars();
    m_pixelValueAxis = m_graph->valueAxis();
    m_xPixelAxis = m_graph->columnAxis();
    m_yPixelAxis = m_graph->rowAxis();

    m_pixelValueAxis->setTitle(i18n("Pixel Values"));
    m_pixelValueAxis->setLabelAutoRotation(30.0f);
    m_pixelValueAxis->setTitleVisible(true);

    m_xPixelAxis->setTitle(i18n("Horizontal"));
    m_xPixelAxis->setLabelAutoRotation(30.0f);
    m_xPixelAxis->setTitleVisible(true);
    m_yPixelAxis->setTitle(i18n("Vertical"));
    m_yPixelAxis->setLabelAutoRotation(30.0f);
    m_yPixelAxis->setTitleVisible(true);

    m_3DPixelSeries = new QBar3DSeries;

    m_3DPixelSeries->setMesh(QAbstract3DSeries::MeshBevelBar);
    m_graph->addSeries(m_3DPixelSeries);

    m_graph->activeTheme()->setLabelBackgroundEnabled(false);

    QWidget *container = QWidget::createWindowContainer(m_graph);

    if (!m_graph->hasContext()) {
        QMessageBox msgBox;
        msgBox.setText(i18n("Couldn't initialize the OpenGL context."));
        msgBox.exec();
        return;
    }

    QSize screenSize = m_graph->screen()->size();
    container->setMinimumSize(QSize(300, 500));
    container->setMaximumSize(screenSize);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    container->setFocusPolicy(Qt::StrongFocus);

    this->setWindowTitle(i18nc("@title:window", "View Star Profile"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *topLayout = new QHBoxLayout();
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    QWidget* rightWidget = new QWidget();
    rightWidget->setVisible(false);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    QGridLayout *sliderLayout = new QGridLayout();

    topLayout->addWidget(container, 1);
    topLayout->addWidget(rightWidget);
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(controlsLayout);
    controlsLayout->setAlignment(Qt::AlignLeft);

    maxValue=new QLabel(this);
    maxValue->setToolTip(i18n("Maximum Value on the graph"));
    cutoffValue=new QLabel(this);
    cutoffValue->setToolTip(i18n("Cuttoff Maximum for eliminating hot pixels and bright stars."));

    QCheckBox *toggleEnableCutoff= new QCheckBox(this);
    toggleEnableCutoff->setToolTip(i18n("Enable or Disable the Max Value Cutoff"));
    toggleEnableCutoff->setText(i18n("Toggle Cutoff"));
    toggleEnableCutoff->setChecked(false);

    blackPointSlider=new QSlider( Qt::Vertical, this);
    blackPointSlider->setToolTip(i18n("Sets the Minimum Value on the graph"));
    sliderLayout->addWidget(blackPointSlider,0,0);
    sliderLayout->addWidget(new QLabel(i18n("Min")),1,0);

    whitePointSlider=new QSlider( Qt::Vertical, this);
    whitePointSlider->setToolTip(i18n("Sets the Maximum Value on the graph"));
    sliderLayout->addWidget(whitePointSlider,0,1);
    sliderLayout->addWidget(new QLabel(i18n("Max")),1,1);

    cutoffSlider=new QSlider( Qt::Vertical, this);
    cutoffSlider->setToolTip(i18n("Sets the Cuttoff Maximum for eliminating hot pixels and bright stars."));
    sliderLayout->addWidget(cutoffSlider,0,2);
    sliderLayout->addWidget(new QLabel(i18n("Cut")),1,2);
    cutoffSlider->setEnabled(false);

    minValue = new QLabel(this);
    minValue->setToolTip(i18n("Minimum Value on the graph"));

    autoScale = new QCheckBox(this);
    autoScale->setText(i18n("AutoScale"));
    autoScale->setToolTip(i18n("Automatically scales the sliders for the subFrame.\nUncheck to leave them unchanged when you pan around."));
    autoScale->setChecked(true);

    showScaling = new QPushButton(this);
    showScaling->setIcon(QIcon::fromTheme("transform-move-vertical"));
    showScaling->setCheckable(true);
    showScaling->setMaximumSize(22, 22);
    showScaling->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    showScaling->setToolTip(i18n("Hides and shows the scaling side panel"));
    showScaling->setChecked(false);

    rightLayout->addWidget(toggleEnableCutoff);
    rightLayout->addWidget(cutoffValue);
    rightLayout->addWidget(maxValue);
    rightLayout->addLayout(sliderLayout);
    rightLayout->addWidget(minValue);
    rightLayout->addWidget(autoScale);

    selectionType = new QComboBox(this);
    selectionType->setToolTip(i18n("Changes the type of selection"));
    selectionType->addItem(i18n("Item"));
    selectionType->addItem(i18n("Horizontal"));
    selectionType->addItem(i18n("Vertical"));
    selectionType->setCurrentIndex(0);

    sliceB = new QPushButton(this);
    sliceB->setIcon(QIcon::fromTheme("view-object-histogram-linear"));
    sliceB->setCheckable(true);
    sliceB->setMaximumSize(22, 22);
    sliceB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sliceB->setToolTip(i18n("Toggles the slice view when horizontal or vertical items are selected"));
    sliceB->setCheckable(true);
    sliceB->setChecked(false);
    sliceB->setEnabled(false);
    sliceB->setDefault(false);

    showCoordinates = new QPushButton(this);
    showCoordinates->setIcon(QIcon::fromTheme("coordinate"));
    showCoordinates->setCheckable(true);
    showCoordinates->setMaximumSize(22, 22);
    showCoordinates->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    showCoordinates->setToolTip(i18n("Shows the x, y coordinates of star centers in the frame"));
    showCoordinates->setChecked(false);

    HFRReport = new QPushButton(this);
    HFRReport->setToolTip(i18n("Shows the HFR of stars in the frame"));
    HFRReport->setIcon(QIcon::fromTheme("tool-measure"));
    HFRReport->setCheckable(true);
    HFRReport->setMaximumSize(22, 22);
    HFRReport->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    HFRReport->setChecked(true);

    reportBox = new QLabel(this);

    showPeakValues = new QPushButton(this);
    showPeakValues->setIcon(QIcon::fromTheme("kruler-east"));
    showPeakValues->setCheckable(true);
    showPeakValues->setMaximumSize(22, 22);
    showPeakValues->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    showPeakValues->setToolTip(i18n("Shows the peak values of star centers in the frame"));
    showPeakValues->setChecked(true);

    sampleSize = new QComboBox(this);
    sampleSize->setToolTip(i18n("Changes the sample size shown in the graph"));
    sampleSize->addItem(QString::number(16));
    sampleSize->addItem(QString::number(32));
    sampleSize->addItem(QString::number(64));
    sampleSize->addItem(QString::number(128));
    sampleSize->addItem(QString::number(256));
    sampleSize->addItem(QString::number(512));
    sampleSize->setCurrentIndex(3);
    sampleSize->setVisible(false);

    zoomView = new QComboBox(this);
    zoomView->setToolTip(i18n("Zooms the view to preset locations."));
    zoomView->addItem(i18n("ZoomTo:"));
    zoomView->addItem(i18n("Front"));
    zoomView->addItem(i18n("Front High"));
    zoomView->addItem(i18n("Overhead"));
    zoomView->addItem(i18n("Iso. L"));
    zoomView->addItem(i18n("Iso. R"));
    zoomView->addItem(i18n("Selected"));
    zoomView->setCurrentIndex(0);

    QPushButton *selectorsVisible = new QPushButton(this);
    selectorsVisible->setIcon(QIcon::fromTheme("adjustlevels"));
    selectorsVisible->setCheckable(true);
    selectorsVisible->setMaximumSize(22, 22);
    selectorsVisible->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectorsVisible->setToolTip(i18n("Hides and shows the Vertical and Horizontal Selection Sliders"));
    selectorsVisible->setChecked(false);

    controlsLayout->addWidget(sampleSize);
    controlsLayout->addWidget(selectionType);
    controlsLayout->addWidget(selectorsVisible);
    controlsLayout->addWidget(sliceB);
    controlsLayout->addWidget(showScaling);
    //bottomLayout->addWidget(barSpacing);
    controlsLayout->addWidget(zoomView);
    //bottomLayout->addWidget(color);
    controlsLayout->addWidget(showCoordinates);
    controlsLayout->addWidget(HFRReport);
    controlsLayout->addWidget(showPeakValues);
    controlsLayout->addWidget(reportBox);

    QWidget *bottomSliderWidget= new QWidget(this);
    QGridLayout *bottomSliders = new QGridLayout(bottomSliderWidget);
    bottomSliderWidget->setLayout(bottomSliders);
    mainLayout->addWidget(bottomSliderWidget);
    bottomSliderWidget->setVisible(false);

    verticalSelector = new QSlider(Qt::Horizontal, this);
    verticalSelector->setToolTip(i18n("Selects the Vertical Value"));
    horizontalSelector = new QSlider(Qt::Horizontal, this);
    horizontalSelector->setToolTip(i18n("Selects the Horizontal Value"));

    bottomSliders->addWidget(new QLabel(i18n("Vertical: ")), 0, 0);
    bottomSliders->addWidget(verticalSelector, 0, 1);
    bottomSliders->addWidget(new QLabel(i18n("Horizontal: ")), 1, 0);
    bottomSliders->addWidget(horizontalSelector, 1, 1);

    QWidget *bottomControlsWidget= new QWidget(this);
    QHBoxLayout *bottomControlLayout = new QHBoxLayout(bottomControlsWidget);
    mainLayout->addWidget(bottomControlsWidget);\
    bottomControlsWidget->setVisible(false);

    exploreMode = new QPushButton(this);
    exploreMode->setIcon(QIcon::fromTheme("visibility"));
    exploreMode->setCheckable(true);
    exploreMode->setMaximumSize(22, 22);
    exploreMode->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    exploreMode->setToolTip(i18n("Zooms automatically as the sliders change"));
    exploreMode->setChecked(true);

    QDial *barSpacing=new QDial(this);
    barSpacing->setMinimum(0);
    barSpacing->setMaximum(100);
    barSpacing->setValue(50);
    barSpacing->setMaximumSize(32, 32);
    barSpacing->setWrapping(false);
    m_graph->setBarSpacing(QSizeF(0.5,0.5));

    QComboBox *color = new QComboBox(this);
    color->setToolTip(i18n("Changes the color scheme"));

    QLinearGradient grGtoR(50, 1, 0, 0);
    grGtoR.setColorAt(1.0, Qt::darkGreen);
    grGtoR.setColorAt(0.5, Qt::yellow);
    grGtoR.setColorAt(0.2, Qt::red);
    grGtoR.setColorAt(0.0, Qt::darkRed);
    QPixmap pm(50, 10);
    QPainter pmp(&pm);
    pmp.setPen(Qt::NoPen);
    pmp.setBrush(QBrush(grGtoR));
    pmp.drawRect(0, 0, 50, 10);
    color->addItem("");
    color->setItemIcon(0,QIcon(pm));

    QLinearGradient grBtoY(50, 1, 0, 0);
    grBtoY.setColorAt(1.0, Qt::black);
    grBtoY.setColorAt(0.67, Qt::blue);
    grBtoY.setColorAt(0.33, Qt::red);
    grBtoY.setColorAt(0.0, Qt::yellow);
    pmp.setBrush(QBrush(grBtoY));
    pmp.drawRect(0, 0, 50, 10);
    color->addItem("");
    color->setItemIcon(1,QIcon(pm));

    color->setIconSize(QSize(50, 10));
    color->setCurrentIndex(0);
    color->setMaximumWidth(80);

    pixelReport = new QLabel("", bottomControlsWidget);

    bottomControlLayout->addWidget(exploreMode);
    bottomControlLayout->addWidget(barSpacing);
    bottomControlLayout->addWidget(color);
    bottomControlLayout->addWidget(pixelReport);

    QObject::connect(selectionType,  SIGNAL(currentIndexChanged(int)),
                     this, SLOT(changeSelectionType(int)));
    QObject::connect(zoomView,  SIGNAL(currentIndexChanged(int)),
                     this, SLOT(zoomViewTo(int)));
    QObject::connect(sliceB,  &QPushButton::pressed,
                     this, &StarProfileViewer::toggleSlice);
    QObject::connect(showCoordinates,  &QCheckBox::toggled,
                     this, &StarProfileViewer::updateHFRandPeakSelection);
    QObject::connect(HFRReport,  &QCheckBox::toggled,
                     this, &StarProfileViewer::updateHFRandPeakSelection);
    QObject::connect(showPeakValues,  &QCheckBox::toggled,
                     this, &StarProfileViewer::updateHFRandPeakSelection);
    QObject::connect(blackPointSlider,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateVerticalAxis);
    QObject::connect(whitePointSlider,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateVerticalAxis);
    QObject::connect(cutoffSlider,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateDisplayData);
    QObject::connect(autoScale,  &QCheckBox::toggled,
                     this, &StarProfileViewer::updateScale);
    QObject::connect(showScaling,  &QCheckBox::toggled,
                     rightWidget, &QWidget::setVisible);
    QObject::connect(sampleSize,  SIGNAL(currentIndexChanged(QString)),
                     this, SLOT(updateSampleSize(QString)));
    QObject::connect(color,  SIGNAL(currentIndexChanged(int)),
                     this, SLOT(updateColor(int)));
    QObject::connect(verticalSelector,  &QSlider::valueChanged,
                     this, &StarProfileViewer::changeSelection);
    QObject::connect(horizontalSelector,  &QSlider::valueChanged,
                     this, &StarProfileViewer::changeSelection);
    QObject::connect(selectorsVisible,  &QCheckBox::toggled,
                     bottomSliderWidget, &QWidget::setVisible);
    QObject::connect(selectorsVisible,  &QCheckBox::toggled,
                     bottomControlsWidget, &QWidget::setVisible);
    QObject::connect(toggleEnableCutoff,  &QCheckBox::toggled,
                     this, &StarProfileViewer::toggleCutoffEnabled);
    QObject::connect(m_3DPixelSeries,  &QBar3DSeries::selectedBarChanged,
                     this, &StarProfileViewer::updateSelectorBars);
    QObject::connect(barSpacing,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateBarSpacing);

    m_graph->activeTheme()->setType(Q3DTheme::Theme(3)); //Stone Moss

    setGreenToRedGradient();

    m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetFront);
    m_graph->scene()->activeCamera()->setTarget(QVector3D(0.0f, 0.0f, 0.0f));
    m_graph->scene()->activeCamera()->setZoomLevel(110);

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    #ifdef Q_OS_OSX
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);
    #endif

    show();
}

StarProfileViewer::~StarProfileViewer()
{
    delete m_graph;
}

void StarProfileViewer::loadData(QSharedPointer<FITSData> data, QRect sub, QList<Edge *> centers)
{
    if(data)
    {
        imageData = data;
        subFrame=sub;
        starCenters=centers;               

        switch (data->getStatistics().dataType)
        {
        case TBYTE:
            loadDataPrivate<uint8_t>();
            break;

        case TSHORT:
            loadDataPrivate<int16_t>();
            break;

        case TUSHORT:
            loadDataPrivate<uint16_t>();
            break;

        case TLONG:
            loadDataPrivate<int32_t>();
            break;

        case TULONG:
            loadDataPrivate<uint32_t>();
            break;

        case TFLOAT:
            loadDataPrivate<float>();
            break;

        case TLONGLONG:
            loadDataPrivate<int64_t>();
            break;

        case TDOUBLE:
            loadDataPrivate<double>();
            break;
        }

        updateScale();

        // Add data to the data proxy (the data proxy assumes ownership of it)
        // We will retain a copy of the data set so that we can update the display
        updateDisplayData();

        updateHFRandPeakSelection();

        horizontalSelector->setRange(0, subFrame.width()-1);
        verticalSelector->setRange(0, subFrame.width()-1);  //Width and height are the same
    }
}

template <typename T>
void StarProfileViewer::loadDataPrivate()
{
    // Create data arrays
    dataSet = new QBarDataArray;
    QBarDataRow *dataRow;
    dataSet->reserve(subFrame.height());
    QStringList rowLabels;
    QStringList columnLabels;

    auto *buffer = reinterpret_cast<T const *>(imageData->getImageBuffer());
    int width = imageData->width();

    for (int j = subFrame.y(); j < subFrame.y() + subFrame.height(); j++)
    {
        if( j % 10 == 0 )
            rowLabels << QString::number(j);
        else
            rowLabels << "";
        dataRow = new QBarDataRow(subFrame.width());
        int x = 0;
        for (int i = subFrame.x(); i < subFrame.x() + subFrame.width(); i++)
        {
            if( i % 10 == 0 )
                columnLabels << QString::number(i);
            else
                columnLabels << "";
            if( i > 0 && i < imageData->width() && j > 0 && j < imageData->height())
                (*dataRow)[x].setValue(*(buffer + i + j * width));
            x++;
        }
        dataSet->insert(0, dataRow); //Note the row axis is displayed in the opposite direction of the y axis in the image.
    }

    std::reverse(rowLabels.begin(), rowLabels.end());

    m_3DPixelSeries->dataProxy()->setRowLabels(rowLabels);
    m_3DPixelSeries->dataProxy()->setColumnLabels(columnLabels);
}

void StarProfileViewer::toggleCutoffEnabled(bool enable)
{
    cutoffSlider->setEnabled(enable);
    cutOffEnabled = enable;
    updateDisplayData();
}

void StarProfileViewer::updateScale()
{

    //We need to disconnect these so that changing their ranges doesn't affect things
    QObject::disconnect(blackPointSlider,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateVerticalAxis);
    QObject::disconnect(whitePointSlider,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateVerticalAxis);
    QObject::disconnect(cutoffSlider,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateDisplayData);

    float subFrameMin, subFrameMax;
    double dataMin, dataMax;
    float min, max;
    getSubFrameMinMax(&subFrameMin, &subFrameMax, &dataMin, &dataMax);

    int sliderDataMin = convertToSliderValue(dataMin) - 1; //Expands the slider range a little beyond the max and min values
    int sliderDataMax = convertToSliderValue(dataMax) + 1;

    if(autoScale->isChecked())
    {
        min = subFrameMin;
        max = subFrameMax;
        int sliderMin = convertToSliderValue(min) - 1; //Expands the slider range a little beyond the max and min values
        int sliderMax = convertToSliderValue(max) + 1;
        blackPointSlider->setRange(sliderMin, sliderMax);
        blackPointSlider->setTickInterval((sliderMax - sliderMin) / 100);
        whitePointSlider->setRange(sliderMin, sliderMax);
        whitePointSlider->setTickInterval((sliderMax - sliderMin) / 100);
        cutoffSlider->setRange(sliderMin, sliderDataMax);
        cutoffSlider->setTickInterval((sliderDataMax - sliderMin) / 100);
        blackPointSlider->setValue(sliderMin);
        whitePointSlider->setValue(sliderMax);
        cutoffSlider->setValue(sliderDataMax);
    }
    else
    {
        min = convertFromSliderValue(blackPointSlider->value());
        max = convertFromSliderValue(whitePointSlider->value());
        blackPointSlider->setRange(sliderDataMin, sliderDataMax);
        blackPointSlider->setTickInterval((sliderDataMax - sliderDataMin) / 100);
        whitePointSlider->setRange(sliderDataMin, sliderDataMax);
        whitePointSlider->setTickInterval((sliderDataMax - sliderDataMin) / 100);
        cutoffSlider->setRange(sliderDataMin, sliderDataMax);
        cutoffSlider->setTickInterval((sliderDataMax - sliderDataMin) / 100);

    }
    m_pixelValueAxis->setRange(min, max);

    if(cutOffEnabled)
        cutoffValue->setText(i18n("Cut: %1", roundf(convertFromSliderValue(cutoffSlider->value()) * 100) / 100));
    else
        cutoffValue->setText("Cut Disabled");

    if(max < 10 )
    {
        m_pixelValueAxis->setLabelFormat(QString(QStringLiteral("%.3f ")));
        m_3DPixelSeries->setItemLabelFormat(QString(QStringLiteral("%.3f ")));
        maxValue->setText(i18n("Max: %1", roundf(max * 100) / 100));
        minValue->setText(i18n("Min: %1", roundf(min * 100) / 100));
    }
    else
    {
        m_pixelValueAxis->setLabelFormat(QString(QStringLiteral("%.0f ")));
        m_3DPixelSeries->setItemLabelFormat(QString(QStringLiteral("%.0f ")));
        maxValue->setText(i18n("Max: %1", max));
        minValue->setText(i18n("Min: %1", min));
    }

    QObject::connect(blackPointSlider,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateVerticalAxis);
    QObject::connect(whitePointSlider,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateVerticalAxis);
    QObject::connect(cutoffSlider,  &QSlider::valueChanged,
                     this, &StarProfileViewer::updateDisplayData);
}

void StarProfileViewer::updateBarSpacing(int value)
{
    float spacing = (float)value/100.0;
    m_graph->setBarSpacing(QSizeF(spacing, spacing));
}

void StarProfileViewer::zoomViewTo(int where)
{
    if(where > 6) //One of the star centers
    {
        int star = where - 7;
        int x = starCenters[star]->x - subFrame.x();
        int y = subFrame.height() - (starCenters[star]->y - subFrame.y());
        m_graph->primarySeries()->setSelectedBar(QPoint( y , x )); //Note row, column    y, x
        where = 6; //This is so it will zoom to the target.
    }

    switch (where) {
    case 0: //Zoom To
        break;

    case 1: //Front
        m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetFront);
        m_graph->scene()->activeCamera()->setTarget(QVector3D(0.0f, 0.0f, 0.0f));
        m_graph->scene()->activeCamera()->setZoomLevel(110);
        zoomView->setCurrentIndex(0);
        break;

    case 2: //Front High
        m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetFrontHigh);
        m_graph->scene()->activeCamera()->setTarget(QVector3D(0.0f, 0.0f, 0.0f));
        m_graph->scene()->activeCamera()->setZoomLevel(110);
        zoomView->setCurrentIndex(0);
        break;

    case 3: //Overhead
        m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetDirectlyAbove);
        m_graph->scene()->activeCamera()->setTarget(QVector3D(0.0f, 0.0f, 0.0f));
        m_graph->scene()->activeCamera()->setZoomLevel(110);
        zoomView->setCurrentIndex(0);
        break;

    case 4: //Isometric L
        m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetIsometricLeftHigh);
        m_graph->scene()->activeCamera()->setTarget(QVector3D(0.0f, 0.0f, 0.0f));
        m_graph->scene()->activeCamera()->setZoomLevel(110);
        zoomView->setCurrentIndex(0);
        break;

    case 5: //Isometric R
        m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetIsometricRightHigh);
        m_graph->scene()->activeCamera()->setTarget(QVector3D(0.0f, 0.0f, 0.0f));
        m_graph->scene()->activeCamera()->setZoomLevel(110);
        zoomView->setCurrentIndex(0);
        break;

    case 6: //Selected Item
    {
        QPoint selectedBar = m_graph->selectedSeries()
                ? m_graph->selectedSeries()->selectedBar()
                : QBar3DSeries::invalidSelectionPosition();
        if (selectedBar != QBar3DSeries::invalidSelectionPosition())
        {
            QVector3D target;
            float xMin = m_graph->columnAxis()->min();
            float xRange = m_graph->columnAxis()->max() - xMin;
            float zMin = m_graph->rowAxis()->min();
            float zRange = m_graph->rowAxis()->max() - zMin;
            target.setX((selectedBar.y() - xMin) / xRange * 2.0f - 1.0f);
            target.setZ((selectedBar.x() - zMin) / zRange * 2.0f - 1.0f);

            qreal endAngleX = qAtan(qreal(target.z() / target.x())) / M_PI * -180.0 + 90.0;
            if (target.x() > 0.0f)
                endAngleX -= 180.0f;
            float barValue = m_graph->selectedSeries()->dataProxy()->itemAt(selectedBar.x(),
                                                                            selectedBar.y())->value();
            float endAngleY = 60.0f;
            float zoom = 150 * 1/qSqrt(barValue / convertFromSliderValue(whitePointSlider->value()));
            m_graph->scene()->activeCamera()->setCameraPosition(endAngleX, endAngleY, zoom);
            m_graph->scene()->activeCamera()->setTarget(target);


        }
        zoomView->setCurrentIndex(0);
        break;
    }


    default:
        zoomView->setCurrentIndex(0);
        break;
    }
}

void StarProfileViewer::changeSelectionType(int type)
{
    switch (type) {
    case 0:
        m_graph->setSelectionMode(QAbstract3DGraph::SelectionItem);
        m_graph->scene()->setSlicingActive(false);
        sliceB->setEnabled(false);
        break;

    case 1:
        m_graph->setSelectionMode(QAbstract3DGraph::SelectionItemAndRow);
        sliceB->setEnabled(true);
        break;

    case 2:
        m_graph->setSelectionMode(QAbstract3DGraph::SelectionItemAndColumn);
        sliceB->setEnabled(true);
        break;

    default:
        break;
    }
}

void StarProfileViewer::changeSelection()
{
    int x = horizontalSelector->value();
    int y = verticalSelector->value();
    m_graph->primarySeries()->setSelectedBar(QPoint( y , x )); //Note row, column    y, x
    if(exploreMode->isChecked())
        zoomViewTo(6); //Zoom to SelectedItem
    updatePixelReport();
}

void StarProfileViewer::updatePixelReport()
{
    int x = horizontalSelector->value();
    int y = verticalSelector->value();
    //They need to be shifted to the location of the subframe
    x += subFrame.x();
    y = (subFrame.height() - 1 - y) + subFrame.y(); //Note: Y is in reverse order on the graph.
    float barValue = getImageDataValue(x, y);
    pixelReport->setText(i18n("Selected Pixel: (%1, %2): %3", x + 1, y + 1, roundf(barValue * 100) / 100)); //Have to add 1 because humans start counting at 1

}


void StarProfileViewer::updateSelectorBars(QPoint position)
{
    //Note that we need to disconnect and then reconnect to avoid triggering changeSelection
    QObject::disconnect(verticalSelector,  &QSlider::valueChanged,
                     this, &StarProfileViewer::changeSelection);
    QObject::disconnect(horizontalSelector,  &QSlider::valueChanged,
                     this, &StarProfileViewer::changeSelection);

    //Note row, column    y, x
    verticalSelector->setValue(position.x());
    horizontalSelector->setValue(position.y());
    updatePixelReport();

    QObject::connect(verticalSelector,  &QSlider::valueChanged,
                     this, &StarProfileViewer::changeSelection);
    QObject::connect(horizontalSelector,  &QSlider::valueChanged,
                     this, &StarProfileViewer::changeSelection);
}

void StarProfileViewer::updateSampleSize(const QString &text)
{
    emit sampleSizeUpdated(text.toInt());
}

void StarProfileViewer::enableTrackingBox(bool enable)
{
    sampleSize->setVisible(enable);
}

void StarProfileViewer::updateDisplayData()
{
    if(cutOffEnabled)
        cutoffValue->setText(i18n("Cut: %1", roundf(convertFromSliderValue(cutoffSlider->value()) * 100) / 100));
    else
        cutoffValue->setText(i18n("Cut Disabled"));
    if(dataSet != nullptr)
    {
        QBarDataArray *displayDataSet = new QBarDataArray;
        displayDataSet->reserve(dataSet->size());

        for (int row = 0; row < dataSet->size(); row++)
        {
            QBarDataRow *dataRow = dataSet->at(row);
            QBarDataRow *newDataRow;
            newDataRow = new QBarDataRow(dataRow->size());
            for (int column = 0; column < dataRow->size(); column++)
            {
                if(cutOffEnabled && dataRow->value(column).value() > convertFromSliderValue(cutoffSlider->value()))
                    (*newDataRow)[column].setValue(0.0f);
                else
                    (*newDataRow)[column].setValue(dataRow->value(column).value());
            }
            displayDataSet->append(newDataRow);

        }
        m_3DPixelSeries->dataProxy()->resetArray(displayDataSet); //, m_3DPixelSeries->dataProxy()->rowLabels(), m_3DPixelSeries->dataProxy()->columnLabels()
    }
}

void StarProfileViewer::getSubFrameMinMax(float *subFrameMin, float *subFrameMax, double *dataMin, double *dataMax)
{
    imageData->getMinMax(dataMin,dataMax);

    //Backwards so that we can find the min and max in subFrame
    *subFrameMin = *dataMax;
    *subFrameMax = *dataMin;

    switch (imageData->getStatistics().dataType)
    {
    case TBYTE:
        getSubFrameMinMax<uint8_t>(subFrameMin, subFrameMax);
        break;

    case TSHORT:
        getSubFrameMinMax<int16_t>(subFrameMin, subFrameMax);
        break;

    case TUSHORT:
        getSubFrameMinMax<uint16_t>(subFrameMin, subFrameMax);
        break;

    case TLONG:
        getSubFrameMinMax<int32_t>(subFrameMin, subFrameMax);
        break;

    case TULONG:
        getSubFrameMinMax<uint32_t>(subFrameMin, subFrameMax);
        break;

    case TFLOAT:
        getSubFrameMinMax<float>(subFrameMin, subFrameMax);
        break;

    case TLONGLONG:
        getSubFrameMinMax<int64_t>(subFrameMin, subFrameMax);
        break;

    case TDOUBLE:
        getSubFrameMinMax<double>(subFrameMin, subFrameMax);
        break;
    }
}

template <typename T>
void StarProfileViewer::getSubFrameMinMax(float *subFrameMin, float *subFrameMax)
{
    auto *buffer = reinterpret_cast<T const *>(imageData->getImageBuffer());
    T min = std::numeric_limits<T>::max();
    T max = std::numeric_limits<T>::min();
    int width = imageData->width();
    for (int y = subFrame.y(); y < subFrame.y() + subFrame.height(); y++)
    {
        for (int x = subFrame.x(); x < subFrame.x() + subFrame.width(); x++)
        {
            if( x > 0 && x < imageData->width() && y > 0 && y < imageData->height())
            {
                min = qMin(min, *(buffer + x + y * width));
                max = qMax(max, *(buffer + x + y * width));
            }
        }
    }

    *subFrameMin = min;
    *subFrameMax = max;
}

template <typename T>
float StarProfileViewer::getImageDataValue(int x, int y)
{
    if(!imageData)
        return 0;
    auto *buffer = reinterpret_cast<T const *>(imageData->getImageBuffer());
    return (float) buffer[y * imageData->width() + x];
}



float StarProfileViewer::getImageDataValue(int x, int y)
{
    switch (imageData->getStatistics().dataType)
    {
        case TBYTE:
            return getImageDataValue<uint8_t>(x, y);
            break;

        case TSHORT:
            return getImageDataValue<int16_t>(x, y);
            break;

        case TUSHORT:
            return getImageDataValue<uint16_t>(x, y);
            break;

        case TLONG:
            return getImageDataValue<int32_t>(x, y);
            break;

        case TULONG:
            return getImageDataValue<uint32_t>(x, y);
            break;

        case TFLOAT:
            return getImageDataValue<float>(x, y);
            break;

        case TLONGLONG:
            return getImageDataValue<int64_t>(x, y);
            break;

        case TDOUBLE:
            return getImageDataValue<double>(x, y);
            break;

        default:
            return 0;
            break;
    }
}

void StarProfileViewer::toggleSlice()
{
    if(m_graph->selectionMode() == QAbstract3DGraph::SelectionItemAndRow || m_graph->selectionMode() == QAbstract3DGraph::SelectionItemAndColumn)
    {

        if(m_graph->scene()->isSlicingActive())
        {
            m_graph->scene()->setSlicingActive(false);
        }
        else
        {
            QPoint selectedBar = m_graph->selectedSeries()
                    ? m_graph->selectedSeries()->selectedBar()
                    : QBar3DSeries::invalidSelectionPosition();
            if (selectedBar != QBar3DSeries::invalidSelectionPosition())
                m_graph->scene()->setSlicingActive(true);
        }
    }
}

void StarProfileViewer::updateVerticalAxis()
{
    float blackPoint = convertFromSliderValue(blackPointSlider->value());
    float whitePoint = convertFromSliderValue(whitePointSlider->value());
    m_pixelValueAxis->setRange(blackPoint, whitePoint);
    maxValue->setText(i18n("Max: %1", roundf(whitePoint * 100) / 100));
    minValue->setText(i18n("Min: %1", roundf(blackPoint * 100) / 100));
}

void StarProfileViewer::updateHFRandPeakSelection()
{
    m_graph->removeCustomItems();

    reportBox->setText("");
    QString reportString = "";

    //Removes all the stars from the combo box.
    while(zoomView->count() > 7)
        zoomView->removeItem(7);

    for (int i = 0; i < starCenters.count(); i++)
    {
        int x = starCenters[i]->x;
        int row = x - subFrame.x();
        int y = starCenters[i]->y;
        int col = subFrame.height() - (y - subFrame.y());
        if(subFrame.contains(x,y)){
            double newHFR = imageData->getHFR(x,y);
            int value = getImageDataValue(x, y);
            QCustom3DLabel *label = new QCustom3DLabel();
            label->setFacingCamera(true);
            QString labelString = i18n("Star %1: ", i + 1);
            if(showCoordinates->isChecked())
            {
                labelString = labelString + i18n("(%1, %2) ", x + 1, y + 1);
            }
            if(HFRReport->isChecked())
            {
                labelString = labelString + i18n("HFR: %1  ", roundf(newHFR * 100) / 100);
            }
            if(showPeakValues->isChecked())
            {
                labelString = labelString + i18n("Peak: %1", value);

            }
            if(showCoordinates->isChecked() || HFRReport->isChecked() || showPeakValues->isChecked())
            {
                if (!reportString.isEmpty())
                    reportString += '\n';

                reportString += labelString;
                label->setText(labelString);
                label->setPosition(QVector3D(row, value, col));
                label->setScaling(QVector3D(1.0f, 1.0f, 1.0f));
                m_graph->addCustomItem(label);
            }
            //Adds this star to the combo box.
            zoomView->addItem(i18n("Star %1", i + 1));
        }
    }
    if (!reportString.isEmpty())
    {
        reportBox->setText(reportString);
    }
}

void StarProfileViewer::updateColor(int selection)
{
    switch (selection) {
    case 0:
        setGreenToRedGradient();
        break;

    case 1:
        setBlackToYellowGradient();
        break;

    default:
        break;
    }
}

void StarProfileViewer::setBlackToYellowGradient()
{
    QLinearGradient gr;
    gr.setColorAt(0.0, Qt::black);
    gr.setColorAt(0.33, Qt::blue);
    gr.setColorAt(0.67, Qt::red);
    gr.setColorAt(1.0, Qt::yellow);

    QLinearGradient highGr;
    highGr.setColorAt(0.0, Qt::yellow);
    highGr.setColorAt(1.0, Qt::yellow);

    QLinearGradient sinHighGr;
    sinHighGr.setColorAt(0.0, Qt::red);
    sinHighGr.setColorAt(1.0, Qt::red);

    m_3DPixelSeries->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    m_3DPixelSeries->setBaseGradient(gr);
    m_3DPixelSeries->setSingleHighlightGradient(sinHighGr);
    m_3DPixelSeries->setMultiHighlightGradient(highGr);
}

void StarProfileViewer::setGreenToRedGradient()
{
    QLinearGradient gr;
    gr.setColorAt(0.0, Qt::darkGreen);
    gr.setColorAt(0.5, Qt::yellow);
    gr.setColorAt(0.8, Qt::red);
    gr.setColorAt(1.0, Qt::darkRed);

    QLinearGradient highGr;
    highGr.setColorAt(0.0, Qt::black);
    highGr.setColorAt(1.0, Qt::black);

    QLinearGradient sinHighGr;
    sinHighGr.setColorAt(0.0, Qt::red);
    sinHighGr.setColorAt(1.0, Qt::red);

    m_3DPixelSeries->setBaseGradient(gr);
    m_3DPixelSeries->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    m_3DPixelSeries->setSingleHighlightGradient(sinHighGr);
    m_3DPixelSeries->setMultiHighlightGradient(highGr);
}

//Multiplying by 1000000 will take care of preserving decimals in an int slider
//The sqrt function makes the slider non-linear, emphasising the lower values
//Note that it is actually multiplying the number on the slider by 1000 or so since it is square rooted.

int StarProfileViewer::convertToSliderValue(float value)
{
    return (int) qSqrt((value * 1000000.0));
}

float StarProfileViewer::convertFromSliderValue(int value)
{
    return qPow((float)value,2) / 1000000.0;
}


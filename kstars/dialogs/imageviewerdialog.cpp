#include "imageviewerdialog.h"
#include "ui_imageviewerdialog.h"

#include <QWheelEvent>
#include <QDebug>

ImageViewerDialog::ImageViewerDialog(const QString filePath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImageViewerDialog), m_ImagePath(filePath)
{
    ui->setupUi(this);

    m_Scene = new QGraphicsScene( ui->graphicsView );

    ui->graphicsView->setScene(m_Scene);
    ui->graphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    ui->graphicsView->setDragMode(ui->graphicsView->ScrollHandDrag);

    m_Image = new QImage( m_ImagePath );
    m_Pix = QPixmap::fromImage(m_Image->scaled(width(), height(), Qt::KeepAspectRatio));

    m_Scene->addPixmap( m_Pix );

    connect(ui->graphicsView, SIGNAL(wheelEvent(QWheelEvent* event)), this, SLOT(wheelEvent(QWheelEvent* event)));
    connect(ui->InvertColor, SIGNAL(clicked()), this, SLOT(invertColor()));

}

ImageViewerDialog::~ImageViewerDialog()
{
    delete ui;
}

void ImageViewerDialog::wheelEvent(QWheelEvent* event) {

    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // Scale the view / do the zoom
    double scaleFactor = 1.15;
    if(event->delta() > 0) {
        // Zoom in
        ui->graphicsView->scale(scaleFactor, scaleFactor);
    } else {
        // Zooming out
        ui->graphicsView->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }

}

void ImageViewerDialog::invertColor(){
    // Invert colors
    m_Image->invertPixels();
    m_Pix = QPixmap::fromImage(m_Image->scaled(width(), height(), Qt::KeepAspectRatio));
    m_Scene->addPixmap( m_Pix );
}

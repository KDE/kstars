#include "imageviewerdialog.h"
#include "ui_imageviewerdialog.h"

#include <QWheelEvent>
#include <QDebug>
#include <kmessagebox.h>
#include <KFileDialog>

ImageViewerDialog::ImageViewerDialog(const QString filePath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImageViewerDialog), m_ImagePath(filePath)
{
    ui->setupUi(this);

    m_Scene = new QGraphicsScene( ui->graphicsView );

    ui->graphicsView->setScene(m_Scene);
    ui->graphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
//    ui->graphicsView->setDragMode(ui->graphicsView->ScrollHandDrag);

    m_Image = new QImage( m_ImagePath );
    m_Pix = QPixmap::fromImage(m_Image->scaled(width(), height(), Qt::KeepAspectRatio));

    m_Scene->addPixmap( m_Pix );

    connect(ui->graphicsView, SIGNAL(wheelEvent(QWheelEvent* e)), this, SLOT(wheelEvent(QWheelEvent* e)));
    connect(ui->graphicsView, SIGNAL(mousePressEvent(QMouseEvent * e)), this, SLOT(mousePressEvent(QMouseEvent * e)));
    connect(ui->graphicsView, SIGNAL(mouseMoveEvent(QMouseEvent * e)), this, SLOT(mouseReleaseEvent(QMouseEvent * e)));
    connect(ui->InvertColor, SIGNAL(clicked()), this, SLOT(invertColor()));
    connect(ui->Save, SIGNAL(clicked()), this, SLOT(saveImage()));

}

ImageViewerDialog::~ImageViewerDialog()
{
    delete ui;
}

void ImageViewerDialog::wheelEvent(QWheelEvent* e)
{

    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // Scale the view / do the zoom
    double scaleFactor = 1.15;
    if(e->delta() > 0) {
        // Zoom in
        ui->graphicsView->scale(scaleFactor, scaleFactor);
    } else {
        // Zooming out
        ui->graphicsView->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }

}

void ImageViewerDialog::invertColor()
{
    // Invert colors
    m_Image->invertPixels();
    m_Pix = QPixmap::fromImage(m_Image->scaled(width(), height(), Qt::KeepAspectRatio));
    m_Scene->addPixmap( m_Pix );
}

void ImageViewerDialog::saveImage()
{

    KUrl newURL = KFileDialog::getSaveUrl( QDir::homePath(), "*.png | *.png" );
    if (!newURL.isEmpty())
    {
        QPainter painter(m_Image);
        painter.setRenderHint(QPainter::Antialiasing);
        m_Scene->render(&painter);
        m_Image->save(newURL.path());
    }

}

void ImageViewerDialog::mousePressEvent(QMouseEvent * e)
{
    qDebug() << "press event";
    startPoint = ui->graphicsView->mapToScene(e->pos());
}

void ImageViewerDialog::mouseReleaseEvent(QMouseEvent * e)
{
    qDebug() << "release event";
    endPoint = ui->graphicsView->mapToScene(e->pos());
    m_Scene->addEllipse(startPoint.x(), startPoint.y(), startPoint.x() - endPoint.x() , startPoint.y() - endPoint.y(),
    QPen(Qt::red), QBrush(Qt::transparent));
}

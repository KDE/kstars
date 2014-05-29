#include "imageviewerdialog.h"
#include "ui_imageviewerdialog.h"

#include <QWheelEvent>
#include <QGraphicsTextItem>
#include <QDebug>
#include <kmessagebox.h>
#include <KFileDialog>

#include "kinputdialog.h"

ImageViewerDialog::ImageViewerDialog(const QString filePath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImageViewerDialog), m_ImagePath(filePath), drawType(0)
{
    ui->setupUi(this);

    ui->ZoomInButton->setIcon( KIcon("zoom-in") );
    ui->ZoomOutButton->setIcon( KIcon("zoom-out") );
    ui->UndoButton->setIcon( KIcon("edit-undo") );

    m_Scene = new QGraphicsScene( ui->graphicsView );

    ui->graphicsView->setScene(m_Scene);
    ui->graphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

//    ui->graphicsView->setDragMode(ui->graphicsView->ScrollHandDrag);

    m_Image = new QImage( m_ImagePath );
    m_Pix = QPixmap::fromImage(m_Image->scaled(width(), height(), Qt::KeepAspectRatio));

    m_Scene->addPixmap( m_Pix );

    connect(ui->ZoomInButton, SIGNAL(clicked()), this, SLOT(zoomInImage()));
    connect(ui->ZoomOutButton, SIGNAL(clicked()), this, SLOT(zoomOutImage()));
    connect(ui->DrawTextButton, SIGNAL(clicked()), this, SLOT(drawTextClicked()));
    connect(ui->DrawCircleButton, SIGNAL(clicked()), this, SLOT(drawCircleClicked()));
    connect(ui->UndoButton, SIGNAL(clicked()), this, SLOT(undoClicked()));
    connect(ui->InvertColor, SIGNAL(clicked()), this, SLOT(invertColor()));
    connect(ui->Save, SIGNAL(clicked()), this, SLOT(saveImage()));
    connect(ui->Close, SIGNAL(clicked()), this, SLOT(close()));

    setMouseTracking(true);

}

ImageViewerDialog::~ImageViewerDialog()
{
    delete ui;
}

void ImageViewerDialog::wheelEvent( QWheelEvent * event )
{
    if(event->delta() > 0)
    {
        zoomInImage();
    }
    else
    {
        zoomOutImage();
    }

}

void ImageViewerDialog::mousePressEvent( QMouseEvent * event )
{
    clickedPoint = ui->graphicsView->mapToScene(event->pos());

    if(drawType == ImageViewerDialog::Circle)
    {
        int scale = ui->SizeBox->value();
        m_Scene->addEllipse(clickedPoint.x() - scale * 10, clickedPoint.y() - scale * 10, scale * 15 , scale * 15,
        QPen(Qt::red), QBrush(Qt::transparent));
    }
    else if (drawType == ImageViewerDialog::Text)
    {
        QGraphicsTextItem * io = new QGraphicsTextItem;
        io->setPos( clickedPoint.x() - 15, clickedPoint.y() - 25 );
        io->setPlainText( KInputDialog::getText("Type Lable Name", "Text") );
        io->setDefaultTextColor( QColor("red") );
        m_Scene->addItem( io );
    }
}

void ImageViewerDialog::zoomInImage()
{
    ui->graphicsView->scale(1.15, 1.15);
}

void ImageViewerDialog::zoomOutImage()
{
    ui->graphicsView->scale(1.0 / 1.15, 1.0 / 1.15);
}

void ImageViewerDialog::drawTextClicked()
{
    drawType = (drawType == ImageViewerDialog::Text) ? 0 : 1;
    ui->DrawTextButton->setFlat(!ui->DrawTextButton->isFlat());
    ui->DrawCircleButton->setFlat(false);
}

void ImageViewerDialog::drawCircleClicked()
{
    drawType = (drawType == ImageViewerDialog::Circle) ? 0 : 2;
    ui->DrawCircleButton->setFlat(!ui->DrawCircleButton->isFlat());
    ui->DrawTextButton->setFlat(false);
}

void ImageViewerDialog::undoClicked()
{
    itemsList = m_Scene->items();

    if(itemsList.size() > 1)
    {
        m_Scene->removeItem( itemsList.at( 0 ) );
    }
}

void ImageViewerDialog::invertColor()
{
    // Invert colors
    m_Image->invertPixels();
    m_Pix = QPixmap::fromImage(m_Image->scaled(width(), height(), Qt::KeepAspectRatio));
    m_Scene->clear();
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


//void ImageViewerDialog::mouseMoveEvent( QMouseEvent * event )
//{
//    itemsList = m_Scene->items();

//    if(drawType)
//    {
//        m_Scene->removeItem( itemsList.at( itemsList.size() - 1 ) );
//        endPoint = ui->graphicsView->mapToScene(event->pos());
//        m_Scene->addEllipse(clickedPoint.x(), clickedPoint.y(), clickedPoint.x() - endPoint.x() , clickedPoint.y() - endPoint.y(),
//        QPen(Qt::red), QBrush(Qt::transparent));
//    }
//}

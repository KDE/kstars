/***************************************************************************
                          thumbnaileditor.cpp  -  description
                             -------------------
    begin                : Thu Mar 2 2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "thumbnaileditor.h"
#include "ui_thumbnaileditor.h"
#include "thumbnailpicker.h"

#include <QPixmap>
#include <QMouseEvent>
#include <QLabel>
#include <QHBoxLayout>
#include <QPaintEvent>

#include <KLocalizedString>
#include <QDebug>
#include <QPushButton>
#include <QDialogButtonBox>

ThumbnailEditorUI::ThumbnailEditorUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

ThumbnailEditor::ThumbnailEditor(ThumbnailPicker *_tp, double _w, double _h) : QDialog(_tp), tp(_tp)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    ui = new ThumbnailEditorUI(this);
    w  = _w;
    h  = _h;
    ui->MessageLabel->setText(i18n("Crop region will be scaled to [ %1 * %2 ]", w, h));

    setWindowTitle(i18n("Edit Thumbnail Image"));
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    ui->ImageCanvas->setCropRect(tp->imageRect()->x(), tp->imageRect()->y(), tp->imageRect()->width(),
                                 tp->imageRect()->height());
    ui->ImageCanvas->setImage(tp->currentListImage());

    //DEBUG
    //qDebug() << tp->currentListImage()->size();

    connect(ui->ImageCanvas, SIGNAL(cropRegionModified()), SLOT(slotUpdateCropLabel()));
    slotUpdateCropLabel();

    update();
}

ThumbnailEditor::~ThumbnailEditor()
{
}

QPixmap ThumbnailEditor::thumbnail()
{
    QImage im = ui->ImageCanvas->croppedImage().toImage();
    im        = im.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return QPixmap::fromImage(im);
}

void ThumbnailEditor::slotUpdateCropLabel()
{
    QRect *r = ui->ImageCanvas->cropRect();
    ui->CropLabel->setText(i18n("Crop region: [%1,%2  %3x%4]", r->left(), r->top(), r->width(), r->height()));
}

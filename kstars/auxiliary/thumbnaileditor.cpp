/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thumbnaileditor.h"

#include "thumbnailpicker.h"
#include "ui_thumbnaileditor.h"

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPixmap>

#include <QDebug>
#include <QDialogButtonBox>
#include <QPushButton>

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

    setWindowTitle(i18nc("@title:window", "Edit Thumbnail Image"));
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

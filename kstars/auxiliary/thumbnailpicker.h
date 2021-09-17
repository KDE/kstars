/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_thumbnailpicker.h"

#include <KIO/Job>

#include <QDialog>
#include <QPixmap>

class QRect;
class KJob;
class DetailDialog;
class SkyObject;

class ThumbnailPickerUI : public QFrame, public Ui::ThumbnailPicker
{
    Q_OBJECT
  public:
    explicit ThumbnailPickerUI(QWidget *p);
};

/**
 * @short Dialog for modifying an object's thumbnail image
 *
 * @author Jason Harris
 */
class ThumbnailPicker : public QDialog
{
    Q_OBJECT
  public:
    ThumbnailPicker(SkyObject *o, const QPixmap &current, QWidget *parent = nullptr, double w = 200, double h = 200,
                    QString cap = i18n("Choose Thumbnail Image"));
    ~ThumbnailPicker() override;

    QPixmap *image() { return Image; }
    QPixmap *currentListImage() { return PixList.at(SelectedImageIndex); }
    bool imageFound() const { return bImageFound; }
    QRect *imageRect() const { return ImageRect; }

  private slots:
    void slotEditImage();
    void slotUnsetImage();
    void slotSetFromList(int i);
    void slotSetFromURL();
    void slotFillList();
    void slotProcessGoogleResult(KJob *);

    /** Make sure download has finished, then make sure file exists, then add image to list */
    void slotJobResult(KJob *);

  private:
    QPixmap shrinkImage(QPixmap *original, int newSize, bool setImage = false);
    void parseGooglePage(const QString &URL);

    int SelectedImageIndex;
    double thumbWidth, thumbHeight;
    ThumbnailPickerUI *ui;
    QPixmap *Image;
    SkyObject *Object;
    QList<KIO::Job *> JobList;
    QList<QPixmap *> PixList;
    bool bImageFound;
    QRect *ImageRect;
};

/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QMouseEvent>
#include <QPaintEvent>

#include <QDialog>

#include "ui_thumbnaileditor.h"

class ThumbnailPicker;

class ThumbnailEditorUI : public QFrame, public Ui::ThumbnailEditor
{
    Q_OBJECT
  public:
    explicit ThumbnailEditorUI(QWidget *parent);
};

class ThumbnailEditor : public QDialog
{
    Q_OBJECT
  public:
    ThumbnailEditor(ThumbnailPicker *_tp, double _w, double _h);
    ~ThumbnailEditor() override = default;
    QPixmap thumbnail();

  private slots:
    void slotUpdateCropLabel();

  private:
    ThumbnailEditorUI *ui;
    ThumbnailPicker *tp;
    double w, h;
};

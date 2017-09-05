/***************************************************************************
                          exportimagedialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Jun 13 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef EXPORTIMAGEDIALOG_H
#define EXPORTIMAGEDIALOG_H

#include "ui_exportimagedialog.h"

#include "../printing/legend.h"

#include <QDialog>

class KStars;
class QString;
class QSize;
class ImageExporter;

// ExportImageDialog user interface.
class ExportImageDialogUI : public QFrame, public Ui::ExportImageDialog
{
    Q_OBJECT

  public:
    explicit ExportImageDialogUI(QWidget *parent = nullptr);
};

/** @short Export sky image dialog. This dialog enables user to set up basic legend
  properties before image is exported.
  */
class ExportImageDialog : public QDialog
{
    Q_OBJECT

  public:
    /**short Default constructor. Creates dialog operating on passed URL and
          output image width and height.
          *@param url URL for exported image.
          *@param size size of exported image.
          *@param imgExporter A pointer to an ImageExporter that we can use instead of creating our own. if 0, we will create our own.
          */
    ExportImageDialog(const QString &url, const QSize &size, ImageExporter *imgExporter = nullptr);

    /** @short Default destructor. */
    ~ExportImageDialog() override {}

    inline void setOutputUrl(const QString &url) { m_Url = url; }
    inline void setOutputSize(const QSize &size) { m_Size = size; }

  private slots:
    void switchLegendEnabled(bool enabled);
    void previewImage();
    void exportImage();

    void setupWidgets();
    void updateLegendSettings();

  private:
    KStars *m_KStars;
    ExportImageDialogUI *m_DialogUI;
    QString m_Url;
    QSize m_Size;
    ImageExporter *m_ImageExporter;
};

#endif // EXPORTIMAGEDIALOG_H

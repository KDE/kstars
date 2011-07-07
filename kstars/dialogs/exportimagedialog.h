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

class KStars;
class SkyQPainter;

// ExportImageDialog user interface.
class ExportImageDialogUI : public QFrame, public Ui::ExportImageDialog
{
    Q_OBJECT

public:
    explicit ExportImageDialogUI(QWidget *parent = 0);
};


/**@short Export sky image dialog. This dialog enables user to set up basic legend
  properties before image is exported.
  */
class ExportImageDialog : public KDialog
{
    Q_OBJECT

public:
    /**short Default constructor. Creates dialog operating on passed URL and
      output image width and height.
      *@param kstars KStars instance.
      *@param url URL for exported image.
      *@param w exported image width.
      *@param h exported image height.
      */
    ExportImageDialog(KStars *kstars, const QString &url, int w, int h);

    /**@short Default destructor. */
    ~ExportImageDialog();

private slots:
    void exportImage();
    void switchLegendConfig(bool newState);

private:
    void setupWidgets();
    void setupConnections();

    void addLegend(SkyQPainter *painter);
    void addLegend(QPaintDevice *pd);

    KStars *m_KStars;
    ExportImageDialogUI *m_DialogUI;
    QString m_Url;
    int m_Width;
    int m_Height;
};


#endif // EXPORTIMAGEDIALOG_H

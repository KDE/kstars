/***************************************************************************
               exporteyepieceview.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 17 Jan 2016 21:36:25 CST
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/



#ifndef EXPORTEYEPIECEVIEW_H
#define EXPORTEYEPIECEVIEW_H

#include "kstarsdatetime.h"

#include <QDialog>
#include <QImage>

class SkyPoint;
class QLabel;
class QComboBox;
class QPixmap;

/**
 * @class ExportEyepieceView
 * @short Dialog to export the eyepiece view as an image, with some annotations for field-use
 * @author Akarsh Simha <akarsh@kde.org>
 */

class ExportEyepieceView : public QDialog {

    Q_OBJECT;

 public:

    /**
     * @short Constructor
     * @note Class self-destructs (commits suicide). Invoke and forget.
     */
    ExportEyepieceView( const SkyPoint *_sp, const KStarsDateTime &dt, const QPixmap *renderImage, const QPixmap *renderChart, QWidget *parent = 0 );

    /**
     * @short Destructor
     */
    ~ExportEyepieceView();

public slots:

    /**
     * @short Change the tick overlay scheme
     */
    void slotOverlayTicks( int overlayType );

    /**
     * @short Save the image (export), and then close the dialog by calling slotCloseDialog()
     */
    void slotSaveImage();

    /**
     * @short Closes the dialog, and sets up deleteLater() so that the dialog is destructed.
     */
    void slotCloseDialog();

private slots:

    /**
     * @short render the output
     */
    void render();

 private:

    QLabel *m_outputDisplay;
    QLabel *m_tickWarningLabel;
    QComboBox *m_tickConfigCombo;

    KStarsDateTime m_dt;
    SkyPoint *m_sp;
    QPixmap *m_renderImage;
    QPixmap *m_renderChart;
    QImage m_output;
    int m_tickConfig;
};

#endif

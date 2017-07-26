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

#pragma once

#include "kstarsdatetime.h"

#include <QDialog>
#include <QImage>

#include <memory>

class QComboBox;
class QLabel;
class QPixmap;

class SkyPoint;

/**
 * @class ExportEyepieceView
 * @short Dialog to export the eyepiece view as an image, with some annotations for field-use
 *
 * @author Akarsh Simha <akarsh@kde.org>
 */
class ExportEyepieceView : public QDialog
{
    Q_OBJECT;

  public:
    /**
     * @short Constructor
     * @note Class self-destructs (commits suicide). Invoke and forget.
     */
    ExportEyepieceView(const SkyPoint *_sp, const KStarsDateTime &dt, const QPixmap *renderImage,
                       const QPixmap *renderChart, QWidget *parent = 0);

  public slots:

    /** Change the tick overlay scheme */
    void slotOverlayTicks(int overlayType);

    /** Save the image (export), and then close the dialog by calling slotCloseDialog() */
    void slotSaveImage();

    /** Closes the dialog, and sets up deleteLater() so that the dialog is destructed. */
    void slotCloseDialog();

  private slots:

    /** Render the output */
    void render();

  private:
    QLabel *m_outputDisplay { nullptr };
    QLabel *m_tickWarningLabel { nullptr };
    QComboBox *m_tickConfigCombo { nullptr };

    KStarsDateTime m_dt;
    std::unique_ptr<SkyPoint> m_sp;
    std::unique_ptr<QPixmap> m_renderImage;
    std::unique_ptr<QPixmap> m_renderChart;
    QImage m_output;
    int m_tickConfig { 0 };
};

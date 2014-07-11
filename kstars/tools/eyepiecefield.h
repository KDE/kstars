/***************************************************************************
                 eyepiecefield.h  -  K Desktop Planetarium
                             -------------------
    begin                : Fri 30 May 2014 14:39:37 CDT
    copyright            : (c) 2014 by Akarsh Simha
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



#ifndef EYEPIECEFIELD_H
#define EYEPIECEFIELD_H

#include <QDialog>
#include <QImage>

class SkyPoint;
class FOV;
class QString;
class QLabel;
class QSlider;
class QCheckBox;
class QObject;

/**
 * @class EyepieceField
 * @short Renders the view through the eyepiece of various telescope types
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 */

class EyepieceField : public QDialog { // FIXME: Rename to EyepieceView

    Q_OBJECT;

 public:
    
    /**
     * @short Constructor
     */
    EyepieceField( QWidget *parent = 0 );

    /**
     * @short Destructor
     */
    ~EyepieceField();

    /**
     * @short Show the eyepiece field dialog
     * @param sp Sky point to draw the eyepiece field around.
     * @param fov Pointer to the FOV object describing the field of view. Default is a 1 degree FOV.
     * @param imagePath Optional path to DSS or other image. North should be on the top of the image.
     * @note The SkyPoint must have correct Alt/Az coordinates, maybe by calling update() already before calling this method.
     */
    void showEyepieceField( SkyPoint *sp, FOV const * const fov = 0, const QString &imagePath = QString() );

public slots:

    /**
     * @short Re-renders the view
     * Takes care of things like inverting colors, inverting orientation, flipping, rotation
     */
    void render( int dummy = 0 );

 private:
    QLabel *m_skyChartDisplay;
    QLabel *m_skyImageDisplay;
    QImage *m_skyChart;
    QImage *m_skyImage;
    QSlider *m_rotationSlider;
    QCheckBox *m_invertColors;
    QCheckBox *m_invertView;
    QCheckBox *m_flipView;

};

#endif

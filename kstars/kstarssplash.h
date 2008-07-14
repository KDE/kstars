/***************************************************************************
                          KStarsSplash.h  -  description
                             -------------------
    begin                : Thu Jul 26 2001
    copyright            : (C) 2001 by Heiko Evermann
    email                : heiko@evermann.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSTARSSPLASH_H__
#define KSTARSSPLASH_H__

#include <QImage>
#include <QLabel>
#include <QPaintEvent>
#include <QCloseEvent>

#include <QDialog>
//#include <kdialog.h>

/**@class KStarsSplash
	*The KStars Splash Screen.  The splash screen shows the KStars logo and 
	*progress messages while data files are parsed and objects are initialized.
	*@short the KStars Splash Screen.
	*@author Heiko Evermann
	*@version 1.0
	*/

class QLabel;

class KStarsSplash : public QDialog
{
    Q_OBJECT

public:
    /**Constructor. Create widgets.  Load KStars logo.  Start load timer.
     * A non-empty customMessage will replace "Welcome to KStars [...]".
    */
    explicit KStarsSplash( QWidget *parent, const QString& customMessage="" );

    /**Destructor
    	*/
    ~KStarsSplash();

public slots:
    /**Display the text argument in the Splash Screen's status label.
    	*This is connected to KStarsData::progressText(QString)*/
    void setMessage(const QString &s);

protected:
    /**If the user clicks on the "X" close-window button, then abort loading
    	*as soon as possible and shut down the program.
    	*/
    void closeEvent( QCloseEvent *e );

signals:
    void closeWindow();

private:
    QLabel *textCurrentStatus, *label, *Banner;
};

#endif

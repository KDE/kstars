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

#ifndef KSTARSSPLASH_H_
#define KSTARSSPLASH_H_

#include <kdialogbase.h>

/**@class KStarsSplash
	*The KStars Splash Screen.  The splash screen shows the KStars logo and 
	*progress messages while data files are parsed and objects are initialized.
	*@short the KStars Splash Screen.
	*@author Heiko Evermann
	*@version 1.0
	*/

class QLabel;

class KStarsSplash : public KDialogBase
{
	Q_OBJECT

	public:
	/**Constructor. Create widgets.  Load KStars logo.  Start load timer.
		*/
		KStarsSplash( QWidget *parent, const char* name );

	/**Destructor
		*/
		~KStarsSplash();

	public slots:
	/**Display the text argument in the Splash Screen's status label.
		*This is connected to KStarsData::progressText(QString)*/
		void setMessage(QString s);

	protected:
	/**Paint event to redraw the widgets.  This only gets called when the timer fires.
		*It should also repaint if another window was on top of the splash screen, but
		*this may be difficult to implement (it may be that the program is too busy loading data
		*to notice that a redraw is required).
		*/
		virtual void paintEvent( QPaintEvent* );

	/**If the user clicks on the "X" close-window button, then abort loading 
		*as soon as possible and shut down the program.
		*/
		void closeEvent( QCloseEvent *e );

		signals:
			void closeWindow();

	private:
		QLabel *textCurrentStatus, *label;
		QWidget *Banner;
		QPixmap *splashImage;
};

#endif

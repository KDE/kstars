/***************************************************************************
                          NewDialog.h  -  description
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

#include <qlabel.h>
#include <qlcdnumber.h>
#include <qpushbutton.h>
#include <qscrollbar.h>
#include <kdialogbase.h>
#include <qtimer.h>

#include "kstarsdata.h"

class KStarsSplash : public KDialogBase
{
	Q_OBJECT
public:
	KStarsSplash( KStarsData* kstarsData, QWidget *parent, const char* name, bool modal );
	void KludgeError( QString s, bool required=true );

protected:
	virtual void paintEvent( QPaintEvent *e );

private:
  KStarsData* kstarsData;
  QLabel* textCurrentStatus;
	QWidget *Banner;
	QPixmap *splashImage;
	QTimer* qtimer;
  int     loadStatus;
private slots:
	void slotLoadDataFile();
};

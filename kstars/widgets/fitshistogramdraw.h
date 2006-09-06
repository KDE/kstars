/***************************************************************************
                          fitshistogramdraw.h  -  FITS Historgram
                          ---------------
    copyright            : (C) 2006 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FITSHISTOGRAMAREA_H
#define FITSHISTOGRAMAREA_H

#include <QFrame>

class FITSHistogram;

class histDrawArea : public QFrame
{
   Q_OBJECT
	
	public:
		histDrawArea(QWidget *parent);
		~histDrawArea();

	protected:
		 void paintEvent(QPaintEvent *event);

	private:
		FITSHistogram *data;
};

#endif


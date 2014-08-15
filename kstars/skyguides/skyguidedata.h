/***************************************************************************
                          skyguideslistview.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2014/04/07
    copyright            : (C) 2014 by Gioacchino Mazzurco
    email                : gmazzurco89@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYGUIDEDATA_H
#define SKYGUIDEDATA_H

#include <QObject>
#include <QString>

class SkyGuideData : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString title READ title CONSTANT FINAL)
	Q_PROPERTY(QString path READ path CONSTANT FINAL)
	Q_PROPERTY(uint pWidth READ pWidth CONSTANT FINAL)
	Q_PROPERTY(uint pHeight READ pHeight CONSTANT FINAL)

public:
	SkyGuideData(
		const QString & title,
		const QString & path,
		uint width,
		uint height,
		QObject * parent = NULL
	);

	~SkyGuideData() {};

	inline QString title() const { return mTitle; };
	inline QString path() const { return mPath; };
	inline uint pWidth() const { return mWidth; };
	inline uint pHeight() const { return mHeight; };

private:
	const QString mTitle;
	const QString mPath;
	const uint mWidth;
	const uint mHeight;
};

#endif // SKYGUIDEDATA_H

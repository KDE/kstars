/***************************************************************************
                          addlinkdialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Oct 21 2001
    copyright            : (C) 2001 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ADDLINKDIALOG_H
#define ADDLINKDIALOG_H

#include <kdialogbase.h>

class KLineEdit;
class QString;
class QHBoxLayout;
class QVBoxLayout;
class QPushButton;
class QRadioButton;
class QButtonGroup;

/**
  *@author Jason Harris
  */

class AddLinkDialog : public KDialogBase  {
	Q_OBJECT
public:
	AddLinkDialog( QWidget* parent = 0 );
	~AddLinkDialog();
	QString url();
	QString title();
	bool isImageLink();
private slots:
	void checkURL( void );
	void changeDefaultTitle( int id );
private:
	QVBoxLayout *vlay;
	QHBoxLayout *hlayTitle, *hlayURL, *hlayBrowse;
	KLineEdit *URLEntry, *TitleEntry;
	QPushButton *BrowserButton;
	QButtonGroup *TypeGroup;
	QRadioButton *ImageRadio;
	QRadioButton *InfoRadio;
};

#endif

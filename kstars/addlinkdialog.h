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
#include <klineedit.h>
#include <qradiobutton.h>

class QString;
class QHBoxLayout;
class QVBoxLayout;
class QPushButton;
class QRadioButton;
class QButtonGroup;

/**@short Simple dialog for adding a custom URL to a popup menu.
  *@author Jason Harris
	*@version 0.9
  */

class AddLinkDialog : public KDialogBase  {
	Q_OBJECT
public:
	/**Constructor. */
	AddLinkDialog( QWidget* parent = 0 );

	/**Destructor (empty) */
	~AddLinkDialog() {}

	/**@returns QString of the entered URL */
	QString url() const { return URLEntry->text(); }

	/**@returns QString of the entered menu entry text */
	QString title() const { return TitleEntry->text(); }

	/**@returns true if user declared the link is an image */
	bool isImageLink() const { return ImageRadio->isChecked(); }

private slots:
	/**Open the entered URL in the web browser */
	void checkURL( void );

	/**We provide a default menu text string; this function changes the
		*default string if the link type (image/webpage) is changed.  Note
		*that if the user has changed the menu text, this function does nothing.
		*@param id 0=show image string; 1=show webpage string.
		*/
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

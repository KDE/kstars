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
#include <klocale.h>
#include <qradiobutton.h>

#include "addlinkdialogui.h"

class QLayout;
class QString;
class SkyMap;

/**@class Simple dialog for adding a custom URL to a popup menu.
  *@author Jason Harris
  *@version 1.0
  */

class AddLinkDialog : public KDialogBase  {
	Q_OBJECT
public:
/**Constructor. */
	AddLinkDialog( QWidget* parent = 0, const QString &oname=i18n("object") );

/**Destructor (empty) */
	~AddLinkDialog() {}

/**@return QString of the entered URL */
	QString url() const { return ald->URLBox->text(); }

/**@short Set the URL text
	*@param s the new URL text
	*/
	void setURL( const QString &s ) { ald->URLBox->setText( s ); }
	
/**@return QString of the entered menu entry text */
	QString desc() const { return ald->DescBox->text(); }

/**@short Set the Description text
	*@param s the new description text
	*/
	void setDesc( const QString &s ) { ald->DescBox->setText( s ); }
	
/**@return TRUE if user declared the link is an image */
	bool isImageLink() const { return ald->ImageRadio->isChecked(); }

/**@short Set the link type
	*@param b if true, link is an image link.
	*/
	void setImageLink( bool b ) { ald->ImageRadio->setChecked( b ); }

private slots:
/**Open the entered URL in the web browser 
	*/
	void checkURL( void );

	/**We provide a default menu text string; this function changes the
		*default string if the link type (image/webpage) is changed.  Note
		*that if the user has changed the menu text, this function does nothing.
		*@param id 0=show image string; 1=show webpage string.
		*/
	void changeDefaultDescription( int id );

private:
	QString ObjectName;
	QVBoxLayout *vlay;
	AddLinkDialogUI *ald;
};

#endif

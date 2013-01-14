/***************************************************************************
                          opssupernovae.h  -  K Desktop Planetarium
                             -------------------
    begin                : Thu, 25 Aug 2011
    copyright            : (C) 2011 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OPSSUPERNOVAE_H_
#define OPSSUPERNOVAE_H_

#include "ui_opssupernovae.h"

#include <kconfigdialog.h>

class KStars;


/**@class OpsSupernovae
 *The Supernovae Tab of the Options window.  In this Tab the user can configure
 *supernovae options and select if supernovae should be drawn on the skymap.
 *Also the user is given the option to check for updates on startup. And whether
 *to be alerted on startup.
 *@author Samikshan Bairagya
 *@version 1.0
 */
class OpsSupernovae : public QFrame, public Ui::OpsSupernovae
{
    Q_OBJECT

public:
    /**
     * Constructor
     */
    explicit OpsSupernovae( KStars *_ks );

    /**
     * Destructor
     */
    ~OpsSupernovae();

private:

    KStars *ksw;

private slots:
    void slotUpdateRecentSupernovae();
    void slotShowSupernovae( bool on );
    void slotUpdateOnStartup( bool on );
    void slotShowSupernovaAlerts( bool on );
    void slotSetShowMagnitudeLimit( double value );
    void slotSetAlertMagnitudeLimit( double value );
};

#endif  //OPSSUPERNOVAE_H_

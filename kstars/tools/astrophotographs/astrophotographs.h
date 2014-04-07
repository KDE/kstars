/***************************************************************************
                          astrophotographs.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2014/05/04
    copyright            : (C) 2014 by Vijay Dhameliya
    email                : vijay.atwork13@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ASTROPHOTOGRAPHS_H
#define ASTROPHOTOGRAPHS_H

class QDeclarativeView;

#include "QDeclarativeContext"
#include "skyobject.h"
#include "astrobinapi.h"
#include "astrobinapijson.h"
#include "astrobinapixml.h"
#include "tools/whatsinteresting/obsconditions.h"

/**
  * \class Astrophotographs
  * \brief Manages the QML user interface for astrophotographs browser.
  * Astrophotographs is used to display the QML UI using a QDeclarativeView.
  * It acts on all signals emitted by the UI and manages the data
  * sent to the UI for display.
  * \author Vijay Dhameliya
  */
class Astrophotographs : public QWidget
{
    Q_OBJECT
public:

    Astrophotographs(QWidget *parent = 0);

    ~Astrophotographs(){}

    inline QDeclarativeView *getABBaseView() const { return m_BaseView; }
    
signals:
    
public slots:
    
private:
    QObject *m_BaseObj;
    QDeclarativeContext *m_Ctxt;
    QDeclarativeView *m_BaseView;
};

#endif // ASTROPHOTOGRAPHS_H

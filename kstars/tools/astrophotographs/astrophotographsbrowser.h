/***************************************************************************
                          astrophotographsbrowser.h  -  K Desktop Planetarium
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

#ifndef ASTROPHOTOGRAPHSBROWSER_H
#define ASTROPHOTOGRAPHSBROWSER_H

class QDeclarativeView;

#include "QDeclarativeContext"
#include "skyobject.h"
#include "astrobinapijson.h"
#include "tools/whatsinteresting/obsconditions.h"

/**
  * \class AstrophotographsBrowser
  * \brief Manages the QML user interface for astrophotographs browser.
  * AstrophotographsBrowser is used to display the QML UI using a QDeclarativeView.
  * It acts on all signals emitted by the UI and manages the data
  * sent to the UI for display.
  * \author Vijay Dhameliya
  */
class AstrophotographsBrowser : public QWidget
{
    Q_OBJECT
public:

    /**
      * \brief Constructor - Store QML components as QObject pointers.
      * Connect signals from various QML components into public slots.
      * Displays the user interface for Astrophotographs Browser
      */
    AstrophotographsBrowser(QWidget *parent = 0);

    ~AstrophotographsBrowser();

    inline QDeclarativeView *getABBaseView() const { return m_BaseView; }
    
signals:
    
public slots:

    void slotAstrobinSearch();
    
private:
    QObject *m_BaseObj, *m_SearchContainerObj;
    QDeclarativeContext *m_Ctxt;
    QDeclarativeView *m_BaseView;

    QNetworkAccessManager *m_NetworkManager;
    AstroBinApi *m_AstrobinApi;
};

#endif // ASTROPHOTOGRAPHSBROWSER_H

/***************************************************************************
                          wiview.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/26/05
    copyright            : (C) 2012 by Samikshan Bairagya
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

#ifndef WIVIEW_H
#define WIVIEW_H

#include "QtDeclarative/QDeclarativeView"
#include "QtDeclarative/QDeclarativeItem"
#include "QtDeclarative/QDeclarativeContext"
#include "QModelIndex"
#include "skyobject.h"
#include "modelmanager.h"
#include "skyobjlistmodel.h"
#include "obsconditions.h"

/**
  * \class WIView
  * \brief Manages the QML user interface for What's Interesting.
  * WIView is used to display the QML UI using a QDeclarativeView.
  * It acts on all signals emitted by the UI and manages the data
  * sent to the UI for display.
  * \author Samikshan Bairagya
  */
class WIView : public QWidget
{
    Q_OBJECT
public:

    /**
      * \brief Constructor - Store QML components as QObject pointers.
      * Connect signals from various QML components into public slots.
      * Displays the user interface for What's Interesting
      */
    WIView(QWidget *parent = 0, ObsConditions *obs = 0);

    /**
      * \brief Destructor
      */
    ~WIView();

    /**
      * \brief Load details-view for selected sky-object
      */
    void loadDetailsView(SkyObjItem* soitem, int index);

public slots:

    /**
      * \brief public slot - Act upon signal emitted when category of sky-object is selected
      * from category selection view of the QML UI.
      * \param type Category selected
      */
    void onCategorySelected(int type);

    /**
      * \brief public slot - Act upon signal emitted when an item is selected from list of sky-objects.
      * Display details-view for the skyobject selected.
      * \param type        Category selected.
      * \param typename    Name of category selected.
      * \param index       Index of item in the list of skyobjects.
      */
    void onSoListItemClicked(int type, QString typeName, int index);

    /**
      * \brief public slot - Show details-view for next sky-object from list of current sky-objects's category.
      */
    void onNextObjClicked();

    /**
      * \brief public slot - Show details-view for previous sky-object from list of current sky-objects's category.
      */
    void onPrevObjClicked();

private:
    QObject *m_BaseObj, *m_ViewsRowObj, *m_SoListObj, *m_DetailsViewObj, *m_NextObj, *m_PrevObj;
    QDeclarativeContext *ctxt;
    ModelManager *m;
    SkyObjItem *m_CurSoItem;  ///Current sky-object item.
    int m_CurIndex;           ///Index of current sky-object item in details-view.
    double m_OptMag;          ///Optimum magnification
};

#endif

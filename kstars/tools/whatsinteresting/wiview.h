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

#include "QtDeclarative/QDeclarativeView"
#include "QtDeclarative/QDeclarativeContext"
#include "QModelIndex"
#include "skyobject.h"
#include "modelmanager.h"
#include "skyobjlistmodel.h"
#include "obsconditions.h"

class WIView : public QObject
{
    Q_OBJECT
public:
    WIView(QObject *parent = 0, ObsConditions *obs = 0);
    ~WIView();
    void loadDetailsView(SkyObjItem* soitem, int index);

public slots:
    void onCategorySelected(int);
    void onSoListItemClicked(int type, QString typeName, int index);
    void onNextObjTextClicked();
private:
    QObject *m_BaseObj, *m_ViewsRowObj, *m_SoListObj, *m_DetailsViewObj, *m_NextObj;
    QDeclarativeContext *ctxt;
    ModelManager *m;
    SkyObjItem *m_CurSoItem;
    int m_CurIndex;
    double m_OptMag;
};

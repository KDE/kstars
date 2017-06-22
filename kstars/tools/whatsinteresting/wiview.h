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

#pragma once

#include "skyobject.h"
#include "modelmanager.h"
#include "skyobjlistmodel.h"
#include "obsconditions.h"

class QQmlContext;
class QQuickView;
class QQuickItem;

/**
  * \class WIView
  * \brief Manages the QML user interface for What's Interesting.
  * WIView is used to display the QML UI using a QQuickView.
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
    explicit WIView(QWidget *parent = nullptr);

    /**
     * \brief Destructor
     */
    ~WIView();

    /**
     * \brief Load details-view for selected sky-object
     */
    void loadDetailsView(SkyObjItem *soitem, int index);

    /**
     * \brief Updates sky-object list models
     */
    void updateModel(ObsConditions& obs);

    inline QQuickView *getWIBaseView() const { return m_BaseView; }

  public slots:

    /**
     * \brief public slot - Act upon signal emitted when category of sky-object is selected
     * from category selection view of the QML UI.
     * \param type Category selected
     */
    void onCategorySelected(QString model);

    /**
     * \brief public slot - Act upon signal emitted when an item is selected from list of sky-objects.
     * Display details-view for the skyobject selected.
     * \param type        Category selected.
     * \param typename    Name of category selected.
     * \param index       Index of item in the list of skyobjects.
     */
    void onSoListItemClicked(int index);

    /**
     * \brief public slot - Show details-view for next sky-object from list of current sky-objects's category.
     */
    void onNextObjClicked();

    /**
     * \brief public slot - Show details-view for previous sky-object from list of current sky-objects's
     * category.
     */
    void onPrevObjClicked();

    /**
     * \brief public slot - Slew map to current sky-object in the details view.
     */
    void onCenterButtonClicked();

    /**
     * \brief public slot - Slew map to current sky-object in the details view.
     */
    void onSlewTelescopeButtonClicked();

    /**
     * \brief public slot - Open Details Dialog to show more details for current sky-object.
     */
    void onDetailsButtonClicked();

    /**
     * \brief public slot - Open WI settings dialog.
     */
    void onSettingsIconClicked();

    void onInspectIconClicked(bool checked) { inspectOnClick = checked; }

    /**
     * \brief public slot - Reload list of visible sky-objects.
     */
    void onReloadIconClicked();

    void onVisibleIconClicked(bool checked);

    void onFavoriteIconClicked(bool checked);

    void onUpdateIconClicked();

    void updateWikipediaDescription(SkyObjItem *soitem);
    void loadObjectDescription(SkyObjItem *soitem);
    void tryToUpdateWikipediaInfo(SkyObjItem *soitem, QString name);
    void loadObjectInfoBox(SkyObjItem *soitem);
    void saveImageURL(SkyObjItem *soitem, QString imageURL);
    void saveInfoURL(SkyObjItem *soitem, QString infoURL);
    void saveObjectInfoBoxText(SkyObjItem *soitem, QString type, QString infoText);
    void downloadWikipediaImage(SkyObjItem *soitem, QString imageURL);
    void inspectSkyObject(const QString& name);
    void inspectSkyObjectOnClick(SkyObject *obj);
    void inspectSkyObject(SkyObject *obj);
    bool inspectOnClickIsActive() { return inspectOnClick; }
    void updateObservingConditions();
    void tryToUpdateWikipediaInfoInModel(bool onlyMissing);
    void refreshListView();
    void updateProgress(double value);
    void setProgressBarVisible(bool visible);
    void setNightVisionOn(bool on);

  private:
    QQuickItem *m_BaseObj, *m_ViewsRowObj, *m_CategoryTitle, *m_SoListObj, *m_DetailsViewObj, *m_skyObjView,
        *m_ContainerObj, *m_ProgressBar, *m_loadingMessage, *m_NextObj, *m_PrevObj, *m_CenterButtonObj,
        *m_SlewTelescopeButtonObj, *m_DetailsButtonObj, *inspectIconObj, *visibleIconObj, *favoriteIconObj;
    QQmlContext *m_Ctxt;
    QObject *infoBoxText, *descTextObj, *nightVision, *autoTrackCheckbox, *autoCenterCheckbox;

    QQuickView *m_BaseView;
    ObsConditions *m_Obs = nullptr;
    ModelManager *m_ModManager;
    /// Current sky-object item.
    SkyObjItem *m_CurSoItem = nullptr;
    /// Index of current sky-object item in Details view.
    int m_CurIndex;
    /// Currently selected category from WI QML view
    QString m_CurrentObjectListName;
    QString getWikipediaName(SkyObjItem *soitem);
    QNetworkAccessManager *manager;
    bool inspectOnClick;
};

/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QWidget>

#include <memory>

class QNetworkAccessManager;
class QQmlContext;
class QQuickItem;
class QQuickView;

class ModelManager;
class ObsConditions;
class SkyObject;
class SkyObjItem;

/**
 * @class WIView
 * @brief Manages the QML user interface for What's Interesting. WIView is used to display the
 * QML UI using a QQuickView. It acts on all signals emitted by the UI and manages the data
 * sent to the UI for display.
 *
 * @author Samikshan Bairagya
 */
class WIView : public QWidget
{
    Q_OBJECT
  public:
    /**
     * @brief Constructor - Store QML components as QObject pointers.
     * Connect signals from various QML components into public slots.
     * Displays the user interface for What's Interesting
     */
    explicit WIView(QWidget *parent = nullptr);

    virtual ~WIView() override = default;

    /** Load details-view for selected sky-object */
    void loadDetailsView(SkyObjItem *soitem, int index);

    /** Updates sky-object list models */
    void updateModel(ObsConditions& obs);

    inline QQuickView *getWIBaseView() const { return m_BaseView; }

  public slots:

    /**
     * @brief public slot - Act upon signal emitted when category of sky-object is selected
     * from category selection view of the QML UI.
     * @param model Category selected
     */
    void onCategorySelected(QString model);

    /**
     * @brief public slot - Act upon signal emitted when an item is selected from list of sky-objects.
     * Display details-view for the skyobject selected.
     * @param index       Index of item in the list of skyobjects.
     */
    void onSoListItemClicked(int index);

    /** public slot - Show details-view for next sky-object from list of current sky-objects's category. */
    void onNextObjClicked();

    /**
     * @brief public slot - Show details-view for previous sky-object from list of current sky-objects's
     * category.
     */
    void onPrevObjClicked();

    /** public slot - Slew map to current sky-object in the details view. */
    void onCenterButtonClicked();

    /** public slot - Slew map to current sky-object in the details view. */
    void onSlewTelescopeButtonClicked();

    /** public slot - Open Details Dialog to show more details for current sky-object. */
    void onDetailsButtonClicked();

    /** public slot - Open WI settings dialog. */
    void onSettingsIconClicked();

    void onInspectIconClicked(bool checked) { inspectOnClick = checked; }

    /** public slot - Reload list of visible sky-objects. */
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
    QString getWikipediaName(SkyObjItem *soitem);

    QQuickItem *m_BaseObj { nullptr };
    QQuickItem *m_ViewsRowObj { nullptr };
    QQuickItem *m_CategoryTitle { nullptr };
    QQuickItem *m_SoListObj { nullptr };
    QQuickItem *m_DetailsViewObj { nullptr };
    QQuickItem *m_ProgressBar { nullptr };
    QQuickItem *m_loadingMessage { nullptr };
    QQuickItem *m_NextObj { nullptr };
    QQuickItem *m_PrevObj { nullptr };
    QQuickItem *m_CenterButtonObj { nullptr };
    QQuickItem *m_SlewTelescopeButtonObj { nullptr };
    QQuickItem *m_DetailsButtonObj { nullptr };
    QQuickItem *inspectIconObj { nullptr };
    QQuickItem *visibleIconObj { nullptr };
    QQuickItem *favoriteIconObj { nullptr };
    QQmlContext *m_Ctxt { nullptr };
    QObject *infoBoxText { nullptr };
    QObject *descTextObj { nullptr };
    QObject *nightVision { nullptr };
    QObject *autoTrackCheckbox { nullptr };
    QObject *autoCenterCheckbox { nullptr };

    QQuickView *m_BaseView { nullptr };
    ObsConditions *m_Obs { nullptr };
    std::unique_ptr<ModelManager> m_ModManager;
    /// Current sky object item.
    SkyObjItem *m_CurSoItem { nullptr };
    /// Created sky object item on-demand
    std::unique_ptr<SkyObjItem> trackedItem;
    /// Index of current sky-object item in Details view.
    int m_CurIndex { 0 };
    /// Currently selected category from WI QML view
    QString m_CurrentObjectListName;
    std::unique_ptr<QNetworkAccessManager> manager;
    bool inspectOnClick { false };
};

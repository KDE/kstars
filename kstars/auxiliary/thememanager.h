/*
    This file is a part of digiKam project
    https://www.digikam.org

    SPDX-FileCopyrightText: 2006-2018 Gilles Caulier <caulier dot gilles at gmail dot com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <ksharedconfig.h>

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QListWidget>

class QMenu;
class KXmlGuiWindow;

namespace KSTheme
{

class Manager : public QObject
{
    Q_OBJECT

public:

    typedef enum { BREEZE_THEME, BREEZE_DARK_THEME } IconTheme;

    ~Manager();
    static Manager* instance();

    QString currentThemeName() const;
    void    setCurrentTheme(const QString& name);

    QString defaultThemeName() const;

    void    setThemeMenuAction(QMenu* const action);
    void    registerThemeActions(KXmlGuiWindow * const win);
    void    populateThemeQListWidget(QListWidget *themeWidget);

    void setIconTheme(IconTheme theme);

signals:

    void signalThemeChanged();

private Q_SLOTS:

    void slotChangePalette();
    void slotSettingsChanged();

private:

    Manager();

    void    populateThemeMenu();
    QPixmap createSchemePreviewIcon(const KSharedConfigPtr& config) const;
    QString currentDesktopdefaultTheme() const;
    void    updateCurrentDesktopDefaultThemePreview();

private:

    friend class ThemeManagerCreator;

    class Private;
    Private* const d;
};

}

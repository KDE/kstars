/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2004-08-02
 * Description : colors theme manager
 *
 * Copyright (C) 2006-2018 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

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

/*
    This file is a part of digiKam project
    https://www.digikam.org

    SPDX-FileCopyrightText: 2006-2018 Gilles Caulier <caulier dot gilles at gmail dot com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thememanager.h"

#include "kstars.h"
#include "kstars_debug.h"
#include "Options.h"
#include "schememanager.h"
#include "widgets/dmsbox.h"

#include <KLocalizedString>
#include <KActionCollection>
#include <KConfigGroup>
#include <KXmlGuiWindow>

#include <QTabBar>
#include <QPixmapCache>
#include <QStringList>
#include <QStyleFactory>
#include <QFileInfo>
#include <QPalette>
#include <QColor>
#include <QActionGroup>
#include <QBitmap>
#include <QPainter>
#include <QPixmap>
#include <QApplication>
#include <QAction>
#include <QStandardPaths>
#include <QDirIterator>
#include <QMenu>
#include <QStyle>
#include <QResource>

namespace KSTheme
{

class ThemeManagerCreator
{
    public:

        Manager object;
};

Q_GLOBAL_STATIC(ThemeManagerCreator, creator)

// ---------------------------------------------------------------


class Manager::Private
{
    public:

        Private()
            : defaultThemeName(i18nc("default theme name", "Default")),
              themeMenuActionGroup(0),
              themeMenuAction(0)
        {
        }

        const QString          defaultThemeName;
        QMap<QString, QString> themeMap;            // map<theme name, theme config path>

        QActionGroup*          themeMenuActionGroup;
        QMenu*                 themeMenuAction;
};

Manager::Manager()
    : d(new Private)
{
}

Manager::~Manager()
{
    delete d;
}

Manager* Manager::instance()
{
    return &creator->object;
}

QString Manager::defaultThemeName() const
{
    return d->defaultThemeName;
}

QString Manager::currentThemeName() const
{
    if (!d->themeMenuAction || !d->themeMenuActionGroup)
    {
        return defaultThemeName();
    }

    QAction* const action = d->themeMenuActionGroup->checkedAction();
    return (!action ? defaultThemeName()
            : action->text().remove(QLatin1Char('&')));
}

void Manager::setCurrentTheme(const QString &name)
{
    if (!d->themeMenuAction || !d->themeMenuActionGroup)
    {
        return;
    }

    QList<QAction*> list = d->themeMenuActionGroup->actions();

    foreach(QAction* const action, list)
    {
        if (action->text().remove(QLatin1Char('&')) == name)
        {
            action->setChecked(true);
            slotChangePalette();
        }
    }
}

void Manager::slotChangePalette()
{
    updateCurrentDesktopDefaultThemePreview();

    QString theme(currentThemeName());

    /*if (theme == defaultThemeName() || theme.isEmpty())
    {
        theme = currentDesktopdefaultTheme();
    }*/

    //QString themeIconName("breeze-dark");
    IconTheme themeIconType = BREEZE_DARK_THEME;

    if (theme == "Macintosh" || theme == "White Balance" || theme == "High Key" || (theme == "Default"
            && currentDesktopdefaultTheme().contains("Dark") == false))
        themeIconType = BREEZE_THEME;

    setIconTheme(themeIconType);

    QString filename        = d->themeMap.value(theme);
    KSharedConfigPtr config = KSharedConfig::openConfig(filename);
    // hint for the style to synchronize the color scheme with the window manager/compositor
    qApp->setProperty("KDE_COLOR_SCHEME_PATH", filename);
    QPalette palette = SchemeManager::createApplicationPalette(config);

    qApp->setPalette(palette);

    QList<QWidget *> widgets = qApp->allWidgets();
    foreach(QWidget *w, widgets)
    {
        dmsBox *box = qobject_cast<dmsBox *>(w);
        if(box)
            box->setPalette(palette);
        QTabBar *bar = qobject_cast<QTabBar *>(w);
        if(bar)
            bar->setPalette(palette);
    }

    if(theme == "Macintosh")
        qApp->setStyle(QStyleFactory::create("macintosh"));
    else
    {
        if (qgetenv("XDG_CURRENT_DESKTOP") != "KDE")
            qApp->setStyle(QStyleFactory::create("Fusion"));
    }

    // Special case. For Night Vision theme, we also change the Sky Color Scheme
    if (theme == "Default" && Options::colorSchemeFile() == "night.colors")
        KStars::Instance()->actionCollection()->action("cs_moonless-night")->setChecked(true);
    else if (theme == "Night Vision")
        KStars::Instance()->actionCollection()->action("cs_night")->setChecked(true);

    QPixmapCache::clear();

    qApp->style()->polish(qApp);

    qCDebug(KSTARS) << theme << " :: " << filename;

    emit signalThemeChanged();
}

void Manager::setThemeMenuAction(QMenu* const action)
{
    d->themeMenuAction = action;

    action->setStyleSheet("QMenu::icon:checked {background: gray;border: 1px inset gray;position: absolute;"
                          "top: 1px;right: 1px;bottom: 1px;left: 1px;}");

    populateThemeMenu();
}

void Manager::registerThemeActions(KXmlGuiWindow* const win)
{
    if (!win)
    {
        return;
    }

    if (!d->themeMenuAction)
    {
        qCDebug(KSTARS) << "Cannot register theme actions to " << win->windowTitle();
        return;
    }

    win->actionCollection()->addAction(QLatin1String("themes"), d->themeMenuAction->menuAction());
}

void Manager::populateThemeQListWidget(QListWidget *themeWidget)
{
    themeWidget->clear();

    QList<QAction*> list = d->themeMenuActionGroup->actions();

    foreach(QAction* const action, list)
    {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText( action->text().remove('&') );
        item->setIcon( action->icon() );
        themeWidget->addItem(item);
    }
    themeWidget->sortItems();
}

void Manager::populateThemeMenu()
{
    if (!d->themeMenuAction)
    {
        return;
    }

    //QString theme(currentThemeName());

    d->themeMenuAction->clear();
    delete d->themeMenuActionGroup;

    d->themeMenuActionGroup = new QActionGroup(d->themeMenuAction);

    connect(d->themeMenuActionGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotChangePalette()));

    QAction* const action   = new QAction(defaultThemeName(), d->themeMenuActionGroup);
    action->setCheckable(true);
    d->themeMenuAction->addAction(action);

    QStringList schemeFiles;
    QStringList dirs;

    // kstars themes
    dirs << QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                      QString::fromLatin1("kstars/themes"),
                                      QStandardPaths::LocateDirectory);

    qCDebug(KSTARS) << "Paths to color scheme : " << dirs;

    Q_FOREACH (const QString &dir, dirs)
    {
        QDirIterator it(dir, QStringList() << QLatin1String("*.colors"));

        while (it.hasNext())
        {
            schemeFiles.append(it.next());
        }
    }

    QMap<QString, QAction*> actionMap;

    for (int i = 0; i < schemeFiles.size(); ++i)
    {
        const QString filename  = schemeFiles.at(i);
        const QFileInfo info(filename);
        KSharedConfigPtr config = KSharedConfig::openConfig(filename);
        QIcon icon              = createSchemePreviewIcon(config);
        KConfigGroup group(config, "General");
        const QString name      = group.readEntry("Name", info.baseName());
        QAction* const ac       = new QAction(name, d->themeMenuActionGroup);
        d->themeMap.insert(name, filename);
        ac->setIcon(icon);
        ac->setCheckable(true);
        actionMap.insert(name, ac);
    }

#ifdef Q_OS_MAC
    QAction* const macAction   = new QAction(QLatin1String("Macintosh"), d->themeMenuActionGroup);
    macAction->setCheckable(true);
    //TODO Set appropriate icon
    //macAction->setAction(..)
    actionMap.insert(QLatin1String("Macintosh"), macAction);
#endif

    // sort the list
    QStringList actionMapKeys = actionMap.keys();
    actionMapKeys.sort();

    foreach(const QString &name, actionMapKeys)
    {
        d->themeMenuAction->addAction(actionMap.value(name));
    }

    updateCurrentDesktopDefaultThemePreview();
    //setCurrentTheme(theme);
}

void Manager::updateCurrentDesktopDefaultThemePreview()
{
    QList<QAction*> list = d->themeMenuActionGroup->actions();

    foreach(QAction* const action, list)
    {
        if (action->text().remove(QLatin1Char('&')) == defaultThemeName())
        {
            KSharedConfigPtr config = KSharedConfig::openConfig(d->themeMap.value(currentDesktopdefaultTheme()));
            QIcon icon              = createSchemePreviewIcon(config);
            action->setIcon(icon);
        }
    }
}

QPixmap Manager::createSchemePreviewIcon(const KSharedConfigPtr &config) const
{
    const uchar bits1[] = { 0xff, 0xff, 0xff, 0x2c, 0x16, 0x0b };
    const uchar bits2[] = { 0x68, 0x34, 0x1a, 0xff, 0xff, 0xff };
    const QSize bitsSize(24, 2);
    const QBitmap b1    = QBitmap::fromData(bitsSize, bits1);
    const QBitmap b2    = QBitmap::fromData(bitsSize, bits2);

    QPixmap pixmap(23, 16);
    pixmap.fill(Qt::black); // FIXME use some color other than black for borders?

    KConfigGroup group(config, QLatin1String("WM"));
    QPainter p(&pixmap);
    SchemeManager windowScheme(QPalette::Active, SchemeManager::Window, config);
    p.fillRect(1,  1, 7, 7, windowScheme.background());
    p.fillRect(2,  2, 5, 2, QBrush(windowScheme.foreground().color(), b1));

    SchemeManager buttonScheme(QPalette::Active, SchemeManager::Button, config);
    p.fillRect(8,   1, 7, 7, buttonScheme.background());
    p.fillRect(9,   2, 5, 2, QBrush(buttonScheme.foreground().color(), b1));

    p.fillRect(15,  1, 7, 7, group.readEntry(QLatin1String("activeBackground"),        QColor(96, 148, 207)));
    p.fillRect(16,  2, 5, 2, QBrush(group.readEntry(QLatin1String("activeForeground"), QColor(255, 255, 255)), b1));

    SchemeManager viewScheme(QPalette::Active, SchemeManager::View, config);
    p.fillRect(1,   8, 7, 7, viewScheme.background());
    p.fillRect(2,  12, 5, 2, QBrush(viewScheme.foreground().color(), b2));

    SchemeManager selectionScheme(QPalette::Active, SchemeManager::Selection, config);
    p.fillRect(8,   8, 7, 7, selectionScheme.background());
    p.fillRect(9,  12, 5, 2, QBrush(selectionScheme.foreground().color(), b2));

    p.fillRect(15,  8, 7, 7, group.readEntry(QLatin1String("inactiveBackground"),        QColor(224, 223, 222)));
    p.fillRect(16, 12, 5, 2, QBrush(group.readEntry(QLatin1String("inactiveForeground"), QColor(20,  19,  18)), b2));

    p.end();
    return pixmap;
}

QString Manager::currentDesktopdefaultTheme() const
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QLatin1String("kdeglobals"));
    KConfigGroup group(config, "General");
    return group.readEntry("ColorScheme");
}

void Manager::slotSettingsChanged()
{
    populateThemeMenu();
    slotChangePalette();
}

void Manager::setIconTheme(IconTheme theme)
{
    QString rccFile("breeze-icons.rcc");
    QString iconTheme("breeze");

#if QT_VERSION < 0x051200
    qCWarning(KSTARS) << "Current icon theme is" << QIcon::themeName();
#else
    qCWarning(KSTARS) << "Current icon theme is" << QIcon::themeName() << "(fallback" << QIcon::fallbackThemeName() << ")";
#endif

    if (theme == BREEZE_DARK_THEME)
    {
        rccFile = "breeze-icons-dark.rcc";
        iconTheme = "breeze-dark";
    }

    QStringList themeSearchPaths = (QStringList() << QIcon::themeSearchPaths());
#ifdef Q_OS_OSX
    themeSearchPaths = themeSearchPaths << QDir(QCoreApplication::applicationDirPath() + "/../Resources/icons").absolutePath();
    QString resourcePath = QDir(QCoreApplication::applicationDirPath() + "/../Resources/icons/" + rccFile).absolutePath();
    QResource::registerResource(resourcePath, "/icons/" + iconTheme);
#elif defined(Q_OS_WIN)
    themeSearchPaths = themeSearchPaths << QStandardPaths::locate(QStandardPaths::GenericDataLocation, "icons",
                       QStandardPaths::LocateDirectory);
    QString resourcePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "icons",
                           QStandardPaths::LocateDirectory) + QDir::separator() + iconTheme + QDir::separator() + rccFile;
    QResource::registerResource(resourcePath, "/icons/" + iconTheme);
#else
    // On Linux on non-KDE Distros, find out if the themes are installed or not and perhaps warn the user
    // The important point is that the current theme must be left as is to avoid empty icons
    {
        bool missing = true;
        foreach (auto &path, themeSearchPaths)
            if (QFile(path + '/' + iconTheme + "/index.theme").exists())
                missing = false;

        if (missing)
        {
            qCWarning(KSTARS) << "Warning: icon theme" << iconTheme << "not found, keeping original" << QIcon::themeName() << ".";
            iconTheme = QIcon::themeName();
        }
    }
#endif

    QIcon::setThemeSearchPaths(themeSearchPaths);
    //Note: in order to get it to actually load breeze from resources on mac, we had to add the index.theme, and just one icon from breeze into the qrc.  Not sure why this was needed, but it works.
    QIcon::setThemeName(iconTheme);
}

}

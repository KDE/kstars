/*
    SPDX-FileCopyrightText: 2008 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsxplanet.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include <QDesktopServices>
#include <QFileDialog>

OpsXplanet::OpsXplanet(KStars *_ks) : QFrame(_ks), ksw(_ks)
{
    setupUi(this);

#ifdef Q_OS_OSX
    connect(kcfg_xplanetIsInternal, SIGNAL(clicked()), this, SLOT(toggleXPlanetInternal()));
    kcfg_xplanetIsInternal->setToolTip(i18n("Internal or External XPlanet?"));

    if (Options::xplanetIsInternal())
        kcfg_XplanetPath->setEnabled(false);

#else
    kcfg_xplanetIsInternal->setVisible(false);
#endif

    // Init projections combobox
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "No projection"), "no projection");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Ancient"), "ancient");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Azimuthal"), "azimuthal");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Bonne"), "bonne");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Gnomonic"), "gnomonic");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Hemisphere"), "hemisphere");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Lambert"), "lambert");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Mercator"), "mercator");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Mollweide"), "mollweide");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Orthographic"), "orthographic");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Peters"), "peters");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Polyconic"), "polyconic");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "Rectangular"), "rectangular");
    kcfg_XplanetProjection->addItem(i18nc("Map projection method", "TSC"), "tsc");

    // Enable/Disable somme widgets
    connect(kcfg_XplanetConfigFile, SIGNAL(toggled(bool)), SLOT(slotConfigFileWidgets(bool)));
    connect(kcfg_XplanetStarmap, SIGNAL(toggled(bool)), SLOT(slotStarmapFileWidgets(bool)));
    connect(kcfg_XplanetArcFile, SIGNAL(toggled(bool)), SLOT(slotArcFileWidgets(bool)));
    connect(kcfg_XplanetLabel, SIGNAL(toggled(bool)), SLOT(slotLabelWidgets(bool)));
    connect(kcfg_XplanetMarkerFile, SIGNAL(toggled(bool)), SLOT(slotMarkerFileWidgets(bool)));
    connect(kcfg_XplanetMarkerBounds, SIGNAL(toggled(bool)), SLOT(slotMarkerBoundsWidgets(bool)));
    connect(kcfg_XplanetProjection, SIGNAL(currentIndexChanged(int)), SLOT(slotProjectionWidgets(int)));
    connect(kcfg_XplanetBackground, SIGNAL(toggled(bool)), SLOT(slotBackgroundWidgets(bool)));

    kcfg_XplanetConfigFilePath->setEnabled(Options::xplanetConfigFile());
    configFileB->setEnabled(Options::xplanetConfigFile());
    configFileB->setIcon(QIcon::fromTheme("document-open-folder"));
    configFileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(configFileB, SIGNAL(clicked()), this, SLOT(slotSelectConfigFile()));
    kcfg_XplanetStarmapPath->setEnabled(Options::xplanetStarmap());
    starmapFileB->setEnabled(Options::xplanetStarmap());
    starmapFileB->setIcon(QIcon::fromTheme("document-open-folder"));
    starmapFileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(starmapFileB, SIGNAL(clicked()), this, SLOT(slotSelectStarMapFile()));
    kcfg_XplanetArcFilePath->setEnabled(Options::xplanetArcFile());
    arcFileB->setEnabled(Options::xplanetArcFile());
    arcFileB->setIcon(QIcon::fromTheme("document-open-folder"));
    arcFileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(arcFileB, SIGNAL(clicked()), this, SLOT(slotSelectArcFile()));
    kcfg_XplanetLabelLocalTime->setEnabled(Options::xplanetLabel());
    kcfg_XplanetLabelGMT->setEnabled(Options::xplanetLabel());
    textLabelXplanetLabelString->setEnabled(Options::xplanetLabel());
    kcfg_XplanetLabelString->setEnabled(Options::xplanetLabel());
    textLabelXplanetDateFormat->setEnabled(Options::xplanetLabel());
    kcfg_XplanetDateFormat->setEnabled(Options::xplanetLabel());
    textLabelXplanetFontSize->setEnabled(Options::xplanetLabel());
    kcfg_XplanetFontSize->setEnabled(Options::xplanetLabel());
    textLabelXplanetColor->setEnabled(Options::xplanetLabel());
    kcfg_XplanetColor->setEnabled(Options::xplanetLabel());
    textLabelLabelPos->setEnabled(Options::xplanetLabel());
    kcfg_XplanetLabelTL->setEnabled(Options::xplanetLabel());
    kcfg_XplanetLabelTR->setEnabled(Options::xplanetLabel());
    kcfg_XplanetLabelBR->setEnabled(Options::xplanetLabel());
    kcfg_XplanetLabelBL->setEnabled(Options::xplanetLabel());
    kcfg_XplanetMarkerFilePath->setEnabled(Options::xplanetMarkerFile());
    kcfg_XplanetMarkerBounds->setEnabled(Options::xplanetMarkerFile());
    if (Options::xplanetMarkerFile() && Options::xplanetMarkerBounds())
        kcfg_XplanetMarkerBoundsPath->setEnabled(true);
    else
        kcfg_XplanetMarkerBoundsPath->setEnabled(false);
    if (Options::xplanetProjection() == 0)
        groupBoxBackground->setEnabled(false);
    kcfg_XplanetBackgroundImage->setEnabled(Options::xplanetBackgroundImage());
    kcfg_XplanetBackgroundImagePath->setEnabled(Options::xplanetBackgroundImage());
    kcfg_XplanetBackgroundColor->setEnabled(Options::xplanetBackgroundImage());
    kcfg_XplanetBackgroundColorValue->setEnabled(Options::xplanetBackgroundImage());
    if (Options::xplanetProjection() == 0)
        groupBoxBackground->setEnabled(false);

    #ifdef Q_OS_WIN
        kcfg_XplanetUseFIFO->setChecked(false);
        kcfg_XplanetUseFIFO->setDisabled(true);
        kcfg_XplanetUseFIFO->setToolTip(i18n("FIFO files are not supported on Windows"));
    #endif

    connect(openXPlanetMaps, SIGNAL(clicked()),this,SLOT(showXPlanetMapsDirectory()));
}

void OpsXplanet::showXPlanetMapsDirectory()
{

    QString xplanetMapsDir = XPlanetShareDirectory() + QDir::separator() + "images";

    QUrl path = QUrl::fromLocalFile(xplanetMapsDir);
    QDesktopServices::openUrl(path);
}

QString OpsXplanet::XPlanetShareDirectory()
{
    #ifdef Q_OS_WIN
        const QFileInfo xPlanetLocationInfo(Options::xplanetPath());
        return xPlanetLocationInfo.dir().absolutePath() + QDir::separator() + "xplanet";
    #endif

    if (Options::xplanetIsInternal())
        return QCoreApplication::applicationDirPath() + "/xplanet/share/xplanet";
    else
        return Options::xplanetPath() + "../share/xplanet";
}

void OpsXplanet::toggleXPlanetInternal()
{
    kcfg_XplanetPath->setEnabled(!kcfg_xplanetIsInternal->isChecked());
    if (kcfg_xplanetIsInternal->isChecked())
        kcfg_XplanetPath->setText("*Internal XPlanet*");
    else
        kcfg_XplanetPath->setText(KSUtils::getDefaultPath("XplanetPath"));
}

void OpsXplanet::slotConfigFileWidgets(bool on)
{
    kcfg_XplanetConfigFilePath->setEnabled(on);
    configFileB->setEnabled(on);
}

void OpsXplanet::slotStarmapFileWidgets(bool on)
{
    kcfg_XplanetStarmapPath->setEnabled(on);
    starmapFileB->setEnabled(on);
}

void OpsXplanet::slotArcFileWidgets(bool on)
{
    kcfg_XplanetArcFilePath->setEnabled(on);
    arcFileB->setEnabled(on);
}

void OpsXplanet::slotLabelWidgets(bool on)
{
    kcfg_XplanetLabelLocalTime->setEnabled(on);
    kcfg_XplanetLabelGMT->setEnabled(on);
    textLabelXplanetLabelString->setEnabled(on);
    kcfg_XplanetLabelString->setEnabled(on);
    textLabelXplanetDateFormat->setEnabled(on);
    kcfg_XplanetDateFormat->setEnabled(on);
    textLabelXplanetFontSize->setEnabled(on);
    kcfg_XplanetFontSize->setEnabled(on);
    textLabelXplanetColor->setEnabled(on);
    kcfg_XplanetColor->setEnabled(on);
    textLabelLabelPos->setEnabled(on);
    kcfg_XplanetLabelTL->setEnabled(on);
    kcfg_XplanetLabelTR->setEnabled(on);
    kcfg_XplanetLabelBR->setEnabled(on);
    kcfg_XplanetLabelBL->setEnabled(on);
}

void OpsXplanet::slotMarkerFileWidgets(bool on)
{
    kcfg_XplanetMarkerFilePath->setEnabled(on);
    kcfg_XplanetMarkerBounds->setEnabled(on);
    if (kcfg_XplanetMarkerBounds->isChecked())
        kcfg_XplanetMarkerBoundsPath->setEnabled(on);
}

void OpsXplanet::slotMarkerBoundsWidgets(bool on)
{
    kcfg_XplanetMarkerBoundsPath->setEnabled(on);
}

void OpsXplanet::slotProjectionWidgets(int index)
{
    if (index == 0)
        groupBoxBackground->setEnabled(false);
    else
        groupBoxBackground->setEnabled(true);

    if (!kcfg_XplanetBackground->isChecked())
    {
        kcfg_XplanetBackgroundImage->setEnabled(false);
        kcfg_XplanetBackgroundImagePath->setEnabled(false);
        kcfg_XplanetBackgroundColor->setEnabled(false);
        kcfg_XplanetBackgroundColorValue->setEnabled(false);
    }
}

void OpsXplanet::slotBackgroundWidgets(bool on)
{
    kcfg_XplanetBackgroundImage->setEnabled(on);
    kcfg_XplanetBackgroundImagePath->setEnabled(on);
    kcfg_XplanetBackgroundColor->setEnabled(on);
    kcfg_XplanetBackgroundColorValue->setEnabled(on);
}

void OpsXplanet::slotSelectConfigFile()
{
    QString xplanetConfig = XPlanetShareDirectory() + QDir::separator() + "config";
    QString file =
        QFileDialog::getOpenFileName(KStars::Instance(), i18nc("@title:window", "Select XPlanet Config File"), xplanetConfig);

    if (!file.isEmpty())
        kcfg_XplanetConfigFilePath->setText(QFileInfo(file).completeBaseName());

}

void OpsXplanet::slotSelectStarMapFile()
{
    QString xplanetStarMap = XPlanetShareDirectory() + QDir::separator() + "stars";

    QString file =
        QFileDialog::getOpenFileName(KStars::Instance(), i18nc("@title:window", "Select XPlanet Star Map File"), xplanetStarMap);

    if (!file.isEmpty())
        kcfg_XplanetStarmapPath->setText(QFileInfo(file).completeBaseName());

}

void OpsXplanet::slotSelectArcFile()
{

    QString xplanetArc = XPlanetShareDirectory() + QDir::separator() + "arcs";

    QString file =
        QFileDialog::getOpenFileName(KStars::Instance(), i18nc("@title:window", "Select XPlanet Arc File"), xplanetArc);

    if (!file.isEmpty())
        kcfg_XplanetArcFilePath->setText(QFileInfo(file).completeBaseName());

}

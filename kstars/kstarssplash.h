/*
    SPDX-FileCopyrightText: 2001 Heiko Evermann <heiko@evermann.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSplashScreen>

/**
 * @class KStarsSplash
 * The KStars Splash Screen.  The splash screen shows the KStars logo and
 * progress messages while data files are parsed and objects are initialized.
 *
 * @short the KStars Splash Screen.
 * @author Heiko Evermann
 * @version 1.0
 */
class KStarsSplash : public QSplashScreen
{
    Q_OBJECT

  public:
    /**
     * Constructor. Create widgets.  Load KStars logo.  Start load timer.
     * A non-empty customMessage will replace "Welcome to KStars [...]".
     */
    explicit KStarsSplash(const QString &customMessage = "");

    virtual ~KStarsSplash() override = default;

  public slots:
    /**
     * Display the text argument in the Splash Screen's status label.
     * This is connected to KStarsData::progressText(QString)
     */
    void setMessage(const QString &s);
};

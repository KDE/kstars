/*  KStars Asynchronous Message Box Implementation for Desktop/Android and EkosLive
    Based on QMessageBox.

    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#pragma once

#include <memory>

#include <KLocalizedString>
#include <QMessageBox>
#include <QTimer>
#include <QRoundProgressBar.h>

/**
 * @class KSMessageBox
 * KStars Message Box implementation.
 *
 * @author Jasem Mutlaq
 */
class KSMessageBox: public QMessageBox
{
    Q_OBJECT
    Q_PROPERTY(quint32 timeout MEMBER m_Timeout)

    public:
        static KSMessageBox *Instance();

        void error(const QString &message, const QString &title = i18n("Error"));
        void sorry(const QString &message, const QString &title = i18n("Sorry"));
        void info(const QString &message, const QString &title = i18n("Info"));
        /**
         * @brief transient Non modal message box that gets deleted on close.
         * @param message message content
         * @param title message title
         */
        void transient(const QString &message, const QString &title);

    protected:

        void timerTick();

    private:
        // Dialog timeout in seconds
        quint32 m_Timeout {30};

        static KSMessageBox *m_Instance;

        KSMessageBox();

        std::unique_ptr<QRoundProgressBar> m_ProgressIndicator;

        QTimer m_ProgressTimer;
};


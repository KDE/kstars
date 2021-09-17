/*  KStars Asynchronous Message Box Implementation for Desktop/Android and EkosLive
    Based on QMessageBox.

    SPDX-FileCopyrightText: 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KLocalizedString>

#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QRoundProgressBar.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QHBoxLayout>

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

        void questionYesNo(const QString &message, const QString &title = i18n("Question"), quint32 timeout = 0,
                           bool defaultToYes = true, const QString &yesText = i18n("Yes"), const QString &noText = i18n("No"));
        void warningContinueCancel(const QString &message, const QString &title = i18n("Warning"), quint32 timeout = 0,
                                   bool defaultToContinue = true, const QString &continueText = i18n("Continue"), const QString &cancelText = i18n("Cancel"));
        void error(const QString &message, const QString &title = i18n("Error"));
        void sorry(const QString &message, const QString &title = i18n("Sorry"));
        void info(const QString &message, const QString &title = i18n("Info"));
        /**
         * @brief transient Non modal message box that gets deleted on close.
         * @param message message content
         * @param title message title
         */
        void transient(const QString &message, const QString &title);

        /**
         * @brief selectResponse Programatically select one the buttons in the dialog.
         * @param button text of button to click
         * @return True if button is found and clicked, false otherwise.
         */
        bool selectResponse(const QString &button);

    signals:
        void newMessage(const QJsonObject &message);

    protected:
        void timerTick();

    private:
        // Dialog timeout in seconds
        quint32 m_Timeout {60};

        static KSMessageBox *m_Instance;

        void reset();
        void addOKButton();
        void setupTimeout();
        QJsonObject createMessageObject();
        KSMessageBox();

        QPointer<QRoundProgressBar> m_ProgressIndicator;
        QPointer<QLabel> m_ProgressLabel;
        QPointer<QHBoxLayout> m_ProgressLayout;

        QTimer m_ProgressTimer;
};


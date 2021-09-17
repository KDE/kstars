/*  KStars Asynchronous Message Box Implementation for Desktop/Android and EkosLive
    Based on QMessageBox.

    SPDX-FileCopyrightText: 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksmessagebox.h"
#include "config-kstars.h"
#include "Options.h"

#ifdef KSTARS_LITE
#include "kstarslite.h"
#else
#include "kstars.h"
#endif

#include <QPushButton>
#include <QCheckBox>
#include <QLayout>
#include <QSpacerItem>
#include <QLabel>

KSMessageBox * KSMessageBox::m_Instance = nullptr;

KSMessageBox * KSMessageBox::Instance()
{
    if (!m_Instance)
        m_Instance = new KSMessageBox();

    return m_Instance;
}

KSMessageBox::KSMessageBox() : QMessageBox()
{
    setDefaultButton(StandardButton::Ok);
    setIcon(QMessageBox::Information);
    setObjectName("KSMessageBox");

    setWindowFlags(Qt::WindowStaysOnTopHint);

    m_ProgressTimer.setInterval(1000);
    m_ProgressTimer.setSingleShot(false);
    connect(&m_ProgressTimer, &QTimer::timeout, this, &KSMessageBox::timerTick);

    connect(this, &KSMessageBox::rejected, [this]()
    {
        m_ProgressTimer.stop();
        emit newMessage(QJsonObject());
    });

    connect(this, &KSMessageBox::accepted, [this]()
    {
        m_ProgressTimer.stop();
        emit newMessage(QJsonObject());
    });
}

void KSMessageBox::error(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    //KMessageBox::error(nullptr, message, title);
    reset();

    addOKButton();
    setIcon(QMessageBox::Critical);
    setText(message);
    setWindowTitle(title);
    open();

    emit newMessage(createMessageObject());
#endif
}

void KSMessageBox::sorry(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    //KSNotification::sorry(message, title);
    reset();

    addOKButton();
    setIcon(QMessageBox::Warning);
    setText(message);
    setWindowTitle(title);
    open();

    emit newMessage(createMessageObject());
#endif
}

void KSMessageBox::info(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    //KMessageBox::information(nullptr, message, title);
    reset();

    addOKButton();
    setIcon(QMessageBox::Information);
    setText(message);
    setWindowTitle(title);
    open();

    emit newMessage(createMessageObject());


#endif
}

void KSMessageBox::setupTimeout()
{
    if (m_Timeout == 0)
        return;

    m_ProgressLabel = new QLabel(this);
    m_ProgressLabel->setText(i18n("Auto close in ..."));

    m_ProgressIndicator = new QRoundProgressBar(this);
    m_ProgressIndicator->setDecimals(0);
    m_ProgressIndicator->setFormat("%v");
    m_ProgressIndicator->setBarStyle(QRoundProgressBar::StyleLine);
    m_ProgressIndicator->setFixedSize(QSize(32, 32));

    m_ProgressLayout = new QHBoxLayout();
    m_ProgressLayout->setObjectName("ProgressLayout");
    m_ProgressLayout->addWidget(m_ProgressLabel);
    m_ProgressLayout->addWidget(m_ProgressIndicator);

    QGridLayout *gridLayout = qobject_cast<QGridLayout*>(layout());
    if (gridLayout)
    {
        //gridLayout->addWidget(m_ProgressLabel.get(), 1, 2, 1, 1, Qt::AlignHCenter | Qt::AlignVCenter);
        //gridLayout->addWidget(m_ProgressIndicator.get(), 1, 2, 1, 1, Qt::AlignHCenter | Qt::AlignVCenter);
        gridLayout->addItem(m_ProgressLayout, 1, 2, 1, 2, Qt::AlignHCenter | Qt::AlignVCenter);
    }

    m_ProgressTimer.start();
    m_ProgressIndicator->setRange(0, m_Timeout);
    m_ProgressIndicator->setValue(static_cast<int>(m_Timeout));
}

void KSMessageBox::reset()
{
    m_ProgressTimer.stop();
    m_Timeout = 0;

    QList<QPushButton *> allButtons = findChildren<QPushButton*>();
    qDeleteAll(allButtons);

    delete (m_ProgressIndicator);
    delete (m_ProgressLabel);
    if (m_ProgressLayout)
    {
        layout()->removeItem(m_ProgressLayout);
        delete (m_ProgressLayout);
    }

}

void KSMessageBox::questionYesNo(const QString &message, const QString &title, quint32 timeout, bool defaultToYes,
                                 const QString &yesText, const QString &noText)
{
    reset();

    setIcon(QMessageBox::Question);
    setText(message);
    setWindowTitle(title);

    QPushButton *yesButton = new QPushButton(yesText, this);
    QPushButton *noButton = new QPushButton(noText, this);

    connect(yesButton, &QPushButton::clicked, this, &KSMessageBox::accepted);
    connect(noButton, &QPushButton::clicked, this, &KSMessageBox::rejected);

    addButton(yesButton, QMessageBox::ActionRole);
    addButton(noButton, QMessageBox::ActionRole);


    setDefaultButton(defaultToYes ? yesButton : noButton);
    yesButton->setDefault(defaultToYes);
    noButton->setDefault(!defaultToYes);

    m_Timeout = timeout;

    setupTimeout();

    open();

    emit newMessage(createMessageObject());
}

void KSMessageBox::warningContinueCancel(const QString &message, const QString &title, quint32 timeout,
        bool defaultToContinue,
        const QString &continueText, const QString &cancelText)
{
    reset();

    setIcon(QMessageBox::Warning);
    setText(message);
    setWindowTitle(title);

    QPushButton *continueButton = new QPushButton(continueText, this);
    QPushButton *cancelButton = new QPushButton(cancelText, this);

    connect(continueButton, &QPushButton::clicked, this, &KSMessageBox::accepted);
    connect(cancelButton, &QPushButton::clicked, this, &KSMessageBox::rejected);

    addButton(continueButton, QMessageBox::ActionRole);
    addButton(cancelButton, QMessageBox::ActionRole);

    setDefaultButton(defaultToContinue ? continueButton : cancelButton);
    continueButton->setDefault(defaultToContinue);
    cancelButton->setDefault(!defaultToContinue);

    m_Timeout = timeout;

    setupTimeout();

    open();

    emit newMessage(createMessageObject());
}

void KSMessageBox::transient(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    QPointer<QMessageBox> msgBox = new QMessageBox();
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowTitle(title);
    msgBox->setText(message);
    msgBox->setModal(false);
    msgBox->setIcon(QMessageBox::Warning);
    msgBox->show();
#endif
}

void KSMessageBox::addOKButton()
{
    QPushButton *okButton = new QPushButton(i18n("Ok"), this);
    connect(okButton, &QPushButton::clicked, this, &KSMessageBox::accepted);
    addButton(okButton, QMessageBox::AcceptRole);
    setDefaultButton(okButton);
}

void KSMessageBox::timerTick()
{
    m_Timeout--;
    m_ProgressIndicator->setValue(static_cast<int>(m_Timeout));

    if (m_Timeout == 0)
    {
        m_ProgressTimer.stop();
        if (defaultButton())
            defaultButton()->animateClick();
        else
            reject();
    }
}

QJsonObject KSMessageBox::createMessageObject()
{
    QJsonObject message;
    QJsonArray buttons;

    message.insert("title", windowTitle());
    message.insert("message", text());
    message.insert("icon", icon());
    message.insert("timeout", static_cast<int32_t>(m_Timeout));

    for (const auto oneButton : findChildren<QPushButton*>())
    {
        buttons.append(oneButton->text());
        if (oneButton == defaultButton())
            message.insert("default", oneButton->text());
    }

    message.insert("buttons", buttons);

    return message;
}

bool KSMessageBox::selectResponse(const QString &button)
{
    for (const auto oneButton : findChildren<QPushButton*>())
    {
        const QString buttonText = oneButton->text().remove("&");

        if (button == buttonText)
        {
            oneButton->animateClick();
            return true;
        }
    }

    return false;
}

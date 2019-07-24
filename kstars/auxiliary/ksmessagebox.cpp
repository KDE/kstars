/*  KStars Asynchronous Message Box Implementation for Desktop/Android and EkosLive
    Based on QMessageBox.

    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

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
#include <QHBoxLayout>
#include <QSpacerItem>

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

    m_ProgressIndicator.reset(new QRoundProgressBar(this));
    m_ProgressIndicator->setDecimals(0);
    m_ProgressIndicator->setFormat("%v");
    m_ProgressIndicator->setBarStyle(QRoundProgressBar::StyleLine);
    m_ProgressIndicator->setMinimumSize(QSize(50, 50));

    //horizontalLayout->addWidget(m_ProgressIndicator.get());
    //horizontalLayout->setAlignment(m_ProgressIndicator.get(), Qt::AlignHCenter);

    QGridLayout *gridLayout = qobject_cast<QGridLayout*>(layout());
    if (gridLayout)
    {
        qDebug() << "Rows" << gridLayout->rowCount() << " colmumns" << gridLayout->columnCount();
        gridLayout->addWidget(m_ProgressIndicator.get(), 1,
                              1,  2, 2, Qt::AlignHCenter);


    }

    m_ProgressTimer.setInterval(1000);
    m_ProgressTimer.setSingleShot(false);
    connect(&m_ProgressTimer, &QTimer::timeout, this, &KSMessageBox::timerTick);
}

void KSMessageBox::error(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    //KMessageBox::error(nullptr, message, title);
#endif
}

void KSMessageBox::sorry(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    //KSNotification::sorry(message, title);
#endif
}

void KSMessageBox::info(const QString &message, const QString &title)
{
#ifdef KSTARS_LITE
    Q_UNUSED(title);
    KStarsLite::Instance()->notificationMessage(message);
#else
    //KMessageBox::information(nullptr, message, title);
    setText(message);
    setWindowTitle(title);

    if (m_Timeout > 0)
    {
        m_ProgressTimer.start();
        m_ProgressIndicator->setRange(0, m_Timeout);
        m_ProgressIndicator->setValue(static_cast<int>(m_Timeout));
    }

    open();


#endif
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

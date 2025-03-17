/*
    SPDX-FileCopyrightText: 2023 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "manualpulse.h"

namespace Ekos
{
ManualPulse::ManualPulse(QWidget *parent) : QDialog(parent)
{
    setupUi(this);

    connect(northPulseB, &QPushButton::clicked, this, [this]()
    {
        emit newSinglePulse(DEC_INC_DIR, pulseDuration->value(), DontCaptureAfterPulses);
        northPulseB->setEnabled(false);
        QTimer::singleShot(pulseDuration->value(), this, [this]()
        {
            northPulseB->setEnabled(true);
        });
    });

    connect(southPulseB, &QPushButton::clicked, this, [this]()
    {
        emit newSinglePulse(DEC_DEC_DIR, pulseDuration->value(), DontCaptureAfterPulses);
        southPulseB->setEnabled(false);
        QTimer::singleShot(pulseDuration->value(), this, [this]()
        {
            southPulseB->setEnabled(true);
        });
    });

    connect(westPulseB, &QPushButton::clicked, this, [this]()
    {
        emit newSinglePulse(RA_DEC_DIR, pulseDuration->value(), DontCaptureAfterPulses);
        westPulseB->setEnabled(false);
        QTimer::singleShot(pulseDuration->value(), this, [this]()
        {
            westPulseB->setEnabled(true);
        });
    });

    connect(eastPulseB, &QPushButton::clicked, this, [this]()
    {
        emit newSinglePulse(RA_INC_DIR, pulseDuration->value(), DontCaptureAfterPulses);
        eastPulseB->setEnabled(false);
        QTimer::singleShot(pulseDuration->value(), this, [this]()
        {
            eastPulseB->setEnabled(true);
        });
    });

    connect(resetB, &QPushButton::clicked, this, &ManualPulse::reset);
}

void ManualPulse::reset()
{
    m_LastCoord = SkyPoint();
    raOffset->setText("0");
    raOffset->setStyleSheet(QString());
    deOffset->setText("0");
    deOffset->setStyleSheet(QString());
}

void ManualPulse::setMountCoords(const SkyPoint &position)
{
    if (m_LastCoord.ra().Degrees() < 0)
    {
        m_LastCoord = position;
        return;
    }

    auto raDiff = position.ra().deltaAngle(m_LastCoord.ra()).Degrees() * 3600;
    auto deDiff = position.dec().deltaAngle(m_LastCoord.dec()).Degrees() * 3600;

    if (raDiff > 0)
        raOffset->setStyleSheet("color:green");
    else if (raDiff < 0)
        raOffset->setStyleSheet("color:red");

    if (deDiff > 0)
        deOffset->setStyleSheet("color:green");
    else if (deDiff < 0)
        deOffset->setStyleSheet("color:red");

    raOffset->setText(QString::number(raDiff, 'f', 2));
    deOffset->setText(QString::number(deDiff, 'f', 2));
}
}

/*  Ekos Alignment Manual Rotator
    SPDX-FileCopyrightText: 2021 Rick Bassham

    SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "manualrotator.h"

#include "align.h"
#include "ksnotification.h"
#include "ksmessagebox.h"

// Options
#include "Options.h"

#include <QIcon>
#include <ekos_align_debug.h>

namespace Ekos
{

ManualRotator::ManualRotator(Align *parent) : QDialog(parent)
{
    setupUi(this);

    m_AlignInstance = parent;
    //setWindowFlags(Qt::WindowStaysOnTopHint);
    connect(takeImageB, &QPushButton::clicked, this, &Ekos::ManualRotator::captureAndSolve);
    connect(cancelB, &QPushButton::clicked, this, &QDialog::reject);

}

ManualRotator::~ManualRotator()
{

}

void ManualRotator::setRotatorDiff(double current, double target, double diff)
{
    double threshold = Options::astrometryRotatorThreshold() / 60.0;
    QString iconName;

    if (std::abs(diff) < threshold)
    {
        iconName = "checkmark";
        diffLabel->setText(i18n("Done"));
    }
    else
    {
        diffLabel->setText(i18n("%1°", QString::number(diff, 'f', 1)));
        iconName = (diff > 0.0) ? "object-rotate-left" : "object-rotate-right";
    }

    icon->setPixmap(QIcon::fromTheme(iconName).pixmap(300, 300));
    targetRotation->setText(i18n("%1°", QString::number(target, 'f', 1)));
    currentRotation->setText(i18n("%1°", QString::number(current, 'f', 1)));
}

}

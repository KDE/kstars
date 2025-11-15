/*  Widget to display the mount position.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mountpositionwidget.h"

#include "kstarsdata.h"

namespace Ekos
{
MountPositionWidget::MountPositionWidget(QWidget *parent)
    : QWidget{parent}
{
    setupUi(this);

    raValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    deValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    azValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    altValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    haValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    zaValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // Connect to button group to detect changes from both JNow and J2000 buttons
    connect(coordinateEpochGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, [this](QAbstractButton * button)
    {
        if (button == j2000CheckObject)
            emit J2000Enabled(true);
        else if (button == jnowCheckObject)
            emit J2000Enabled(false);
    });
}

void MountPositionWidget::updateTelescopeCoords(const SkyPoint &position, const dms &ha)
{

    // Mount Control Panel coords depend on the switch
    if (isJ2000Enabled())
    {
        raValueObject->setProperty("text", position.ra0().toHMSString());
        deValueObject->setProperty("text", position.dec0().toDMSString());
    }
    else
    {
        raValueObject->setProperty("text", position.ra().toHMSString());
        deValueObject->setProperty("text", position.dec().toDMSString());
    }
    azValueObject->setProperty("text", position.az().toDMSString());
    altValueObject->setProperty("text", position.alt().toDMSString());

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms haSigned(ha);
    QChar sgn('+');

    if (haSigned.Hours() > 12.0)
    {
        haSigned.setH(24.0 - haSigned.Hours());
        sgn = '-';
    }

    haValueObject->setProperty("text", QString("%1%2").arg(sgn).arg(haSigned.toHMSString()));
    zaValueObject->setProperty("text", dms(90 - position.altRefracted().Degrees()).toDMSString());

}

bool MountPositionWidget::isJ2000Enabled()
{
    return j2000CheckObject->isChecked();
}

void MountPositionWidget::setJ2000Enabled(bool enabled)
{
    if (enabled)
        j2000CheckObject->setChecked(true);
    else
        jnowCheckObject->setChecked(true);
}
} // namespace

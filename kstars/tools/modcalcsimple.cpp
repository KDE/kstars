#include "modcalcsimple.h"
#include "widgets/dmsbox.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QTextStream>
#include <QFileDialog>

modCalcSimple::modCalcSimple(QWidget *parentSplit) : QFrame(parentSplit)
{
    setupUi(this);

    angle1Input->setUnits(dmsBox::HOURS);
    angle2Input->setUnits(dmsBox::DEGREES);
    connect(angle1Input, SIGNAL(editingFinished()), this, SLOT(slotCompute1()));
    connect(angle2Input, SIGNAL(editingFinished()), this, SLOT(slotCompute2()));

    show();
}

void modCalcSimple::slotCompute1()
{
    bool ok(false);
    dms angle_new = angle1Input->createDms(&ok);
    if (ok)
    {
        double angle_new_double = angle_new.Degrees();
        angle1Result->setText(QString::number(angle_new_double, 'f', 11));
    }
}

void modCalcSimple::slotCompute2()
{
    bool ok(false);
    dms angle_new = angle2Input->createDms(&ok);
    if (ok)
    {
        double angle_new_double = angle_new.Degrees();
        angle2Result->setText(QString::number(angle_new_double, 'f', 11));
    }
}

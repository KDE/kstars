#include "pwizfovsh.h"
#include "printingwizard.h"
#include "dialogs/finddialog.h"
#include "pwizobjectselection.h"
#include "kstars/kstarsdata.h"

PWizFovShUI::PWizFovShUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    setupUi(this);

    setupWidgets();
    setupConnections();
}

void PWizFovShUI::setBeginObject(SkyObject *obj)
{
    objInfoLabel->setText(PWizObjectSelectionUI::objectInfoString(obj));
}

void PWizFovShUI::slotSelectFromList()
{
    FindDialog findDlg(this);
    if(findDlg.exec() == QDialog::Accepted)
    {
        SkyObject *obj = findDlg.selectedObject();
        if(obj)
        {
            setBeginObject(obj);
        }
    }
}

void PWizFovShUI::slotPointObject()
{
    m_ParentWizard->beginShBeginPointing();
}

void PWizFovShUI::slotBeginCapture()
{
    m_ParentWizard->beginShFovCapture();
}

void PWizFovShUI::setupWidgets()
{
    QStringList fovNames;
    foreach(FOV *fov, KStarsData::Instance()->getAvailableFOVs())
    {
        fovNames.append(fov->name());
    }
    fovCombo->addItems(fovNames);
}

void PWizFovShUI::setupConnections()
{
    connect(selectFromListButton, SIGNAL(clicked()), this, SLOT(slotSelectFromList()));
    connect(pointButton, SIGNAL(clicked()), this, SLOT(slotPointObject()));
    connect(captureButton, SIGNAL(clicked()), this, SLOT(slotBeginCapture()));
}



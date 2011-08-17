#include "pwizfovbrowse.h"
#include "foveditordialog.h"

PWizFovBrowseUI::PWizFovBrowseUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    setupUi(this);

    connect(browseButton, SIGNAL(clicked()), this, SLOT(slotOpenFovEditor()));
}

void PWizFovBrowseUI::slotOpenFovEditor()
{
    FovEditorDialog dialog(m_ParentWizard, this);
    dialog.exec();
}

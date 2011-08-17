#include "foveditordialog.h"
#include "printingwizard.h"

FovEditorDialogUI::FovEditorDialogUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

FovEditorDialog::FovEditorDialog(PrintingWizard *wizard, QWidget *parent) : KDialog(parent),
    m_ParentWizard(wizard), m_CurrentIndex(0)
{
    m_EditorUi = new FovEditorDialogUI(this);
    setMainWidget(m_EditorUi);
    setButtons(KDialog::Close);

    setupWidgets();
    setupConnections();
}

FovEditorDialog::~FovEditorDialog()
{}

void FovEditorDialog::slotNextFov()
{
    slotSaveDescription();

    if(m_CurrentIndex < m_ParentWizard->getFovSnapshotList()->size() - 1)
    {
        m_CurrentIndex++;

        updateFovImage();
        updateButtons();
        updateDescriptions();
    }
}

void FovEditorDialog::slotPreviousFov()
{
    slotSaveDescription();

    if(m_CurrentIndex > 0)
    {
        m_CurrentIndex--;

        updateFovImage();
        updateButtons();
        updateDescriptions();
    }
}

void FovEditorDialog::slotCaptureAgain()
{
    hide();
    m_ParentWizard->beginFovCapture(&m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->getCentralPoint(),
                                    m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->getFov());
}

void FovEditorDialog::slotDelete()
{
    if(m_CurrentIndex > m_ParentWizard->getFovSnapshotList()->size() - 1)
    {
        return;
    }

    m_ParentWizard->getFovSnapshotList()->removeAt(m_CurrentIndex);

    updateFovImage();
    updateButtons();
    updateDescriptions();
}

void FovEditorDialog::slotSaveDescription()
{
    if(m_CurrentIndex < m_ParentWizard->getFovSnapshotList()->size())
    {
        m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->setDescription(m_EditorUi->descriptionEdit->text());
    }
}

void FovEditorDialog::setupWidgets()
{
    if(m_ParentWizard->getFovSnapshotList()->size() > 0)
    {
        m_EditorUi->imageLabel->setPixmap(m_ParentWizard->getFovSnapshotList()->first()->getPixmap());
    }

    updateButtons();
    updateDescriptions();
}

void FovEditorDialog::setupConnections()
{
    connect(m_EditorUi->previousButton, SIGNAL(clicked()), this, SLOT(slotPreviousFov()));
    connect(m_EditorUi->nextButton, SIGNAL(clicked()), this, SLOT(slotNextFov()));
    connect(m_EditorUi->recaptureButton, SIGNAL(clicked()), this, SLOT(slotCaptureAgain()));
    connect(m_EditorUi->deleteButton, SIGNAL(clicked()), this, SLOT(slotDelete()));
    connect(m_EditorUi->descriptionEdit, SIGNAL(editingFinished()), this, SLOT(slotSaveDescription()));
}

void FovEditorDialog::updateButtons()
{
    m_EditorUi->previousButton->setEnabled(m_CurrentIndex > 0);
    m_EditorUi->nextButton->setEnabled(m_CurrentIndex < m_ParentWizard->getFovSnapshotList()->size() - 1);
}

void FovEditorDialog::updateDescriptions()
{
    if(m_ParentWizard->getFovSnapshotList()->size() == 0)
    {
        m_EditorUi->imageLabel->setText("No captured field of view images.");
        m_EditorUi->fovInfoLabel->setText(QString());
        m_EditorUi->recaptureButton->setEnabled(false);
        m_EditorUi->deleteButton->setEnabled(false);
        m_EditorUi->descriptionEdit->setEnabled(false);
    }

    else
    {
        FOV *fov = m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->getFov();

        QString fovDescription = i18n("FOV (") + QString::number(m_CurrentIndex + 1) + "/" +
                                 QString::number(m_ParentWizard->getFovSnapshotList()->size()) + "): " +
                                 fov->name() + " (" + QString::number(fov->sizeX()) + i18n("'") + " x " +
                                 QString::number(fov->sizeY()) + i18n("'") + ")";
        m_EditorUi->fovInfoLabel->setText(fovDescription);

        m_EditorUi->descriptionEdit->setText(m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->getDescription());
    }
}

void FovEditorDialog::updateFovImage()
{
    if(m_CurrentIndex < m_ParentWizard->getFovSnapshotList()->size())
    {
        m_EditorUi->imageLabel->setPixmap(m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->getPixmap());
    }
}

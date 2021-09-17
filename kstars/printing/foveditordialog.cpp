/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "foveditordialog.h"

#include "kstars.h"
#include "ksnotification.h"
#include "printingwizard.h"

#include <KJob>
#include <KMessageBox>
#include <KIO/StoredTransferJob>

#include <QDebug>
#include <QFileDialog>
#include <QTemporaryFile>

FovEditorDialogUI::FovEditorDialogUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setWindowTitle(i18nc("@title:window", "Field of View Snapshot Browser"));
}

FovEditorDialog::FovEditorDialog(PrintingWizard *wizard, QWidget *parent)
    : QDialog(parent), m_ParentWizard(wizard), m_CurrentIndex(0)
{
    m_EditorUi = new FovEditorDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_EditorUi);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    setupWidgets();
    setupConnections();
}

void FovEditorDialog::slotNextFov()
{
    slotSaveDescription();

    if (m_CurrentIndex < m_ParentWizard->getFovSnapshotList()->size() - 1)
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

    if (m_CurrentIndex > 0)
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
    m_ParentWizard->recaptureFov(m_CurrentIndex);
}

void FovEditorDialog::slotDelete()
{
    if (m_CurrentIndex > m_ParentWizard->getFovSnapshotList()->size() - 1)
    {
        return;
    }

    delete m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex);
    m_ParentWizard->getFovSnapshotList()->removeAt(m_CurrentIndex);

    if (m_CurrentIndex == m_ParentWizard->getFovSnapshotList()->size())
    {
        m_CurrentIndex--;
    }

    updateFovImage();
    updateButtons();
    updateDescriptions();
}

void FovEditorDialog::slotSaveDescription()
{
    if (m_CurrentIndex < m_ParentWizard->getFovSnapshotList()->size())
    {
        m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->setDescription(m_EditorUi->descriptionEdit->text());
    }
}

void FovEditorDialog::slotSaveImage()
{
    if (m_CurrentIndex >= m_ParentWizard->getFovSnapshotList()->size())
    {
        return;
    }

    //If the filename string contains no "/" separators, assume the
    //user wanted to place a file in their home directory.
    QString url = QFileDialog::getSaveFileUrl(KStars::Instance(), i18nc("@title:window", "Save Image"), QUrl(QDir::homePath()),
                  "image/png image/jpeg image/gif image/x-portable-pixmap image/bmp")
                  .url();
    QUrl fileUrl;
    if (!url.contains(QDir::separator()))
    {
        fileUrl = QUrl::fromLocalFile(QDir::homePath() + '/' + url);
    }

    else
    {
        fileUrl = QUrl::fromLocalFile(url);
    }

    QTemporaryFile tmpfile;
    tmpfile.open();
    QString fname;

    if (fileUrl.isValid())
    {
        if (fileUrl.isLocalFile())
        {
            fname = fileUrl.toLocalFile();
        }

        else
        {
            fname = tmpfile.fileName();
        }

        //Determine desired image format from filename extension
        QString ext = fname.mid(fname.lastIndexOf(".") + 1);
        // export as raster graphics
        const char *format = "PNG";

        if (ext.toLower() == "png")
        {
            format = "PNG";
        }
        else if (ext.toLower() == "jpg" || ext.toLower() == "jpeg")
        {
            format = "JPG";
        }
        else if (ext.toLower() == "gif")
        {
            format = "GIF";
        }
        else if (ext.toLower() == "pnm")
        {
            format = "PNM";
        }
        else if (ext.toLower() == "bmp")
        {
            format = "BMP";
        }
        else
        {
            qWarning() << i18n("Could not parse image format of %1; assuming PNG.", fname);
        }

        if (!m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->getPixmap().save(fname, format))
        {
            qDebug() << "Error: Unable to save image: " << fname;
        }

        else
        {
            qDebug() << "Image saved to file: " << fname;
        }
    }

    if (tmpfile.fileName() == fname)
    {
        //attempt to upload image to remote location
        KIO::TransferJob *uploadJob = KIO::storedHttpPost(&tmpfile, fileUrl);
        //if(!KIO::NetAccess::upload(tmpfile.fileName(), fileUrl, this))
        if (uploadJob->exec() == false)
        {
            QString message = i18n("Could not upload image to remote location: %1", fileUrl.url());
            KSNotification::sorry(message, i18n("Could not upload file"));
        }
        uploadJob->kill();
    }
}

void FovEditorDialog::setupWidgets()
{
    if (m_ParentWizard->getFovSnapshotList()->size() > 0)
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
    connect(m_EditorUi->saveButton, SIGNAL(clicked()), this, SLOT(slotSaveImage()));
}

void FovEditorDialog::updateButtons()
{
    m_EditorUi->previousButton->setEnabled(m_CurrentIndex > 0);
    m_EditorUi->nextButton->setEnabled(m_CurrentIndex < m_ParentWizard->getFovSnapshotList()->size() - 1);
}

void FovEditorDialog::updateDescriptions()
{
    if (m_ParentWizard->getFovSnapshotList()->size() == 0)
    {
        m_EditorUi->imageLabel->setText("No captured field of view images.");
        m_EditorUi->fovInfoLabel->setText(QString());
        m_EditorUi->recaptureButton->setEnabled(false);
        m_EditorUi->deleteButton->setEnabled(false);
        m_EditorUi->descriptionEdit->setEnabled(false);
        m_EditorUi->saveButton->setEnabled(false);
    }

    else
    {
        FOV *fov = m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->getFov();

        QString fovDescription = i18n("FOV (%1/%2): %3 (%4' x %5')", QString::number(m_CurrentIndex + 1),
                                      QString::number(m_ParentWizard->getFovSnapshotList()->size()), fov->name(),
                                      QString::number(fov->sizeX()), QString::number(fov->sizeY()));

        m_EditorUi->fovInfoLabel->setText(fovDescription);

        m_EditorUi->descriptionEdit->setText(
            m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->getDescription());
    }
}

void FovEditorDialog::updateFovImage()
{
    if (m_CurrentIndex < m_ParentWizard->getFovSnapshotList()->size())
    {
        m_EditorUi->imageLabel->setPixmap(m_ParentWizard->getFovSnapshotList()->at(m_CurrentIndex)->getPixmap());
    }
}

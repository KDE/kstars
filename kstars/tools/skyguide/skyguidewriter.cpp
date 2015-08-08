/***************************************************************************
                          skyguidewriter.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2015/07/28
    copyright            : (C) 2015 by Marcos Cardinot
    email                : mcardinot@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QFileDialog>

#include <KMessageBox>

#include "kstars.h"
#include "skyguidemgr.h"
#include "skyguidewriter.h"

SkyGuideWriterUI::SkyGuideWriterUI(QWidget *parent) : QFrame(parent) {
    setupUi(this);
}

SkyGuideWriter::SkyGuideWriter(SkyGuideMgr *mgr, QWidget *parent)
        : QDialog(parent)
        , m_ui(new SkyGuideWriterUI)
        , m_skyGuideMgr(mgr)
        , m_skyGuideObject(NULL)
        , m_unsavedChanges(false)
{
    // setup main frame
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addWidget(m_ui);
    setLayout(mainLayout);
    setWindowTitle(xi18n("SkyGuide Writer"));
    setModal( false );

    // add icons to push buttons
    m_ui->bNew->setIcon(QIcon::fromTheme("document-new"));
    m_ui->bOpen->setIcon(QIcon::fromTheme("document-open"));
    m_ui->bSave->setIcon(QIcon::fromTheme("document-save"));
    m_ui->bSaveAs->setIcon(QIcon::fromTheme("document-save-as"));
    m_ui->bInstall->setIcon(QIcon::fromTheme("system-software-install"));
    m_ui->bAddAuthor->setIcon(QIcon::fromTheme("list-add"));
    m_ui->bEditAuthor->setIcon(QIcon::fromTheme("insert-text"));
    m_ui->bRemoveAuthor->setIcon(QIcon::fromTheme("list-remove"));
    m_ui->bAddSlide->setIcon(QIcon::fromTheme("list-add"));
    m_ui->bEditSlide->setIcon(QIcon::fromTheme("insert-text"));
    m_ui->bRemoveSlide->setIcon(QIcon::fromTheme("list-remove"));

    // connect signals&slots of toolbar
    connect(m_ui->bNew, SIGNAL(clicked()), this, SLOT(slotNew()));
    connect(m_ui->bOpen, SIGNAL(clicked()), this, SLOT(slotOpen()));
    connect(m_ui->bSave, SIGNAL(clicked()), this, SLOT(slotSave()));
    connect(m_ui->bSaveAs, SIGNAL(clicked()), this, SLOT(slotSaveAs()));
    connect(m_ui->bInstall, SIGNAL(clicked()), this, SLOT(slotInstall()));

    // some field changed? calls slotFieldsChanged()
    connect(m_ui->fTitle, SIGNAL(textChanged(QString)), this, SLOT(slotFieldsChanged()));
    connect(m_ui->fDescription, SIGNAL(textChanged()), this, SLOT(slotFieldsChanged()));
    connect(m_ui->fCreationDate, SIGNAL(dateChanged(QDate)), this, SLOT(slotFieldsChanged()));
    connect(m_ui->fVersion, SIGNAL(valueChanged(int)), this, SLOT(slotFieldsChanged()));

    // create new SkyGuide
    slotNew();
}

SkyGuideWriter::~SkyGuideWriter() {
    delete m_ui;
    delete m_skyGuideObject;
    m_skyGuideObject = NULL;
}

void SkyGuideWriter::slotNew() {
    saveWarning();
    if (m_unsavedChanges) {
        return;
    }

    this->blockSignals(true);
    m_ui->fTitle->clear();
    m_ui->fDescription->clear();
    m_ui->fCreationDate->setDate(QDate::currentDate());
    m_ui->fVersion->clear();
    m_ui->listOfAuthors->clear();
    m_ui->listOfSlides->clear();
    this->blockSignals(false);

    m_ui->bSave->setEnabled(false);
    m_ui->bInstall->setEnabled(false);

    QVariantMap map;
    map.insert("formatVersion", SKYGUIDE_FORMAT_VERSION);
    map.insert("header", QVariant());
    map.insert("slides", QVariant());
    m_skyGuideObject = new SkyGuideObject("", map);
}

void SkyGuideWriter::slotOpen() {
    saveWarning();
    if (m_unsavedChanges) {
        return;
    }

    QString initialDir = m_skyGuideObject
                       ? m_skyGuideObject->path()
                       : m_skyGuideMgr->getGuidesDir().absolutePath();

    QString jsonPath = QFileDialog::getOpenFileName(KStars::Instance(),
                                                    "Open SkyGuide",
                                                    initialDir,
                                                    "JSON (*.json)");

    SkyGuideObject* s = m_skyGuideMgr->buildSkyGuideObject(jsonPath);
    if (!s) {
        return;
    }

    m_ui->fTitle->setText(s->title());
    m_ui->fDescription->setText(s->description());
    m_ui->fCreationDate->setDate(s->creationDate());
    m_ui->fVersion->setValue(s->version());

    QList<SkyGuideObject::Author> authors = s->authors();
    foreach (SkyGuideObject::Author a, authors) {
        m_ui->listOfAuthors->addItem(a.name);
    }

    QStringList contents = s->contents();
    foreach (QString t, contents) {
        m_ui->listOfSlides->addItem(t);
    }

    m_skyGuideObject = s;
}

void SkyGuideWriter::slotSave() {
    if (!m_skyGuideObject) {
        return;
    }
    QString savePath;
    if (m_skyGuideObject->path().isEmpty()) {
        savePath = QFileDialog::getSaveFileName(KStars::Instance(),
                                                "Save SkyGuide",
                                                m_skyGuideObject->title() + ".zip",
                                                "Zip (*.zip)");
    } else {
        savePath = m_skyGuideObject->path() + "/" + m_skyGuideObject->title() + ".zip";
    }
    QFile::remove(savePath);
    QFile tmpFile(m_skyGuideObject->toZip());
    if (savePath.isEmpty() || !tmpFile.copy(savePath)) {
        qWarning() << "SkyGuideMgr: the SkyGuide could not be stored on: "
                   << QDir::toNativeSeparators(savePath);
    } else {
        m_skyGuideObject->setPath(QFileInfo(savePath).absolutePath());
        setUnsavedChanges(false);
    }
    tmpFile.remove();
}

void SkyGuideWriter::slotSaveAs() {
    if (!m_skyGuideObject) {
        return;
    }

    QString oldPath = m_skyGuideObject->path();
    m_skyGuideObject->setPath("");
    slotSave();

    if (m_skyGuideObject->path().isEmpty()) {
        m_skyGuideObject->setPath(oldPath);
    }
}

void SkyGuideWriter::slotInstall() {
    // TODO
}

void SkyGuideWriter::slotFieldsChanged() {
    if (!m_skyGuideObject) {
        return;
    }
    m_skyGuideObject->setTitle(m_ui->fTitle->text());
    m_skyGuideObject->setDescription(m_ui->fDescription->toPlainText());
    m_skyGuideObject->setCreationDate(m_ui->fCreationDate->date());
    m_skyGuideObject->setVersion(m_ui->fVersion->value());
    setUnsavedChanges(true);
}

void SkyGuideWriter::saveWarning() {
    if (!m_unsavedChanges) {
        return;
    }
    QString caption = xi18n("Save Changes to SkyGuide?");
    QString message = xi18n("The current SkyGuide has unsaved changes. "
                            "Would you like to save before closing it?");
    int ans = KMessageBox::warningYesNoCancel(0, message, caption,
                                              KStandardGuiItem::save(),
                                              KStandardGuiItem::discard());
    if (ans == KMessageBox::Yes) {
        slotSave();
        setUnsavedChanges(false);
    } else if (ans == KMessageBox::No) {
        setUnsavedChanges(false);
    }
}

void SkyGuideWriter::setUnsavedChanges(bool b) {
    m_unsavedChanges = b;
    m_ui->bSave->setEnabled(b);
}

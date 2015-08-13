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
#include "ui_skyguideauthor.h"
#include "ui_skyguideslide.h"

SkyGuideWriterUI::SkyGuideWriterUI(QWidget *parent) : QFrame(parent) {
    setupUi(this);
}

SkyGuideWriter::SkyGuideWriter(SkyGuideMgr *mgr, QWidget *parent)
        : QDialog(parent)
        , m_ui(new SkyGuideWriterUI)
        , m_skyGuideMgr(mgr)
        , m_skyGuideObject(NULL)
        , m_unsavedChanges(false)
        , m_isEditingAuthorIdx(-1)
        , m_isEditingSlideIdx(-1)
{
    // setup main frame
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addWidget(m_ui);
    setLayout(mainLayout);
    setWindowTitle(xi18n("SkyGuide Writer"));
    setModal(false);

    // setup author dialog
    m_authorDlg = new QDialog(this);
    m_authorDlg->setWindowTitle(xi18n("SkyGuide Author"));
    m_authorDlg->setModal(true);
    m_uiAuthor = new Ui_SkyGuideAuthor;
    m_uiAuthor->setupUi(m_authorDlg);

    // setup slides dialog
    m_slideDlg = new QDialog(this);
    m_slideDlg->setWindowTitle(xi18n("SkyGuide Slide"));
    m_slideDlg->setModal(true);
    m_uiSlide = new Ui_SkyGuideSlide;
    m_uiSlide->setupUi(m_slideDlg);
    m_uiSlide->fCenterPoint->setEnabled(false);
    m_uiSlide->fSkyDateTime->setEnabled(false);
    m_uiSlide->fZoom->setEnabled(false);

    // add icons to push buttons
    m_ui->bNew->setIcon(QIcon::fromTheme("document-new"));
    m_ui->bOpen->setIcon(QIcon::fromTheme("document-open"));
    m_ui->bSave->setIcon(QIcon::fromTheme("document-save"));
    m_ui->bSaveAs->setIcon(QIcon::fromTheme("document-save-as"));
    m_ui->bInstall->setIcon(QIcon::fromTheme("system-software-install"));
    m_ui->bAddAuthor->setIcon(QIcon::fromTheme("list-add"));
    m_ui->bRemoveAuthor->setIcon(QIcon::fromTheme("list-remove"));
    m_ui->bAddSlide->setIcon(QIcon::fromTheme("list-add"));
    m_ui->bRemoveSlide->setIcon(QIcon::fromTheme("list-remove"));

    // connect signals&slots of toolbar
    connect(m_ui->bNew, SIGNAL(clicked()), this, SLOT(slotNew()));
    connect(m_ui->bOpen, SIGNAL(clicked()), this, SLOT(slotOpen()));
    connect(m_ui->bSave, SIGNAL(clicked()), this, SLOT(slotSave()));
    connect(m_ui->bSaveAs, SIGNAL(clicked()), this, SLOT(slotSaveAs()));
    connect(m_ui->bInstall, SIGNAL(clicked()), this, SLOT(slotInstall()));

    // connect signals&slots of the list widgets (authors and slides)
    connect(m_ui->listOfAuthors, SIGNAL(itemSelectionChanged()), this, SLOT(slotUpdateButtons()));
    connect(m_ui->listOfSlides, SIGNAL(itemSelectionChanged()), this, SLOT(slotUpdateButtons()));
    connect(m_ui->listOfAuthors, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(slotEditAuthor(QModelIndex)));
    connect(m_ui->listOfSlides, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(slotEditSlide(QModelIndex)));
    connect(m_ui->bAddAuthor, SIGNAL(clicked()), this, SLOT(slotShowAuthorDlg()));
    connect(m_ui->bAddSlide, SIGNAL(clicked()), this, SLOT(slotShowSlideDlg()));
    connect(m_ui->bRemoveAuthor, SIGNAL(clicked()), this, SLOT(slotRemoveAuthor()));
    connect(m_ui->bRemoveSlide, SIGNAL(clicked()), this, SLOT(slotRemoveSlide()));

    // connect signals&slots of the authors dialog
    connect(m_uiAuthor->bOk, SIGNAL(clicked()), this, SLOT(slotAddAuthor()));
    connect(m_uiAuthor->bCancel, SIGNAL(clicked()), m_authorDlg, SLOT(close()));

    // connect signals&slots of the slides dialog
    connect(m_uiSlide->bOk, SIGNAL(clicked()), this, SLOT(slotAddSlide()));
    connect(m_uiSlide->bCancel, SIGNAL(clicked()), m_slideDlg, SLOT(close()));
    connect(m_uiSlide->cCenterPoint, SIGNAL(clicked(bool)), m_uiSlide->fCenterPoint, SLOT(setEnabled(bool)));
    connect(m_uiSlide->cSkyDate, SIGNAL(clicked(bool)), m_uiSlide->fSkyDateTime, SLOT(setEnabled(bool)));
    connect(m_uiSlide->cZoom, SIGNAL(clicked(bool)), m_uiSlide->fZoom, SLOT(setEnabled(bool)));
    connect(m_uiSlide->bOpenImage, SIGNAL(clicked()), this, SLOT(slotGetImagePath()));

    // some field changed? calls slotFieldsChanged()
    connect(m_ui->fTitle, SIGNAL(textChanged(QString)), this, SLOT(slotFieldsChanged()));
    connect(m_ui->fDescription, SIGNAL(textChanged()), this, SLOT(slotFieldsChanged()));
    connect(m_ui->fLanguage, SIGNAL(textChanged(QString)), this, SLOT(slotFieldsChanged()));
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

    blockSignals(true);
    m_ui->fTitle->clear();
    m_ui->fDescription->clear();
    m_ui->fLanguage->clear();
    m_ui->fCreationDate->setDate(QDate::currentDate());
    m_ui->fVersion->clear();
    m_ui->listOfAuthors->clear();
    m_ui->listOfSlides->clear();
    blockSignals(false);

    m_ui->bSave->setEnabled(false);
    m_ui->bRemoveAuthor->setEnabled(false);
    m_ui->bRemoveSlide->setEnabled(false);

    QVariantMap map;
    map.insert("formatVersion", SKYGUIDE_FORMAT_VERSION);
    map.insert("header", QVariant());
    map.insert("slides", QVariant());
    m_skyGuideObject = new SkyGuideObject("", map);

    m_currentDir.clear();
}

void SkyGuideWriter::slotOpen() {
    saveWarning();
    if (m_unsavedChanges) {
        return;
    }

    QString initialDir = m_currentDir.isEmpty()
                       ? m_skyGuideMgr->getGuidesDir().absolutePath()
                       : m_currentDir;

    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "Open SkyGuide",
                                                    initialDir,
                                                    "SkyGuide (*.zip *.json)");

    SkyGuideObject* s = NULL;
    if (filePath.endsWith(".zip", Qt::CaseInsensitive)) {
        s = m_skyGuideMgr->buildSGOFromZip(filePath);
    } else if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
        s = m_skyGuideMgr->buildSGOFromJson(filePath);
    }

    if (!s) {
        return;
    }
    m_currentDir = QFileInfo(filePath).absolutePath();
    m_skyGuideObject = s;
    populateFields();
}

void SkyGuideWriter::slotSave() {
    if (!m_skyGuideObject || !checkRequiredFieldsWarning()) {
        return;
    }

    QString savePath;
    if (m_currentDir.isEmpty()) {
        savePath = QFileDialog::getSaveFileName(KStars::Instance(),
                                                "Save SkyGuide",
                                                m_skyGuideObject->title() + ".zip",
                                                "Zip (*.zip)");
        m_currentDir = QFileInfo(savePath).absolutePath();
    } else {
        savePath = m_currentDir + "/" + m_skyGuideObject->title() + ".zip";
    }
    QFile::remove(savePath);
    QFile tmpFile(m_skyGuideObject->toZip());
    if (m_currentDir.isEmpty() || !tmpFile.copy(savePath)) {
        QString message = "Unable to store the current SkyGuide on: \n" + savePath;
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideWriter: " << message;
    } else {
        m_skyGuideObject->setPath(m_currentDir);
        setUnsavedChanges(false);
    }
    tmpFile.remove();
}

void SkyGuideWriter::slotSaveAs() {
    if (!m_skyGuideObject) {
        return;
    }

    QString oldPath = m_currentDir;
    m_currentDir.clear();
    slotSave();

    if (m_currentDir.isEmpty()) {
        m_currentDir = oldPath;
    }
}

void SkyGuideWriter::slotInstall() {
    if (!checkRequiredFieldsWarning()) {
        return;
    }
    slotSave();
    QString zipPath = m_skyGuideObject->path() + "/" + m_skyGuideObject->title() + ".zip";
    if (QFileInfo(zipPath).exists()) {
        m_skyGuideMgr->installSkyGuide(zipPath);
    }
}

void SkyGuideWriter::slotFieldsChanged() {
    if (!m_skyGuideObject) {
        return;
    }
    m_skyGuideObject->setTitle(m_ui->fTitle->text());
    m_skyGuideObject->setDescription(m_ui->fDescription->toPlainText());
    m_skyGuideObject->setLanguage(m_ui->fLanguage->text());
    m_skyGuideObject->setCreationDate(m_ui->fCreationDate->date());
    m_skyGuideObject->setVersion(m_ui->fVersion->value());
    setUnsavedChanges(true);
}

void SkyGuideWriter::slotShowAuthorDlg() {
    m_isEditingAuthorIdx = -1;
    m_uiAuthor->fName->clear();
    m_uiAuthor->fEmail->clear();
    m_uiAuthor->fUrl->clear();
    m_authorDlg->show();
}

void SkyGuideWriter::slotShowSlideDlg() {
    m_isEditingSlideIdx = -1;
    m_uiSlide->fTitle->clear();
    m_uiSlide->fText->clear();
    m_uiSlide->fImage->clear();

    m_uiSlide->fSkyDateTime->clear();
    m_uiSlide->fSkyDateTime->setEnabled(false);
    m_uiSlide->cSkyDate->setChecked(false);

    m_uiSlide->fCenterPoint->clear();
    m_uiSlide->fCenterPoint->setEnabled(false);
    m_uiSlide->cCenterPoint->setChecked(false);

    m_uiSlide->fZoom->clear();
    m_uiSlide->fZoom->setEnabled(false);
    m_uiSlide->cZoom->setChecked(false);

    m_slideDlg->show();
}

void SkyGuideWriter::slotAddAuthor() {
    if (m_uiAuthor->fName->text().isEmpty()) {
        QString caption = xi18n("Required fields");
        QString message = xi18n("The field 'name' must be filled!");
        KMessageBox::sorry(0, message, caption);
        return;
    }

    m_authorDlg->hide();

    SkyGuideObject::Author author;
    author.name = m_uiAuthor->fName->text();
    author.email = m_uiAuthor->fEmail->text();
    author.url = m_uiAuthor->fUrl->text();

    QList<SkyGuideObject::Author> authors = m_skyGuideObject->authors();
    if (m_isEditingAuthorIdx >= 0 && m_isEditingAuthorIdx < authors.size()) {
        authors.replace(m_isEditingAuthorIdx, author);
        m_isEditingAuthorIdx = -1;
    } else {
        authors.append(author);
    }
    m_skyGuideObject->setAuthors(authors);
    populateFields();
}

void SkyGuideWriter::slotAddSlide() {
    if (m_uiSlide->fTitle->text().isEmpty() || m_uiSlide->fText->toPlainText().isEmpty()) {
        QString caption = xi18n("Required fields");
        QString message = xi18n("A slide must have title and text at least.");
        KMessageBox::sorry(0, message, caption);
        return;
    }

    m_slideDlg->hide();

    SkyGuideObject::Slide slide;
    slide.title = m_uiSlide->fTitle->text();
    slide.text = m_uiSlide->fText->toPlainText();
    slide.image = m_uiSlide->fImage->text();
    slide.zoomFactor = 0;

    if (m_uiSlide->cCenterPoint->isChecked()) {
        slide.centerPoint = m_uiSlide->fCenterPoint->text();
    }
    if (m_uiSlide->cSkyDate->isChecked()) {
        slide.skyDateTime = m_uiSlide->fSkyDateTime->dateTime();
    }
    if (m_uiSlide->cZoom->isChecked()) {
        slide.zoomFactor = m_uiSlide->fZoom->value();
    }

    QList<SkyGuideObject::Slide> slides = m_skyGuideObject->slides();
    if (m_isEditingSlideIdx >= 0 && m_isEditingSlideIdx < slides.size()) {
        slides.replace(m_isEditingSlideIdx, slide);
        m_isEditingSlideIdx = -1;
    } else {
        slides.append(slide);
    }
    m_skyGuideObject->setSlides(slides);
    populateFields();
}

void SkyGuideWriter::slotEditAuthor(QModelIndex idx) {
    m_isEditingAuthorIdx = idx.row();
    SkyGuideObject::Author author = m_skyGuideObject->authors().at(m_isEditingAuthorIdx);
    m_uiAuthor->fName->setText(author.name);
    m_uiAuthor->fEmail->setText(author.email);
    m_uiAuthor->fUrl->setText(author.url);
    m_authorDlg->show();
}

void SkyGuideWriter::slotEditSlide(QModelIndex idx) {
    m_isEditingSlideIdx = idx.row();
    SkyGuideObject::Slide slide = m_skyGuideObject->slides().at(m_isEditingSlideIdx);
    m_uiSlide->fTitle->setText(slide.title);
    m_uiSlide->fText->setPlainText(slide.text);
    m_uiSlide->fImage->setText(slide.image);

    m_uiSlide->fCenterPoint->setText(slide.centerPoint);
    m_uiSlide->fCenterPoint->setEnabled(!slide.centerPoint.isEmpty());
    m_uiSlide->cCenterPoint->setChecked(!slide.centerPoint.isEmpty());

    m_uiSlide->fSkyDateTime->setDateTime(slide.skyDateTime);
    m_uiSlide->fSkyDateTime->setEnabled(!slide.skyDateTime.isNull());
    m_uiSlide->cSkyDate->setChecked(!slide.skyDateTime.isNull());

    m_uiSlide->fZoom->setValue(slide.zoomFactor);
    m_uiSlide->fZoom->setEnabled(slide.zoomFactor > 0);
    m_uiSlide->cZoom->setChecked(slide.zoomFactor > 0);

    m_slideDlg->show();
}

void SkyGuideWriter::slotGetImagePath() {
    QString imgPath = QFileDialog::getOpenFileName(m_slideDlg, "Image",
                                                   "", "Images (*.png *.jpg *.jpeg)");
    m_uiSlide->fImage->setText(imgPath);
}

void SkyGuideWriter::slotUpdateButtons() {
    m_ui->bRemoveAuthor->setEnabled(!m_ui->listOfAuthors->selectedItems().isEmpty());
    m_ui->bRemoveSlide->setEnabled(!m_ui->listOfSlides->selectedItems().isEmpty());
}

void SkyGuideWriter::slotRemoveAuthor() {
    QList<SkyGuideObject::Author> authors = m_skyGuideObject->authors();
    for (int i = 0; i < m_ui->listOfAuthors->count(); ++i) {
        QListWidgetItem* item = m_ui->listOfAuthors->item(i);
        if (item->isSelected()) {
            delete m_ui->listOfAuthors->takeItem(i);
            authors.removeAt(i);
            --i;
        }
    }
    m_skyGuideObject->setAuthors(authors);
    populateFields();
}

void SkyGuideWriter::slotRemoveSlide() {
    QList<SkyGuideObject::Slide> slides = m_skyGuideObject->slides();
    for (int i = 0; i < m_ui->listOfSlides->count(); ++i) {
        QListWidgetItem* item = m_ui->listOfSlides->item(i);
        if (item->isSelected()) {
            delete m_ui->listOfSlides->takeItem(i);
            slides.removeAt(i);
            --i;
        }
    }
    m_skyGuideObject->setSlides(slides);
    populateFields();
}

void SkyGuideWriter::populateFields() {
    blockSignals(true);
    m_ui->fTitle->setText(m_skyGuideObject->title());
    m_ui->fDescription->setText(m_skyGuideObject->description());
    m_ui->fLanguage->setText(m_skyGuideObject->language());
    m_ui->fCreationDate->setDate(m_skyGuideObject->creationDate());
    m_ui->fVersion->setValue(m_skyGuideObject->version());

    m_ui->listOfAuthors->clear();
    QList<SkyGuideObject::Author> authors = m_skyGuideObject->authors();
    foreach (SkyGuideObject::Author a, authors) {
        m_ui->listOfAuthors->addItem(a.name);
    }

    m_ui->listOfSlides->clear();
    QStringList contents = m_skyGuideObject->contents();
    foreach (QString t, contents) {
        m_ui->listOfSlides->addItem(t);
    }
    blockSignals(false);
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

bool SkyGuideWriter::checkRequiredFieldsWarning() {
    if (!m_ui->fTitle->text().isEmpty()
            && !m_ui->fDescription->toPlainText().isEmpty()
            && m_ui->listOfSlides->count()) {
        return true; // it's ok
    }
    QString caption = xi18n("Required fields");
    QString message = xi18n("A SkyGuide must have a title, description and one slide at least.");
    KMessageBox::sorry(0, message, caption);
    return false;
}

void SkyGuideWriter::setUnsavedChanges(bool b) {
    m_unsavedChanges = b;
    m_ui->bSave->setEnabled(b);
}

void SkyGuideWriter::blockSignals(bool b) {
    m_ui->fTitle->blockSignals(b);
    m_ui->fDescription->blockSignals(b);
    m_ui->fLanguage->blockSignals(b);
    m_ui->fCreationDate->blockSignals(b);
    m_ui->fVersion->blockSignals(b);
    m_ui->listOfAuthors->blockSignals(b);
    m_ui->listOfSlides->blockSignals(b);
}

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

#include "skyguidewriter.h"

SkyGuideWriterUI::SkyGuideWriterUI(QWidget *parent) : QFrame(parent) {
    setupUi(this);
}

SkyGuideWriter::SkyGuideWriter(QWidget *parent) : QDialog(parent),
                                                  m_ui(new SkyGuideWriterUI) {
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
}

SkyGuideWriter::~SkyGuideWriter() {
    delete m_ui;
}

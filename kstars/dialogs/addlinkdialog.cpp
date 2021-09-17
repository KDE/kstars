/*
    SPDX-FileCopyrightText: 2001 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addlinkdialog.h"
#include "Options.h"

#include <QPushButton>
#include <QUrl>
#include <QDesktopServices>

#include <KMessageBox>

#include "skyobjects/skyobject.h"

AddLinkDialogUI::AddLinkDialogUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

AddLinkDialog::AddLinkDialog(QWidget *parent, const QString &oname) : QDialog(parent), ObjectName(oname)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    ald = new AddLinkDialogUI(this);

    setWindowTitle(i18nc("@title:window", "Add Custom URL to %1", oname));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ald);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    //connect signals to slots
    connect(ald->URLButton, SIGNAL(clicked()), this, SLOT(checkURL()));
    connect(ald->ImageRadio, SIGNAL(toggled(bool)), this, SLOT(changeDefaultDescription(bool)));

    ald->ImageRadio->setChecked(true);
    ald->DescBox->setText(i18n("Show image of ") + ObjectName);
}

void AddLinkDialog::checkURL(void)
{
    QUrl _url(url());
    if (_url.isValid()) //Is the string a valid URL?
    {
        QDesktopServices::openUrl(_url); //If so, launch the browser to see if it's the correct document
    }
    else //If not, print a warning message box that offers to open the browser to a search engine.
    {
        QString message =
            i18n("The URL is not valid. Would you like to open a browser window\nto the Google search engine?");
        QString caption = i18n("Invalid URL");
        if (KMessageBox::warningYesNo(nullptr, message, caption, KGuiItem(i18n("Browse Google")),
                                      KGuiItem(i18n("Do Not Browse"))) == KMessageBox::Yes)
        {
            QDesktopServices::openUrl(QUrl("https://www.google.com"));
        }
    }
}

void AddLinkDialog::changeDefaultDescription(bool imageEnabled)
{
    if (imageEnabled)
        ald->DescBox->setText(i18n("Show image of ") + ObjectName);
    else
        ald->DescBox->setText(i18n("Show webpage about ") + ObjectName);
}

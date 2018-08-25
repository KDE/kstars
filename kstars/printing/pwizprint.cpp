/***************************************************************************
                          pwizprint.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 3 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "pwizprint.h"

#include "finderchart.h"
#include "kstars.h"
#include "printingwizard.h"

#include <KMessageBox>
#include <KIO/StoredTransferJob>

#include <QFileDialog>
#include <QTemporaryFile>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPrintDialog>
#include <QPointer>

PWizPrintUI::PWizPrintUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent), m_ParentWizard(wizard)
{
    setupUi(this);

    connect(previewButton, SIGNAL(clicked()), this, SLOT(slotPreview()));
    connect(printButton, SIGNAL(clicked()), this, SLOT(slotPrint()));
    connect(exportButton, SIGNAL(clicked()), this, SLOT(slotExport()));
}

void PWizPrintUI::slotPreview()
{
    QPointer<QPrintPreviewDialog> previewDlg(new QPrintPreviewDialog(m_ParentWizard->getPrinter(), KStars::Instance()));
    connect(previewDlg, SIGNAL(paintRequested(QPrinter*)), SLOT(slotPrintPreview(QPrinter*)));
    previewDlg->exec();
    delete previewDlg;
}

void PWizPrintUI::slotPrintPreview(QPrinter *printer)
{
    printDocument(printer);
}

void PWizPrintUI::slotPrint()
{
    QPointer<QPrintDialog> dialog(new QPrintDialog(m_ParentWizard->getPrinter(), KStars::Instance()));
    if (dialog->exec() == QDialog::Accepted)
    {
        printDocument(m_ParentWizard->getPrinter());
    }
    delete dialog;
}

void PWizPrintUI::printDocument(QPrinter *printer)
{
    m_ParentWizard->getFinderChart()->print(printer);
}

void PWizPrintUI::slotExport()
{
    QUrl url =
        QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Export"), QUrl(QDir::homePath()),
                                    "application/pdf application/postscript application/vnd.oasis.opendocument.text");
    //User cancelled file selection dialog - abort image export
    if (url.isEmpty())
    {
        return;
    }

    //Warn user if file exists!
    if (QFile::exists(url.toLocalFile()))
    {
        int r = KMessageBox::warningContinueCancel(
            parentWidget(), i18n("A file named \"%1\" already exists. Overwrite it?", url.fileName()),
            i18n("Overwrite File?"), KStandardGuiItem::overwrite());
        if (r == KMessageBox::Cancel)
            return;
    }

    QString urlStr = url.url();
    if (!urlStr.contains(QDir::separator()))
    {
        urlStr = QDir::homePath() + '/' + urlStr;
    }

    QTemporaryFile tmpfile;
    tmpfile.open();
    QString fname;

    if (url.isValid())
    {
        if (url.isLocalFile())
        {
            fname = url.toLocalFile();
        }

        else
        {
            fname = tmpfile.fileName();
        }

        //Determine desired image format from filename extension
        QString ext = fname.mid(fname.lastIndexOf(".") + 1);
        if (ext == "pdf" || ext == "ps")
        {
            m_ParentWizard->getFinderChart()->writePsPdf(fname);
        }
        else if (ext == "odt")
        {
            m_ParentWizard->getFinderChart()->writeOdt(fname);
        }
        else
        {
            return;
        }

        if (tmpfile.fileName() == fname)
        {
            //attempt to upload image to remote location
            if (KIO::storedHttpPost(&tmpfile, url)->exec() == false)
            //if(!KIO::NetAccess::upload(tmpfile.fileName(), url, this))
            {
                QString message = i18n("Could not upload file to remote location: %1", url.url());
                KMessageBox::sorry(nullptr, message, i18n("Could not upload file"));
            }
        }
    }
}

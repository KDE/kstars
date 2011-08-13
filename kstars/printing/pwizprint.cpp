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

#include "printingwizard.h"
#include "finderchart.h"
#include "loggingform.h"
#include "kfiledialog.h"
#include "kmessagebox.h"
#include "kstars.h"
#include "ktemporaryfile.h"
#include "kio/netaccess.h"
#include "QPrintPreviewDialog"
#include "QPrinter"
#include "QTextDocumentWriter"
#include "QPrintDialog"

PWizPrintUI::PWizPrintUI(PrintingWizard *wizard, QWidget *parent) : QFrame(parent),
    m_ParentWizard(wizard)
{
    setupUi(this);

    connect(previewButton, SIGNAL(clicked()), this, SLOT(slotPreview()));
    connect(printButton, SIGNAL(clicked()), this, SLOT(slotPrint()));
    connect(exportButton, SIGNAL(clicked()), this, SLOT(slotExport()));
}

void PWizPrintUI::slotPreview()
{
    QPrintPreviewDialog  previewDlg(m_ParentWizard->getPrinter(), KStars::Instance());
    connect(&previewDlg, SIGNAL(paintRequested(QPrinter*)), SLOT(slotPrintPreview(QPrinter*)));
    previewDlg.exec();
}

void PWizPrintUI::slotPrintPreview(QPrinter *printer)
{
    printDocument(printer);
}

void PWizPrintUI::slotPrint()
{
    QPrintDialog dialog(m_ParentWizard->getPrinter(), KStars::Instance());
    if(dialog.exec() == QDialog::Accepted)
    {
        printDocument(m_ParentWizard->getPrinter());
    }
}

void PWizPrintUI::printDocument(QPrinter *printer)
{
    m_ParentWizard->getDocument()->print(printer);
}

void PWizPrintUI::slotExport()
{
    KUrl url = KFileDialog::getSaveUrl(QDir::homePath(), "application/pdf application/postscript application/vnd.oasis.opendocument.text");
    //User cancelled file selection dialog - abort image export
    if(url.isEmpty())
    {
        return;
    }

    //Warn user if file exists!
    if(QFile::exists(url.path()))
    {
        int r=KMessageBox::warningContinueCancel(parentWidget(),
                i18n( "A file named \"%1\" already exists. Overwrite it?" , url.fileName()),
                i18n( "Overwrite File?" ),
                KStandardGuiItem::overwrite() );
        if(r == KMessageBox::Cancel)
            return;
    }

    QString urlStr = url.url();
    if(!urlStr.contains("/"))
    {
        urlStr = QDir::homePath() + '/' + urlStr;
    }

    KTemporaryFile tmpfile;
    tmpfile.open();
    QString fname;

    if(url.isValid())
    {
        if(url.isLocalFile())
        {
            fname = url.toLocalFile();
        }

        else
        {
            fname = tmpfile.fileName();
        }

        //Determine desired image format from filename extension
        QString ext = fname.mid(fname.lastIndexOf(".") + 1);
        if(ext == "pdf" || ext == "ps") {
            m_ParentWizard->getDocument()->writePsPdf(fname);
        } else if(ext == "odt") {
            m_ParentWizard->getDocument()->writeOdt(fname);
        } else {
            return;
        }

        if(tmpfile.fileName() == fname)
        {
            //attempt to upload image to remote location
            if(!KIO::NetAccess::upload(tmpfile.fileName(), url, this))
            {
                QString message = i18n( "Could not upload file to remote location: %1", url.prettyUrl() );
                KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
            }
        }
    }
}

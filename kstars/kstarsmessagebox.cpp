/***************************************************************************
                          kstarsmessagebox.cpp  -  description
                             -------------------
    begin                : Mon Mar 11 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <qcheckbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qstringlist.h>
#include <qvbox.h>
#include <qvgroupbox.h>
#include <qstylesheet.h>
#include <qstyle.h>
#include <qsimplerichtext.h>

#include <kconfig.h>
#include <kdebug.h>
#include <kdialogbase.h>
#include <klistbox.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <qlayout.h>
//KDE3-only
//#include <kguiitem.h>
//#include <kstdguiitem.h>
//#include <kactivelabel.h>

#if (KDE_VERSION <= 222)
#include <kapp.h>
#else
#include <kapplication.h>
#endif

#include "kstarsmessagebox.h"

static bool KMessageBox_queue = false;

//This is entirely cut-and-paste from kmessagebox.cpp...*crosses fingers*
//Using the KDE 2.2 version of KMessageBox for 2.2 compatibility...I should really just
//my own messagebox!! :)

static int createKMessageBox(KDialogBase *dialog, QMessageBox::Icon icon,
               const QString &text, const QStringList &strlist, const QString &ask,
               bool *checkboxReturn, const QString &details=QString::null)
{
    QVBox *topcontents = new QVBox (dialog);
    topcontents->setSpacing(KDialog::spacingHint()*2);
    topcontents->setMargin(KDialog::marginHint()*2);

    QWidget *contents = new QWidget(topcontents);
    QHBoxLayout * lay = new QHBoxLayout(contents);
    lay->setSpacing(KDialog::spacingHint()*2);

    lay->addStretch(1);
    QLabel *label1 = new QLabel( contents);
		//GUIStyle has changed between KDE2.2 and KDE3...omitting
    //label1->setPixmap(QMessageBox::standardIcon(icon, kapp->style().guiStyle()));
    lay->add( label1 );
    QLabel *label2 = new QLabel( text, contents);
    label2->setMinimumSize(label2->sizeHint());
    lay->add( label2 );
    lay->addStretch(1);

    QSize extraSize = QSize(50,30);
    if (!strlist.isEmpty())
    {
       KListBox *listbox=new KListBox( topcontents );
       listbox->insertStringList( strlist );
    }

    QCheckBox *checkbox = 0;
    if (!ask.isEmpty())
    {
       checkbox = new QCheckBox(ask, topcontents);
       extraSize = QSize(50,0);
    }

    if (!details.isEmpty())
    {
       QVGroupBox *detailsGroup = new QVGroupBox( i18n("Details:"), dialog);
       QLabel *label3 = new QLabel(details, detailsGroup);
       label3->setMinimumSize(label3->sizeHint());
       dialog->setDetailsWidget(detailsGroup);
    }

    dialog->setMainWidget(topcontents);
    dialog->enableButtonSeparator(false);
    dialog->incInitialSize( extraSize );

    if (KMessageBox_queue)
    {
       KDialogQueue::queueDialog(dialog);
       return KMessageBox::Cancel; // We have to return something.
    }

    int result = dialog->exec();
    if (checkbox && checkboxReturn)
       *checkboxReturn = checkbox->isChecked();
    delete dialog;
    return result;
}

//Cut-and-paste copy of KMessageBox::warningContinueCancelList, with second button removed
int KStarsMessageBox::badCatalog(QWidget *parent, const QString &text,
                             const QStringList &strlist,
                             const QString &caption,
	  	  	                     const QString &dontAskAgainName,
  	  	  	                   bool /*notify*/)
{
    KConfig *config = 0;
    QString grpNotifMsgs = QString::fromLatin1("Notification Messages");
    bool showMsg = true;

    if (!dontAskAgainName.isEmpty())
    {
       config = kapp->config();
       KConfigGroupSaver saver( config, grpNotifMsgs );
       showMsg = config->readBoolEntry( dontAskAgainName, true);
       if (!showMsg)
       {
          return Continue;
       }
    }

    KDialogBase *dialog= new KDialogBase(
                       caption.isEmpty() ? i18n("No Valid Data found") : caption,
                       KDialogBase::Yes,
                       KDialogBase::Yes, KDialogBase::Yes,
                       parent, "warningYesNo", true, true,
                       i18n("&Close"));

    bool checkboxResult;
    int result = createKMessageBox(dialog, QMessageBox::Warning, text, strlist,
                       dontAskAgainName.isEmpty() ? QString::null : i18n("Do not ask again"),
                       &checkboxResult);

    switch( result )
    {
      case KDialogBase::Yes:
      {
         if (!dontAskAgainName.isEmpty())
         {
            showMsg = !checkboxResult;
            if (!showMsg)
            {
               KConfigGroupSaver saver( config, grpNotifMsgs );
               config->writeEntry( dontAskAgainName, showMsg);
            }
            config->sync();
         }
         return Continue;
      }

      case KDialogBase::No:
         return Cancel;

      default: // Huh?
         break;
    }

    return Cancel; // Default
}


/*
    SPDX-FileCopyrightText: 2001 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_addlinkdialog.h"

#include <klineedit.h>
#include <KLocalizedString>

#include <QDialog>
#include <QVBoxLayout>

class QString;

class AddLinkDialogUI : public QFrame, public Ui::AddLinkDialog
{
    Q_OBJECT
  public:
    explicit AddLinkDialogUI(QWidget *parent = nullptr);
};

/**
 * @class AddLinkDialog
 * AddLinkDialog is a simple dialog for adding a custom URL to a popup menu.
 *
 * @author Jason Harris
 * @version 1.0
 */
class AddLinkDialog : public QDialog
{
    Q_OBJECT
  public:
    /** Constructor */
    explicit AddLinkDialog(QWidget *parent = nullptr, const QString &oname = i18n("object"));

    /** Destructor */
    ~AddLinkDialog() override = default;

    /** @return QString of the entered URL */
    QString url() const { return ald->URLBox->text(); }

    /**
     * @short Set the URL text
     * @param s the new URL text
     */
    void setURL(const QString &s) { ald->URLBox->setText(s); }

    /** @return QString of the entered menu entry text */
    QString desc() const { return ald->DescBox->text(); }

    /**
     * @short Set the Description text
     * @param s the new description text
     */
    void setDesc(const QString &s) { ald->DescBox->setText(s); }

    /** @return true if user declared the link is an image */
    bool isImageLink() const { return ald->ImageRadio->isChecked(); }

    /**
     * @short Set the link type
     * @param b if true, link is an image link.
     */
    void setImageLink(bool b) { ald->ImageRadio->setChecked(b); }

  private slots:
    /** Open the entered URL in the web browser */
    void checkURL(void);

    /**
     * We provide a default menu text string; this function changes the
     * default string if the link type (image/webpage) is changed.  Note
     * that if the user has changed the menu text, this function does nothing.
     * @param imageEnabled if true, show image string; otherwise show webpage string.
     */
    void changeDefaultDescription(bool imageEnabled);

  private:
    QString ObjectName;
    AddLinkDialogUI *ald;
};

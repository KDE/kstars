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

#ifndef SKYGUIDEWRITER_H
#define SKYGUIDEWRITER_H

#include <QDialog>

#include "skyguideobject.h"
#include "ui_skyguideauthor.h"
#include "ui_skyguideslide.h"
#include "ui_skyguidewriter.h"

class SkyGuideMgr;

class SkyGuideWriterUI : public QFrame, public Ui::SkyGuideWriter {
    Q_OBJECT

    public:
        SkyGuideWriterUI(QWidget *p = 0);
};

//! @class SkyGuideWriter
//! @author Marcos CARDINOT <mcardinot@gmail.com>
class SkyGuideWriter : public QDialog
{
    Q_OBJECT

    public:
        SkyGuideWriter(SkyGuideMgr *mgr, QWidget *parent=0);
        ~SkyGuideWriter();

    private slots:
        void slotNew();
        void slotOpen();
        void slotSave();
        void slotSaveAs();
        void slotInstall();

        void slotUpdateButtons();
        void slotRemoveAuthor();
        void slotRemoveSlide();

        void slotFieldsChanged();

    private:
        QDialog* m_authorDlg;
        QDialog* m_slideDlg;

        SkyGuideWriterUI* m_ui;
        Ui_SkyGuideAuthor* m_uiAuthor;
        Ui_SkyGuideSlide* m_uiSlide;

        SkyGuideMgr* m_skyGuideMgr;
        SkyGuideObject* m_skyGuideObject;
        bool m_unsavedChanges;
        QString m_currentDir;

        void populateFields();
        void setUnsavedChanges(bool b);
        void blockSignals(bool b);

        void saveWarning();
        bool checkRequiredFieldsWarning();
};

#endif // SKYGUIDEWRITER_H

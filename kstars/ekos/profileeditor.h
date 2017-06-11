/*  Profile Editor
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef PROFILEEDITOR_H
#define PROFILEEDITOR_H

#include <QDialog>

#include "ui_profileeditor.h"

class ProfileInfo;

class ProfileEditorUI : public QFrame, public Ui::ProfileEditorUI
{
    Q_OBJECT

  public:
    /** @short Constructor
         */
    explicit ProfileEditorUI(QWidget *parent);
};

class ProfileEditor : public QDialog
{
    Q_OBJECT
  public:
    /**
         *@short Constructor.
         */
    explicit ProfileEditor(QWidget *ks);

    /**
         *@short Destructor.
         */
    ~ProfileEditor();

    ProfileInfo *getPi() const;
    void setPi(ProfileInfo *value);

    void loadDrivers();

    void setProfileName(const QString &name);
    void setAuxDrivers(const QStringList &aux);
    void setHostPort(const QString &host, const QString &port);
    void setWebManager(bool enabled, const QString &port = "8624");
    void setExternalGuider(const QString &name);
    void setConnectionOptionsEnabled(bool enable);

  public slots:
    void saveProfile();
    void setRemoteMode(bool enable);

  private:
    ProfileEditorUI *ui;
    ProfileInfo *pi;
};

#endif // PROFILEEDITOR_H

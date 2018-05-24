/*  Profile Editor
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "oal/scope.h"
#include "ui_profileeditor.h"

#include <QDialog>
#include <QFrame>
#include <QString>
#include <QStringList>

class ProfileInfo;

class ProfileEditorUI : public QFrame, public Ui::ProfileEditorUI
{
    Q_OBJECT

  public:
    /** @short Constructor */
    explicit ProfileEditorUI(QWidget *parent);
};

class ProfileEditor : public QDialog
{
    Q_OBJECT
  public:
    /** @short Constructor */
    explicit ProfileEditor(QWidget *ks);

    /** @short Destructor */
    virtual ~ProfileEditor() override = default;

    void setPi(ProfileInfo *value);

    void loadDrivers();
    void loadScopeEquipment();

    void setProfileName(const QString &name);
    void setAuxDrivers(const QStringList &aux);
    void setHostPort(const QString &host, const QString &port);
    void setWebManager(bool enabled, const QString &port = "8624");
    void setGuiderType(int type);
    void setConnectionOptionsEnabled(bool enable);

  public slots:
    void saveProfile();
    void setRemoteMode(bool enable);

  private slots:
    void updateGuiderSelection(int id);

  private:
    ProfileEditorUI *ui { nullptr };
    ProfileInfo *pi { nullptr };
    QList<OAL::Scope *> m_scopeList;
};

/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QFocusEvent>
#include <QTextEdit>

/**
 * @class LogEdit
 * @brief This is a simple derivative of QTextEdit, that just adds a
 * focusOut() signal, emitted when the edit loses focus.
 *
 * @author Jason Harris
 * @version 1.0
 */
class LogEdit : public QTextEdit
{
    Q_OBJECT

  public:
    explicit LogEdit(QWidget *parent = nullptr);
    virtual ~LogEdit() override = default;

  signals:
    void focusOut();

  protected:
    void focusOutEvent(QFocusEvent *e) override;
};

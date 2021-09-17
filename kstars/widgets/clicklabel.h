/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CLICKLABEL_H
#define CLICKLABEL_H

#include <QLabel>
#include <QMouseEvent>

/** @class ClickLabel
        * @brief This is a QLabel with a clicked() signal.
	*@author Jason Harris
	*@version 1.0
	*/
class ClickLabel : public QLabel
{
    Q_OBJECT
  public:
    explicit ClickLabel(QWidget *parent = nullptr, const char *name = nullptr);
    ~ClickLabel() override = default;

  signals:
    void clicked();

  protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
            emit clicked();
    }
};

#endif

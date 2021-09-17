/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QListWidget>

class QDragEnterEvent;
class QDropEvent;
class QMouseEvent;

/**
 * @class DragListBox
 * @short Extension of KListWidget that allows Drag-and-Drop with other DragListBoxes
 *
 * @author Jason Harris
 * @version 1.0
 */
class DragListBox : public QListWidget
{
    Q_OBJECT
  public:
    explicit DragListBox(QWidget *parent = nullptr, const char *name = nullptr);

    int ignoreIndex() const { return IgnoreIndex; }
    bool contains(const QString &s) const;

    void dragEnterEvent(QDragEnterEvent *evt) override;
    void dragMoveEvent(QDragMoveEvent *evt) override;
    void dropEvent(QDropEvent *evt) override;
    void mousePressEvent(QMouseEvent *evt) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

  private:
    bool leftButtonDown { false };
    int IgnoreIndex { 0 };
};

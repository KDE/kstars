/***************************************************************************
                          draglistbox.h  -  description
                             -------------------
    begin                : Sun May 29 2005
    copyright            : (C) 2005 by Jason Harris
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

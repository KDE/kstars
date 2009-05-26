/***************************************************************************
                          astrocalc.h  -  description
                             -------------------
    begin                : wed dec 19 16:20:11 CET 2002
    copyright            : (C) 2001-2002 by Pablo de Vicente
    email                : vicente@oan.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ASTROCALC_H_
#define ASTROCALC_H_

#include <QMap>
#include <QString>
#include <kdialog.h>


class QSplitter;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class KTextEdit;

/** Astrocalc is the base class for the KStars astronomical calculator
 * @author: Pablo de Vicente
 * @version 0.9
 */

class AstroCalc : public KDialog
{

    Q_OBJECT
public:
    AstroCalc(QWidget *parent = 0);

    ~AstroCalc();

    /**@returns suggested size of calculator window. */
    QSize sizeHint() const;
public slots:
    /** Display calculator module or help text based on item selected.
	 */
    void slotItemSelection(QTreeWidgetItem *it);

private:
    /** Create widget of type T and put it to widget stack. Widget must
     *  have construtor of type T(QWidget*). Returns constructed widget. */
    template<typename T>
    inline T* addToStack();
    
    /** Add top level item to navigation panel. At the same time adds item to htmlTable 
        @param title name of item
        @param html  string to be displayed in splash screen
     */
    QTreeWidgetItem* addTreeTopItem(QTreeWidget* parent, QString title, QString html);

    /** Add item to navigation panel. At the same time adds item to dispatchTable
        @param title  name of item
        @param widget widget to be selected on click
     */
    QTreeWidgetItem* addTreeItem(QTreeWidgetItem* parent, QString title, QWidget* widget);

    /** Lookup table for help texts. Maps navpanel item to help text. */
    QMap<QTreeWidgetItem*, QString>  htmlTable;
    /** Lookup table for widgets. Maps navpanel item to widget to be displayed. */
    QMap<QTreeWidgetItem*, QWidget*> dispatchTable;
    QSplitter *split;
    QTreeWidget *navigationPanel;
    QStackedWidget *acStack;
    KTextEdit *splashScreen;
};

#endif

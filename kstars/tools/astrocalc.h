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
#include <QDialog>


class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;

/** Astrocalc is the base class for the KStars astronomical calculator
 * @author: Pablo de Vicente
 * @version 0.9
 */

class AstroCalc : public QDialog
{
    Q_OBJECT
public:
    AstroCalc(QWidget *parent = 0);

    ~AstroCalc();

    /** @returns suggested size of calculator window. */
    QSize sizeHint() const;
public slots:
    /** Display calculator module or help text based on item selected.
	 */
    void slotItemSelection(QTreeWidgetItem *it); // Q: Why is this public when we don't have access to navigationPanel anyway? Also doesn't seem to be used from outside -- asimha

private:
    /** Pointer to function which return QWidget* 
     */
    typedef QWidget* (AstroCalc::*WidgetConstructor)();
    /** Data structure used for lazy widget construction. This class
     *  construct widget when it requested. 
     */
    class WidgetThunk
    {
    public:
        /** Create thunk
         *  @param acalc  pointer to class.
         *  @param f      function which construct widget.
         */
        WidgetThunk(AstroCalc* acalc, WidgetConstructor f) :
            widget(0), calc(acalc), func(f) {}
        /** Request widget.
         *  @return newly created widget or cached value. */
        QWidget* eval();
    private:
        QWidget* widget;        // Cached value
        AstroCalc* calc;        // Pointer to calculator
        WidgetConstructor func; // Function to call to construct widget. 
    };
    
    /** Create widget of type T and put it to widget stack. Widget must
     *  have construtor of type T(QWidget*). Returns constructed widget. */
    template<typename T>
    inline QWidget* addToStack();
    
    /** Add top level item to navigation panel. At the same time adds item to htmlTable 
        @param title name of item
        @param html  string to be displayed in splash screen
     */
    QTreeWidgetItem* addTreeTopItem(QTreeWidget* parent, QString title, QString html);

    /** Add item to navigation panel. At the same time adds item to
        dispatchTable Template parameter is type of widget to be
        constructed and added to widget stack. It must have T()
        constructor.

        @param title  name of item
     */
    template<typename T>
    QTreeWidgetItem* addTreeItem(QTreeWidgetItem* parent, QString title);

    /** Lookup table for help texts. Maps navpanel item to help text. */
    QMap<QTreeWidgetItem*, QString>  htmlTable;
    /** Lookup table for widgets. Maps navpanel item to widget to be displayed. */
    QMap<QTreeWidgetItem*, WidgetThunk> dispatchTable;
    QTreeWidget *navigationPanel;
    QStackedWidget *acStack;
    QTextEdit *splashScreen;
};

#endif

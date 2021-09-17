/*
    SPDX-FileCopyrightText: 2001-2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QMap>
#include <QString>
#include <QDialog>

class QStackedWidget;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * @class AstroCalc
 * @brief This is the base class for the KStars astronomical calculator
 *
 * @author: Pablo de Vicente
 * @version 0.9
 */
class AstroCalc : public QDialog
{
    Q_OBJECT

  public:
    explicit AstroCalc(QWidget *parent = nullptr);

    /** @return suggested size of calculator window. */
    QSize sizeHint() const override;
  public slots:
     // Q: Why is this public when we don't have access to navigationPanel anyway?
     // Also doesn't seem to be used from outside -- asimha
    /** Display calculator module or help text based on item selected. */
    void slotItemSelection(QTreeWidgetItem *it);

  private:
    /** Pointer to function which return QWidget */
    typedef QWidget *(AstroCalc::*WidgetConstructor)();
    /**
     * Data structure used for lazy widget construction. This class
     * construct widget when it requested.
     */
    class WidgetThunk
    {
      public:
        /**
         * Create thunk
         * @param acalc  pointer to class.
         * @param f      function which construct widget.
         */
        WidgetThunk(AstroCalc *acalc, const WidgetConstructor& f) : widget(nullptr), calc(acalc), func(f) { }
        /**
         * Request widget.
         * @return newly created widget or cached value.
         */
        QWidget *eval();

      private:
        /// Cached value
        QWidget *widget { nullptr };
        /// Pointer to calculator
        AstroCalc *calc { nullptr };
        /// Function call to construct the widget.
        WidgetConstructor func;
    };

    /**
     * Create widget of type T and put it to widget stack. Widget must
     * have constructor of type T(QWidget*). Returns constructed widget.
     */
    template <typename T>
    inline QWidget *addToStack();

    /**
     * Add top level item to navigation panel. At the same time adds item to htmlTable
     * @param title name of item
     * @param html  string to be displayed in splash screen
     */
    QTreeWidgetItem *addTreeTopItem(QTreeWidget *parent, const QString &title, const QString &html);

    /**
     * Add item to navigation panel. At the same time adds item to dispatchTable Template
     * parameter is type of widget to be constructed and added to widget stack. It must
     * have T() constructor.
     * @param title  name of item
     */
    template <typename T>
    QTreeWidgetItem *addTreeItem(QTreeWidgetItem *parent, const QString &title);

    /** Lookup table for help texts. Maps navpanel item to help text. */
    QMap<QTreeWidgetItem *, QString> htmlTable;
    /** Lookup table for widgets. Maps navpanel item to widget to be displayed. */
    QMap<QTreeWidgetItem *, WidgetThunk> dispatchTable;
    QTreeWidget *navigationPanel { nullptr };
    QStackedWidget *acStack { nullptr };
    QTextEdit *splashScreen { nullptr };
};

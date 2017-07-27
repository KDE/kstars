/***************************************************************************
                          pwizprint.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 3 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
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

#include "ui_pwizprint.h"

class QPrinter;

class PrintingWizard;

/**
 * @class PWizPrintUI
 * @brief User interface for last "Print and export finder chart" step of the Printing Wizard.
 *
 * @author Rafał Kułaga
 */
class PWizPrintUI : public QFrame, public Ui::PWizPrint
{
    Q_OBJECT
  public:
    /** Constructor. */
    explicit PWizPrintUI(PrintingWizard *wizard, QWidget *parent = nullptr);

  private slots:
    /** Slot: show "Print preview" dialog window for finder chart. */
    void slotPreview();

    /**
     * @brief Slot: show "Print preview" dialog window for finder chart (on passed QPrinter).
     * @param printer Printer on which preview should be painted.
     */
    void slotPrintPreview(QPrinter *printer);

    /** Slot: open printing dialog and print document. */
    void slotPrint();

    /** Slot: open "Save file" dialog to select export file name and format. */
    void slotExport();

  private:
    /**
     * @brief Print document on passed printer.
     * @param printer Printer.
     */
    void printDocument(QPrinter *printer);

    PrintingWizard *m_ParentWizard { nullptr };
};

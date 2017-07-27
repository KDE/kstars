/***************************************************************************
                          loggingform.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 20 2011
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

#include "kstarsdocument.h"

class QTextDocument;

/**
 * @class LoggingForm
 * @brief Class that represents logging form.
 * Currently, LoggingForm class is used to create logging forms for finder charts.
 *
 * @author Rafał Kułaga
 */
class LoggingForm : public KStarsDocument
{
  public:
    /** Constructor */
    LoggingForm();

    /** Create simple logging form for finder charts. */
    void createFinderChartLogger();

    /**
     * @brief Get logging form internal QTextDocument.
     * This method is used to enable inserting of LoggingForm objects into QTextDocument
     * instances.
     * @return QTextDocument that contains logging form.
     */
    QTextDocument *getDocument();
};

/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    LoggingForm() = default;

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

/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PWIZCHARTCONFIG_H
#define PWIZCHARTCONFIG_H

#include "ui_pwizchartconfig.h"

/**
  * \class PWizChartConfigUI
  * \brief User interface for "Configure basic finder chart settings" step of the Printing Wizard.
  * \author Rafał Kułaga
  */
class PWizChartConfigUI : public QFrame, public Ui::PWizChartConfig
{
    Q_OBJECT
  public:
    /**
          * \brief Constructor.
          */
    explicit PWizChartConfigUI(QWidget *parent = nullptr);

    /**
          * \brief Get entered chart title.
          * \return Chart title.
          */
    QString getChartTitle() { return titleEdit->text(); }

    /**
          * \brief Get entered chart subtitle.
          * \return Chart subtitle.
          */
    QString getChartSubtitle() { return subtitleEdit->text(); }

    /**
          * \brief Get entered chart description.
          * \return Chart description.
          */
    QString getChartDescription() { return descriptionTextEdit->toPlainText(); }
};

#endif // PWIZCHARTCONFIG_H

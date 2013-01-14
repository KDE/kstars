/***************************************************************************
                          pwizchartconfig.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 10 2011
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

#ifndef PWIZCHARTCONFIG_H
#define PWIZCHARTCONFIG_H

#include "ui_pwizchartconfig.h"

class PrintingWizard;

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
    explicit PWizChartConfigUI(QWidget *parent = 0);

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

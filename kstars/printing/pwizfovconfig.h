/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PWIZFOVCONFIG_H
#define PWIZFOVCONFIG_H

#include "ui_pwizfovconfig.h"
#include "legend.h"

/**
  * \class PWizFovConfigUI
  * \brief User interface for "Configure common FOV export options" step of the Printing Wizard.
  * \author Rafał Kułaga
  */
class PWizFovConfigUI : public QFrame, public Ui::PWizFovConfig
{
    Q_OBJECT
  public:
    /**
          * \brief Constructor.
          */
    explicit PWizFovConfigUI(QWidget *parent = nullptr);

    /**
          * \brief Check if switching to "Sky Chart" color scheme is enabled.
          * \return True if color scheme switching is enabled.
          */
    bool isSwitchColorsEnabled() { return switchColorsBox->isChecked(); }

    /**
          * \brief Check if FOV shape is always rectangular.
          * \return True if FOV shape is always rectangular.
          */
    bool isFovShapeOverriden() { return overrideShapeBox->isChecked(); }

    /**
          * \brief Check if legend will be added to FOV images.
          * \return True if legend will be added to FOV images.
          */
    bool isLegendEnabled() { return addLegendBox->isChecked(); }

    /**
          * \brief Check if alpha blending is enabled.
          * \return True if alpha blending is enabled.
          */
    bool isAlphaBlendingEnabled() { return useAlphaBlendBox->isChecked(); }

    /**
          * \brief Get selected legend type.
          * \return Selected legend type.
          */
    Legend::LEGEND_TYPE getLegendType();

    /**
          * \brief Get selected legend orientation.
          * \return Selected legend orientation.
          */
    Legend::LEGEND_ORIENTATION getLegendOrientation()
    {
        return static_cast<Legend::LEGEND_ORIENTATION>(orientationCombo->currentIndex());
    }

    /**
          * \brief Get selected legend position.
          * \return Selected legend position.
          */
    Legend::LEGEND_POSITION getLegendPosition()
    {
        return static_cast<Legend::LEGEND_POSITION>(positionCombo->currentIndex());
    }

  private slots:
    /**
          * \brief Slot: enable or disable legend configuration fields.
          * \param enabled True if legend configuration fields should be enabled.
          */
    void slotUpdateLegendFields(bool enabled);

  private:
    /**
          * \brief Configure widgets.
          */
    void setupWidgets();

    /**
          * \brief Configure signal-slot connections.
          */
    void setupConnections();
};

#endif // PWIZFOVCONFIG_H

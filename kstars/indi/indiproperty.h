/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later


*/

#pragma once

#include "indicommon.h"
#include <libindi/indiproperty.h>

#include <QWidget>

#include <memory>

namespace INDI
{
class Property;
}

class INDI_G;
class INDI_E;

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QPushButton;
class QSpacerItem;
class QVBoxLayout;

class KLed;
class KSqueezedTextLabel;

/**
 * @class INDI_P
 * INDI_P represents a single INDI property (Switch, Text, Number, Light, or BLOB). It handles building the GUI and updating the property status and/or value as new data
 * arrive from INDI Serve. It also sends any changes in the property value back to INDI server via the ClientManager.
 *
 * @author Jasem Mutlaq
 */
class INDI_P : public QWidget
{
        Q_OBJECT
    public:
        INDI_P(INDI_G *ipg, INDI::Property prop);

        /* Draw state LED */
        void updateStateLED();

        /* Update menu gui */
        void updateMenuGUI();

        void initGUI();

        void buildSwitchGUI();
        void buildMenuGUI();
        void buildTextGUI();
        void buildNumberGUI();
        void buildLightGUI();
        void buildBLOBGUI();

        /** Setup the 'set' button in the property */
        void setupSetButton(const QString &caption);

        /**
         * @brief newTime Display dialog to set UTC date and time to the driver.
         */
        void newTime();

        PGui getGUIType() const
        {
            return guiType;
        }

        INDI_G *getGroup() const
        {
            return pg;
        }

        const QString &getName() const
        {
            return name;
        }

        void addWidget(QWidget *w);
        void addLayout(QHBoxLayout *layout);

        INDI_E *getElement(const QString &elementName) const;

        QList<INDI_E *> getElements() const
        {
            return elementList;
        }
        bool isRegistered() const;
        const INDI::Property getProperty() const
        {
            return dataProp;
        }

    public slots:
        void processSetButton();
        void newSwitch(QAbstractButton *button);
        void newSwitch(int index);
        void newSwitch(const QString &name);
        void resetSwitch();

        void sendBlob();
        void sendSwitch();
        void sendText();

        void setBLOBOption(int state);

    private:
        /// Parent group
        INDI_G *pg { nullptr };
        INDI::Property dataProp;
        QCheckBox *enableBLOBC { nullptr };
        /// Label widget
        KSqueezedTextLabel* labelW { nullptr };
        /// Set button
        QPushButton* setB { nullptr };
        /// Status LED
        KLed* ledStatus { nullptr };
        /// GUI type
        PGui guiType;
        /// Horizontal spacer
        QSpacerItem *horSpacer { nullptr };
        /// Horizontal container
        QHBoxLayout *PHBox { nullptr };
        /// Vertical container
        QVBoxLayout *PVBox { nullptr };
        /// Group button for radio and check boxes (Elements)
        QButtonGroup *groupB { nullptr };
        /// Combo box for menu
        QComboBox* menuC { nullptr };
        QString name;
        /// List of elements
        QList<INDI_E *> elementList;
};

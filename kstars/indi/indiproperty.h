/*  INDI Property
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.


 */

#pragma once

#include "indicommon.h"
#include <libindi/indiproperty.h>

#include <QObject>

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
class INDI_P : public QObject
{
        Q_OBJECT
    public:
        INDI_P(INDI_G *ipg, INDI::Property prop);
        ~INDI_P();

        /* Draw state LED */
        void updateStateLED();

        /* Update menu gui */
        void updateMenuGUI();

        void initGUI();

        /* First step in adding a new GUI element */
        //void addGUI(XMLEle *root);

        /* Set Property's parent group */
        //void setGroup(INDI_G *parentGroup) { pg = parentGroup; }

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

        QHBoxLayout *getContainer() const
        {
            return PHBox.get();
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
        std::unique_ptr<KSqueezedTextLabel> labelW;
        /// Set button
        std::unique_ptr<QPushButton> setB;
        /// Status LED
        std::unique_ptr<KLed> ledStatus;
        /// GUI type
        PGui guiType;
        /// Horizontal spacer
        QSpacerItem *horSpacer { nullptr };
        /// Horizontal container
        std::unique_ptr<QHBoxLayout> PHBox;
        /// Vertical container
        QVBoxLayout *PVBox { nullptr };
        /// Group button for radio and check boxes (Elements)
        std::unique_ptr<QButtonGroup> groupB;
        /// Combo box for menu
        std::unique_ptr<QComboBox> menuC;
        QString name;
        /// List of elements
        QList<INDI_E *> elementList;
};

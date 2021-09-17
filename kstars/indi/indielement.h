/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    2004-01-15	INDI element is the most basic unit of the INDI KStars client.
*/

#pragma once

#include "indicommon.h"

#include <indiproperty.h>

#include <QDialog>
#include <QHBoxLayout>

/* Forward declaration */
class QLineEdit;
class QDoubleSpinBox;
class QPushButton;

class QHBoxLayout;
class QSpacerItem;
class QCheckBox;
class QButtonGroup;
class QSlider;

class KLed;
class KSqueezedTextLabel;

class INDI_P;

/**
 * @class INDI_E
 * INDI_E represents an INDI GUI element (Number, Text, Switch, Light, or BLOB) within an INDI property.
 * It is the most basic GUI representation of property elements.
 *
 * @author Jasem Mutlaq
 */
class INDI_E : public QWidget
{
        Q_OBJECT
    public:
        INDI_E(INDI_P *gProp, INDI::Property dProp);

        const QString &getLabel()
        {
            return label;
        }
        const QString &getName()
        {
            return name;
        }

        QString getWriteField();
        QString getReadField();

        void buildSwitch(QButtonGroup *groupB, ISwitch *sw);
        void buildMenuItem(ISwitch *sw);
        void buildText(IText *itp);
        void buildNumber(INumber *inp);
        void buildLight(ILight *ilp);
        void buildBLOB(IBLOB *ibp);

        // Updates GUI from data in INDI properties
        void syncSwitch();
        void syncText();
        void syncNumber();
        void syncLight();

        // Save GUI data in INDI properties
        void updateTP();
        void updateNP();

        void setText(const QString &newText);
        void setValue(double value);

        void setMin();
        void setMax();

        void setupElementLabel();
        void setupElementRead(int length);
        void setupElementWrite(int length);
        void setupElementScale(int length);
        void setupBrowseButton();

        bool getBLOBDirty()
        {
            return blobDirty;
        }
        void setBLOBDirty(bool isDirty)
        {
            blobDirty = isDirty;
        }

    public slots:
        void spinChanged(double value);
        void sliderChanged(int value);
        void browseBlob();

    private:
        /// Name
        QString name;
        /// Label is the name by default, unless specified
        QString label;
        /// Parent GUI property
        INDI_P *guiProp { nullptr };
        /// Parent DATA property
        INDI::Property dataProp;
        /// Horizontal layout
        QHBoxLayout *EHBox { nullptr };
        /// Label widget
        KSqueezedTextLabel *label_w { nullptr };
        /// Read field widget
        QLineEdit *read_w { nullptr };
        /// Write field widget
        QLineEdit *write_w { nullptr };
        /// Light led widget
        KLed *led_w { nullptr };
        /// Spinbox widget
        QDoubleSpinBox *spin_w { nullptr };
        /// Slider widget
        QSlider *slider_w { nullptr };
        /// Push button widget
        QPushButton *push_w { nullptr };
        /// Browse button widget
        QPushButton *browse_w { nullptr };
        /// Check box widget
        QCheckBox *check_w { nullptr };
        /// Horizontal spacer widget
        QSpacerItem *hSpacer { nullptr };

        ISwitch *sp { nullptr };
        INumber *np { nullptr };
        IText *tp { nullptr };
        ILight *lp { nullptr };
        IBLOB *bp { nullptr };

        bool blobDirty { false };
        /// Current text
        QString text;
};

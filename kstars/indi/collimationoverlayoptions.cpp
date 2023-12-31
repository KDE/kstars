/*
    SPDX-FileCopyrightText: 2023 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "collimationoverlayoptions.h"
#include <kstars_debug.h>

#include "kstarsdata.h"
#include "kstars.h"
#include "qmetaobject.h"

#include <QTimer>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <basedevice.h>
#include <algorithm>

CollimationOverlayOptions *CollimationOverlayOptions::m_Instance = nullptr;

CollimationOverlayOptions *CollimationOverlayOptions::Instance(QWidget *parent)
{
    if (m_Instance == nullptr) {
        m_Instance = new CollimationOverlayOptions(parent);
    }
    return m_Instance;
}

void CollimationOverlayOptions::release()
{
    delete(m_Instance);
    m_Instance = nullptr;
}

CollimationOverlayOptions::CollimationOverlayOptions(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setupUi(this);

    // Enable Checkbox
    connect(EnableCheckBox, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), this,
            [this](int state) {
                updateValue(state, "Enabled");
            });

    // Type Combo
    connect(typeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]() {
                updateValue(typeComboBox, "Type");

                // Anchor types can't have a size, rotation, count, PCD, thickness, or colour
                if (typeComboBox->currentIndex() == 0) {
                    sizeXSpinBox->setValue(0);
                    sizeYSpinBox->setValue(0);
                    rotationDoubleSpinBox->setValue(0.0);
                    countSpinBox->setValue(0);
                    pcdSpinBox->setValue(0);
                    thicknessSpinBox->setEnabled(false);
                    sizeXSpinBox->setEnabled(false);
                    sizeYSpinBox->setEnabled(false);
                    rotationDoubleSpinBox->setEnabled(false);
                    countSpinBox->setEnabled(false);
                    pcdSpinBox->setEnabled(false);
                    thicknessSpinBox->setEnabled(false);
                    colourButton->setColor("Black");
                    colourButton->setEnabled(false);
                } else {
                    sizeXSpinBox->setEnabled(true);
                    sizeYSpinBox->setEnabled(true);
                    rotationDoubleSpinBox->setEnabled(true);
                    countSpinBox->setEnabled(true);
                    pcdSpinBox->setEnabled(true);
                    thicknessSpinBox->setEnabled(true);
                    colourButton->setEnabled(true);
                }

                // Default to linked XY size for Ellipse types only
                if (typeComboBox->currentIndex() == 1) {
                    linkXYB->setIcon(QIcon::fromTheme("document-encrypt"));
                } else {
                    linkXYB->setIcon(QIcon::fromTheme("document-decrypt"));
                }

                // Allow sizeY = 0 for lines
                if (typeComboBox->currentIndex() == 3){
                    sizeYSpinBox->setMinimum(0);
                } else {
                    sizeYSpinBox->setMinimum(1);
                }
            });

    //Populate typeComboBox
    QStringList typeValues;
    collimationoverlaytype m_types;
    const QMetaObject *m_metaobject = m_types.metaObject();
    QMetaEnum m_metaEnum = m_metaobject->enumerator(m_metaobject->indexOfEnumerator("Types"));
    for (int i = 0; i < m_metaEnum.keyCount(); i++) {
        typeValues << tr(m_metaEnum.key(i));
    }
    typeComboBox->clear();
    typeComboBox->addItems(typeValues);
    typeComboBox->setCurrentIndex(0);

    // SizeX SpinBox
    connect(sizeXSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) {
                updateValue(value, "SizeX");
                if (linkXYB->icon().name() == "document-encrypt") {
                    sizeYSpinBox->setValue(sizeXSpinBox->value());
                    updateValue(value, "SizeY");
                }
            });

    // SizeY SpinBox
    connect(sizeYSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) {
                updateValue(value, "SizeY");
            });

    //LinkXY Button
    linkXYB->setIcon(QIcon::fromTheme("document-decrypt"));
    connect(linkXYB, &QPushButton::clicked, this, [this] {
        if (linkXYB->icon().name() == "document-decrypt") {
            sizeYSpinBox->setValue(sizeXSpinBox->value());
            linkXYB->setIcon(QIcon::fromTheme("document-encrypt"));
            sizeYSpinBox->setEnabled(false);
        } else if (linkXYB->icon().name() == "document-encrypt") {
            linkXYB->setIcon(QIcon::fromTheme("document-decrypt"));
            sizeYSpinBox->setEnabled(true);
        }
    });

    // OffsetX SpinBox
    connect(offsetXSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) {
                updateValue(value, "OffsetX");
            });

    // OffsetY SpinBox
    connect(offsetYSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) {
                updateValue(value, "OffsetY");
            });

    // Count SpinBox
    connect(countSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) {
                updateValue(value, "Count");
                // Single count elements can't have rotation or PCD
                if (value == 1) {
                    pcdSpinBox->setEnabled(false);
                    rotationDoubleSpinBox->setEnabled(false);
                } else {
                    pcdSpinBox->setEnabled(true);
                    rotationDoubleSpinBox->setEnabled(true);
                }
            });

    //PCD SpinBox
    connect(pcdSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) {
                updateValue(value, "PCD");
            });

    // Rotation DoubleSpinBox
    connect(rotationDoubleSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
                updateValue(value, "Rotation");
            });

    // Color KColorButton
    colourButton->setAlphaChannelEnabled(true);
    connect(colourButton, static_cast<void (KColorButton::*)(const QColor&)>(&KColorButton::changed), this,
            [this](QColor value) {
                updateValue(value, "Colour");
            });

    // Thickness SpinBox
    connect(thicknessSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) {
                updateValue(value, "Thickness");
            });

    connect(addB, &QPushButton::clicked, this, [this]() {
        if (addB->icon().name() == "dialog-ok-apply") {
            elementNamesList->clearSelection();
            addB->setIcon(QIcon::fromTheme("list-add"));
            selectCollimationOverlayElement("");
        } else {
            addElement(nameLineEdit->text());
            m_CollimationOverlayElementsModel->select();
            refreshModel();
            elementNamesList->clearSelection();
        }
    });

    connect(removeB, &QPushButton::clicked, this, [this]() {
        if (elementNamesList->currentItem() != nullptr) {
            removeCollimationOverlayElement(elementNamesList->currentItem()->text());
            refreshElements();
            elementNamesList->clearSelection();
            addB->setIcon(QIcon::fromTheme("list-add"));
        }
    });

    connect(elementNamesList, &QListWidget::itemClicked, this, [this](QListWidgetItem * item) {
                Q_UNUSED(item);
                addB->setIcon(QIcon::fromTheme("list-add"));
                removeB->setEnabled(true);
            });

    connect(elementNamesList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem * item) {
        selectCollimationOverlayElement(item);
        addB->setIcon(QIcon::fromTheme("dialog-ok-apply"));
        if (typeComboBox->currentIndex() == 1) {
            linkXYB->setIcon(QIcon::fromTheme("document-encrypt"));
        } else {
            linkXYB->setIcon(QIcon::fromTheme("document-decrypt"));
        }
    });

    connect(renameB, &QPushButton::clicked, this, [this] {
        renameCollimationOverlayElement(nameLineEdit->text());
    })  ;

    connect(elementNamesList, &QListWidget::currentRowChanged, this, [this](int row) {
        Q_UNUSED(row);
        selectCollimationOverlayElement("");
    });

    initModel();
}

void CollimationOverlayOptions::initModel()
{
    m_CollimationOverlayElements.clear();
    auto userdb = QSqlDatabase::database(KStarsData::Instance()->userdb()->connectionName());
    m_CollimationOverlayElementsModel = new QSqlTableModel(this, userdb);
    connect(m_CollimationOverlayElementsModel, &QSqlTableModel::dataChanged, this, [this]() {
        m_CollimationOverlayElements.clear();
        for (int i = 0; i < m_CollimationOverlayElementsModel->rowCount(); ++i) {
            QVariantMap recordMap;
            QSqlRecord record = m_CollimationOverlayElementsModel->record(i);
            for (int j = 0; j < record.count(); j++)
                recordMap[record.fieldName(j)] = record.value(j);

            m_CollimationOverlayElements.append(recordMap);
        }
        m_ElementNames.clear();
        for (auto &oneElement : m_CollimationOverlayElements) {
            m_ElementNames << oneElement["Name"].toString();
        }
        elementNamesList->clear();
        elementNamesList->addItems(m_ElementNames);
        elementNamesList->setEditTriggers(QAbstractItemView::AllEditTriggers);
        emit updated();
    });
    refreshModel();
}

void CollimationOverlayOptions::refreshModel()
{
    m_CollimationOverlayElements.clear();
    KStars::Instance()->data()->userdb()->GetCollimationOverlayElements(m_CollimationOverlayElements);
    m_ElementNames.clear();
    for (auto &oneElement : m_CollimationOverlayElements) {
        m_ElementNames << oneElement["Name"].toString();
    }
    elementNamesList->clear();
    elementNamesList->addItems(m_ElementNames);
}

QString CollimationOverlayOptions::addElement(const QString &name)
{
    QVariantMap element;
    element["Name"] = uniqueElementName(name, typeComboBox->currentText());
    element["Enabled"] = EnableCheckBox->checkState();
    element["Type"] = typeComboBox->currentText();
    element["SizeX"] = sizeXSpinBox->value();
    element["SizeY"] = sizeYSpinBox->value();
    element["OffsetX"] = offsetXSpinBox->value();
    element["OffsetY"] = offsetYSpinBox->value();
    element["Count"] = countSpinBox->value();
    element["PCD"] = pcdSpinBox->value();
    element["Rotation"] = rotationDoubleSpinBox->value();
    element["Colour"] = colourButton->color();
    element["Thickness"] = thicknessSpinBox->value();

    KStarsData::Instance()->userdb()->AddCollimationOverlayElement(element);
    emit updated();
    return element["Name"].toString();
}

bool CollimationOverlayOptions::setCollimationOverlayElementValue(const QString &name, const QString &field, const QVariant &value)
{
    for (auto &oneElement : m_CollimationOverlayElements) {
        if (oneElement["Name"].toString() == name) {
            // If value did not change, just return true
            if (oneElement[field] == value) {
                return true;
            }
            // Update field and database.
            oneElement[field] = value;
            KStarsData::Instance()->userdb()->UpdateCollimationOverlayElement(oneElement, oneElement["id"].toInt());
            emit updated();
            return true;
        }
    }
    return false;
}

void CollimationOverlayOptions::renameCollimationOverlayElement(const QString &name)
{
    if (m_CurrentElement != nullptr && (*m_CurrentElement)["Name"] != name) {
        auto pos = elementNamesList->currentRow();
        // ensure element name uniqueness
        auto unique = uniqueElementName(name, (*m_CurrentElement)["Type"].toString());
        // update the element database entry
        setCollimationOverlayElementValue((*m_CurrentElement)["Name"].toString(), "Name", unique);
        // propagate the unique name to the current selection
        elementNamesList->currentItem()->setText(unique);
        // refresh the trains
        refreshElements();
        // refresh selection
        elementNamesList->setCurrentRow(pos);
        selectCollimationOverlayElement(unique);
    }
}

bool CollimationOverlayOptions::setCollimationOverlayElement(const QJsonObject &element)
{
    auto oneElement = getCollimationOverlayElement(element["id"].toInt());
    if (!oneElement.empty()) {
        KStarsData::Instance()->userdb()->UpdateCollimationOverlayElement(element.toVariantMap(), oneElement["id"].toInt());
        refreshElements();
        return true;
    }
    return false;
}

bool CollimationOverlayOptions::removeCollimationOverlayElement(const QString &name)
{
    for (auto &oneElement : m_CollimationOverlayElements) {
        if (oneElement["Name"].toString() == name) {
            auto id = oneElement["id"].toInt();
            KStarsData::Instance()->userdb()->DeleteCollimationOverlayElement(id);
            refreshElements();
            return true;
        }
    }
    return false;
}

QString CollimationOverlayOptions::uniqueElementName(QString name, QString type)
{
    if ("" == name) name = type;
    QString result = name;
    int nr = 1;
    while (m_ElementNames.contains(result)) {
        result = QString("%1 (%2)").arg(name).arg(nr++);
    }
    return result;
}

bool CollimationOverlayOptions::selectCollimationOverlayElement(QListWidgetItem *item)
{
    if (item != nullptr && selectCollimationOverlayElement(item->text())) {
        return true;
    }
    return false;
}

bool CollimationOverlayOptions::selectCollimationOverlayElement(const QString &name)
{
    for (auto &oneElement : m_CollimationOverlayElements) {
        if (oneElement["Name"].toString() == name) {
            editing = true;
            m_CurrentElement = &oneElement;
            nameLineEdit->setText(oneElement["Name"].toString());
            renameB->setEnabled(true);
            EnableCheckBox->setChecked(oneElement["Enabled"].toUInt());
            typeComboBox->setCurrentText(oneElement["Type"].toString());
            sizeXSpinBox->setValue(oneElement["SizeX"].toUInt());
            sizeYSpinBox->setValue(oneElement["SizeY"].toUInt());
            offsetXSpinBox->setValue(oneElement["OffsetX"].toUInt());
            offsetYSpinBox->setValue(oneElement["OffsetY"].toUInt());
            countSpinBox->setValue(oneElement["Count"].toUInt());
            pcdSpinBox->setValue(oneElement["PCD"].toUInt());
            rotationDoubleSpinBox->setValue(oneElement["Rotation"].toDouble());
            QColor tempColour;
            tempColour.setNamedColor(oneElement["Colour"].toString());
            colourButton->setColor(tempColour);
            thicknessSpinBox->setValue(oneElement["Thickness"].toUInt());
            removeB->setEnabled(m_CollimationOverlayElements.length() > 0);
            elementConfigBox->setEnabled(true);
            return true;
        }
    }

    // none found
    editing = false;
    nameLineEdit->setText("");
    renameB->setEnabled(false);
    EnableCheckBox->setCheckState(Qt::Checked);
    typeComboBox->setCurrentText("--");
    sizeXSpinBox->setValue(0);
    sizeYSpinBox->setValue(0);
    offsetXSpinBox->setValue(0);
    offsetYSpinBox->setValue(0);
    countSpinBox->setValue(0);
    pcdSpinBox->setValue(0);
    rotationDoubleSpinBox->setValue(0.0);
    QColor tempColour;
    tempColour.setNamedColor("White");
    colourButton->setColor(tempColour);
    thicknessSpinBox->setValue(1);

    removeB->setEnabled(false);
    return false;
}

void CollimationOverlayOptions::openEditor()
{
    initModel();
    show();
}

const QVariantMap CollimationOverlayOptions::getCollimationOverlayElement(uint8_t id) const
{
    for (auto &oneElement : m_CollimationOverlayElements) {
        if (oneElement["id"].toInt() == id)
            return oneElement;
    }
    return QVariantMap();
}

bool CollimationOverlayOptions::exists(uint8_t id) const
{
    for (auto &oneElement : m_CollimationOverlayElements) {
        if (oneElement["id"].toInt() == id)
            return true;
    }
    return false;
}

const QVariantMap CollimationOverlayOptions::getCollimationOverlayElement(const QString &name) const
{
    for (auto &oneElement : m_CollimationOverlayElements) {
        if (oneElement["Name"].toString() == name) {
            return oneElement;
        }
    }
    return QVariantMap();
}

void CollimationOverlayOptions::refreshElements()
{
    refreshModel();
    emit updated();
}

int CollimationOverlayOptions::id(const QString &name) const
{
    for (auto &oneElement : m_CollimationOverlayElements) {
        if (oneElement["Name"].toString() == name)
            return oneElement["id"].toUInt();
    }
    return -1;
}

QString CollimationOverlayOptions::name(int id) const
{
    for (auto &oneElement : m_CollimationOverlayElements) {
        if (oneElement["id"].toInt() == id)
            return oneElement["name"].toString();
    }
    return QString();
}

void CollimationOverlayOptions::updateValue(QComboBox *cb, const QString &element)
{
    if (elementNamesList->currentItem() != nullptr && editing == true) {
        setCollimationOverlayElementValue(elementNamesList->currentItem()->text(), element, cb->currentText());
    }
}

void CollimationOverlayOptions::updateValue(double value, const QString &element)
{
    if (elementNamesList->currentItem() != nullptr && editing == true) {
        setCollimationOverlayElementValue(elementNamesList->currentItem()->text(), element, value);
    }
}

void CollimationOverlayOptions::updateValue(int value, const QString &element)
{
    if (elementNamesList->currentItem() != nullptr && editing == true) {
        setCollimationOverlayElementValue(elementNamesList->currentItem()->text(), element, value);
    }
}

void CollimationOverlayOptions::updateValue(QColor value, const QString &element)
{
    if (elementNamesList->currentItem() != nullptr && editing == true) {
        setCollimationOverlayElementValue(elementNamesList->currentItem()->text(), element, value);
    }
}

void CollimationOverlayOptions::updateValue(QString value, const QString &element)
{
    if (elementNamesList->currentItem() != nullptr && editing == true) {
        setCollimationOverlayElementValue(elementNamesList->currentItem()->text(), element, value);
    }
}

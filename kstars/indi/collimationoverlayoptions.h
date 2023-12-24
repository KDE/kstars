/*
    SPDX-FileCopyrightText: 2023 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_collimationOptions.h"
#include "collimationoverlaytypes.h"

#include <QDialog>
#include <QSqlDatabase>
#include <QQueue>
#include <QPointer>
#include <QMetaEnum>

class QSqlTableModel;

class CollimationOverlayOptions: public QDialog, public Ui::collimationOptions
{
        Q_OBJECT
        Q_PROPERTY(QList <QString> elementNames READ getElementNames)

    public:

        static CollimationOverlayOptions *Instance(QWidget *parent);
        static void release();

        void checkElements();

        bool exists(uint8_t id) const;
        const QVariantMap getCollimationOverlayElement(uint8_t id) const;
        const QVariantMap getCollimationOverlayElement(const QString &name) const;
        const QList<QVariantMap> &getCollimationOverlayElements() const
        {
            return m_CollimationOverlayElements;
        }
        const QList <QString> &getElementNames() const
        {
            return m_ElementNames;
        }

        /**
         * @brief Select a collimation overlay element and fill the field values in the element editor
         *        with the appropriate values of the selected collimation overlay element.
         * @param item collimation overlay element list item
         * @return true if collimation overlay element found
         */
        bool selectCollimationOverlayElement(QListWidgetItem *item);

        /**
         * @brief Select a collimation overlay element and fill the field values in the element editor
         *        with the appropriate values of the selected collimation overlay element.
         * @param name collimation overlay element name
         * @return true if collimation overlay element found
         */
        bool selectCollimationOverlayElement(const QString &name);

        /**
         * @brief Show the dialog and select a collimation overlay element for editing.
         * @param name collimation overlay element name
         */
        void openEditor();

        /**
         * @brief setCollimationOverlayElementValue Set specific field of collimation overlay element
         * @param name Name of collimation overlay element
         * @param field Name of element field
         * @param value Value of element field
         * @return True if set is successful, false otherwise.
         */
        bool setCollimationOverlayElementValue(const QString &name, const QString &field, const QVariant &value);

        /**
         * @brief Change the name of the currently selected collimation overlay element to a new value
         * @param name new element name
         */
        void renameCollimationOverlayElement(const QString &name);

        /**
         * @brief setCollimationOverlayElement Replaces collimation overlay element matching the name of the passed element.
         * @param train element information, including name and database id
         * @return True if element is successfully updated in the database.
         */
        bool setCollimationOverlayElement(const QJsonObject &element);

        /**
         * @brief removeCollimationOverlayElement Remove collimation overlay element from database and all associated settings
         * @param name name of the element to remove
         * @return True if successful, false if id is not found.
         */
        bool removeCollimationOverlayElement(const QString &name);

        void refreshModel();
        void refreshElements();

        /**
         * @brief syncValues Sync delegates and then update model accordingly.
         */
        void syncValues();

        /**
         * @brief id Get database ID for a given element
         * @param name Name of element
         * @return ID if exists, or -1 if not found.
         */
        int id(const QString &name) const;

        /**
         * @brief name Get database name for a given id
         * @param id database ID for the element to get
         * @return Element name, or empty string if not found.
         */
        QString name(int id) const;

    signals:
        void updated();

    protected:
        void initModel();

    private slots:
        /**
         * @brief Update a value in the currently selected element
         * @param cb combo box holding the new value
         * @param element value name
         */
        void updateValue(QComboBox *cb, const QString &valueName);
        /**
         * @brief Update a value in the currently selected element
         * @param value the new value
         * @param element element name
         */
        void updateValue(double value, const QString &valueName);
        void updateValue(int value, const QString &valueName);
        void updateValue(QColor value, const QString &valueName);
        void updateValue(QString value, const QString &valueName);

    private:

        CollimationOverlayOptions(QWidget *parent);
        static CollimationOverlayOptions *m_Instance;

        /**
         * @brief generateOpticalTrains Automatically generate optical trains based on the current profile information.
         * This happens when users use the tool for the first time.
         */
        void generateElement();

        /**
         * @brief Add a new collimation overlay element with the given name
         * @param name element name
         * @return unique element name
         */
        QString addElement(const QString &name);

        /**
         * @brief Create a unique element name
         * @param name original element name
         * @param type element type
         * @return name, eventually added (i) to make the element name unique
         */
        QString uniqueElementName(QString name, QString type);

        QList<QVariantMap> m_CollimationOverlayElements;
        QVariantMap *m_CurrentElement = nullptr;

        bool editing = false;

        // Table model
        QSqlTableModel *m_CollimationOverlayElementsModel = { nullptr };

        QList <QString> m_ElementNames;
};

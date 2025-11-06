/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>

namespace Ekos
{

/**
 * @class CollectionsDialog
 * @brief Dialog for browsing and loading pre-defined task collections
 */
class CollectionsDialog : public QDialog
{
        Q_OBJECT

    public:
        explicit CollectionsDialog(QWidget *parent = nullptr);
        ~CollectionsDialog() override;

        /** @brief Get the selected collection file path */
        QString selectedCollection() const
        {
            return m_selectedCollection;
        }

    private slots:
        void onCollectionSelected(QListWidgetItem *current, QListWidgetItem *previous);
        void onLoadClicked();

    private:
        void setupUI();
        void loadCollections();
        void displayCollectionDetails(const QString &filePath);

        QListWidget *m_collectionList = nullptr;
        QLabel *m_nameLabel = nullptr;
        QLabel *m_descriptionLabel = nullptr;
        QLabel *m_tasksLabel = nullptr;
        QPushButton *m_loadButton = nullptr;
        QPushButton *m_cancelButton = nullptr;

        QString m_selectedCollection;
        QMap<QString, QString> m_collectionFiles; // Display name -> file path
};

} // namespace Ekos

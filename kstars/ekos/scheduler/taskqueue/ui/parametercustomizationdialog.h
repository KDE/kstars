/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../tasktemplate.h"

#include <QDialog>
#include <QString>
#include <QMap>
#include <QVariant>

class QFormLayout;
class QWidget;
class QPushButton;
class QLabel;
class QScrollArea;

namespace Ekos
{

/**
 * @class ParameterCustomizationDialog
 * @brief Dialog for customizing template parameter values
 *
 * This dialog displays a form with appropriate input widgets for each
 * parameter in the template, allowing users to customize values before
 * adding the task to the queue.
 */
class ParameterCustomizationDialog : public QDialog
{
        Q_OBJECT

    public:
        explicit ParameterCustomizationDialog(QWidget *parent = nullptr);
        ~ParameterCustomizationDialog() override;

        /**
         * @brief Set the template to customize
         * @param tmpl Template whose parameters will be customized
         */
        void setTemplate(TaskTemplate *tmpl);

        /**
         * @brief Set the device name for display
         * @param deviceName Name of the device this task will run on
         */
        void setDeviceName(const QString &deviceName);

        /**
         * @brief Get the customized parameter values
         * @return Map of parameter name to customized value
         */
        QMap<QString, QVariant> parameterValues() const
        {
            return m_parameterValues;
        }

    protected:
        void accept() override;

    private:
        void setupUI();
        void populateParameters();
        QWidget* createParameterWidget(const TaskTemplate::Parameter &param);
        bool validateParameters(QString &errorMsg);

        TaskTemplate *m_template = nullptr;
        QString m_deviceName;
        QMap<QString, QVariant> m_parameterValues;

        // UI widgets
        QLabel *m_titleLabel = nullptr;
        QLabel *m_deviceLabel = nullptr;
        QScrollArea *m_scrollArea = nullptr;
        QWidget *m_formWidget = nullptr;
        QFormLayout *m_formLayout = nullptr;
        QPushButton *m_okButton = nullptr;
        QPushButton *m_cancelButton = nullptr;

        // Map parameter names to their input widgets
        QMap<QString, QWidget*> m_parameterWidgets;
};

} // namespace Ekos

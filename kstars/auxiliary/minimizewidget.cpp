/*
    SPDX-FileCopyrightText: 2025 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "minimizewidget.h"

#include <QDebug>

MinimizeWidget::MinimizeWidget(QWidget *parent) : QWidget(parent)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
}

void MinimizeWidget::setupUI(bool initiallyMinimized, void(*setOption)(bool))
{
    if (m_initialized)
        return;

    m_initialized = true;
    QList<QWidget*> childrenWidgets = findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);

    QWidget *minimizedWidget = nullptr;
    QWidget *maximizedWidget = nullptr;
    for (QWidget* child : childrenWidgets)
    {
        if (maximizedWidget == nullptr)
            maximizedWidget = child;
        else if (minimizedWidget == nullptr)
            minimizedWidget = child;
        else
        {
            fprintf(stderr, "unmanaged child \"%s\"\n", child->objectName().toLatin1().data());
            qDebug() << "MinimizeWidget: Found unmanaged child: " << child->objectName();
        }
    }

    if (minimizedWidget && maximizedWidget)
    {
        m_MaximizeButton = new QPushButton(this);
        m_MaximizeButton->setFixedWidth(24);
        m_MaximizeButton->setFixedHeight(24);
        m_MaximizeButton->setIcon(QIcon::fromTheme("window-maximize"));

        m_MinimizeButton = new QPushButton(this);
        m_MinimizeButton->setFixedWidth(24);
        m_MinimizeButton->setFixedHeight(24);
        m_MinimizeButton->setIcon(QIcon::fromTheme("window-minimize"));

        m_MinimizedWidget = new QWidget(this);
        auto minimizedLayout = new QHBoxLayout(m_MinimizedWidget);
        minimizedLayout->addWidget(m_MaximizeButton, 0, Qt::AlignTop);
        minimizedLayout->addWidget(minimizedWidget, 0, Qt::AlignTop);
        minimizedLayout->addStretch();
        m_MinimizedWidget->setVisible(m_isMinimized);
        m_mainLayout->addWidget(m_MinimizedWidget);

        m_MaximizedWidget = new QWidget(this);
        auto maximizedLayout = new QHBoxLayout(m_MaximizedWidget);
        maximizedLayout->addWidget(m_MinimizeButton, 0, Qt::AlignTop);
        maximizedLayout->addWidget(maximizedWidget, 1, Qt::AlignTop);
        maximizedLayout->addStretch();
        m_MaximizedWidget->setVisible(!m_isMinimized);
        m_mainLayout->addWidget(m_MaximizedWidget);

        connect(m_MaximizeButton, &QPushButton::clicked, this, &MinimizeWidget::maximize);
        connect(m_MinimizeButton, &QPushButton::clicked, this, &MinimizeWidget::minimize);
    }
    else
    {
        fprintf(stderr, "missing a child\n");
        qDebug() << "MinimizeWidget: WARNING: Could not find both a minimized and maximized widget child.";
    }

    setMinimized(initiallyMinimized);
    if (setOption)
    {
        connect(this, &MinimizeWidget::changed, [setOption](bool minimized)
        {
            setOption(minimized);
        });
    }
}

void MinimizeWidget::maximize()
{
    if (m_MinimizedWidget == nullptr || m_MaximizedWidget == nullptr)
        return;
    m_MinimizedWidget->setVisible(false);
    m_MaximizedWidget->setVisible(true);
    m_isMinimized = false;
    emit changed(false);
}

void MinimizeWidget::minimize()
{
    if (m_MinimizedWidget == nullptr || m_MaximizedWidget == nullptr)
        return;
    m_MaximizedWidget->setVisible(false);
    m_MinimizedWidget->setVisible(true);
    m_isMinimized = true;
    emit changed(true);
}

void MinimizeWidget::setMinimized(bool minimized)
{
    if (minimized)
        minimize();
    else
        maximize();
}


/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "queueviewerwidget.h"
#include "templatelibrarywidget.h"
#include "deviceselectiondialog.h"
#include "parametercustomizationdialog.h"
#include "collectionsdialog.h"
#include "../task.h"
#include "../tasktemplate.h"
#include "../templatemanager.h"
#include "indi/indilistener.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QCloseEvent>
#include <KLocalizedString>
#include <ekos_scheduler_debug.h>

namespace Ekos
{

QueueViewerWidget::QueueViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    // Set window flags to make this a standalone window
    setWindowFlags(Qt::Window);
    setWindowTitle(i18n("Task Queue Viewer"));

    // Set reasonable initial size
    resize(800, 600);

    setupUI();
}

QueueViewerWidget::~QueueViewerWidget()
{
}

void QueueViewerWidget::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Top toolbar
    auto *toolbarLayout = new QHBoxLayout();

    // Group 1: Template and collection management
    m_addTemplateButton = new QPushButton(QIcon::fromTheme("list-add"), i18n("Add from Template"), this);
    m_collectionsButton = new QPushButton(QIcon::fromTheme("folder-templates"), i18n("Load Collection"), this);
    toolbarLayout->addWidget(m_addTemplateButton);
    toolbarLayout->addWidget(m_collectionsButton);

    // Separator 1
    auto *separator1 = new QFrame(this);
    separator1->setFrameShape(QFrame::VLine);
    separator1->setFrameShadow(QFrame::Sunken);
    toolbarLayout->addWidget(separator1);

    // Group 2: Save/Load buttons (icons only)
    m_saveQueueButton = new QPushButton(QIcon::fromTheme("document-save"), QString(), this);
    m_saveQueueButton->setToolTip(i18n("Save"));
    m_saveAsQueueButton = new QPushButton(QIcon::fromTheme("document-save-as"), QString(), this);
    m_saveAsQueueButton->setToolTip(i18n("Save As"));
    m_loadQueueButton = new QPushButton(QIcon::fromTheme("document-open"), QString(), this);
    m_loadQueueButton->setToolTip(i18n("Load Queue"));
    toolbarLayout->addWidget(m_saveQueueButton);
    toolbarLayout->addWidget(m_saveAsQueueButton);
    toolbarLayout->addWidget(m_loadQueueButton);

    // Separator 2
    auto *separator2 = new QFrame(this);
    separator2->setFrameShape(QFrame::VLine);
    separator2->setFrameShadow(QFrame::Sunken);
    toolbarLayout->addWidget(separator2);

    // Stretch before right-aligned control buttons
    toolbarLayout->addStretch();

    // Group 3: Queue control buttons (icons only, right-aligned)
    m_startButton = new QPushButton(QIcon::fromTheme("media-playback-start"), QString(), this);
    m_startButton->setToolTip(i18n("Start"));
    m_pauseButton = new QPushButton(QIcon::fromTheme("media-playback-pause"), QString(), this);
    m_pauseButton->setToolTip(i18n("Pause"));
    m_pauseButton->setCheckable(true);
    m_stopButton = new QPushButton(QIcon::fromTheme("media-playback-stop"), QString(), this);
    m_stopButton->setToolTip(i18n("Stop"));
    m_clearButton = new QPushButton(QIcon::fromTheme("edit-clear"), QString(), this);
    m_clearButton->setToolTip(i18n("Clear"));

    toolbarLayout->addWidget(m_startButton);
    toolbarLayout->addWidget(m_pauseButton);
    toolbarLayout->addWidget(m_stopButton);
    toolbarLayout->addWidget(m_clearButton);

    mainLayout->addLayout(toolbarLayout);

    // Queue table
    m_queueTable = new QTableWidget(this);
    m_queueTable->setColumnCount(6);
    m_queueTable->setHorizontalHeaderLabels(
    {
        i18n("Status"),
        i18n("Task"),
        i18n("Device"),
        i18n("Actions"),
        i18n("Progress"),
        i18n("Created")
    });

    m_queueTable->horizontalHeader()->setStretchLastSection(false);
    m_queueTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_queueTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_queueTable->setAlternatingRowColors(true);
    m_queueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    mainLayout->addWidget(m_queueTable);

    // Item controls
    auto *itemControlsLayout = new QHBoxLayout();

    m_removeButton = new QPushButton(QIcon::fromTheme("list-remove"), i18n("Remove"), this);
    m_moveUpButton = new QPushButton(QIcon::fromTheme("arrow-up"), i18n("Move Up"), this);
    m_moveDownButton = new QPushButton(QIcon::fromTheme("arrow-down"), i18n("Move Down"), this);

    itemControlsLayout->addWidget(m_removeButton);
    itemControlsLayout->addWidget(m_moveUpButton);
    itemControlsLayout->addWidget(m_moveDownButton);
    itemControlsLayout->addStretch();

    mainLayout->addLayout(itemControlsLayout);

    // Status bar
    m_statusBar = new QStatusBar(this);
    m_statusLabel = new QLabel(i18n("Queue idle"));
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumWidth(200);
    m_statsLabel = new QLabel();

    m_statusBar->addWidget(m_statusLabel, 1);  // Stretch factor 1
    m_statusBar->addPermanentWidget(m_progressBar);
    m_statusBar->addPermanentWidget(m_statsLabel);

    mainLayout->addWidget(m_statusBar);

    // Connect button signals
    connect(m_addTemplateButton, &QPushButton::clicked, this, &QueueViewerWidget::onAddFromTemplate);
    connect(m_collectionsButton, &QPushButton::clicked, this, &QueueViewerWidget::onLoadCollection);
    connect(m_saveQueueButton, &QPushButton::clicked, this, &QueueViewerWidget::onSaveQueue);
    connect(m_saveAsQueueButton, &QPushButton::clicked, this, &QueueViewerWidget::onSaveAsQueue);
    connect(m_loadQueueButton, &QPushButton::clicked, this, &QueueViewerWidget::onLoadQueue);
    connect(m_startButton, &QPushButton::clicked, this, &QueueViewerWidget::onStartQueue);
    connect(m_pauseButton, &QPushButton::clicked, this, &QueueViewerWidget::onPauseQueue);
    connect(m_stopButton, &QPushButton::clicked, this, &QueueViewerWidget::onStopQueue);
    connect(m_clearButton, &QPushButton::clicked, this, &QueueViewerWidget::onClearQueue);
    connect(m_removeButton, &QPushButton::clicked, this, &QueueViewerWidget::onRemoveItem);
    connect(m_moveUpButton, &QPushButton::clicked, this, &QueueViewerWidget::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &QueueViewerWidget::onMoveDown);

    connect(m_queueTable, &QTableWidget::itemSelectionChanged,
            this, &QueueViewerWidget::onSelectionChanged);
    connect(m_queueTable, &QTableWidget::cellDoubleClicked,
            this, &QueueViewerWidget::onItemDoubleClicked);

    updateControls();
}

void QueueViewerWidget::setQueueManager(QueueManager *manager)
{
    if (m_manager)
    {
        disconnect(m_manager, nullptr, this, nullptr);
    }

    m_manager = manager;

    if (m_manager)
    {
        connect(m_manager, &QueueManager::itemAdded, this, &QueueViewerWidget::onItemAdded);
        connect(m_manager, &QueueManager::itemRemoved, this, &QueueViewerWidget::onItemRemoved);
        connect(m_manager, &QueueManager::itemMoved, this, &QueueViewerWidget::onItemMoved);
        connect(m_manager, &QueueManager::queueCleared, this, &QueueViewerWidget::onQueueCleared);
        connect(m_manager, &QueueManager::stateChanged, this, &QueueViewerWidget::onStateChanged);

        refreshQueue();
    }
}

void QueueViewerWidget::setQueueExecutor(QueueExecutor *executor)
{
    if (m_executor)
    {
        disconnect(m_executor, nullptr, this, nullptr);
    }

    m_executor = executor;

    if (m_executor)
    {
        connect(m_executor, &QueueExecutor::started, this, &QueueViewerWidget::onExecutorStarted);
        connect(m_executor, &QueueExecutor::paused, this, &QueueViewerWidget::onExecutorPaused);
        connect(m_executor, &QueueExecutor::stopped, this, &QueueViewerWidget::onExecutorStopped);
        connect(m_executor, &QueueExecutor::completed, this, &QueueViewerWidget::onExecutorCompleted);
        connect(m_executor, &QueueExecutor::itemStarted, this, &QueueViewerWidget::onItemStarted);
        connect(m_executor, &QueueExecutor::itemCompleted, this, &QueueViewerWidget::onItemCompleted);
        connect(m_executor, &QueueExecutor::itemFailed, this, &QueueViewerWidget::onItemFailed);
        connect(m_executor, &QueueExecutor::progress, this, &QueueViewerWidget::onProgress);
    }

    updateControls();
}

void QueueViewerWidget::refreshQueue()
{
    if (!m_manager)
        return;

    m_queueTable->setRowCount(0);

    for (int i = 0; i < m_manager->count(); ++i)
    {
        QueueItem *item = m_manager->itemAt(i);
        if (item)
        {
            m_queueTable->insertRow(i);
            updateItemRow(i, item);
        }
    }

    updateControls();

    // Update stats
    if (m_statsLabel)
    {
        m_statsLabel->setText(i18n("Total: %1  Completed: %2  Failed: %3  Pending: %4",
                                   m_manager->count(),
                                   m_manager->completedCount(),
                                   m_manager->failedCount(),
                                   m_manager->pendingCount()));
    }
}

void QueueViewerWidget::updateItemRow(int row, QueueItem *item)
{
    if (!item || row >= m_queueTable->rowCount())
        return;

    // Status icon
    auto *statusItem = new QTableWidgetItem();
    statusItem->setIcon(statusToIcon(item->status()));
    statusItem->setToolTip(statusToString(item->status()));
    m_queueTable->setItem(row, 0, statusItem);

    // Task name
    QString taskName = item->task() ? item->task()->name() : i18n("Unknown");
    m_queueTable->setItem(row, 1, new QTableWidgetItem(taskName));

    // Device name
    m_queueTable->setItem(row, 2, new QTableWidgetItem(item->device()));

    // Action count
    int actionCount = item->task() ? item->task()->actionCount() : 0;
    m_queueTable->setItem(row, 3, new QTableWidgetItem(QString::number(actionCount)));

    // Progress
    m_queueTable->setItem(row, 4, new QTableWidgetItem(QString("%1%").arg(item->progress())));

    // Created time
    QString createdTime = item->createdTime().toString("yyyy-MM-dd hh:mm");
    m_queueTable->setItem(row, 5, new QTableWidgetItem(createdTime));
}

void QueueViewerWidget::onStartQueue()
{
    if (m_executor && !m_executor->isRunning())
    {
        if (!m_executor->start())
        {
            QMessageBox::warning(this, i18n("Queue Start Failed"),
                                 i18n("Failed to start queue execution. Check that queue has items."));
        }
    }
}

void QueueViewerWidget::onPauseQueue()
{
    if (!m_executor)
        return;

    if (m_pauseButton->isChecked())
    {
        m_executor->pause();
    }
    else
    {
        m_executor->resume();
    }
}

void QueueViewerWidget::onStopQueue()
{
    if (m_executor && m_executor->isRunning())
    {
        m_executor->stop();
    }
}

void QueueViewerWidget::onClearQueue()
{
    if (!m_manager)
        return;

    m_manager->clear();
    setModified(true);
}

void QueueViewerWidget::onRemoveItem()
{
    if (!m_manager)
        return;

    int row = m_queueTable->currentRow();
    if (row >= 0)
    {
        m_manager->removeItem(row);
        setModified(true);
    }
}

void QueueViewerWidget::onMoveUp()
{
    if (!m_manager)
        return;

    int row = m_queueTable->currentRow();
    if (row > 0)
    {
        m_manager->moveUp(row);
        m_queueTable->selectRow(row - 1);
        setModified(true);
    }
}

void QueueViewerWidget::onMoveDown()
{
    if (!m_manager)
        return;

    int row = m_queueTable->currentRow();
    if (row >= 0 && row < m_manager->count() - 1)
    {
        m_manager->moveDown(row);
        m_queueTable->selectRow(row + 1);
        setModified(true);
    }
}

void QueueViewerWidget::onItemDoubleClicked(int row, int /*column*/)
{
    if (!m_manager || !m_executor)
        return;

    // Don't allow editing while queue is running
    if (m_executor->isRunning())
    {
        QMessageBox::information(this, i18n("Queue Running"),
                                 i18n("Cannot edit tasks while queue is running. Please stop the queue first."));
        return;
    }

    // Get the queue item
    QueueItem *item = m_manager->itemAt(row);
    if (!item || !item->task())
        return;

    Task *task = item->task();

    // Get template manager
    TemplateManager *templateMgr = TemplateManager::Instance();
    if (!templateMgr)
    {
        QMessageBox::critical(this, i18n("Error"),
                              i18n("Template manager not available"));
        return;
    }

    // Initialize template manager if not already done
    if (templateMgr->allTemplates().isEmpty())
    {
        if (!templateMgr->initialize())
        {
            QMessageBox::critical(this, i18n("Error"),
                                  i18n("Failed to initialize template library"));
            return;
        }
    }

    // Find the template
    TaskTemplate *tmpl = templateMgr->getTemplate(task->templateId());
    if (!tmpl)
    {
        QMessageBox::warning(this, i18n("Template Not Found"),
                             i18n("The template for this task is no longer available.\n"
             "Template ID: %1", task->templateId()));
        return;
    }

    // Open parameter customization dialog with current values
    ParameterCustomizationDialog paramDlg(this);
    paramDlg.setTemplate(tmpl);
    paramDlg.setDeviceName(task->device());
    paramDlg.setInitialParameterValues(task->parameters());

    if (paramDlg.exec() != QDialog::Accepted)
        return;

    // Get updated parameters
    QMap<QString, QVariant> newParams = paramDlg.parameterValues();

    // Re-instantiate the task with new parameters
    Task *newTask = new Task(this);
    if (!newTask->instantiateFromTemplate(tmpl, task->device(), newParams))
    {
        QMessageBox::warning(this, i18n("Task Update Failed"),
                             i18n("Failed to update task with new parameters"));
        delete newTask;
        return;
    }

    // Replace the task in the queue item
    item->setTask(newTask);

    // Update the row display
    updateItemRow(row, item);

    // Mark queue as modified
    setModified(true);

    m_statusLabel->setText(i18n("Task updated"));
}

void QueueViewerWidget::onItemAdded(QueueItem *item, int index)
{
    m_queueTable->insertRow(index);
    updateItemRow(index, item);
    updateControls();
    setModified(true);
}

void QueueViewerWidget::onItemRemoved(QueueItem * /*item*/, int index)
{
    m_queueTable->removeRow(index);
    updateControls();
    setModified(true);
}

void QueueViewerWidget::onItemMoved(int fromIndex, int toIndex)
{
    if (!m_manager)
        return;

    // Refresh the rows that were affected by the move
    int minIndex = qMin(fromIndex, toIndex);
    int maxIndex = qMax(fromIndex, toIndex);

    for (int i = minIndex; i <= maxIndex; ++i)
    {
        QueueItem *item = m_manager->itemAt(i);
        if (item)
            updateItemRow(i, item);
    }

    updateControls();
    setModified(true);
}

void QueueViewerWidget::onQueueCleared()
{
    m_queueTable->setRowCount(0);
    updateControls();
    setModified(true);
}

void QueueViewerWidget::onStateChanged(QueueManager::QueueState state)
{
    QString stateStr;
    switch (state)
    {
        case QueueManager::IDLE:
            stateStr = i18n("Queue idle");
            break;
        case QueueManager::RUNNING:
            stateStr = i18n("Queue running");
            break;
        case QueueManager::PAUSED:
            stateStr = i18n("Queue paused");
            break;
        case QueueManager::COMPLETED:
            stateStr = i18n("Queue completed");
            break;
        case QueueManager::ABORTED:
            stateStr = i18n("Queue aborted");
            break;
    }

    m_statusLabel->setText(stateStr);
    updateControls();
}

void QueueViewerWidget::onExecutorStarted()
{
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    updateControls();
}

void QueueViewerWidget::onExecutorPaused()
{
    updateControls();
}

void QueueViewerWidget::onExecutorStopped()
{
    m_progressBar->setVisible(false);
    m_pauseButton->setChecked(false);
    updateControls();
}

void QueueViewerWidget::onExecutorCompleted()
{
    m_progressBar->setVisible(false);
    m_statusLabel->setText(i18n("Queue completed successfully"));
    refreshQueue();
}

void QueueViewerWidget::onItemStarted(QueueItem *item)
{
    if (!m_manager)
        return;

    int index = m_manager->indexOf(item);
    if (index >= 0)
    {
        updateItemRow(index, item);
    }
}

void QueueViewerWidget::onItemCompleted(QueueItem *item)
{
    if (!m_manager)
        return;

    int index = m_manager->indexOf(item);
    if (index >= 0)
    {
        updateItemRow(index, item);
    }
}

void QueueViewerWidget::onItemFailed(QueueItem *item, const QString &error)
{
    if (!m_manager)
        return;

    int index = m_manager->indexOf(item);
    if (index >= 0)
    {
        updateItemRow(index, item);
    }

    m_statusLabel->setText(i18n("Item failed: %1", error));
}

void QueueViewerWidget::onProgress(int percent, const QString &message)
{
    m_progressBar->setValue(percent);
    if (!message.isEmpty())
    {
        m_statusLabel->setText(message);
    }
}

void QueueViewerWidget::onSelectionChanged()
{
    updateControls();
}

void QueueViewerWidget::updateControls()
{
    bool hasManager = (m_manager != nullptr);
    bool hasExecutor = (m_executor != nullptr);
    bool hasItems = hasManager && m_manager->count() > 0;
    bool isRunning = hasExecutor && m_executor->isRunning();
    int selectedRow = m_queueTable->currentRow();
    bool hasSelection = selectedRow >= 0;

    // Queue controls
    m_startButton->setEnabled(hasManager && hasExecutor && hasItems && !isRunning);
    m_pauseButton->setEnabled(hasExecutor && isRunning);
    m_stopButton->setEnabled(hasExecutor && isRunning);
    m_clearButton->setEnabled(hasManager && hasItems && !isRunning);

    // Item controls
    m_removeButton->setEnabled(hasManager && hasSelection && !isRunning);
    m_moveUpButton->setEnabled(hasManager && hasSelection && selectedRow > 0 && !isRunning);
    m_moveDownButton->setEnabled(hasManager && hasSelection &&
                                 selectedRow < m_manager->count() - 1 && !isRunning);
}

QString QueueViewerWidget::statusToString(QueueItem::Status status) const
{
    switch (status)
    {
        case QueueItem::PENDING:
            return i18n("Pending");
        case QueueItem::SCHEDULED:
            return i18n("Scheduled");
        case QueueItem::RUNNING:
            return i18n("Running");
        case QueueItem::COMPLETED:
            return i18n("Completed");
        case QueueItem::FAILED:
            return i18n("Failed");
        case QueueItem::SKIPPED:
            return i18n("Skipped");
        default:
            return i18n("Unknown");
    }
}

QIcon QueueViewerWidget::statusToIcon(QueueItem::Status status) const
{
    switch (status)
    {
        case QueueItem::PENDING:
        case QueueItem::SCHEDULED:
            return QIcon::fromTheme("clock");
        case QueueItem::RUNNING:
            return QIcon::fromTheme("media-playback-start");
        case QueueItem::COMPLETED:
            return QIcon::fromTheme("dialog-ok");
        case QueueItem::FAILED:
            return QIcon::fromTheme("dialog-error");
        case QueueItem::SKIPPED:
            return QIcon::fromTheme("dialog-warning");
        default:
            return QIcon::fromTheme("help-about");
    }
}

void QueueViewerWidget::onAddFromTemplate()
{
    // Create template library widget if needed
    if (!m_templateLibrary)
    {
        m_templateLibrary = new TemplateLibraryWidget(this);
        connect(m_templateLibrary, &TemplateLibraryWidget::addTemplateRequested,
                this, &QueueViewerWidget::onTemplateSelected);
    }

    // Show template library as dialog
    m_templateLibrary->refreshLibrary();
    m_templateLibrary->show();
    m_templateLibrary->raise();
}

void QueueViewerWidget::onTemplateSelected(TaskTemplate *tmpl)
{
    if (!tmpl || !m_manager)
        return;

    // Hide template library
    if (m_templateLibrary)
        m_templateLibrary->hide();

    QString selectedDevice;

    // Step 1: Device Selection (only if template requires a device)
    if (!tmpl->supportedInterfaces().isEmpty())
    {
        DeviceSelectionDialog deviceDlg(this);
        deviceDlg.setTemplate(tmpl);
        // populateDevices() is called automatically by setTemplate()

        if (deviceDlg.exec() != QDialog::Accepted)
            return;

        selectedDevice = deviceDlg.selectedDevice();
        if (selectedDevice.isEmpty())
            return;
    }

    QMap<QString, QVariant> params;

    // Step 2: Parameter Customization
    // Skip for user templates since they already have saved parameter values
    if (tmpl->isSystemTemplate())
    {
        // System templates: show parameter customization dialog
        ParameterCustomizationDialog paramDlg(this);
        paramDlg.setTemplate(tmpl);
        paramDlg.setDeviceName(selectedDevice);

        if (paramDlg.exec() != QDialog::Accepted)
            return;

        params = paramDlg.parameterValues();
    }
    else
    {
        // User templates: use default parameter values from template
        QList<TaskTemplate::Parameter> templateParams = tmpl->parameters();
        for (const TaskTemplate::Parameter &param : templateParams)
        {
            params[param.name] = param.defaultValue;
        }
    }

    // Step 3: Create and add task to queue
    Task *task = new Task(this);
    if (!task->instantiateFromTemplate(tmpl, selectedDevice, params))
    {
        QMessageBox::warning(this, i18n("Task Creation Failed"),
                             i18n("Failed to create task from template"));
        delete task;
        return;
    }

    QueueItem *item = new QueueItem(task, this);
    m_manager->addItem(item);
    setModified(true);
}

void QueueViewerWidget::onSaveQueue()
{
    if (!m_manager)
        return;

    if (m_manager->count() == 0)
    {
        QMessageBox::warning(this, i18n("Empty Queue"),
                             i18n("Queue is empty. Nothing to save."));
        return;
    }

    // If we have a saved file path, save directly without dialog
    if (!m_lastQueueFilePath.isEmpty())
    {
        if (m_manager->saveQueue(m_lastQueueFilePath))
        {
            setModified(false);
        }
        else
        {
            QMessageBox::critical(this, i18n("Error"),
                                  i18n("Failed to save queue to %1", m_lastQueueFilePath));
        }
    }
    else
    {
        // No file path yet, use Save As
        onSaveAsQueue();
    }
}

void QueueViewerWidget::onSaveAsQueue()
{
    if (!m_manager)
        return;

    if (m_manager->count() == 0)
    {
        QMessageBox::warning(this, i18n("Empty Queue"),
                             i18n("Queue is empty. Nothing to save."));
        return;
    }

    // Use last saved/loaded path if available, otherwise default to Documents directory
    QString defaultPath;
    if (!m_lastQueueFilePath.isEmpty())
    {
        defaultPath = m_lastQueueFilePath;
    }
    else
    {
        QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        defaultPath = documentsPath + "/my_queue.kstarsqueue";
    }

    QString filePath = QFileDialog::getSaveFileName(this,
                       i18n("Save Queue As"),
                       defaultPath,
                       i18n("KStars Queue Files (*.kstarsqueue);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    if (m_manager->saveQueue(filePath))
    {
        // Remember the last used file path for next time
        m_lastQueueFilePath = filePath;
        // Mark as not modified after successful save
        setModified(false);
    }
    else
    {
        QMessageBox::critical(this, i18n("Error"),
                              i18n("Failed to save queue to %1", filePath));
    }
}

void QueueViewerWidget::onLoadQueue()
{
    if (!m_manager)
        return;

    // Check for unsaved changes first
    if (!promptToSave())
        return;

    // Check if queue has items and confirm
    if (m_manager->count() > 0)
    {
        auto reply = QMessageBox::question(this, i18n("Clear Existing Queue?"),
                                           i18n("Loading a queue will clear the current queue. Continue?"),
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }

    // Use last saved/loaded path if available, otherwise default to Documents directory
    QString defaultPath;
    if (!m_lastQueueFilePath.isEmpty())
    {
        defaultPath = m_lastQueueFilePath;
    }
    else
    {
        QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        defaultPath = documentsPath;
    }

    QString filePath = QFileDialog::getOpenFileName(this,
                       i18n("Load Queue"),
                       defaultPath,
                       i18n("KStars Queue Files (*.kstarsqueue);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    if (m_manager->loadQueue(filePath))
    {
        // Remember the last used file path for next time
        m_lastQueueFilePath = filePath;
        refreshQueue();
        // Mark as not modified after successful load
        setModified(false);
    }
    else
    {
        QMessageBox::critical(this, i18n("Error"),
                              i18n("Failed to load queue from %1", filePath));
    }
}

void QueueViewerWidget::onLoadCollection()
{
    if (!m_manager)
        return;

    CollectionsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString collectionPath = dialog.selectedCollection();
        if (!collectionPath.isEmpty())
        {
            loadCollectionFile(collectionPath);
        }
    }
}

QStringList QueueViewerWidget::findDevicesByInterface(uint32_t interfaceMask)
{
    QStringList deviceNames;

    // Use INDIListener to get devices by interface
    auto devices = INDIListener::devicesByInterface(interfaceMask);

    for (const auto &device : devices)
    {
        if (device && device->isConnected())
        {
            deviceNames.append(device->getDeviceName());
        }
    }

    return deviceNames;
}

void QueueViewerWidget::loadCollectionFile(const QString &filePath)
{
    if (!m_manager)
        return;

    // Read collection file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(this, i18n("Error"),
                              i18n("Failed to open collection file: %1", filePath));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
    {
        QMessageBox::critical(this, i18n("Error"),
                              i18n("Invalid collection file format"));
        return;
    }

    QJsonObject collectionObj = doc.object();
    QString collectionName = collectionObj["name"].toString();
    QJsonArray tasksArray = collectionObj["tasks"].toArray();

    if (tasksArray.isEmpty())
    {
        QMessageBox::warning(this, i18n("Empty Collection"),
                             i18n("The collection contains no tasks"));
        return;
    }

    // Get template manager and ensure it's initialized
    TemplateManager *templateMgr = TemplateManager::Instance();
    if (!templateMgr)
    {
        QMessageBox::critical(this, i18n("Error"),
                              i18n("Template manager not available"));
        return;
    }

    // Initialize template manager if not already done
    if (templateMgr->allTemplates().isEmpty())
    {
        if (!templateMgr->initialize())
        {
            QMessageBox::critical(this, i18n("Error"),
                                  i18n("Failed to initialize template library"));
            return;
        }
    }

    int tasksAdded = 0;
    int tasksFailed = 0;
    QStringList warnings;

    // Process each task in the collection
    for (const QJsonValue &taskVal : tasksArray)
    {
        QJsonObject taskObj = taskVal.toObject();
        QString templateId = taskObj["template_id"].toString();
        QString deviceName = taskObj["device"].toString();
        QJsonObject paramsObj = taskObj["parameters"].toObject();

        // Find the template
        TaskTemplate *tmpl = templateMgr->getTemplate(templateId);
        if (!tmpl)
        {
            warnings.append(i18n("Template not found: %1", templateId));
            tasksFailed++;
            continue;
        }

        // Convert parameters from JSON to QMap
        QMap<QString, QVariant> params;
        for (const QString &key : paramsObj.keys())
        {
            params[key] = paramsObj[key].toVariant();
        }

        // Load task with device name from collection (empty string means dynamic assignment at execution)
        Task *task = new Task(this);
        if (task->instantiateFromTemplate(tmpl, deviceName, params))
        {
            QueueItem *item = new QueueItem(task, this);
            m_manager->addItem(item);
            tasksAdded++;
        }
        else
        {
            delete task;
            warnings.append(i18n("Failed to create task from template: %1", tmpl->name()));
            tasksFailed++;
        }
    }

    // Show results
    QString message;
    if (tasksAdded == 0)
    {
        message = i18n("No tasks were added from collection '%1'.", collectionName);
    }

    if (tasksFailed > 0)
    {
        message += i18n("\n\n%1 tasks failed to load.", tasksFailed);
    }

    if (!warnings.isEmpty())
    {
        message += i18n("\n\nWarnings:\n") + warnings.join("\n");
    }

    if (!message.isEmpty())
        QMessageBox::information(this, i18n("Collection Loaded"), message);
    refreshQueue();

    // Mark as modified after loading collection (user may want to save combined queue)
    if (tasksAdded > 0)
        setModified(true);
}

void QueueViewerWidget::setModified(bool modified)
{
    if (m_isModified == modified)
        return;

    m_isModified = modified;

    // Update window title to show modified state
    QString title = i18n("Task Queue Viewer");
    if (m_isModified)
    {
        title += " *";
    }
    setWindowTitle(title);
}

bool QueueViewerWidget::promptToSave()
{
    if (!m_isModified || !m_manager || m_manager->count() == 0)
        return true;  // No need to save or nothing to save

    QMessageBox::StandardButton reply = QMessageBox::question(
                                            this,
                                            i18n("Save Changes?"),
                                            i18n("The queue has unsaved changes. Do you want to save them?"),
                                            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                            QMessageBox::Save
                                        );

    if (reply == QMessageBox::Save)
    {
        onSaveQueue();
        // Check if save was successful (user might have cancelled)
        return !m_isModified;  // If still modified, save was cancelled
    }
    else if (reply == QMessageBox::Discard)
    {
        return true;
    }
    else  // Cancel
    {
        return false;
    }
}

void QueueViewerWidget::closeEvent(QCloseEvent *event)
{
    // Prompt to save if there are unsaved changes
    if (!promptToSave())
    {
        event->ignore();
        return;
    }

    QWidget::closeEvent(event);
}

} // namespace Ekos

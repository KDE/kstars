/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsekos.h"

#include "manager.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "mcp/mcpserver.h"
#include "mcp/mcptoolregistry.h"

#include <KConfigDialog>
#include <KLed>
#include <KLocalizedString>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QIcon>
#include <QMap>
#include <QTimer>
#include <QTreeWidgetItem>

OpsEkos::OpsEkos() : QTabWidget(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("settings");

    connect(kcfg_EkosTopIcons, &QRadioButton::toggled, this, [this]()
    {
        if (Options::ekosTopIcons() != kcfg_EkosTopIcons->isChecked())
            KSNotification::info(i18n("You must restart KStars for this change to take effect."));
    });


    connect(analyzeAlternativeDirectoryB, &QPushButton::clicked, [this] ()
    {
        auto dir = QFileDialog::getExistingDirectory(
                       this, i18n("Set an alternate base directory for Analyze's captured images"),
                       QDir::homePath(),
                       QFileDialog::ShowDirsOnly);
        if (!dir.isEmpty())
            kcfg_AnalyzeAlternativeDirectoryName->setText(dir);
    });
    connect(kcfg_AnalyzeAlternativeDirectoryName, &QLineEdit::editingFinished, [this] ()
    {
        auto text = kcfg_AnalyzeAlternativeDirectoryName->text();

        QFileInfo newdir(text);
        if (text.size() > 0 && newdir.exists() && newdir.isDir())
            kcfg_AnalyzeAlternativeDirectoryName->setText(text);
    });

    // MCP: keep port spinner enabled only when the server is enabled
    connect(kcfg_MCPEnabled, &QCheckBox::toggled, kcfg_MCPPort, &QSpinBox::setEnabled);
    kcfg_MCPPort->setEnabled(kcfg_MCPEnabled->isChecked());

    // Live enable/disable the MCP server when the checkbox is toggled in the dialog
    connect(kcfg_MCPEnabled, &QCheckBox::toggled, this, [this](bool enabled)
    {
        auto *mgr = Ekos::Manager::Instance();
        if (!mgr)
            return;
        if (enabled)
        {
            mgr->ensureMCPServer();
            // First-time enable creates the server; wire token signals now so
            // subsequent regenerate/start calls update the UI live.
            if (auto *server = mgr->mcpServer())
            {
                connect(server, &MCP::Server::tokenChanged,
                        this, &OpsEkos::refreshMCPTokens, Qt::UniqueConnection);
                connect(server, &MCP::Server::readOnlyTokenChanged,
                        this, &OpsEkos::refreshMCPTokens, Qt::UniqueConnection);
            }
            refreshMCPTokens();
            populateMCPTools();
        }
        else if (mgr->mcpServer())
        {
            mgr->mcpServer()->stop();
            mgr->updateMCPStatusLabel();
        }
    });

    // Restart on the new port when the user applies settings changes
    if (m_ConfigDialog)
    {
        connect(m_ConfigDialog, &KConfigDialog::settingsChanged, this, [this]()
        {
            auto *mgr = Ekos::Manager::Instance();
            if (!mgr || !mgr->mcpServer())
                return;
            if (Options::mCPEnabled() && mgr->mcpServer()->isListening()
                    && mgr->mcpServer()->port() != static_cast<quint16>(Options::mCPPort()))
            {
                if (!mgr->mcpServer()->restart(Options::mCPPort()))
                    setMCPState(false, 0, i18n("Failed: port in use"));
                else
                    mgr->updateMCPStatusLabel();
            }
        });
    }

    // Populate token fields from the server's in-memory cache. The server only
    // exists once MCP has been enabled at least once (Manager::ensureMCPServer);
    // if it's not up yet, fields stay empty until the user toggles Enable.
    refreshMCPTokens();
    // Safe to connect before the first populate — populateMCPTools() blocks
    // the tree's signals while it rebuilds.
    connect(mcpToolsTree, &QTreeWidget::itemChanged, this, &OpsEkos::onMCPToolItemChanged);
    populateMCPTools();
    if (auto *mgr = Ekos::Manager::Instance())
    {
        if (auto *server = mgr->mcpServer())
        {
            connect(server, &MCP::Server::tokenChanged,          this, &OpsEkos::refreshMCPTokens);
            connect(server, &MCP::Server::readOnlyTokenChanged,  this, &OpsEkos::refreshMCPTokens);
        }
    }

    // Toggle token visibility
    connect(mcpShowTokenButton, &QPushButton::toggled, this, [this](bool shown)
    {
        mcpTokenEdit->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
        mcpShowTokenButton->setIcon(QIcon::fromTheme(shown ? "password-show-off" : "password-show-on"));
        mcpShowTokenButton->setToolTip(shown ? i18n("Hide token") : i18n("Show token"));
    });

    // Copy token to clipboard
    connect(mcpCopyTokenButton, &QPushButton::clicked, this, [this]()
    {
        QApplication::clipboard()->setText(mcpTokenEdit->text());
    });

    // Regenerate token
    connect(mcpRegenTokenButton, &QPushButton::clicked, this, []()
    {
        auto *mgr = Ekos::Manager::Instance();
        if (mgr && mgr->mcpServer())
            mgr->mcpServer()->regenerateToken();
        // tokenChanged signal triggers refreshMCPTokens automatically.
    });

    // Toggle read-only token visibility
    connect(mcpShowROTokenButton, &QPushButton::toggled, this, [this](bool shown)
    {
        mcpROTokenEdit->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
        mcpShowROTokenButton->setIcon(QIcon::fromTheme(shown ? "password-show-off" : "password-show-on"));
        mcpShowROTokenButton->setToolTip(shown ? i18n("Hide token") : i18n("Show token"));
    });

    // Copy read-only token to clipboard
    connect(mcpCopyROTokenButton, &QPushButton::clicked, this, [this]()
    {
        QApplication::clipboard()->setText(mcpROTokenEdit->text());
    });

    // Regenerate read-only token (lazy: generate on first click)
    connect(mcpRegenROTokenButton, &QPushButton::clicked, this, []()
    {
        auto *mgr = Ekos::Manager::Instance();
        if (mgr && mgr->mcpServer())
            mgr->mcpServer()->regenerateReadOnlyToken();
        // readOnlyTokenChanged signal triggers refreshMCPTokens automatically.
    });
}

void OpsEkos::refreshMCPTokens()
{
    auto *mgr = Ekos::Manager::Instance();
    if (!mgr || !mgr->mcpServer())
    {
        mcpTokenEdit->clear();
        mcpROTokenEdit->clear();
        return;
    }
    mcpTokenEdit->setText(mgr->mcpServer()->token());
    mcpROTokenEdit->setText(mgr->mcpServer()->readOnlyToken());
}

void OpsEkos::populateMCPTools()
{
    // Rebuilding the tree fires itemChanged for every checkbox we set; block
    // signals so onMCPToolItemChanged doesn't run on our own writes.
    QSignalBlocker blocker(mcpToolsTree);

    mcpToolsTree->clear();

    auto *mgr = Ekos::Manager::Instance();
    if (!mgr || !mgr->mcpServer() || !mgr->mcpServer()->registry())
    {
        // Show a placeholder so the empty tree doesn't look broken.
        auto *placeholder = new QTreeWidgetItem(mcpToolsTree);
        placeholder->setText(0, i18n("(enable the MCP server to populate)"));
        placeholder->setFirstColumnSpanned(true);
        return;
    }

    // Group tools by family prefix — the substring before the first underscore
    // ("mount_coords" → "mount"). One top-level item per family, tool leaves
    // sorted by name underneath.
    QMap<QString, QTreeWidgetItem *> familyRoots;
    const QStringList disabled = Options::mCPDisabledTools();
    const auto &tools = mgr->mcpServer()->registry()->tools();
    for (const auto &t : tools)
    {
        const int sep = t.name.indexOf(QLatin1Char('_'));
        const QString family = (sep > 0) ? t.name.left(sep) : QStringLiteral("(other)");

        QTreeWidgetItem *root = familyRoots.value(family, nullptr);
        if (!root)
        {
            root = new QTreeWidgetItem(mcpToolsTree);
            root->setText(0, family);
            root->setExpanded(false);
            // Tristate checkbox: toggling the family toggles every child,
            // mixed children render as partially checked.
            root->setFlags(root->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
            familyRoots.insert(family, root);
        }

        // Leaves show the name without the redundant family prefix
        // ("mount_coords" → "coords"); the full tool name stays available in
        // UserRole and in the tooltip.
        auto *leaf = new QTreeWidgetItem(root);
        leaf->setText(0, sep > 0 ? t.name.mid(sep + 1) : t.name);
        leaf->setData(0, Qt::UserRole, t.name);
        leaf->setCheckState(0, disabled.contains(t.name) ? Qt::Unchecked : Qt::Checked);
        leaf->setText(1, t.description);

        // Read-only annotation: lock icon for safe tools, gear for mutating
        // ones. Tooltip lists every MCP annotation flag so power users can see
        // destructive / idempotent / openWorld at a glance.
        leaf->setIcon(0, QIcon::fromTheme(t.readOnly
                                          ? QStringLiteral("object-locked")
                                          : QStringLiteral("configure")));
        QStringList flags;
        flags << (t.readOnly    ? i18n("read-only")    : i18n("mutating"));
        if (t.destructive) flags << i18n("destructive");
        if (t.idempotent)  flags << i18n("idempotent");
        if (t.openWorld)   flags << i18n("open-world");
        leaf->setToolTip(0, QStringLiteral("%1 — %2").arg(t.name, flags.join(QStringLiteral(", "))));
        leaf->setToolTip(1, t.description);
    }

    // Update the family count suffix and checkbox once all leaves are placed.
    // The root state is set explicitly because auto-tristate aggregation does
    // not run while the tree's signals are blocked.
    for (auto it = familyRoots.constBegin(); it != familyRoots.constEnd(); ++it)
    {
        QTreeWidgetItem *root = it.value();
        root->setText(0, QStringLiteral("%1 (%2)").arg(it.key()).arg(root->childCount()));

        int uncheckedCount = 0;
        for (int i = 0; i < root->childCount(); ++i)
            if (root->child(i)->checkState(0) == Qt::Unchecked)
                ++uncheckedCount;
        root->setCheckState(0, uncheckedCount == 0 ? Qt::Checked
                            : uncheckedCount == root->childCount() ? Qt::Unchecked
                            : Qt::PartiallyChecked);
    }

    mcpToolsTree->sortItems(0, Qt::AscendingOrder);
    mcpToolsTree->resizeColumnToContents(0);
}

void OpsEkos::onMCPToolItemChanged()
{
    // A family toggle cascades one itemChanged per child; coalesce the burst
    // into a single save on the next event-loop pass.
    if (m_MCPToolsSavePending)
        return;
    m_MCPToolsSavePending = true;

    QTimer::singleShot(0, this, [this]()
    {
        m_MCPToolsSavePending = false;

        QStringList disabled;
        for (int i = 0; i < mcpToolsTree->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *root = mcpToolsTree->topLevelItem(i);
            for (int j = 0; j < root->childCount(); ++j)
            {
                QTreeWidgetItem *leaf = root->child(j);
                const QString name = leaf->data(0, Qt::UserRole).toString();
                if (!name.isEmpty() && leaf->checkState(0) == Qt::Unchecked)
                    disabled.append(name);
            }
        }
        disabled.sort();

        QStringList current = Options::mCPDisabledTools();
        current.sort();
        if (disabled == current)
            return;

        // Live-apply like the MCP enable checkbox: the server reads the
        // option on every tools/list and tools/call, nothing to restart.
        Options::setMCPDisabledTools(disabled);
        Options::self()->save();
    });
}

void OpsEkos::setMCPState(bool listening, quint16 port, const QString &error)
{
    if (!error.isEmpty())
    {
        mcpStatusLed->setColor(Qt::red);
        mcpStatusLed->setToolTip(error);
        return;
    }
    if (listening)
    {
        mcpStatusLed->setColor(Qt::green);
        mcpStatusLed->setToolTip(i18n("Listening on port %1", QString::number(port)));
    }
    else
    {
        mcpStatusLed->setColor(Qt::gray);
        mcpStatusLed->setToolTip(i18n("Server is not running"));
    }
}

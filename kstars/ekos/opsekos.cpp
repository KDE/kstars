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

#include <KConfigDialog>
#include <KLed>
#include <KLocalizedString>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QIcon>

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

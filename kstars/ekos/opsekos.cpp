/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsekos.h"

#include "manager.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"

#include <KConfigDialog>
#include <QFileDialog>

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
}

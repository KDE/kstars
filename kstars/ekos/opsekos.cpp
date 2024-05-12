/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsekos.h"

#include "manager.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"

#include <KConfigDialog>

OpsEkos::OpsEkos() : QTabWidget(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("settings");

    connect(clearDSLRInfoB, &QPushButton::clicked, [ = ] ()
    {
        KStarsData::Instance()->userdb()->DeleteAllDSLRInfo();
    });

    connect(kcfg_EkosTopIcons, &QRadioButton::toggled, this, [this]()
    {
        if (Options::ekosTopIcons() != kcfg_EkosTopIcons->isChecked())
            KSNotification::info(i18n("You must restart KStars for this change to take effect."));
    });

    if (Options::analyzeAlternativeImageDirectory().size() == 0)
        Options::setAnalyzeAlternativeImageDirectory(QDir::homePath());

    kcfg_AnalyzeAlternativeDirectoryName->setText(Options::analyzeAlternativeImageDirectory());

    connect(analyzeAlternativeDirectoryB, &QPushButton::clicked, [this] ()
    {
        QString dir = QFileDialog::getExistingDirectory(
                          this, i18n("Set an alternate base directory for Analyze's captured images"),
                          QDir::homePath(),
                          QFileDialog::ShowDirsOnly);
        if (dir.size() > 0)
        {
            Options::setAnalyzeAlternativeImageDirectory(dir);
            kcfg_AnalyzeAlternativeDirectoryName->setText(Options::analyzeAlternativeImageDirectory());
        }
    });
    connect(kcfg_AnalyzeAlternativeDirectoryName, &QLineEdit::editingFinished, [this] ()
    {
        const QString &text = kcfg_AnalyzeAlternativeDirectoryName->text();

        QFileInfo newdir(text);
        if (text.size() > 0 && newdir.exists() && newdir.isDir())
            Options::setAnalyzeAlternativeImageDirectory(text);
        else
            kcfg_AnalyzeAlternativeDirectoryName->setText(Options::analyzeAlternativeImageDirectory());
    });
}
